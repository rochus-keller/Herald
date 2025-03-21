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

#include "MailObj.h"
#include <QFileInfo>
#include <QDir>
#include <QBitArray>
#include <QInputDialog>
#include <QMessageBox>
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
#include "HeTypeDefs.h"
#include "ObjectHelper.h"
#include "HeraldApp.h"
#include <QtDebug>
#include <QTextDocument> // wegen Qt::escape
using namespace He;

MailObj::MailAddr MailObj::getPartyAddr( const Udb::Obj& party, bool nameNotEmpty )
{
    MailObj::MailAddr res;
	if( party.isNull() )
		return res;
    res.d_party = party;
    Udb::Obj addr = party.getValueAsObj( AttrPartyAddr );
    res.d_name = party.getString( AttrText ); // Gebe der Bezeichnung in Party den Vorzug
    if( !addr.isNull() )
    {
        res.d_addr = addr.getValue( AttrEmailAddress ).getArr();
        if( res.d_name.isEmpty() )
            res.d_name = addr.getString( AttrText );
    }
    if( nameNotEmpty && res.d_name.isEmpty() )
        res.d_name = res.d_addr;
    return res;
}

MailObj::MailAddr MailObj::getFrom(bool nameNotEmpty) const
{
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeFromParty )
            return getPartyAddr( sub, nameNotEmpty );
    }while( sub.next() );
    return MailAddr();
}

MailObj::MailAddrList MailObj::getTo(bool nameNotEmpty) const
{
    MailAddrList list;
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeToParty )
            list.append( getPartyAddr( sub, nameNotEmpty ) );
    }while( sub.next() );
    return list;
}

MailObj::MailAddrList MailObj::getCc(bool nameNotEmpty) const
{
    MailAddrList list;
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeCcParty )
        {
            const MailAddr a = getPartyAddr( sub, nameNotEmpty );
            list.append( a );
        }
    }while( sub.next() );
    return list;
}

MailObj::MailAddrList MailObj::getBcc(bool nameNotEmpty) const
{
    MailAddrList list;
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeBccParty )
            list.append( getPartyAddr( sub, nameNotEmpty ) );
    }while( sub.next() );
    return list;
}

MailObj::MailAddrList MailObj::getResentTo(bool nameNotEmpty) const
{
    MailAddrList list;
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeResentParty )
            list.append( getPartyAddr( sub, nameNotEmpty ) );
    }while( sub.next() );
    return list;
}

MailObj::MailAddr MailObj::getReplyTo(bool nameNotEmpty) const
{
    MailAddr res;
    Udb::Obj addr = getValueAsObj(AttrReplyTo);
    if( !addr.isNull() )
    {
        res.d_name = addr.getString( AttrText );
        res.d_addr = addr.getValue( AttrEmailAddress ).getArr();
        if( res.d_name.isEmpty() && nameNotEmpty )
            res.d_name = res.d_addr;
    }
    return res;
}

MailObj::Attachments MailObj::getAttachments() const
{
    Attachments list;
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeAttachment )
            list.append( sub );
    }while( sub.next() );
    return list;
}

QDateTime MailObj::getLocalSentOn() const
{
    return getValue( AttrSentOn ).getDateTime().toLocalTime();
}

Udb::Obj MailObj::findIdentity() const
{
    Udb::Idx idx( getTxn(), IndexDefs::IdxIdentAddr );
    Udb::Obj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( HeTypeDefs::isParty( sub.getType() ) && idx.seek( sub.getValue(AttrPartyAddr) ) )
        {
            return getObject( idx.getOid() );
        }
    }while( sub.next() );
    return Udb::Obj();
}

Udb::Obj MailObj::createParty(const QByteArray &addr, const QString &name, quint32 type)
{
    return createParty( *this, addr, name, type );
}

Udb::Obj MailObj::createAttachment(const QString &filePath, const QString &name, bool acquire, bool toDispose)
{
    return createAttachment( *this, filePath, name, acquire, toDispose );
}

