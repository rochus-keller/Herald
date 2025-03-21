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

#include "MailEdit.h"
#include <QTextEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QToolBar>
#include <QComboBox>
#include <QLineEdit>
#include <QtDebug>
#include <QMessageBox>
#include <QImage>
#include <QBuffer>
#include <QApplication>
#include <QClipboard>
#include <QCryptographicHash>
#include <GuiTools/BottomStretcher.h>
#include <GuiTools/AutoShortcut.h>
#include <GuiTools/AutoToolBar.h>
#include <Udb/Transaction.h>
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
#include <Txt/TextHtmlExporter.h>
//#include <Txt/TextHtmlImporter.h>
#include <private/qtextdocumentfragment_p.h>
#include "HeraldApp.h"
#include "MailBodyEditor.h"
#include "HeTypeDefs.h"
#include "AddressListCtrl.h"
#include "AddressIndexer.h"
#include "MailTextEdit.h"
#include "MailObj.h"
#include "MailEditAttachmentList.h"
#include "ObjectHelper.h"
#include <QFileInfo>
using namespace He;

static QLabel* _label( const QString& text, QWidget* parent )
{
    QLabel* l = new QLabel( text, parent );
    QFont f = l->font();
    f.setBold( true );
    l->setFont( f );
    return l;
}

