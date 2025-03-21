/*
* Copyright 2013-2025 Rochus Keller <mailto:me@rochus-keller.ch>
*
* This file is part of the Herald application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.ch.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "UploadManager.h"
#include <Mail/SmtpClient.h>
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
#include <Mail/Base64Stream.h>
#include <QtDebug>
#include <QTextDocument>
#include <QFileInfo>
#include <QBuffer>
#include <Txt/TextInStream.h>
#include <Txt/TextHtmlExporter.h>
#include <Udb/Idx.h>
#include "HeTypeDefs.h"
#include "MailObj.h"
#include "MailTextEdit.h"
#include "ObjectHelper.h"
using namespace He;

UploadManager::UploadManager(Udb::Transaction* txn, QObject *parent) :
    QObject(parent),d_txn(txn)
{
    Q_ASSERT( txn != 0 );
    d_smtp = new SmtpClient( this );
    // NOTE: Test haben ergeben, dass ohne Buffer die Events blockieren während toRFC822Stream
    d_smtp->setOutboxPath( ObjectHelper::getOutboxPath( txn ) );
    connect( d_smtp, SIGNAL(notifyError(int,QString)), this, SLOT(onNotifyError(int,QString)) );
    connect( d_smtp, SIGNAL(updateStatus(int,int,QString)), this, SLOT( onUpdateStatus(int,int,QString) ) );
    connect( d_smtp, SIGNAL(uploadCommitted(MailMessage*)), this, SLOT(onUploadCommitted(MailMessage*)) );
}

bool UploadManager::resendMessage(const QString &path, const Udb::Obj &identity, const QStringList &to)
{
    if( path.isEmpty() || identity.isNull() || to.isEmpty() || d_smtp->isBusy() || !d_queue.isEmpty() )
        return false;

    if( !setAccountFromIdentity( identity ) )
        return false;

    Udb::Obj addrObj = identity.getValueAsObj(AttrIdentAddr);
    if( addrObj.isNull(true) )
    {
        onNotifyError( SmtpClient::ConfigurationError,
            tr("Identity '%1' has no SMTP account").arg( identity.getString( AttrText ) ) );
        return false;
    }

    MailMessage* msg = new MailMessage(this);
    msg->fromRFC822( LongString( path, false ) ); // TODO: Errors?
	msg->setFileStreamPath( QString() ); // da sonst SMTP die Originaldatei verwendet
    msg->setResentFrom( MailObj::formatAddress( addrObj, true ) );
    msg->setResentTo( to );
    msg->setResentDate( QDateTime::currentDateTime().toUTC() );

    emit sigStatus( tr("Resending message from %1" ).arg(msg->from() ) );
    d_smtp->addMail( msg );
	d_smtp->transmitAllMessages();
}

bool UploadManager::sendFromFile(const QString &path)
{
	if( path.isEmpty() || d_smtp->isBusy() || !d_queue.isEmpty() )
		return false;

	MailMessage* msg = new MailMessage(d_smtp);
	msg->fromRFC822( LongString( path, false ) );
	// TODO: wie erkennt man hier allfällige Einlesefehler?

	if( msg->fromEmail().isEmpty() || msg->to().isEmpty() )
	{
		emit sigError( tr("RFC822 file is invalid: %1" ).arg( path ) );
		delete msg;
		return false;
	}
	Udb::Obj id = MailObj::getOrCreateIdentity( MailObj::getEmailAddress( d_txn, msg->fromEmail() ), QString(), false );
	if( id.getType() != TypeIdentity )
	{
		emit sigError( tr("No identity exists for: %1" ).arg( msg->from() ) );
		delete msg;
		return false;
	}
	if( !setAccountFromIdentity( id ) )
		return false;

	emit sigStatus( tr("Sending message from file %1" ).arg( path ) );
	d_smtp->addMail( msg );
	d_smtp->transmitAllMessages();
	return true;
}

bool UploadManager::sendMessage(MailMessage * msg)
{
	if( msg == 0 || d_smtp->isBusy() || !d_queue.isEmpty() )
		return false;
	Udb::Obj id = MailObj::getOrCreateIdentity( MailObj::getEmailAddress( d_txn, msg->fromEmail() ), QString(), false );
	if( id.getType() != TypeIdentity )
	{
		emit sigError( tr("No identity exists for: %1" ).arg( msg->from() ) );
		delete msg;
		return false;
	}
	if( !setAccountFromIdentity( id ) )
		return false;

	emit sigStatus( tr("Sending message '%1' to '%2'" ).arg( msg->subject() ).arg( msg->to().join( ", " ) ) );
	d_smtp->addMail( msg );
	d_smtp->transmitAllMessages();
	return true;
}

bool UploadManager::resendTo(const Udb::Obj &draftParty)
{
    d_queue.push_back( draftParty.getOid() );
    sendAll();
    return true;
}

bool UploadManager::isIdle() const
{
    return !d_smtp->isBusy() && d_queue.isEmpty();
}

void UploadManager::sendAll()
{
    if( !d_queue.isEmpty() && !d_smtp->isBusy() )
    {
        Udb::Obj o = d_txn->getObject(d_queue.first());
        d_queue.pop_front();
        switch( o.getType() )
        {
        case TypeResentPartyDraft:
            if( o.hasValue( AttrDraftSubmitted ) || !o.getValue( AttrDraftScheduled ).getBool() )
            {
                // Already sent
                sendAll(); // Try another one
                return;
            }
            sendParty( o );
            break;
        case TypeMailDraft:
            if( o.hasValue( AttrDraftSubmitted ) || !o.getValue( AttrDraftScheduled ).getBool() )
            {
                // Already sent
                sendAll(); // Try another one
                return;
            }
            sendMessage( o );
            break;
        default:
            qWarning() << "onUploadCommitted: Send unknown object type" << o.getOid();
        }
    }
}

void UploadManager::onSendMessage(quint64 draftOid )
{
    d_queue.push_back( draftOid );
    sendAll();
}

void UploadManager::onUpdateStatus(int code, int msg, QString internalId )
{
    Q_ASSERT( code >= 0 );
    switch( code )
    {
    case SmtpClient::DnsLookup:
        emit sigStatus( tr("Looking up %1@%2").arg( d_smtp->getUser().data() ).
            arg( d_smtp->getServer().data() ) );
        break;
    case SmtpClient::ConnectionEstablished:
        emit sigStatus( tr("Connected to %1@%2").arg( d_smtp->getUser().data() ).
            arg( d_smtp->getServer().data() ) );
        break;
    case SmtpClient::StartMessageBody:
        if( !internalId.isEmpty() )
            emit sigStatus( tr("Sending %1").arg( internalId ) );
        break;
    case SmtpClient::BodyBytesSent:
        break; // interessiert nicht
    case SmtpClient::UploadComplete:
        if( !internalId.isEmpty() )
            emit sigStatus( tr("Email uploaded %1").arg( internalId ) );
        break;
    case SmtpClient::UploadCommitted:
        if( !internalId.isEmpty() )
            emit sigStatus( tr("Email committed %1").arg( internalId ) );
        break;
    case SmtpClient::QueueCompelete:
        emit sigStatus( tr("Queue complete, %1 messages sent").arg( msg ) );
        break;
    default:
        emit sigStatus( tr("Unknown Status %1 %2").arg( code ).arg( msg ) );
    }
}

void UploadManager::onNotifyError(int code, const QString &msg)
{
    Q_ASSERT( code < 0 );
    switch( code )
    {
    case SmtpClient::SocketError:
        emit sigError( tr("Socket error while fetching from %1@%2: %3").arg( d_smtp->getUser().data() ).
                       arg( d_smtp->getServer().data() ).arg( msg ) );
        break;
    case SmtpClient::UnknownResponse:
        emit sigError( tr("Protocol error while fetching from %1@%2: %3").arg( d_smtp->getUser().data() ).
                       arg( d_smtp->getServer().data() ).arg( msg ) );
        break;
    case SmtpClient::ConfigurationError:
        emit sigError( tr("SMTP Client not properly configured: %1").arg( msg ) );
        break;
    default:
        qWarning() << "UploadManager::onNotifyError" << tr("Unknown Error %1 %2").arg( code ).arg( msg );
    }
}

void UploadManager::onUploadCommitted(MailMessage * msg)
{
    Q_ASSERT( msg != 0 );
    if( !msg->getInternalId().isEmpty() )
    {
        if( msg->getInternalId().startsWith( "OID" ) )
        {
            Udb::Obj draftParty = d_txn->getObject( msg->getInternalId().mid( 3 ).toULongLong() );
            if( draftParty.getType() == TypeResentPartyDraft )
            {
                draftParty.setTimeStamp( AttrDraftCommitted );
                draftParty.commit(); // Falls irgend etwas während accept schief geht, kennen wir wenigstens den Stand
                if( MailObj::acceptDraftParty( draftParty, msg ) )
                    d_txn->commit();
                else
                    d_txn->rollback();
            }else
                qWarning() << "UploadManager::onUploadCommitted: unknown party oid or type" << draftParty.getOid();
        }else
        {
            Udb::Idx idx( d_txn, IndexDefs::IdxInternalId );
            if( idx.seek( Stream::DataCell().setString( msg->getInternalId() ) ) )
            {
                Udb::Obj draft = d_txn->getObject( idx.getOid() );
                if( draft.getType() == TypeMailDraft )
                {
                    draft.setTimeStamp( AttrDraftCommitted );
                    draft.commit(); // Falls irgend etwas während accept schief geht, kennen wir wenigstens den Stand
                    Udb::Obj o = MailObj::acceptOutbound( draft, msg );
                    if( !o.isNull() )
                        d_txn->commit();
                    else
                        d_txn->rollback();
                }else
                    qWarning() << "UploadManager::onUploadCommitted: unknown message oid or type" << idx.getOid();
            }else
                qWarning() << "UploadManager::onUploadCommitted: unknown message id" << msg->getInternalId();
        }
    }else
		emit sigStatus( tr("Message from %1 committed (File %2)" ).arg(msg->from()).arg(msg->getFileStreamPath()) );
    delete msg;
    sendAll();
}

static void _renderTo( QTextDocument& doc, const Udb::Obj& o, Udb::Atom attr )
{
    Txt::TextInStream in;
    doc.clear();
    in.readFromTo( o.getValue( attr ), &doc );
}

bool UploadManager::renderDraftTo(const Udb::Obj& draft, MailMessage * msg)
{
    if( draft.isNull( true, true ) )
        return false;
    const QString draftId = draft.getString( AttrInternalId );
    msg->setInternalId( draftId );

    Udb::Obj id = draft.getValueAsObj(AttrDraftFrom);
    if( id.isNull(true) )
    {
        msg->setError( tr("%1: No identity available").arg( draftId ) );
        return false;
    }
    Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
    if( addrObj.isNull(true) )
    {
        msg->setError( tr("%1: identity has no associated Email address").arg( draftId ) );
        return false;
    }
    msg->setFrom( MailObj::formatAddress( addrObj, true ) );

	if( draft.getValue(AttrIsEncrypted).getBool() )
		msg->setEncrypted(true);
	else if( draft.getValue(AttrIsSigned).getBool() )
		msg->setSigned(true);

	if( msg->isSigned() )
	{
		const QString cert = MailObj::findCertificate( d_txn, addrObj.getValue(AttrEmailAddress).getArr() );
		const QString key = id.getString( AttrPrivateKey );
		if( cert.isEmpty() || key.isEmpty() )
		{
			msg->setError( tr("Selected identity '%1' has not certificate or private key!" ).arg( msg->from() ) );
			return false;
		}
		msg->setKeyPath( key );
		msg->setCertPath( cert );
	}

    QStringList addrs;
    QTextDocument doc;
    _renderTo( doc, draft, AttrDraftTo );
    QList<MailTextEdit::Party> tos = MailTextEdit::getParties(&doc, d_txn);
    foreach( MailTextEdit::Party p, tos )
    {
        if( !p.d_error.isEmpty() )
        {
            msg->setError( tr("%3: invalid Email address in To: %1 (%2)")
                                       .arg( p.d_name ).arg( p.d_error ).arg( draftId ) );
            return false;
        }
		if( msg->isEncrypted() )
		{
			msg->setKeyPath( MailObj::findCertificate( d_txn, p.d_addr ) );
			if( msg->getKeyPath().isEmpty() )
			{
				msg->setError( tr("Cannot send encrypted Email because %1 has no certificate" )
												  .arg( p.d_name ) );
				return false;
			}
			Udb::Obj toAddrObj = MailObj::getEmailAddress( d_txn, p.d_addr );
			if( !toAddrObj.isNull() && toAddrObj.getValue(AttrUseKeyId).getBool() )
				msg->setUseKeyId(true);
		}
		addrs.append( p.render() );
    }
    if( addrs.isEmpty() )
    {
        msg->setError( tr("%1: to Email address is missing" ).arg( draftId ) );
        return false;
    }
    msg->setTo( addrs );
    addrs.clear();

    _renderTo( doc, draft, AttrDraftCc );
    QList<MailTextEdit::Party> ccs = MailTextEdit::getParties(&doc, d_txn);
    foreach( MailTextEdit::Party p, ccs )
    {
        if( !p.d_error.isEmpty() )
        {
            msg->setError( tr("%3: invalid Email address in Cc: %1 (%2)")
                                                  .arg( p.d_name ).arg( p.d_error ).arg( draftId ) );
            return false;
        }
		addrs.append( p.render() );
    }
    msg->setCc( addrs );
    addrs.clear();

    _renderTo( doc, draft, AttrDraftBcc );
    QList<MailTextEdit::Party> bccs = MailTextEdit::getParties(&doc, d_txn);
    foreach( MailTextEdit::Party p, bccs )
    {
        if( !p.d_error.isEmpty() )
        {
            msg->setError( tr("%3: invalid Email address in Bcc: %1 (%2)")
                                                  .arg( p.d_name ).arg( p.d_error ).arg( draftId ) );
            return false;
        }
        addrs.append( p.render() );
    }
    msg->setBcc( addrs );
    addrs.clear();

    if( draft.getValue( AttrNotifyTo ).getBool() )
        msg->setDispositionNotificationTo( '<' + addrObj.getValue( AttrEmailAddress ).getArr() + '>' );

    msg->setSubject( draft.getString( AttrText ) );
    msg->setMessageId( '<' +draft.getValue( AttrMessageId ).getArr() + '>');

    if( draft.getValue( AttrInReplyTo ).isOid() )
        msg->setInReplyTo( '<' + draft.getValueAsObj(AttrInReplyTo).getValue(AttrMessageId).getArr() + '>' );
    if( draft.getValue( AttrForwardOf ).isOid() )
        msg->setReferences( QList<QByteArray>() << '<' + draft.getValueAsObj(AttrForwardOf).
              getValue(AttrMessageId).getArr() + '>' );


    // NOTE: wenn MailEdit die Mail wieder korrekt lesen soll, muss Txt::TextHtmlExporter mit qtSpecifics=true
    // verwendet werden. Wenn es im Browser richtig angezeigt werden soll, qtSpecifics=false
    doc.setHtml( MailObj::removeAllMetaTags( draft.getString( AttrBody ) ) );
    Txt::TextHtmlExporter hexp( &doc );
    hexp.setQtSpecifics(false);
    msg->setHtmlBody( hexp.toHtml( "utf-8" ) );

    Stream::DataReader r( draft.getValue( AttrDraftAttachments ).getBml() );
    Stream::DataReader::Token t = r.nextToken();
    int inlineCount = 1;
    while( t == Stream::DataReader::BeginFrame && r.getName().getTag().equals( "att" ) )
    {
        t = r.nextToken();
        QByteArray cid;
        QString path;
        QString name;
        QImage img;
        MailMessagePart part;
        while( t == Stream::DataReader::Slot )
        {
            if( r.getName().getTag().equals("cid") )
                cid = r.getValue().getArr(); // cid kommt ohne <> aus Stream; aber part.setContentID fügt das an
            else if( r.getName().getTag().equals("img") )
            {
                if( !r.getValue().getImage( img ) )
                    qDebug() << "MailEdit::loadFrom: error loading image from database";
            }else if( r.getName().getTag().equals("path") )
                path = r.getValue().getStr();
            else if( r.getName().getTag().equals("name") )
                            name = r.getValue().getStr();

            t = r.nextToken();
        }
        part.setContentID( cid );
        if( !path.isEmpty() )
        {
            QFileInfo info(path);
            if( !info.exists() )
            {
                msg->setError( tr("%2: attached file does not exist: %1")
                                                      .arg( info.filePath() ).arg( draftId ) );
                return false;
            }
            part.setContentType( "application/octet-stream" );
            if( !name.isEmpty() )
                part.setName(name );
            else
                part.setName( info.fileName() ); // ansonsten macht z.B. Barca ".txt" daran!
            part.setFileName( info.fileName() );
            part.setSourceFilePath( info.filePath() );
            part.setDisposition( (part.contentID().isEmpty())?"attachment":"inline" );
        }else
        {
            part.setName( tr("Inline Image %1.jpg" ).arg( inlineCount++ )  );
            part.setFileName( part.name() );
            part.setDisposition( "inline" );
            part.setContentType( "image/jpeg" );
            QBuffer imgbuf;
            imgbuf.open(QIODevice::ReadWrite );
            img.save( &imgbuf, "jpeg" );
            QBuffer out;
            out.open(QIODevice::WriteOnly );
            Base64Stream s;
            imgbuf.reset();
            s.encode( &imgbuf, &out );
            out.close();
            // the encoded body is sent 1:1 by MailMessagePart, so we have to take care here
            // also for proper line breaks etc. why we use Base64Stream, not just QByteArray::toBase64().
            part.setEncodedBody( LongString( out.buffer() ), MailMessage::Base64 );
        }
        msg->addMessagePart( part );
        Q_ASSERT( t == Stream::DataReader::EndFrame );
        t = r.nextToken();
    }
    return true;
}

bool UploadManager::sendMessage(Udb::Obj draft)
{
    const QString draftId = draft.getString( AttrInternalId );

    Udb::Obj id = draft.getValueAsObj(AttrDraftFrom);
    if( id.isNull(true,true) )
    {
        onNotifyError( SmtpClient::ConfigurationError,
            tr("%1: No identity available").arg( draftId ) );
        return false;
    }
    if( !setAccountFromIdentity( id ) )
        return false;

    MailMessage* msg = new MailMessage(this);
    if( !renderDraftTo( draft, msg ) )
    {
        emit sigError( tr("sending %1: %2").arg( draftId ).arg( msg->getError() ) );
        delete msg;
        return false;
    }
    d_smtp->addMail( msg );
    d_smtp->transmitAllMessages();
    draft.setTimeStamp( AttrDraftSubmitted );
    draft.commit();
    // NOTE: bei grossen Messages kann es mehrere Minuten dauern bis vollständig in den Stream
    // geschaufelt; dann nochmals mehrere Minuten bis Commit vom Server kommt
    return true;
}

bool UploadManager::sendParty(Udb::Obj draftParty)
{
    MailMessage* msg = new MailMessage(this);
    Q_ASSERT( HeTypeDefs::isEmail( draftParty.getParent().getType() ) );
    if( !renderMessageTo( draftParty.getParent(), msg, draftParty ) )
    {
        emit sigError( tr("sending %1: %2").arg( msg->getInternalId() ).arg( msg->getError() ) );
        delete msg;
        return false;
    }

    Udb::Obj id = draftParty.getValueAsObj(AttrDraftFrom);
    Q_ASSERT( !id.isNull() );

    if( !setAccountFromIdentity( id ) )
    {
        delete msg;
        return false;
    }

    d_smtp->addMail( msg );
    d_smtp->transmitAllMessages();

    draftParty.setTimeStamp( AttrDraftSubmitted );
    draftParty.commit();
    return true;
}

bool UploadManager::setAccountFromIdentity(const Udb::Obj &id)
{
    Udb::Obj acc = id.getValueAsObj(AttrIdentSmtp);
    if( acc.isNull(true,true) )
    {
        onNotifyError( SmtpClient::ConfigurationError,
            tr("Identity '%1' has no SMTP account!").arg( id.getString( AttrText ) ) );
        return false;
    }

    int port = -1;
    if( acc.getValue(AttrSrvPort).getType() == Stream::DataCell::TypeUInt16 )
        port = acc.getValue(AttrSrvPort).getUInt16();
    d_smtp->setAccount( acc.getValue(AttrSrvDomain).getArr(), acc.getValue(AttrSrvUser).getArr(),
        acc.getValue(AttrSrvPwd).getArr(), acc.getValue(AttrSrvSsl).getBool(),
        acc.getValue(AttrSrvTls).getBool(), port );
    return true;
}

static QStringList _formatAddr( const MailObj::MailAddrList& l )
{
    QStringList res;
    foreach( MailObj::MailAddr a, l )
    {
        res.append( a.prettyNameEmail(true) );
    }
    return res;
}

bool UploadManager::renderMessageTo(const Udb::Obj& o, MailMessage * msg, bool checkAttExists )
{
    if( o.isNull( true, true ) )
        return false;
	MailObj mail = o;

    QString intId = mail.getString( AttrInternalId );
    msg->setInternalId( intId );

	if( mail.getValue(AttrIsEncrypted).getBool() )
		msg->setEncrypted(true);
	else if( mail.getValue(AttrIsSigned).getBool() )
		msg->setSigned(true);

	msg->setFrom( mail.getFrom().prettyNameEmail(true));
	const MailObj::MailAddrList to = mail.getTo(false);
	msg->setTo( _formatAddr( to ) );
	if( msg->isEncrypted() && to.size() == 1 )
	{
		msg->setKeyPath( MailObj::findCertificate( o.getTxn(), to.first().d_addr ) );
		if( msg->getKeyPath().isEmpty() )
		{
			msg->setError( tr("Cannot send encrypted Email because %1 has no certificate" )
						   .arg( to.first().d_addr.data() ) );
			return false;
		}
		Udb::Obj toAddrObj = MailObj::getEmailAddress( o.getTxn(), to.first().d_addr );
		if( !toAddrObj.isNull() && toAddrObj.getValue(AttrUseKeyId).getBool() )
			msg->setUseKeyId(true);
	}
	if( msg->isSigned() )
	{
		Udb::Obj id = mail.findIdentity(); // geht das nicht billiger?
		if( id.isNull() )
		{
			msg->setError( tr("Cannot find identity of %1" ).arg( msg->from() ) );
			return false;
		}
		const QString cert = MailObj::findCertificate( o.getTxn(), mail.getFrom().d_addr );
		const QString key = id.getString( AttrPrivateKey );
		if( cert.isEmpty() || key.isEmpty() )
		{
			msg->setError( tr("Selected identity '%1' has not certificate or private key!" ).arg( msg->from() ) );
			return false;
		}
		msg->setKeyPath( key );
		msg->setCertPath( cert );
	}

    msg->setCc( _formatAddr( mail.getCc(false) ) );
    msg->setBcc( _formatAddr( mail.getBcc(false) ) );
    if( mail.getValue( AttrNotifyTo ).getBool() )
        msg->setDispositionNotificationTo( '<' + mail.getFrom().d_addr + '>' );

    msg->setSubject( mail.getString( AttrText ) );
    msg->setMessageId( '<' + mail.getValue( AttrMessageId ).getArr() + '>' );
    msg->setDateTime( mail.getValue(AttrSentOn).getDateTime() );

    if( mail.getValue( AttrInReplyTo ).isOid() )
        msg->setInReplyTo( '<' + mail.getValueAsObj(AttrInReplyTo).getValue(AttrMessageId).getArr() + '>' );
    if( mail.getValue( AttrForwardOf ).isOid() )
        msg->setReferences( QList<QByteArray>() << '<' + mail.getValueAsObj(AttrForwardOf).
              getValue(AttrMessageId).getArr() + '>' );

    QTextDocument doc;
    doc.setHtml( MailObj::removeAllMetaTags( mail.getString( AttrBody ) ) );
    Txt::TextHtmlExporter hexp( &doc );
    hexp.setQtSpecifics(false);
    msg->setHtmlBody( hexp.toHtml( "utf-8" ) );

    MailObj::Attachments atts = mail.getAttachments();
    foreach( AttachmentObj a, atts )
    {
        MailMessagePart part;
        QByteArray cid = a.getContentId();
        QString path = a.getDocumentPath();
        bool isInline = a.isInline();
        part.setContentID( cid );
        QFileInfo info(path);
        if( checkAttExists )
        {
            if( !info.exists() )
            {
                msg->setError( tr("%2: attached file does not exist: %1")
                               .arg( info.filePath() ).arg( intId ) );
                return false;
            }else
                continue;
        }
        part.setContentType( "application/octet-stream" );
        part.setName( a.getString( AttrText ) ); // ansonsten macht z.B. Barca ".txt" daran!
        part.setFileName( info.fileName() );
        part.setSourceFilePath( info.filePath() );
        part.setDisposition( (isInline)?"inline":"attachment" );
        msg->addMessagePart( part );
    }
    return true;
}

bool UploadManager::renderMessageTo(const Udb::Obj& o, MailMessage * msg, const Udb::Obj& draftParty)
{
    Q_ASSERT( !draftParty.isNull() );
    if( !renderMessageTo( o, msg, false ) )
        return false;

    msg->setInternalId( tr("OID%1").arg( draftParty.getOid() ) );
    msg->setDispositionNotificationTo( QString() ); // keine Notification bei resend

    if( !draftParty.isNull() )
    {
        Udb::Obj id = draftParty.getValueAsObj(AttrDraftFrom);
        if( id.isNull(true,true) )
        {
            onNotifyError( SmtpClient::ConfigurationError,
                           tr("%1: No identity available").arg( msg->getInternalId() ) );
            return false;
        }
        Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
        if( addrObj.isNull(true) )
        {
            onNotifyError( SmtpClient::ConfigurationError,
                tr("%1: identity has no associated Email address").arg( msg->getInternalId() ) );
            return false;
        }
        msg->setResentFrom( MailObj::formatAddress( addrObj, true ) );

        QStringList addrs;
        QTextDocument doc;
        _renderTo( doc, draftParty, AttrDraftTo );
        QList<MailTextEdit::Party> tos = MailTextEdit::getParties(&doc, d_txn);
        foreach( MailTextEdit::Party p, tos )
        {
            if( !p.d_error.isEmpty() )
            {
                emit sigError( tr("%3: invalid Email address in To: %1 (%2)")
                                           .arg( p.d_name ).arg( p.d_error ).arg( msg->getInternalId() ) );
                return false;
            }
            addrs.append( p.render() );
        }
        if( addrs.isEmpty() )
        {
            emit sigError( tr("%1: Resend-To address is missing" ).arg( msg->getInternalId() ) );
            return false;
        }
        msg->setResentTo( addrs );
        msg->setResentDate( QDateTime::currentDateTime().toUTC() );
    }
    return true;
}