Udb::Obj MailObj::findAttachment(const QByteArray &contentId)
{
    // contentId hat bereits die Form <cid>; der Wert in der Datei müsste auch diese Form haben.
    AttachmentObj sub = getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeAttachment )
        {
            QByteArray cid = sub.getContentId(true);
            if( cid == contentId )
                return sub;
        }
    }while( sub.next() );
    return Udb::Obj();
}

bool MailObj::accept(MailMessage *msg)
{
	// bei Encrypted in Klartext übersetzen
	if( msg->isEncrypted() )
	{
		setValue( AttrIsEncrypted, Stream::DataCell().setBool(true ) );
		if( !findPrivateKey( getTxn(), msg ).isEmpty() )
		{
			MailMessage* msg2 = MailMessage::createDecrypted( msg );
			if( msg2 != 0 )
			{
				msg2->setParent( msg );
				msg = msg2;
			}
		}
	}else if( msg->isSigned() )
	{
		setValue( AttrIsSigned, Stream::DataCell().setBool(true ) );
		MailMessage* msg2 = MailMessage::createUnpacked( msg );
		if( msg2 != 0 )
		{
			msg2->setParent( msg );
			msg = msg2;
		}
	}

	if( !hasValue( AttrBody ) )
    {
		setString( AttrText, msg->subject() );
		if( !msg->htmlBody().isEmpty() )
            setValue( AttrBody, Stream::DataCell().setHtml(
				MailObj::removeAllMetaTags( msg->htmlBody() ) ) );
        else
			setString( AttrBody, msg->plainTextBody() );
		QByteArray messageId = msg->messageId().trimmed();
        if( !messageId.isEmpty() && messageId[0] == '<' )
            messageId = messageId.mid( 1, messageId.size() - 2 );
        setValue( AttrMessageId, Stream::DataCell().setLatin1( messageId ) );
		setValue( AttrRawHeaders, Stream::DataCell().setLatin1( msg->rawHeaders() ));
		if( msg->received().isValid() )
			setValue( AttrReceivedOn, Stream::DataCell().setDateTime( msg->received() ) );
    }
	setValue( AttrSentOn, Stream::DataCell().setDateTime( msg->dateTime() ) );
    // AttrInReplyTo
    {
        Udb::Idx idx( getTxn(), IndexDefs::IdxMessageId );
		QByteArray msgId = msg->inReplyTo().trimmed();
        if( !msgId.isEmpty() && msgId[0] == '<' )
            msgId = msgId.mid( 1, msgId.size() - 2 );
        if( idx.seek( Stream::DataCell().setLatin1(msgId) ) ) do
        {
            Udb::Obj m = getObject( idx.getOid() );
            if( HeTypeDefs::isEmail( m.getType() ) )
            {
                // Im Prinzip könnten mehrere Objekte mit derselben MessageId existieren
                setValue( AttrInReplyTo, m );
                break;
            }
        }while( idx.nextKey() );
        // else ignore
		QList<QByteArray> refs = msg->getReferences();
        int row = 0;
        foreach( QByteArray addr, refs )
        {
            if( !addr.isEmpty() && addr[0] == '<' )
                addr = addr.mid( 1, addr.size() - 2 );
            if( idx.seek( Stream::DataCell().setLatin1(addr) ) )
            {
                setCell( Udb::Obj::KeyList() << Stream::DataCell().setAtom(AttrReference) <<
                    Stream::DataCell().setUInt8( row ), Stream::DataCell().setOid( idx.getOid() ) );
                row++;
            }
        }
    }
	// From
	createParty( *this, msg->fromEmail(), msg->fromName(), TypeFromParty );
    // To
	foreach( QString s, msg->to() )
    {
        QString name;
        QByteArray addr;
        MailMessage::parseEmailAddress( s, name, addr );
        createParty( *this, addr, name, TypeToParty );
    }
    // CC
	foreach( QString s, msg->cc() )
    {
        QString name;
        QByteArray addr;
        MailMessage::parseEmailAddress( s, name, addr );
        createParty( *this, addr, name, TypeCcParty );
    }
    // BCC
	foreach( QString s, msg->bcc() )
    {
        QString name;
        QByteArray addr;
        MailMessage::parseEmailAddress( s, name, addr );
        createParty( *this, addr, name, TypeBccParty );
    }
    // ReplyTo
    {
        QString name;
        QByteArray addr;
		MailMessage::parseEmailAddress( msg->replyTo(), name, addr );
		if( !addr.isEmpty() && addr.toLower() != msg->fromEmail().toLower() )
            setValueAsObj( AttrReplyTo, MailObj::getOrCreateEmailAddress( getTxn(), addr, name ) );
    }
    // Disposition Notification
    {
        QString name;
        QByteArray addr;
		MailMessage::parseEmailAddress( msg->dispositionNotificationTo(), name, addr );
        if( !addr.isEmpty() )
            setValueAsObj( AttrNotifyTo, MailObj::getOrCreateEmailAddress( getTxn(), addr, name ) );
        // TODO: wer sendet wann Notification?
    }
	QBitArray toDispose( msg->messagePartCount() );
	if( !msg->getFileStreamPath().isNull() )
    {
		QFileInfo info( msg->getFileStreamPath() );
        toDispose = HeraldApp::inst()->getSet()->value(
            QString("Inbox/%1").arg(info.baseName()) ).toBitArray();
		if( toDispose.size() < msg->messagePartCount() )
			toDispose.resize( msg->messagePartCount() );
    }
	for( quint32 i = 0; i < msg->messagePartCount(); i++ )
    {
		const MailMessagePart& part = msg->messagePartAt(i);
        QString fileName = part.sourceFilePath();
        bool acquire = false;
        if( fileName.isEmpty() )
        {
            QFile file( QDir::temp().absoluteFilePath( QUuid::createUuid().toString() ) );
            if( !file.open( QIODevice::WriteOnly ) )
                return false;
            part.decodedBody( &file );
            fileName = file.fileName();
            acquire = true;
            file.close();
        }

        Q_ASSERT( i < toDispose.size() );
        AttachmentObj att = createAttachment( *this, fileName, part.prettyName(), acquire, toDispose[i] );
        if( att.isNull() )
            return false;
        att.setValue( AttrInlineDispo, Stream::DataCell().setBool( part.isInline() ) );
        att.setContentId( part.contentID() );
    }
	if( !msg->getFileStreamPath().isNull() )
    {
		QFileInfo info( msg->getFileStreamPath() );
        HeraldApp::inst()->getSet()->remove( QString("Inbox/%1").arg(info.baseName()) );
    }
    return true;
}