MailEdit::MailEdit(AddressIndexer *idx, QWidget *parent) :
	QFrame(parent), d_idx(idx),d_notify(false),d_encrypt(false),d_sign(false)
{
    Q_ASSERT( idx != 0 );

    //setFrameShape( QFrame::StyledPanel );
    //setFrameShadow( QFrame::Raised );
    //setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
    setAutoFillBackground( true );
    setBackgroundRole( QPalette::Light );

    QVBoxLayout* vbox = new QVBoxLayout( this );
    vbox->setMargin( 0 );
    vbox->setSpacing( 2 );

    QFormLayout* form = new QFormLayout();
    form->setMargin( 0 );
    form->setSpacing( 2 );
    vbox->addLayout( form );

    QHBoxLayout* hbox = new QHBoxLayout();
    hbox->setMargin(0);
    hbox->setSpacing( 2 );
    d_from = new QComboBox( this );
    // nützt nichts d_from->setBackgroundRole( QPalette::Light );
    d_from->setInsertPolicy( QComboBox::InsertAlphabetically );
    d_from->setSizeAdjustPolicy( QComboBox::AdjustToContents );
    d_from->setEditable(true);
    d_from->lineEdit()->setReadOnly(true); // Damit Hintergrund nicht grau
    hbox->addWidget( d_from );
    hbox->addStretch();

    Gui::AutoToolBar* topTb = new Gui::AutoToolBar( this );
    topTb->setIconSize( QSize(16,16) );
	topTb->addCommand( tr("Delete"), this, SLOT( onDelete() ), tr("CTRL+D") )->setIcon(QIcon(":/images/cross.png"));
	topTb->addCommand( tr("Disposition Notification"), this, SLOT( onDispoNot() ) )->setIcon(QIcon(":/images/receipt-stamp.png"));
	topTb->addCommand( tr("Encrypt"), this, SLOT( onEncrypt() ) )->setIcon(QIcon(":/images/lock.png"));
	topTb->addCommand( tr("Sign"), this, SLOT( onSign() ) )->setIcon(QIcon(":/images/stamp.png"));
	topTb->addCommand( tr("Save"), this, SLOT( onSave() ), tr("CTRL+S") )->setIcon(QIcon(":/images/disk.png"));
	topTb->addCommand( tr("Send"), this, SLOT( onSend() ) )->setIcon(QIcon(":/images/mail-send.png"));
    hbox->addWidget( topTb );

    form->addRow( _label( tr("From:"), this ), hbox );

    d_to = new MailTextEdit( d_idx->getTxn(), this );
    connect( d_to, SIGNAL(sigKeyPressed(QString)), this, SLOT(onKeyPress(QString)) );
    form->addRow( _label( tr("To:"), this ), d_to );

    d_cc = new MailTextEdit( d_idx->getTxn(), this );
    connect( d_cc, SIGNAL(sigKeyPressed(QString)), this, SLOT(onKeyPress(QString)) );
    form->addRow( _label( tr("Cc:"), this ), d_cc );

    d_bcc = new MailTextEdit( d_idx->getTxn(), this );
    connect( d_bcc, SIGNAL(sigKeyPressed(QString)), this, SLOT(onKeyPress(QString)) );
    form->addRow( _label( tr("Bcc:"), this ), d_bcc );

    d_subject = new Txt::TextEdit( this );
    QFont tf = font();
	tf.setBold( true );
    tf.setFamily( "Arial" );
    tf.setPointSizeF( tf.pointSizeF() * 1.5 );
    d_subject->setDefaultFont( tf );

    d_subject->setBackgroundRole( QPalette::AlternateBase );
    d_subject->setAutoFillBackground( true );
    //d_subject->setFrameShape( QFrame::StyledPanel );
    vbox->addWidget( d_subject );

    QToolBar* tb = new QToolBar( this );
    tb->setIconSize( QSize(16,16) );
    vbox->addWidget( tb );

    d_body = new MailBodyEditor( this );
    d_body->setDefaultFont( QFont("Arial", 10 ));
    d_body->fillToolBar( tb );
    vbox->addWidget( d_body );

    Gui::TopStretcher* ts = new Gui::TopStretcher( this );
    ts->setFixedHeight( HeraldApp::inst()->getSet()->value( "MailEdit/Height", 200 ).toInt() );
    connect( ts, SIGNAL(heightChanged(int)), this, SLOT( onBodyHeight(int) ) );
    vbox->addWidget( ts );

    d_attachments = new MailEditAttachmentList( ts );
    d_attachments->setFrameShape( QFrame::NoFrame );
    d_attachments->setViewMode( QListView::ListMode );
    d_attachments->setResizeMode( QListView::Adjust );
    d_attachments->setWrapping( true );
    d_attachments->setMovement( QListView::Static );
    d_attachments->setDragDropMode( QAbstractItemView::DropOnly );
    d_attachments->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ts->setBody( d_attachments );
    d_body->setRessourceManager( d_attachments );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_attachments, true );
    pop->addCommand( tr("Paste"), d_attachments, SLOT(onPaste()), tr("CTRL+V") );
    pop->addCommand( tr("Delete..."), d_attachments, SLOT(onDelete()), tr("Del"), true );
    pop->addCommand( tr("Open..."), d_attachments, SLOT(onOpen()), tr("Return"), true );
    connect( d_attachments,SIGNAL(itemDoubleClicked(QListWidgetItem*)),
             d_attachments,SLOT(onOpen()));

    new Gui::AutoShortcut( tr("CTRL+C"), this, SLOT(onCopy()) );
    new Gui::AutoShortcut( tr("CTRL+X"), this, SLOT(onCut()) );
    new Gui::AutoShortcut( tr("CTRL+V"), this, SLOT(onPaste()) );
    new Gui::AutoShortcut( tr("CTRL+SHIFT+V"), this, SLOT(onPastePlainText()) );
    new Gui::AutoShortcut( tr("CTRL+Z"), this, SLOT(onUndo()) );
    new Gui::AutoShortcut( tr("CTRL+Y"), this, SLOT(onRedo()) );

    fillFrom();
    d_lastFrom = d_from->currentIndex();
    d_to->setFocus();
    setCaption();
}

void MailEdit::replyTo(const Udb::Obj &o, bool all)
{
    Q_ASSERT( !o.isNull() );

    clear();
    MailObj mail = o;
    d_inReplyTo = o;

	if( mail.getValue(AttrIsEncrypted).getBool() )
		d_encrypt = true;

    d_subject->setPlainText( tr("Re: ") + mail.getString( AttrText ) );

    MailObj::MailAddr replyTo = mail.getReplyTo();
    if( replyTo.d_addr.isEmpty() )
        replyTo = mail.getFrom();
    d_to->addAddress( replyTo.d_name, replyTo.d_addr );

    Udb::Obj id = mail.findIdentity();
    d_from->setCurrentIndex( d_from->findData( id.getOid() ) );
    Udb::Obj idAddr;
    if( !id.isNull() )
        idAddr = id.getValueAsObj(AttrIdentAddr);

    if( all )
    {
        MailObj::MailAddrList tos = mail.getTo();
        foreach( MailObj::MailAddr a, tos )
        {
            // Ich sende nicht an mich selber
            if( a.d_party.getValue( AttrPartyAddr ).getOid() != idAddr.getOid() )
                d_cc->addAddress( a.d_name, a.d_addr );
        }
        MailObj::MailAddrList ccs = mail.getCc();
        foreach( MailObj::MailAddr a, ccs )
        {
            if( a.d_party.getValue( AttrPartyAddr ).getOid() != idAddr.getOid() )
                d_cc->addAddress( a.d_name, a.d_addr );
        }
    }

    generatePrefix( o );
    // Body erst nach Prefix, da ansonsten das ganze Dokumentenformat von Body bestimmt
    generateBody( o );
    d_body->setFocus();

    setModified( false );
    setCaption();
}

void MailEdit::forward(const Udb::Obj &o)
{
    Q_ASSERT( !o.isNull() );

    clear();
    MailObj mail = o;
    d_forwardOf = o;

	if( mail.getValue(AttrIsEncrypted).getBool() )
		d_encrypt = true;

    d_subject->setPlainText( tr("Fwd: ") + mail.getString( AttrText ) );

    Udb::Obj id = mail.findIdentity();
    d_from->setCurrentIndex( d_from->findData( id.getOid() ) );

    generatePrefix( o );
    // Body erst nach Prefix, da ansonsten das ganze Dokumentenformat von Body bestimmt
    generateBody( o );
    d_to->setFocus();

    MailObj::Attachments atts = mail.getAttachments();
    foreach( AttachmentObj a, atts )
    {
        d_attachments->addFile( a.getDocumentPath(), a.getString(AttrText), a.getContentId(false) );
    }
    setModified(false);
    setCaption();
}

void MailEdit::clear()
{
    d_from->setCurrentIndex( d_from->findData( 0 ) );
    d_lastFrom = d_from->currentIndex();
    d_to->clear();
    d_cc->clear();
    d_bcc->clear();
    d_subject->clear();
    d_body->clear();
    d_attachments->clear();
    d_notify = false;
	d_encrypt = false;
	d_sign = false;
    setCaption();
}

bool MailEdit::isModified() const
{
    return d_to->isModified() || d_cc->isModified() || d_bcc->isModified() ||
    d_subject->isModified() || d_body->document()->isModified() || d_attachments->isModified() ||
    d_from->currentIndex() != d_lastFrom;
}

void MailEdit::setModified(bool y)
{
    d_to->setModified( y );
    d_cc->setModified( y );
    d_bcc->setModified( y );
    d_subject->setModified( y );
    d_body->document()->setModified( y );
    d_attachments->setModified( y );
    if( y )
        d_lastFrom = -1;
    else
        d_lastFrom = d_from->currentIndex();
}