Udb::Obj MailObj::acceptInbound(Udb::Transaction* txn, const QString& path )
{
    QFileInfo info( path );
    if( !info.exists() )
        return Udb::Obj();

    Udb::Obj mailObj;
    // Hier im Scope einrücken da msg die Datei offenhält bzw. rename dann zu copy wird
    {
        MailMessage msg;
        msg.fromRFC822( LongString( info.filePath(), false ) ); // TODO: Errors?
        msg.setReceived( info.created() );

		mailObj = acceptInOrOutbound( txn, &msg, true );
    }
    if( mailObj.isNull() )
        return Udb::Obj();
    QFile file(info.filePath());
    if( !file.rename( QDir( ObjectHelper::getInboxPath( txn ) ).absoluteFilePath(
                     QString("%1.eml").arg( mailObj.getString( AttrInternalId ) ) ) ) )
        return Udb::Obj();
    return mailObj;
}

Udb::Obj MailObj::acceptInbound(Udb::Transaction* txn, const QByteArray& stream )
{
    // Obsolet?
    MailMessage msg;
    msg.fromRFC822( LongString( stream ) ); // TODO: Errors?
	return acceptInOrOutbound( txn, &msg, true );
}

Udb::Obj MailObj::acceptInOrOutbound(Udb::Transaction* txn, MailMessage *msg, bool inbound )
{
    Q_ASSERT( txn != 0 );
    MailObj mailObj = ObjectHelper::createObject( (inbound)?TypeInboundMessage:TypeOutboundMessage, txn );
    mailObj.accept( msg );
    return mailObj;
}