void MailEdit::saveTo(Udb::Obj &o)
{
    Q_ASSERT( !o.isNull(true, true) );
    o.setValueAsObj( AttrDraftFrom, d_idx->getTxn()->getObject(
            d_from->itemData( d_from->currentIndex() ).toULongLong() ) );
    o.setValue( AttrDraftTo, Stream::DataCell().setBml( d_to->getBml() ) );
    o.setValue( AttrDraftCc, Stream::DataCell().setBml( d_cc->getBml() ) );
    o.setValue( AttrDraftBcc, Stream::DataCell().setBml( d_bcc->getBml() ) );
    o.setString( AttrText, d_subject->getPlainText() );
    // NOTE: wenn MailEdit die Mail wieder korrekt lesen soll, muss Txt::TextHtmlExporter mit qtSpecifics=true
    // verwendet werden. Wenn es im Browser richtig angezeigt werden soll, qtSpecifics=false
    Txt::TextHtmlExporter hexp( d_body->document() );
    hexp.setQtSpecifics(true);
    o.setValue( AttrBody, Stream::DataCell().setHtml( hexp.toHtml( "utf-8" ) ) );
    if( !d_inReplyTo.isNull() )
        o.setValueAsObj( AttrInReplyTo, d_inReplyTo );
    if( !d_forwardOf.isNull() )
        o.setValueAsObj( AttrForwardOf, d_forwardOf );

    o.setValue(AttrNotifyTo, Stream::DataCell().setBool(d_notify) );
	o.setValue(AttrIsEncrypted, Stream::DataCell().setBool( d_encrypt ) );
	o.setValue(AttrIsSigned, Stream::DataCell().setBool( d_sign ) );


    Stream::DataWriter out;
    for( int i = 0; i < d_attachments->count(); i++ )
    {
        QListWidgetItem* item = d_attachments->item(i);
        out.startFrame( Stream::NameTag("att") );
        // cid wir im Stream ohne <> gespeichert!
        QByteArray cid = item->data( MailEditAttachmentList::ContentID ).toByteArray();
        QString path = item->data( MailEditAttachmentList::FilePath ).toString();
        QString name = item->data( MailEditAttachmentList::AttrName ).toString();
        if( !cid.isEmpty() )
        {
            out.writeSlot( Stream::DataCell().setAscii( cid ), Stream::NameTag("cid") );
            QImage img = d_attachments->getImage( cid );
            if( !img.isNull() && path.isEmpty() )
                out.writeSlot( Stream::DataCell().setImage( img ), Stream::NameTag("img") );
        }
        if( !path.isEmpty() )
            out.writeSlot( Stream::DataCell().setString( path ), Stream::NameTag("path") );
        if( !name.isEmpty() )
            out.writeSlot( Stream::DataCell().setString( name ), Stream::NameTag("name") );
        out.endFrame();
    }
    o.setValue( AttrDraftAttachments, out.getBml() );
}