Udb::Obj MailObj::acceptOutbound(const Obj &draft, MailMessage *msg)
{
    MailObj mailObj = draft;
    mailObj.deaggregate();
    mailObj.setType( TypeOutboundMessage );
    mailObj.setString( AttrFromDraft, mailObj.getString( AttrInternalId ) );
    mailObj.setString( AttrInternalId, ObjectHelper::getNextIdString(
        mailObj.getTxn(), TypeOutboundMessage ) );
    mailObj.setTimeStamp( AttrCreatedOn );
    mailObj.clearValue( AttrDraftScheduled );
    mailObj.clearValue( AttrDraftAttachments );
    mailObj.clearValue( AttrDraftCommitted );
    mailObj.clearValue( AttrDraftBcc );
    mailObj.clearValue( AttrDraftCc );
    mailObj.clearValue( AttrDraftTo );
    mailObj.clearValue( AttrDraftFrom );
	if( !mailObj.accept( msg ) )
        return Udb::Obj();
    if( !msg->getFileStreamPath().isEmpty() )
    {
        msg->release();
        QFile file( msg->getFileStreamPath() );
        if( !file.rename( QDir( ObjectHelper::getOutboxPath( mailObj.getTxn() ) ).absoluteFilePath(
                         QString("%1.eml").arg( mailObj.getString( AttrInternalId ) ) ) ) )
            return Udb::Obj();
    }
    return mailObj;
}