void MailEdit::loadFrom(const Udb::Obj &draft)
{
    Q_ASSERT( !draft.isNull() );

    clear();
    d_draft = draft;
    d_from->setCurrentIndex( d_from->findData( draft.getValue( AttrDraftFrom ).getOid() ) );
    Stream::DataCell v;

    v = draft.getValue( AttrDraftTo );
    if( v.isBml() )
        d_to->setBml( v.getBml() );
    else
        d_to->setHtml( v.getStr() );
    v = draft.getValue( AttrDraftCc );
    if( v.isBml() )
        d_cc->setBml( v.getBml() );
    else
        d_cc->setHtml( v.getStr() );
    v = draft.getValue( AttrDraftBcc );
    if( v.isBml() )
        d_bcc->setBml( v.getBml() );
    else
        d_bcc->setHtml( v.getStr() );
    d_subject->setPlainText( draft.getString( AttrText ) );
    d_inReplyTo = draft.getValueAsObj( AttrInReplyTo );
    d_forwardOf = draft.getValueAsObj( AttrForwardOf );
    d_notify = draft.getValue(AttrNotifyTo).getBool();
	d_encrypt = draft.getValue(AttrIsEncrypted).getBool();
	d_sign = draft.getValue(AttrIsSigned).getBool();

    Stream::DataReader r( draft.getValue( AttrDraftAttachments ).getBml() );
    Stream::DataReader::Token t = r.nextToken();
    while( t == Stream::DataReader::BeginFrame && r.getName().getTag().equals( "att" ) )
    {
        t = r.nextToken();
        QByteArray cid;
        QString path;
        QImage img;
        QString name;
        while( t == Stream::DataReader::Slot )
        {
            if( r.getName().getTag().equals("cid") )
                cid = r.getValue().getArr();
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
        if( !path.isEmpty() )
            d_attachments->addFile( path, name, cid );
        else
            d_attachments->addImage( img, cid );
        Q_ASSERT( t == Stream::DataReader::EndFrame );
        t = r.nextToken();
    }
	QTextDocument* doc = new QTextDocument(d_body);
    //Txt::TextHtmlImporter imp( doc, draft.getString( AttrBody ) );
    QTextHtmlImporter imp( doc, draft.getString( AttrBody ), QTextHtmlImporter::ImportToDocument);
    imp.import();
	if( d_body->document() )
		d_body->document()->deleteLater(); // QT-BUG: Qt löscht nur, wenn QTextControl der Parent ist
	d_body->setDocument(doc);
    setModified(false);
    setCaption();
}

void MailEdit::onBodyHeight( int h )
{
    HeraldApp::inst()->getSet()->setValue( "MailEdit/Height", h );
}

void MailEdit::onKeyPress(const QString & str)
{
    MailTextEdit* te = dynamic_cast<MailTextEdit*>( sender() );
    Q_ASSERT( te != 0 );

    AddressSelectorPopup* asp = new AddressSelectorPopup( d_idx, this );
    connect( asp, SIGNAL(sigSelected(QString,QString)), te, SLOT(onSelected(QString,QString)) );
    asp->setText( str );
    QRect r = te->geometry();
    r.moveTo( mapToGlobal( r.bottomLeft() ) );
    r.setHeight( 200 );
    asp->setGeometry( r );
    asp->show();
}

void MailEdit::onSend()
{
    ENABLED_IF( !d_to->isEmpty() && d_from->itemData( d_from->currentIndex() ).toULongLong() != 0 );

    if( !check() )
        return;
    onSave();
    emit sigSendMessage( d_draft.getOid() );
    d_draft.clearValue( AttrDraftCommitted );
    d_draft.clearValue( AttrDraftSubmitted );
    d_draft.setValue( AttrDraftScheduled, Stream::DataCell().setBool(true) );
    d_draft.commit();
    close();

//    d_smtp->setAccount( acc.getValue(AttrSrvDomain).getArr(), acc.getValue(AttrSrvUser).getArr(),
//    acc.getValue(AttrSrvPwd).getArr(), acc.getValue(AttrSrvSsl).getBool(), acc.getValue(AttrSrvTls).getBool() );

//    MailMessage* msg = new MailMessage(this);
//    if( !renderTo( msg ) )
//    {
//        delete msg;
//        return;
//    }
//    d_smtp->addMail( msg );
//    d_smtp->transmitAllMessages();
}

void MailEdit::onSave()
{
    ENABLED_IF( isModified() );

    if( d_draft.isNull() )
    {
        Udb::Obj drafts = d_idx->getTxn()->getOrCreateObject( HeraldApp::s_drafts );
        d_draft = ObjectHelper::createObject( TypeMailDraft, drafts );
        QCryptographicHash h( QCryptographicHash::Md5 );
        h.addData( QUuid::createUuid().toString().toLatin1() );
        QByteArray id = h.result().toBase64();
        id.chop(2);
        id += "@hld";
        // NOTE: Message-IDs müssten die Form "<" addr-spec ">" haben
        // Siehe http://tools.ietf.org/html/rfc822; Google ändert ansonsten die ID!
        d_draft.setValue( AttrMessageId, Stream::DataCell().setAscii( id ) );
    }else
        d_draft.setTimeStamp(AttrModifiedOn);
    saveTo( d_draft );
    d_draft.commit();
    setModified( false );
    setCaption();

    // TEST
//    MailMessage msg;
//    if( !renderTo( &msg ) )
//        return;
//    QFile out("out.eml");
//    out.open(QIODevice::WriteOnly );
//    msg.toRFC822Stream( &out );

//    Txt::TextHtmlExporter hexp( d_body->document() );
//    hexp.setQtSpecifics(false);
//    out.write(hexp.toHtml( "utf-8" ).toUtf8() );

    // END TEST
}

void MailEdit::onDelete()
{
    ENABLED_IF( !d_draft.isNull() );

    int res = QMessageBox::warning( this, tr("Deleting Draft Message"),
    tr("Do you really want to delete this draft message? This cannot be undone."),
    QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel );
    if( res == QMessageBox::Cancel )
        return;
    setModified( false );
    d_draft.erase();
    d_idx->getTxn()->commit();
    close();
}

void MailEdit::onDispoNot()
{
    CHECKED_IF( true, d_notify );

	d_notify = !d_notify;
}

void MailEdit::onEncrypt()
{
	CHECKED_IF( true, d_encrypt );

		d_encrypt = !d_encrypt;
		if( d_encrypt )
			d_sign = !d_encrypt;
}

void MailEdit::onSign()
{
	CHECKED_IF( true, d_sign );

		d_sign = !d_sign;
		if( d_sign )
			d_encrypt = !d_sign;
}

void MailEdit::fillFrom()
{
    d_from->addItem( tr("<undefined>"), 0 );

    Udb::Obj accounts = d_idx->getTxn()->getObject( HeraldApp::s_accounts );
    Udb::Obj obj = accounts.getFirstObj();
    if( !obj.isNull() ) do
    {
        if( obj.getType() == TypeIdentity )
        {
            d_from->addItem( obj.getString( AttrText ), obj.getOid() );
        }
    }while( obj.next() );
    d_from->setCurrentIndex( d_from->findData( 0 ));
    d_from->setMaxVisibleItems( d_from->count() );
}

void MailEdit::generatePrefix(const Udb::Obj & o)
{
    MailObj mail = o;

    QTextCursor cur = d_body->textCursor();
    cur.movePosition(QTextCursor::Start);
    cur.insertText( "\n\n" );
    cur.movePosition(QTextCursor::Start);
    cur.insertText( "\n_______________________________\n" );
    //cur.insertHtml( "<hr><br>" ); funktioniert schlecht

    QTextBlockFormat block = cur.blockFormat();
    block.setBackground( d_subject->palette().color( QPalette::AlternateBase ) );
    cur.setBlockFormat( block );

    QTextCharFormat normal = cur.charFormat();
    normal.setFontPointSize( 8 );
    QTextCharFormat bold = normal;
    bold.setFontWeight( QFont::Bold );
    QTextCharFormat anchor = normal;
    anchor.setAnchor(true);
    anchor.setForeground( Qt::blue );
    anchor.setFontUnderline( true );

    MailObj::MailAddr from = mail.getFrom();
    cur.insertText( tr("From: "), bold );
    anchor.setAnchorHref( from.href() );
    cur.insertText( from.d_name, anchor );
    cur.insertText( "\n" );

    cur.insertText( tr("Sent on: "), bold );
    cur.insertText( MailMessage::buildRfC822DateString( mail.getValue(AttrSentOn).getDateTime() ), normal );
    cur.insertText( "\n" );

    MailObj::MailAddrList tos = mail.getTo();
    cur.insertText( tr("To: "), bold );
    for( int i = 0; i < tos.size(); i++ )
    {
        const MailObj::MailAddr& to = tos[i];
        if( i != 0 )
            cur.insertText( " ; ", normal );
        anchor.setAnchorHref( to.href() );
        cur.insertText( to.d_name, anchor );
    }
    cur.insertText( "\n" );

    MailObj::MailAddrList ccs = mail.getCc();
    cur.insertText( tr("Cc: "), bold );
    for( int i = 0; i < ccs.size(); i++ )
    {
        const MailObj::MailAddr& cc = ccs[i];
        if( i != 0 )
            cur.insertText( " ; ", normal );
        anchor.setAnchorHref( cc.href() );
        cur.insertText( cc.d_name, anchor );
    }
    cur.insertText( "\n" );

    cur.insertText( tr("Subject: "), bold );
    cur.insertText( mail.getString( AttrText ), normal );

}

void MailEdit::generateBody(const Udb::Obj & mail )
{
    d_body->moveCursor( QTextCursor::End );
    Stream::DataCell v = mail.getValue( AttrBody );
    if( v.isHtml() )
        d_body->insertHtml( MailObj::removeAllMetaTags( v.getStr() ) );
    else
        d_body->insertPlainText( v.getStr() );
    d_body->moveCursor( QTextCursor::Start );
}

void MailEdit::setCaption()
{
    QString title = d_subject->getPlainText();
    if( title.isEmpty() )
        setWindowTitle( tr("New Message - Herald") );
    else
        setWindowTitle( tr("New Message '%1' - Herald" ).arg( title ) );

}

bool MailEdit::check()
{
    const QString title = tr("Preparing Mail Message - Herald");

    Udb::Obj id = d_idx->getTxn()->getObject( d_from->itemData( d_from->currentIndex() ).toULongLong() );
    if( id.isNull(true) )
    {
        QMessageBox::critical( this, title,
                               tr("No identity selected!") );
        return false;
    }
    Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
    if( addrObj.isNull(true) )
    {
        QMessageBox::critical( this, title,
                               tr("Selected identity has no associated Email address!") );
        return false;
    }
	if( d_encrypt && d_sign )
	{
		QMessageBox::critical( this, title,
							   tr("Cannot encrypt and sign at the same time!") );
		return false;
	}
	if( d_sign )
	{
		const QString cert = MailObj::findCertificate( d_idx->getTxn(), addrObj.getValue(AttrEmailAddress).getArr() );
		const QString key = id.getString( AttrPrivateKey );
		if( cert.isEmpty() || key.isEmpty() )
		{
			QMessageBox::critical( this, title,  tr("Selected identity has not certificate or private key!") );
			return false;
		}
	}
    QList<MailTextEdit::Party> tos = d_to->getParties();
	if( d_encrypt && tos.size() > 1 )
	{
		// TODO: temporäre Einschränkung; später auf mehrere erweitern
		QMessageBox::critical( this, title, tr("Only one TO allowed for encrypted messages") );
		return false;
	}
	int toCount = 0;
    foreach( MailTextEdit::Party p, tos )
    {
        if( !p.d_error.isEmpty() )
        {
            QMessageBox::critical( this, title, tr("Invalid Email address in To: %1 (%2)")
                                                  .arg( p.d_name ).arg( p.d_error ) );
            return false;
        }
		if( d_encrypt && MailObj::findCertificate( d_idx->getTxn(), p.d_addr ).isEmpty() )
		{
			QMessageBox::critical( this, title, tr("Cannot send encrypted Email because %1 has no certificate" )
												  .arg( p.d_name ) );
			return false;
		}
        toCount++;
    }
    if( toCount == 0 )
    {
        QMessageBox::critical( this, title, tr("To Email address is missing" ) );
        return false;
    }

    QList<MailTextEdit::Party> ccs = d_cc->getParties();
    foreach( MailTextEdit::Party p, ccs )
    {
        if( !p.d_error.isEmpty() )
        {
            QMessageBox::critical( this, title, tr("Invalid Email address in Cc: %1 (%2)")
                                                  .arg( p.d_name ).arg( p.d_error ) );
            return false;
        }
    }
	if( d_encrypt && !ccs.isEmpty() )
	{
		QMessageBox::critical( this, title, tr("CC not allowed for encrypted messages") );
		return false;
	}

    QList<MailTextEdit::Party> bccs = d_bcc->getParties();
    foreach( MailTextEdit::Party p, bccs )
    {
        if( !p.d_error.isEmpty() )
        {
            QMessageBox::critical( this, title, tr("Invalid Email address in Bcc: %1 (%2)")
                                                  .arg( p.d_name ).arg( p.d_error ) );
            return false;
        }
    }
	if( d_encrypt && !bccs.isEmpty() )
	{
		QMessageBox::critical( this, title, tr("BCC not allowed for encrypted messages") );
		return false;
	}

    if( d_subject->getPlainText().isEmpty() )
    {
        QMessageBox::critical( this, title, tr("Message has no subject") );
        return false;
    }

    for( int i = 0; i < d_attachments->count(); i++ )
    {
        QListWidgetItem* item = d_attachments->item( i );
        if( !item->data( MailEditAttachmentList::FilePath ).isNull() )
        {
            QFileInfo info = item->data( MailEditAttachmentList::FilePath ).toString();
            if( !info.exists() )
            {
                QMessageBox::critical( this, title, tr("Attached file does not exist: %1")
                                                      .arg( info.filePath() ) );
                return false;
            }
        }
    }
    return true;
}

void MailEdit::closeEvent(QCloseEvent * e)
{
    if( isModified() )
    {
        if( parentWidget() == 0 )
        {
            int res = QMessageBox::warning( this, tr("Closing Draft Message"),
            tr("This draft message was changed. Do you want to save or discard?"),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save );
            switch( res )
            {
            case QMessageBox::Save:
                e->accept();
                onSave();
                break;
            case QMessageBox::Discard:
                e->accept();
                break;
            case QMessageBox::Cancel:
                e->ignore();
                break;
            }
        }else
            onSave();
    }else
        QFrame::closeEvent( e );
}