bool MailObj::acceptDraftParty(const Udb::Obj &draftParty, MailMessage* msg)
{
    Udb::Obj partyObj = draftParty;
    partyObj.setType( TypeResentParty );
    partyObj.clearValue( AttrDraftScheduled );
    partyObj.clearValue( AttrDraftCommitted );
    partyObj.clearValue( AttrDraftTo );
    partyObj.clearValue( AttrDraftFrom );

    Udb::Obj mail = partyObj.getParent();
    Q_ASSERT( !mail.isNull() );

    for( int i = 0; i < msg->resentTo().size(); i++ )
    {
        const QString s = msg->resentTo()[i];
        QString name;
        QByteArray addr;
        MailMessage::parseEmailAddress( s, name, addr );
        if( i > 0 )
            partyObj = mail.createAggregate( TypeResentParty );
        Udb::Obj addrObj = getOrCreateEmailAddress( draftParty.getTxn(), addr, name );
        if( !addrObj.isNull() )
        {
            partyObj.setValueAsObj( AttrPartyAddr, addrObj );
            partyObj.setValueAsObj( AttrPartyPers, addrObj.getParent() );
            addrObj.incCounter( AttrUseCount );
            addrObj.setTimeStamp( AttrLastUse );
        }
        partyObj.setValue( AttrPartyDate, Stream::DataCell().setDateTime(
                QDateTime::currentDateTime().toUTC() ) );
        if( !name.isEmpty() )
            partyObj.setString( AttrText, name );
        else
            partyObj.setString( AttrText, QString::fromLatin1( addr ) );
    }
    if( !msg->getFileStreamPath().isEmpty() )
    {
        QFile file( msg->getFileStreamPath() );
        msg->release();
        QString newName = QDir( ObjectHelper::getOutboxPath( partyObj.getTxn() ) ).absoluteFilePath(
            QString("%1 Resent %2.eml").arg( mail.getString(AttrInternalId) ).arg(
            QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss" ) ) );
        if( !file.rename( newName ) )
            return false;
    }

    return true;
}

Udb::Obj MailObj::getOrCreateEmailAddress(Udb::Transaction* txn, const QByteArray &addr, const QString &name)
{
    const QByteArray fixedAddr = addr.toLower().trimmed();
    Q_ASSERT( txn != 0 );
    if( fixedAddr.isEmpty() )
        return Udb::Obj();
    Udb::Idx idx( txn, IndexDefs::IdxEmailAddress );
    Udb::Obj addrObj;
    if( idx.seek( Stream::DataCell().setLatin1( fixedAddr, false ) ) )
        addrObj = txn->getObject( idx.getOid() );
    if( addrObj.isNull() )
    {
        addrObj = ObjectHelper::createObject( TypeEmailAddress, txn );
        addrObj.setValue( AttrEmailAddress, Stream::DataCell().setLatin1( fixedAddr, false ) );
        if( name.compare( addr, Qt::CaseInsensitive ) != 0 ) // wenn name==addr speichern wir keinen Namen
            addrObj.setString( AttrText, name );
    }
    return addrObj;
}

Udb::Obj MailObj::getEmailAddress(Udb::Transaction * txn, const QByteArray &addr)
{
    QByteArray fixedAddr = addr.toLower().trimmed();
    Q_ASSERT( !fixedAddr.isEmpty() );
    if( fixedAddr[0] == '<' )
        fixedAddr = fixedAddr.mid( 1, fixedAddr.size() - 2 );
    Q_ASSERT( txn != 0 );
    Udb::Idx idx( txn, IndexDefs::IdxEmailAddress );
    if( idx.seek( Stream::DataCell().setLatin1( fixedAddr ) ) )
        return txn->getObject( idx.getOid() );
    return Udb::Obj();
}

Udb::Obj MailObj::createParty(Udb::Obj &mail, const QByteArray &addr, const QString &name, quint32 type)
{
    Q_ASSERT( !mail.isNull() );
    Udb::Obj addrObj = getOrCreateEmailAddress( mail.getTxn(), addr, name );
    Udb::Obj partyObj = mail.createAggregate( type );
    if( !addrObj.isNull() )
    {
        partyObj.setValueAsObj( AttrPartyAddr, addrObj );
        partyObj.setValueAsObj( AttrPartyPers, addrObj.getParent() );
        addrObj.incCounter( AttrUseCount );
        addrObj.setTimeStamp( AttrLastUse );
    }
    partyObj.setValue( AttrPartyDate, mail.getValue( AttrSentOn ) );
    if( !name.isEmpty() )
        partyObj.setString( AttrText, name );
    else
        partyObj.setString( AttrText, QString::fromLatin1( addr ) );

    return partyObj;
}

Udb::Obj MailObj::getOrCreateDocument(Udb::Transaction * txn, const QString &filePath,
        const QString &name, bool acquire, bool toDispose )
{
    // Diese Routine ist robust gegenüber inexistenten filePath; in diesem Fall ist hash.isEmpty()
    Q_ASSERT( txn != 0 );
    const QByteArray hash = HeTypeDefs::calcHash( filePath );
    Udb::Obj doc;
    if( hash.length() != 0 )
    {
        Udb::Idx idx( txn, IndexDefs::IdxFileHash );
        if( idx.seek( Stream::DataCell().setLob( hash ) ) )
            doc = txn->getObject( idx.getOid() );
    }
    if( doc.isNull() )
    {
        doc = ObjectHelper::createObject( TypeDocument, txn );
        doc.setValue( AttrFileHash, Stream::DataCell().setLob( hash ) );
        doc.setString( AttrText, name );
        if( acquire )
        {
            if( hash.length() != 0 )
            {
                QFile f( filePath );
                if( toDispose )
                    f.remove();
                else if( !f.rename( AttachmentObj::getFilePath( doc ) ) )
                    return Udb::Obj();
            }
        }else
            doc.setString( AttrFilePath, filePath );
    }else if( acquire )
    {
        QFile newFile( filePath );
        if( !toDispose )
        {
            QFile oldFile( AttachmentObj::getFilePath( doc ) );
            if( !oldFile.exists() )
            {
                // Wenn wir nicht wegwerfen wollen, aber die existierende Datei bereits gelöscht wurde,
                // nehmen wir stattdessen wieder die neue Datei anstelle der alten.
                newFile.rename( oldFile.fileName() );
            }else
                newFile.remove();
        }else
            newFile.remove();
    }
    return doc;
}

Udb::Obj MailObj::createAttachment(Udb::Obj &mail, const QString &filePath, const QString &name,
        bool acquire, bool toDispose )
{
    Q_ASSERT( !mail.isNull() );
    Udb::Obj doc = getOrCreateDocument( mail.getTxn(), filePath, name, acquire, toDispose );
    if( doc.isNull() )
        return Udb::Obj();
    Udb::Obj att = mail.createAggregate( TypeAttachment );
    att.setValueAsObj( AttrDocumentRef, doc );
    att.setString( AttrText, name );
    mail.incCounter( AttrAttCount );
    return att;
}

Udb::Obj MailObj::getOrCreateIdentity(const Udb::Obj &addr, const QString &name, bool create)
{
    if( addr.isNull() )
        return Udb::Obj();
    Udb::Idx idx( addr.getTxn(), IndexDefs::IdxIdentAddr );
    if( idx.seek( addr ) )
    {
        return addr.getObject( idx.getOid() );
	}else if( create )
    {
        Udb::Obj accounts = addr.getTxn()->getObject( HeraldApp::s_accounts );
        Udb::Obj id = ObjectHelper::createObject( TypeIdentity, accounts );
        id.setString( AttrText, name );
        id.setValueAsObj( AttrIdentAddr, addr );
        return id;
	}else
		return Udb::Obj();
}

QString MailObj::formatAddress(const Udb::Obj & addrOrParty, bool rfc822)
{
    if( addrOrParty.isNull() )
        return QString();
    Udb::Obj addrObj = addrOrParty;
    QString name = addrObj.getString( AttrText );
    if( HeTypeDefs::isParty( addrObj.getType() ) )
    {
        addrObj = addrObj.getValueAsObj(AttrPartyAddr );
        if( name.isEmpty() )
            name = addrObj.getString( AttrText );
    }
    QByteArray addr = addrObj.getValue( AttrEmailAddress ).getArr();
    if( rfc822 )
    {
        if( !name.isEmpty() )
            return QString( "%1 <%2>" ).arg( MailMessage::escapeDisplayName(name) ).arg( addr.data() );
        else
            return QString( "<%1>" ).arg( addr.data() );
    }else
    {
        if( !name.isEmpty() )
            return QString( "%1 [%2]" ).arg( name ).arg( addr.data() );
        else
            return addr;
    }
}

void MailObj::adjustInReplyTo(Udb::Transaction* txn)
{
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        Udb::Obj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        MailMessage m;
        m.fromRFC822( LongString( mail.getValue(AttrRawHeaders).getArr() ) );
        QByteArray messId = m.inReplyTo().trimmed();
        if( !messId.isEmpty() && messId[0] == '<' )
            messId = messId.mid( 1, messId.size() - 2 );
        Udb::Idx idx2( txn, IndexDefs::IdxMessageId );
        mail.clearValue( AttrInReplyTo );
        if( idx2.seek( Stream::DataCell().setLatin1( messId ) ) ) do
        {
            Udb::Obj m = txn->getObject( idx2.getOid() );
            if( HeTypeDefs::isEmail( m.getType() ) )
            {
                mail.setValue( AttrInReplyTo, m );
                break;
            }
        }while( idx2.nextKey() );
        QList<QByteArray> refs = m.getReferences();
        int row = 0;
        foreach( QByteArray addr, refs )
        {
            if( !addr.isEmpty() && addr[0] == '<' )
                addr = addr.mid( 1, addr.size() - 2 );
            if( idx2.seek( Stream::DataCell().setLatin1(addr) ) )
            {
                mail.setCell( Udb::Obj::KeyList() << Stream::DataCell().setAtom(AttrReference) <<
                    Stream::DataCell().setUInt8( row ), Stream::DataCell().setOid( idx2.getOid() ) );
                row++;
            }
        }

    }while( idx.next() );
    txn->commit();
}

QString MailObj::correctWordMailHtml(const QString & html)
{
    // NOTE: diese Routine scheint das Problem nur teilweise zu lösen; es gibt jedoch Dateien,
    // die von Word generiert wurden, wo an den verrücktesten Stellen <meta>-Tags auftauchen;
    // QTextBrowser kommt damit nicht zurecht und stoppt die HTML-Interpretation an diesen Stellen;
    // Folgendes z.B. nach dem Antwortheader: <meta name="Generator" content="Microsoft Word 14 (filtered medium)">
    // darum alle Calls mit removeAllMetaTags ersetzt.

    // Bei Word steht folgendes am Anfang, noch vor <html>, was falsch ist:
    //     <META HTTP-EQUIV="Content-Type" CONTENT="text/html; charset=iso-8859-1">

    const int pos1 = html.indexOf( QLatin1String("<html" ), 0, Qt::CaseInsensitive );
    const int pos2 = html.indexOf( QLatin1String("</html>" ), 0, Qt::CaseInsensitive );
    if( pos1 >= 0 )
    {
        if( pos2 > 0 )
            return html.mid( pos1, pos2 + 7 - pos1 );
        else
            return html.mid( pos1 );
    }
    // zu kompliziert bzw. zu wenig flexibel:
//    if( html.startsWith( QLatin1String( "<META HTTP-EQUIV=" ), Qt::CaseInsensitive ) )
//    {
//        const int pos = html.indexOf( QChar('>' ) );
//        if( pos != -1 )
//            return html.mid( pos + 1 );
//    }// else
    return html;
}

QString MailObj::removeAllMetaTags(QString in)
{
    QString res;
    int old = 0;
    int pos = in.indexOf( QLatin1String("<meta"), 0, Qt::CaseInsensitive );
    const int pos2 = in.indexOf( QLatin1String("</html>" ), 0, Qt::CaseInsensitive );
    while( pos != -1 && pos < pos2 )
    {
        res += in.mid( old, pos - old );
        pos = in.indexOf( QChar('>'), pos + 5 ); // die wenigsten Dateien schliessen meta mit "/>"
        if( pos == -1 )
        {
            qWarning() << "MailObj::removeAllMetaTags: invalid HTML";
            return in;
        }
        old = pos + 3;
        pos = in.indexOf( QLatin1String("<meta"), old, Qt::CaseInsensitive );
    }
    if( pos2 != -1 )
        res += in.mid( old, pos2 + 7 - old );
    else
        res += in.mid( old );
    return res;
}

QString MailObj::plainText2Html(QString str)
{
    str = str.toHtmlEscaped();
    str.replace( QLatin1String("\r\n"), QLatin1String("<br>") );
    str.replace( QLatin1String("\n"), QLatin1String("<br>") );
    return str;
}

Udb::Obj MailObj::simpleAddressEntry(QWidget * parent, Udb::Transaction* txn, const Udb::Obj & inAddr )
{
    const QString title = QObject::tr("Set Email Address - Herald");
    QString in = MailObj::formatAddress( inAddr, true );
    bool ok;
    QString str = QInputDialog::getText( parent, title, QObject::tr("RfC822 Address Format:"),
                                         QLineEdit::Normal, in, &ok );
    if( !ok || str.isEmpty() )
        return Udb::Obj();

    QString name;
    QByteArray addr;
    MailMessage::parseEmailAddress( str, name, addr );
    if( addr.isEmpty() )
    {
        QMessageBox::critical( parent, title, QObject::tr("Invalid Email Address" ) );
        return Udb::Obj();
    }
    Udb::Obj outAddr = getEmailAddress( txn, addr );
    if( outAddr.isNull() )
    {
        if( QMessageBox::warning( parent, title, QObject::tr("Do you want to create '%1'?").
                                  arg( addr.data() ), QMessageBox::Ok | QMessageBox::Cancel,
                              QMessageBox::Ok ) == QMessageBox::Cancel )
            return Udb::Obj();
        outAddr = MailObj::getOrCreateEmailAddress( txn, addr, name );
        txn->commit();
    }
	return outAddr;
}

QString MailObj::findCertificate(Udb::Transaction * txn, const QByteArray &addr)
{
	QFileInfo info = QDir( ObjectHelper::getCertDbPath( txn ) ).absoluteFilePath(
				QString("%1.pem").arg( addr.toLower().data() ) );
	if( info.exists() )
		return info.absoluteFilePath();
	else
		return QString();
}

QString MailObj::findPrivateKey(Udb::Transaction * txn, MailMessage *msg)
{
	if( txn == 0 || msg == 0 )
		return QString();
	foreach( QString s, msg->to() )
	{
		QString name;
		QByteArray addr;
		MailMessage::parseEmailAddress( s, name, addr );
		Udb::Obj id = MailObj::getOrCreateIdentity( MailObj::getEmailAddress( txn, addr ), name, false );
		if( !id.isNull() )
		{
			const QString keyFile = id.getString(AttrPrivateKey);
			if( !keyFile.isEmpty() )
			{
				msg->setKeyPath( keyFile );
				return keyFile;
			}
		}
	}
	return QString();
}

QString MailObj::MailAddr::prettyNameEmail(bool rfc822) const
{
    if( rfc822 )
    {
        if( !d_name.isEmpty() )
            return QString( "%1 <%2>" ).arg( MailMessage::escapeDisplayName(d_name) ).arg( d_addr.data() );
        else
            return QString( "<%1>" ).arg( d_addr.data() );

    }else
    {
        if( d_name.isEmpty() )
            return d_addr;
        else
            return QString( "%1 [%2]" ).arg( d_name ).arg( d_addr.data() );
    }
}

QString MailObj::MailAddr::htmlLink(bool oid) const
{
    QString name = d_name;
    if( name.isEmpty() )
        name = d_addr;
    if( oid )
        return QString("<a href=\"oid:%1\">%2</a>").arg(d_party.getOid()).arg(name);
    else
        return QString("<a href=\"mailto:%1\">%2</a>").arg(d_addr.data()).arg(name);
}

QString MailObj::MailAddr::href(bool oid) const
{
    if( oid )
        return QString("oid:%1").arg(d_party.getOid());
    else
        return QString("mailto:%1").arg(d_addr.data());
}

QString AttachmentObj::getDocumentPath() const
{
    Udb::Obj doc = getValueAsObj( AttrDocumentRef );
    Q_ASSERT( !doc.isNull() );
    return getFilePath( doc );
}

QString AttachmentObj::getFilePath(const Udb::Obj &document)
{
    QString res = HeraldApp::adjustPath(document.getString( AttrFilePath ));
    if( res.isEmpty() && !document.isNull() )
        res = QDir( ObjectHelper::getDocStorePath( document.getTxn() ) ).absoluteFilePath(
                               QString("%1 %2").arg( document.getString( AttrInternalId ) ).
                  arg( document.getString( AttrText ) ) );
    return res;
}

bool AttachmentObj::isInline() const
{
    return getValue( AttrInlineDispo ).getBool();
}

QByteArray AttachmentObj::getContentId(bool withBrackets) const
{
    QByteArray cid = getValue( AttrContentId ).getArr();
    if( !cid.isEmpty() )
    {
        if( withBrackets && !cid.startsWith('<') )
            cid = '<' + cid + '>';
        else if( !withBrackets && cid.startsWith('<') )
            cid = cid.mid(1, cid.size() - 2 );
    }
    return cid;
}

void AttachmentObj::setContentId(QByteArray cid)
{
    if( !cid.isEmpty() && cid.startsWith('<') )
        cid = cid.mid(1, cid.size() - 2 );
    setValue( AttrContentId, Stream::DataCell().setLatin1( cid ) );
}
