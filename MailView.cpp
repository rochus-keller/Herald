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

#include "MailView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QFileInfo>
#include <QtDebug>
#include <QDir>
#include <QLocale>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QBitArray>
#include <QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>
#include <GuiTools/BottomStretcher.h>
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
#include <GuiTools/AutoMenu.h>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
#include <Txt/TextHtmlExporter.h>
#include "HeraldApp.h"
#include "TempFile.h"
#include "HeTypeDefs.h"
#include "ObjectHelper.h"
#include "MailObj.h"
#include "IcsDocument.h"
#include "UploadManager.h"
#include "ScheduleObj.h"
using namespace He;

static int s_tempFileId = 1;

MailView::MailView(Udb::Transaction *txn, QWidget *parent) :
	QFrame(parent),d_msgA(0),d_lock(false), d_txn(txn)
{
    setFrameShape( QFrame::StyledPanel );
    // setMidLineWidth( 2 );
    setFrameShadow( QFrame::Raised );
    setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Preferred );
    setAutoFillBackground( true );
    setBackgroundRole( QPalette::Light );

    QVBoxLayout* vbox = new QVBoxLayout( this );
    vbox->setMargin( 0 );
    vbox->setSpacing( 0 );

    QHBoxLayout* hbox = new QHBoxLayout();
    hbox->setMargin( 0 );
    hbox->setSpacing( 5 );
    vbox->addLayout( hbox );

    d_from = new QLabel( this );
    setLabelAtts( d_from );
    hbox->addWidget( d_from );

    hbox->addStretch();

    d_id = new QLabel( this );
    setLabelAtts( d_id );
    hbox->addWidget( d_id );

    d_sent = new QLabel( this );
    setLabelAtts( d_sent );
    hbox->addWidget( d_sent );

    d_received = new QLabel( this );
    setLabelAtts( d_received );
    hbox->addWidget( d_received );

    d_to = new QLabel( this );
    setLabelAtts( d_to );
    vbox->addWidget( d_to );

    d_cc = new QLabel( this );
    setLabelAtts( d_cc );
    vbox->addWidget( d_cc );

    d_bcc = new QLabel( this );
    setLabelAtts( d_bcc );
    vbox->addWidget( d_bcc );

    d_resentTo = new QLabel( this );
    setLabelAtts( d_resentTo );
    vbox->addWidget( d_resentTo );

    QFont tf = font();
	tf.setBold( true );
    tf.setPointSizeF( tf.pointSizeF() * 1.3 );
    d_subject = new QLabel( this );
    setLabelAtts( d_subject );
    d_subject->setFont( tf );
    d_subject->setBackgroundRole( QPalette::AlternateBase );
    d_subject->setAutoFillBackground( true );
    //d_subject->setFrameShape( QFrame::StyledPanel );
    d_subject->setWordWrap( true );
    vbox->addWidget( d_subject );

    d_body = new MailBodyBrowser( this );
    d_body->setFrameShape( QFrame::NoFrame );
    connect( d_body, SIGNAL(anchorClicked(QUrl)), this,SLOT(onAnchorClicked(QUrl)) );
    vbox->addWidget( d_body );

    Gui::TopStretcher* ts = new Gui::TopStretcher( this );
    //ts->setAutoFillBackground( true ); // TEST
    ts->setFixedHeight( HeraldApp::inst()->getSet()->value( "MailView/Height", 200 ).toInt() );
    connect( ts, SIGNAL(heightChanged(int)), this, SLOT( onBodyHeight(int) ) );
    vbox->addWidget( ts );

    d_attachments = new QListWidget( ts );
    d_attachments->setFrameShape( QFrame::NoFrame );
    d_attachments->setViewMode( QListView::ListMode );
    //d_attachments->setFlow( QListView::LeftToRight );
    d_attachments->setResizeMode( QListView::Adjust );
    d_attachments->setWrapping( true );
    d_attachments->setMovement( QListView::Static );
    d_attachments->setSelectionMode(QAbstractItemView::ExtendedSelection);
    //d_attachments->setIconSize( QSize( 16, 16 ) );
    ts->setBody( d_attachments );

    connect( d_attachments,SIGNAL(itemDoubleClicked(QListWidgetItem*)),
             this,SLOT(onDoubleClicked(QListWidgetItem*)));
    connect( d_attachments, SIGNAL(itemClicked(QListWidgetItem*)),
             this, SLOT(onAttClicked(QListWidgetItem*)) );

    Gui::AutoMenu* pop = new Gui::AutoMenu( d_attachments, true );
    pop->addCommand( tr("Open..."), this, SLOT( onOpenAttachment() ) );
    pop->addCommand( tr("Accept iCalendar..."), this, SLOT(onAcceptIcsAttachment()) );
    pop->addCommand( tr("Save As..."), this, SLOT( onSaveAttachment() ) );
    pop->addCommand( tr("Dispose File"), this, SLOT(onDispose()), tr("DEL"), true );
    pop->addCommand( tr("Copy Ref."), this,SLOT(onCopyAtt()), tr("CTRL+C"), true );
    pop->addCommand( tr("Acquire File..."), this, SLOT( onAcquireFile() ) );
    pop->addCommand( tr("List Inlines"), this, SLOT( onListInlines() ) );

    d_listInlines = HeraldApp::inst()->getSet()->value("MailView/ListInlines" ).toBool();

}

void MailView::addCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Show in Window..."), this, SLOT( onShowInWindow() ), tr("CTRL+O"), true );
    pop->addCommand( tr("Show in Browser..."), this, SLOT( onShowInBrowser() ), tr("CTRL+B"), true );
    pop->addCommand( tr("Show Source..."), this, SLOT( onShowSource() ) );
    pop->addCommand( tr("Save as..."), this, SLOT( onSaveAs() ) );
    pop->addCommand( tr("Show Plain Text"), this, SLOT( onPlainText() ) );
    pop->addCommand( tr("Copy Ref."), this,SLOT(onCopy()), tr("CTRL+SHIFT+C"), true );
    pop->addSeparator();
    pop->addCommand( tr("List Inlines"), this, SLOT( onListInlines() ) ); // ->setCheckable(true);
    pop->addCommand( tr("Open Attachment"), this, SLOT( onOpenAttachment() ) );
}

bool MailView::showMail(const QString &path )
{
    QFileInfo info( path );
    if( !info.exists() )
        return false;
    if( d_msgA && d_msgA->getFileStreamPath() == path )
        return false;
    MailMessage* msg = new MailMessage(this);
    msg->fromRFC822( LongString( path, false ) ); // TODO: Errors?
    msg->setReceived( info.lastModified() ); // nicht created, da Buffer viel früher erstellt werden kann als tatsächlicher Download
    msg->setFileStreamPath( path );
    return showMail( msg );
}

bool MailView::showMail(TempFile *file )
{
    if( file == 0 || !file->exists() )
        return false;
    d_lock = false;
    file->close();
    file->setParent( this );

    const bool res = showMail( file->fileName() );
    d_temps.append( file );
    return res;
}

bool MailView::showMail(MailMessage * msg)
{
    d_lock = false;
    clear();
    d_msgA = msg;

    d_from->setText( tr("<b>From:</b> %1").arg( formatAddress( d_msgA->from() ) ) );
    d_from->setToolTip( d_msgA->from() );

    d_sent->setText( tr( "<b>Sent:</b> %1" ).
                     arg( HeTypeDefs::prettyDateTime( d_msgA->dateTime().toLocalTime(),
                                                      false, false ) ) );

    if( d_msgA->received().isValid() )
        d_received->setText( tr( "<b>Received:</b> %1" ).
                         arg( HeTypeDefs::prettyDateTime( d_msgA->received(),
                                                          false, false ) ) );

    QStringList l = d_msgA->to();
    formatAddressList( l );
    d_to->setText( tr("<b>To:</b> %1").arg( l.join( "; ") ) );
    d_to->setToolTip( d_msgA->to().join( QChar('\n') ) );
    d_to->setWordWrap( true );

    l = d_msgA->cc();
    formatAddressList( l );
    d_cc->setVisible( !d_msgA->cc().isEmpty() );
    d_cc->setText( tr("<b>Cc:</b> %1").arg( l.join( "; " ) ) );
    d_cc->setToolTip( d_msgA->cc().join( QChar('\n') ) );
    d_cc->setWordWrap( true );

    d_bcc->setVisible( false );
    d_resentTo->setVisible( false );

    d_subject->setText( d_msgA->subject() );

    reloadAttachments();

    d_body->showMail( d_msgA );

    if( isWindow() )
        setWindowTitle( tr("%1 - Herald").arg( d_msgA->subject() ) );
    return true;
}

static QString _formatAddrs( const MailObj::MailAddrList& addrs, bool links )
{
    QStringList res;
    foreach( MailObj::MailAddr a, addrs )
    {
        if( links )
            res.append( a.htmlLink(true) );
        else
            res.append( a.prettyNameEmail() );
    }
    if( links )
        return res.join( "; ");
    else
        return res.join( QChar('\n') );
}

bool MailView::showMail(const Udb::Obj & obj)
{
    if( d_lock )
        return false;
    Udb::Obj msg = obj;
    if( obj.getType() == TypeAttachment )
        msg = obj.getParent();
    if( !HeTypeDefs::isEmail( msg.getType() ) )
            return false;
    if( d_msgB.equals( msg ) )
    {
        if( obj.getType() == TypeAttachment )
            gotoAttachment( obj );
        return false;
    }
    clear();
    d_msgB = msg;
    MailObj mailObj = msg;

    MailObj::MailAddrList addrs;
    addrs << mailObj.getFrom();
    d_from->setText( tr("<b>From:</b> %1").arg( _formatAddrs( addrs, true ) ) );
    d_from->setToolTip( _formatAddrs( addrs, false ) );

    d_id->setText( tr("<b>ID:</b> %1").arg(HeTypeDefs::formatObjectId( msg ) ) );

    d_sent->setText( tr( "<b>Sent:</b> %1" ).
                     arg( HeTypeDefs::prettyDateTime( mailObj.getLocalSentOn(),
                                                      false, false ) ) );

    d_received->setVisible( msg.getType() == TypeInboundMessage );
    if( mailObj.hasValue( AttrReceivedOn ) )
        d_received->setText( tr( "<b>Received:</b> %1" ).
                             arg( HeTypeDefs::prettyDateTime(
                                      mailObj.getValue( AttrReceivedOn ).getDateTime(),
                                                          false, false ) ) );

    addrs = mailObj.getTo();
    d_to->setText( tr("<b>To:</b> %1").arg( _formatAddrs( addrs, true ) ) );
    d_to->setToolTip( _formatAddrs( addrs, false ) );
    d_to->setWordWrap( true );

    addrs = mailObj.getCc();
    d_cc->setVisible( !addrs.isEmpty() );
    d_cc->setText( tr("<b>Cc:</b> %1").arg( _formatAddrs( addrs, true ) ) );
    d_cc->setToolTip( _formatAddrs( addrs, false ) );
    d_cc->setWordWrap( true );

    addrs = mailObj.getBcc();
    d_bcc->setVisible( !addrs.isEmpty() );
    d_bcc->setText( tr("<b>Bcc:</b> %1").arg( _formatAddrs( addrs, true ) ) );
    d_bcc->setToolTip( _formatAddrs( addrs, false ) );
    d_bcc->setWordWrap( true );

    addrs = mailObj.getResentTo();
    d_resentTo->setVisible( !addrs.isEmpty() );
    d_resentTo->setText( tr("<b>Resent To:</b> %1").arg( _formatAddrs( addrs, true ) ) );
    d_resentTo->setToolTip( _formatAddrs( addrs, false ) );
    d_resentTo->setWordWrap( true );

    d_subject->setText( mailObj.getString( AttrText ) );

    reloadAttachments();
    d_body->showMail( mailObj );

    if( obj.getType() == TypeAttachment )
        gotoAttachment( obj );

    if( isWindow() )
        setWindowTitle( tr("%1 - Herald").arg( HeTypeDefs::formatObjectTitle( d_msgB ) ) );
    return true;
}

void MailView::clear()
{
    d_body->clear();
    d_attachments->clear();
    d_from->clear();
    d_sent->clear();
    d_received->clear();
    d_to->clear();
    d_cc->clear();
    d_bcc->clear();
    d_resentTo->clear();
    d_subject->clear();
    d_id->clear();

    if( d_msgA != 0 )
    {
        if( d_msgA->parent() == this )
            delete d_msgA;
        d_msgA = 0;
    }
    d_msgB = Udb::Obj();
    removeTemps();
}

void MailView::onLinkActivated(const QString &link)
{
    if( !d_msgB.isNull() )
    {
        if( link.startsWith( "oid:") )
        {
            Udb::Obj o = d_msgB.getObject( link.mid(4).toULongLong() );
            if( !o.isNull( true ) )
                emit sigActivated( o );
        }
    }else if( d_msgA != 0 )
    {
        if( link.startsWith( "mailto:") )
        {
            const QByteArray fixedAddr = link.mid( 7 ).toLower().trimmed().toLatin1();
            emit sigActivated( fixedAddr );
        }
    }
}

void MailView::onAnchorClicked(const QUrl & url)
{
    if( url.scheme() == QLatin1String("mailto") )
    {
        const QByteArray fixedAddr = url.path().toLower().trimmed().toLatin1();
        emit sigActivated( fixedAddr );
    }else if( url.isRelative() )
    {
        QString anch = url.toString();
        if( anch.startsWith( QChar('#') ) )
            anch = anch.mid(1);
        d_body->scrollToAnchor( anch );
    }else
        HeraldApp::openUrl( url );
}

void MailView::onOpenAttachment()
{
    ENABLED_IF( d_attachments->selectionModel()->selectedRows().size() == 1 );
    const qlonglong idx = d_attachments->currentItem()->data( Qt::UserRole ).toULongLong();
    openAttachment( idx );
}

void MailView::onSaveAttachment()
{
    ENABLED_IF( d_attachments->selectionModel()->selectedRows().size() == 1 );
    const qlonglong idx = d_attachments->currentItem()->data( Qt::UserRole ).toULongLong();
    saveAttachment( idx );
}

void MailView::onAcceptIcsAttachment()
{
    if( d_msgB.isNull() || d_msgB.getType() != TypeInboundMessage ||
        d_attachments->selectionModel()->selectedRows().size() != 1 )
        return;
    AttachmentObj att = d_msgB.getObject( d_attachments->currentItem()->data( Qt::UserRole ).toULongLong() );
    ENABLED_IF( !att.isNull() && att.getValueAsObj( AttrCausing ).isNull() );

    const QString title = tr("Accept iCalendar Attachment - Herald");
    // Bestimme zuerst den zuständigen Kalender
    MailObj mail = d_msgB;
    Udb::Obj id = mail.findIdentity();
    Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
    if( addrObj.isNull(true) )
    {
        QMessageBox::critical( this, title,
                               tr("Identity '%1' has no associated Email address.").arg(
                                   id.getString( AttrText) ) );
        return;
    }
    ScheduleObj sched = ScheduleObj::findScheduleFromAddress( addrObj );
    if( sched.isNull() )
    {
        QMessageBox::critical( this, title, tr("There is no default schedule.") );
        return;
    }
    IcsDocument ics;
    QApplication::setOverrideCursor( Qt::WaitCursor );
    if( !ics.loadFrom( att.getDocumentPath() ) )
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::critical( this, title, tr("Error loading iCalendar file: %1" ).arg( ics.getError() ) );
        return;
    }
    QList<ScheduleItemData> items = ScheduleObj::readEvents( ics, false );
    QApplication::restoreOverrideCursor();
    if( items.size() == 0 )
    {
        QMessageBox::information( this, title, tr("The iCalendar file contains no schedule items.") );
        return;
    }else if( items.size() > 1 && QMessageBox::warning( this, title,
                                                        tr("The iCalendar file contains %1 schedule items\n"
                                                        "Accepting the first one?").arg(items.size()),
                                                        QMessageBox::Ok | QMessageBox::Cancel,
                                                        QMessageBox::Ok ) == QMessageBox::Cancel)
        return;
    else if( QMessageBox::warning( this, title,tr("You are going to accept the iCalendar schedule item.\n"
                                                  "This cannot be undone."),
                                                  QMessageBox::Ok | QMessageBox::Cancel,
                                                  QMessageBox::Ok ) == QMessageBox::Cancel)
        return;
    Udb::Obj schedItem = sched.acceptScheduleItem( items.first() );
    if( schedItem.isNull() )
    {
        QMessageBox::critical( this, title, tr("The VEVENT with UID '%1' is not valid!" ).
                               arg( items.first().d_uid.data() ) );
        sched.getTxn()->rollback();
        return;
    }
    att.setValueAsObj( AttrCausing, schedItem );
    sched.commit();
    emit sigActivated( schedItem );
}

void MailView::onAcquireFile()
{
    if( d_msgB.isNull() || d_attachments->selectionModel()->selectedRows().size() != 1 )
        return;
    AttachmentObj att = d_msgB.getObject( d_attachments->currentItem()->data( Qt::UserRole ).toULongLong() );
    Udb::Obj doc;
    if( !att.isNull() )
        doc = att.getValueAsObj( AttrDocumentRef );
    ENABLED_IF( !doc.isNull() &&  doc.hasValue(AttrFilePath) );

    if( QMessageBox::warning( this, tr("Acquire Attachment File - Herald"),
                          tr("Do you want to move the attached file to the local document store? "
                            "This cannot be undone."), QMessageBox::Ok | QMessageBox::Cancel,
                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;

    QFile f( att.getDocumentPath() );
    if( !f.rename( QDir( ObjectHelper::getDocStorePath( doc.getTxn() ) ).absoluteFilePath(
                       QString("%1 %2").arg( doc.getString( AttrInternalId ) ).
                        arg(doc.getString(AttrText)) ) ) )
        return;
    doc.clearValue(AttrFilePath);
    doc.commit();
}

void MailView::onListInlines()
{
    CHECKED_IF( true, d_listInlines );

    d_listInlines = !d_listInlines;
    HeraldApp::inst()->getSet()->setValue("MailView/ListInlines", d_listInlines );
    reloadAttachments();
}

void MailView::onCopy()
{
    ENABLED_IF( !d_msgB.isNull() );
    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    objs.append( d_msgB );
    HeTypeDefs::writeObjectRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );
}

void MailView::onCopyAtt()
{
    ENABLED_IF( !d_msgB.isNull() );
    QList<Udb::Obj> objs;
    QList<QListWidgetItem *> l = d_attachments->selectedItems();
    foreach( QListWidgetItem* i, l )
        objs.append( d_msgB.getObject( i->data( Qt::UserRole ).toULongLong() ) );
    QMimeData* mime = new QMimeData();
    HeTypeDefs::writeObjectRefs( mime, objs );
    HeTypeDefs::writeAttachRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );
}

void MailView::onDispose()
{
    QList<QListWidgetItem *> l = d_attachments->selectedItems();
    const bool disposableA = d_msgA != 0 && !d_msgA->getFileStreamPath().isNull() &&
            d_attachments->currentItem();
    const bool disposedA = d_msgA != 0 && l.size() == 1 && d_attachments->currentItem()->data(Qt::UserRole + 1).toBool();
    CHECKED_IF( disposableA || ( !d_msgB.isNull() && l.size() == 1 ), disposedA );
    if( disposableA )
    {
        foreach( QListWidgetItem* i, l )
        {
            i->setData( Qt::UserRole + 1, !i->data(Qt::UserRole + 1).toBool() );
            if( i->data(Qt::UserRole + 1).toBool() )
            {
                QFont strikeout = d_attachments->font();
                strikeout.setStrikeOut( true );
                i->setFont( strikeout );
            }else
                i->setFont( d_attachments->font() );

        }
        QBitArray disposed(d_msgA->messagePartCount());
        for( int i = 0; i < d_attachments->count(); i++ )
        {
            const int att = d_attachments->item(i)->data( Qt::UserRole ).toInt();
            Q_ASSERT( att < disposed.size() );
            disposed[att] = d_attachments->item(i)->data( Qt::UserRole + 1 ).toBool();
        }
        QFileInfo info( d_msgA->getFileStreamPath() );
        HeraldApp::inst()->getSet()->setValue( QString("Inbox/%1").arg(info.baseName()), disposed );
    }else if( !d_msgB.isNull() && l.size() == 1 )
    {
        AttachmentObj att = d_msgB.getObject( l.first()->data( Qt::UserRole ).toULongLong() );
        Udb::Obj doc = att.getValueAsObj( AttrDocumentRef );
        Udb::Idx i1( doc.getTxn(), IndexDefs::IdxDocumentRef );
        int used = 0;
        if( i1.seek( doc ) ) do
        {
            used++;
        }while( i1.nextKey() );
        if( QMessageBox::warning( this, tr("Delete Attachment - Herald"),
                              tr("Do you want to delete the attached file?\n"
                                 "It is referenced by %1 attachments.").arg(used), QMessageBox::Ok | QMessageBox::Cancel,
                              QMessageBox::Ok ) == QMessageBox::Cancel )
            return;
        QFile f( att.getDocumentPath() );
        if( f.remove() )
        {
            QFont font = l.first()->font();
            font.setStrikeOut(true);
            l.first()->setFont(font);
        }
    }
}

static QString _escapeFileName( QString name )
{
    //< > ? " : | \ / *
    name.replace( QChar(':'), QChar('_') );
    name.replace( QChar('"'), QChar('_') );
    name.replace( QChar('?'), QChar('_') );
    name.replace( QChar('<'), QChar('_') );
    name.replace( QChar('>'), QChar('_') );
    name.replace( QChar('|'), QChar('_') );
    name.replace( QChar('/'), QChar('_') );
    name.replace( QChar('\\'), QChar('_') );
    name.replace( QChar('*'), QChar('_') );
    name.replace( QChar('\t'), QChar('_') );
    return name;
}

static QString _stripBody( const QString& str )
{
    const int lhs = str.indexOf( QLatin1String("<body"), Qt::CaseInsensitive );
    const int rhs = str.indexOf( QLatin1String("</body"), Qt::CaseInsensitive );
    const int lhs2 = str.indexOf( QChar('>'), lhs );
    if( lhs == -1 || rhs == -1 || lhs2 == -1 )
        return str; // Offensichtlich kein oder fehlerhaftes HTML
    return str.mid( lhs2 + 1, rhs - lhs2 + 1 );
}

void MailView::onShowInBrowser()
{
    ENABLED_IF( d_msgA != 0 || !d_msgB.isNull() );

    QString path = QDir::temp().absoluteFilePath( tr("%1 %2.html").arg( s_tempFileId++ ).arg(
        _escapeFileName( d_subject->text() ) ) );
    TempFile* file = new TempFile( path, this );
    if( !file->open( QIODevice::WriteOnly ) )
    {
        QMessageBox::critical( this, tr( "Show in Browser" ),
            tr( "Cannot create temporary file for attachment" ) );
        delete file;
        return;
    }

    QString html = QString( "<html>\n"
        "<META http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
        "<head><title>%1</title></head>\n"
        "<body><hr>\n" ).arg( d_subject->text().toHtmlEscaped() ); //
    html += "<p style=\"font-size:10pt; background-color:#e8e8e8;\">\n";
    if( !d_msgB.isNull() )
        html += d_id->text() + "<br>";
    html += d_from->text() + "<br>";
    html += d_sent->text() + "<br>";
    html += d_to->text() + "<br>";
    if( d_cc->isVisible() )
        html += d_cc->text() + "<br>";
    html += "<b>Subject:</b> " + d_subject->text().toHtmlEscaped() + "</p><hr>\n";

    if( d_msgA != 0 )
    {
        if( !d_msgA->htmlBody().isEmpty() )
            html += _stripBody( d_msgA->htmlBody() );
        else
            html += MailObj::plainText2Html( d_msgA->plainTextBody() );
    }else
    {
        Stream::DataCell v = d_msgB.getValue( AttrBody );
        if( v.isHtml() )
        {
            if( d_msgB.getType() == TypeOutboundMessage )
            {
                QTextDocument doc;
                doc.setHtml( MailObj::removeAllMetaTags( v.getStr() ) );
                Txt::TextHtmlExporter hexp( &doc );
                hexp.setQtSpecifics(false);
                html += _stripBody( hexp.toHtml( "utf-8" ) );
            }else
                html += _stripBody( v.getStr() );
        }else
            html += MailObj::plainText2Html( v.getStr() );
    }
    html += "</body></html>";
    file->write( html.toUtf8() );
    file->close();
    d_temps.append( file );
    HeraldApp::openUrl( QUrl::fromLocalFile( file->fileName() ) );
}

void MailView::onShowInWindow()
{
    ENABLED_IF( !isWindow() && ( d_msgA != 0 || !d_msgB.isNull() ) );

    MailView* view = new MailView( 0 );
    view->setAttribute( Qt::WA_DeleteOnClose );
    if( d_msgA )
    {
        view->showMail(d_msgA);
        if( d_msgA->parent() == this )
        {
            d_msgA->setParent( view );
            clear();
        }
    }else
        view->showMail( d_msgB );
    Gui::AutoMenu* pop = new Gui::AutoMenu( view->getBody(), true );
    view->addCommands( pop );
    view->show();
    HeraldApp::inst()->setWindowGeometry( view );
}

void MailView::onDbUpdate(Udb::UpdateInfo info)
{
    switch( info.d_kind )
	{
    case Udb::UpdateInfo::ObjectErased:
        if( info.d_id == d_msgB.getOid() )
            clear();
        break;
    }
}

void MailView::setLabelAtts(QLabel * l)
{
    l->setIndent( 2 );
    l->setMargin( 2 );
    //l->setWordWrap( true );
    l->setTextInteractionFlags( Qt::TextBrowserInteraction );
    connect( l, SIGNAL(linkActivated(QString)), this, SLOT(onLinkActivated(QString)) );
}

void MailView::removeTemps()
{
    foreach( QFile* f, d_temps )
    {
        delete f;
    }
    d_temps.clear();
}

void MailView::onBodyHeight( int h )
{
    HeraldApp::inst()->getSet()->setValue( "MailView/Height", h );
//	if( d_body )
//        d_body->setSizeHint( 0, QSize( d_body->sizeHint(0).width(), h ) );
}

void MailView::onDoubleClicked(QListWidgetItem * item)
{
    const qlonglong idx = item->data( Qt::UserRole ).toULongLong();
    openAttachment( idx );
}

void MailView::onAttClicked(QListWidgetItem *item)
{
    struct Guard
    {
        bool* lock;
        Guard( bool* l ):lock(l){}
        ~Guard() { *lock = false; }
    };

    d_lock = true;
    Guard g( &d_lock );
    const qlonglong oid = item->data( Qt::UserRole ).toULongLong();
    if( d_msgB.isNull() )
        return;
    AttachmentObj att = d_msgB.getObject( oid );
    Q_ASSERT( att.getType() == TypeAttachment );
    emit sigActivated( att );
}

void MailView::openAttachment(qlonglong i)
{
    if( d_msgA )
    {
		const MailMessagePart& part = d_msgA->messagePartAt( i );
        QFileInfo name( QDir::temp().absoluteFilePath( part.name() ) );
		if( name.fileName() == "smime.p7m" )
		{
			if( d_txn != 0 && d_msgA->isEncrypted() )
			{
				MailObj::findPrivateKey( d_txn, d_msgA );
				MailMessage* msg = 0;
				msg = MailMessage::createDecrypted( d_msgA, false );
				if( msg == 0 )
				{
					QMessageBox::warning( this, tr( "Open Attachment" ), d_msgA->getError() );
					return;
				}
				msg->setParent( this );
				showMail( msg );
				return;
			}else if( d_msgA->isSigned() )
			{
				MailMessage* msg = 0;
				msg = MailMessage::createUnpacked( d_msgA );
				if( msg == 0 )
				{
					QMessageBox::warning( this, tr( "Open Attachment" ), d_msgA->getError() );
					return;
				}
				msg->setParent( this );
				showMail( msg );
				return;
			}
		}
		TempFile* file = new TempFile( name.filePath(), this );
		if( !file->open( QIODevice::WriteOnly ) )
        {
            QMessageBox::critical( this, tr( "Open Attachment" ),
                tr( "Cannot create temporary file for attachment" ) );
            delete file;
            return;
        }
        part.decodedBody( file );
        file->close();
		if( name.isExecutable() )
		{
			QMessageBox::warning( this, tr( "Open Attachment" ),
								  tr( "Cannot open executable file '%1'" ).arg(name.filePath() ) );
			delete file;
			return;
		}
		if( name.suffix() == "eml" )
			showMail( file );
        else if( name.suffix() == "ics" || name.suffix() == "vcs" )
        {
            if( QApplication::keyboardModifiers () == Qt::ControlModifier )
            {
                showIcs( *file, part.name() );
                delete file;
            }else
            {
                emit sigShowIcd( file->fileName() );
                d_temps.append( file );
            }
		}else
        {
            d_temps.append( file );
            HeraldApp::openUrl( QUrl::fromLocalFile( file->fileName() ) );
        }
    }else if( !d_msgB.isNull() )
    {
        MailObj mailObj = d_msgB;
        AttachmentObj att = d_msgB.getObject( i );
        Q_ASSERT( att.getType() == TypeAttachment );
        QFileInfo name( att.getDocumentPath() );
        if( name.isExecutable() )
        {
            QMessageBox::warning( this, tr( "Open Attachment" ),
                                  tr( "Cannot open executable file '%1'" ).arg(name.filePath() ) );
            return;
        }
		if( name.suffix() == "eml" )
            showMail( name.filePath() );
        else if( name.suffix() == "ics" || name.suffix() == "vcs" )
        {
            if( QApplication::keyboardModifiers () == Qt::ControlModifier )
            {
                QFile f( name.filePath() );
                showIcs( f, mailObj.getString( AttrText ) );
            }else
            {
                // ich habe recherchiert; die meisten vcs-Dateien sind sogar VERSION:2.0, also identisch mit ics
                emit sigShowIcd( name.filePath() );
            }
        }else
            HeraldApp::openUrl( QUrl::fromLocalFile( name.filePath() ) );
    }
}

void MailView::showIcs(QFile & f, const QString &title)
{
    if( !f.open( QIODevice::ReadOnly ) )
        return;

    IcsDocument doc;
    if( !doc.loadFrom( f ) )
    {
        QMessageBox::critical( this, tr("Show iCalendar File"), doc.getError() );
        return;
    }
    QString html;
    QTextStream out( &html );
    doc.writeHtml( out );
    HeraldApp::inst()->openHtmlBrowser( html, title );
}

void MailView::saveAttachment(qlonglong i)
{
    if( d_msgA )
    {
        const MailMessagePart& part = d_msgA->messagePartAt( i );
        QFileInfo info( part.name() );
        QString path = QFileDialog::getSaveFileName( this, tr("Save Attachment - Herald"),
                part.name(), tr("*.%1").arg( info.suffix() ) );
        if( path.isEmpty() )
            return;
        QFile file( path );
        file.open( QIODevice::WriteOnly );
        part.decodedBody( &file );
    }else if( !d_msgB.isNull() )
    {
        MailObj mailObj = d_msgB;
        AttachmentObj att = d_msgB.getObject( i );
        Q_ASSERT( att.getType() == TypeAttachment );
        QFile file( att.getDocumentPath() );
        if( !file.exists() )
            return; // RISK

        QFileInfo info( att.getDocumentPath() );
        QString path = QFileDialog::getSaveFileName( this, tr("Save Attachment - Herald"),
                        att.getString(AttrText), tr("*.%1").arg( info.suffix() ) );
        if( path.isEmpty() )
            return;
        file.copy( path );
    }
}

void MailView::reloadAttachments()
{
    d_attachments->clear();

    QLocale loc;
    int count = 0;
    if( d_msgA )
    {
        QBitArray disposed;
        if( !d_msgA->getFileStreamPath().isNull() )
        {
            QFileInfo info( d_msgA->getFileStreamPath() );
            disposed = HeraldApp::inst()->getSet()->value(
                QString("Inbox/%1").arg(info.baseName()) ).toBitArray();
        }
        QFont strikeout = d_attachments->font();
        strikeout.setStrikeOut( true );
        for( quint32 i = 0; i < d_msgA->messagePartCount(); i++ )
        {
            const MailMessagePart& part = d_msgA->messagePartAt(i);
            const bool isInline = part.isInline();
            if( !isInline || d_listInlines )
            {
                QListWidgetItem* item = new QListWidgetItem( d_attachments );
                item->setText( tr("%1 (%2 k%3)").arg( part.prettyName() )
                               .arg( loc.toString( ( part.length() * 10 / 1024 ) * 0.1 ) ).
                               arg( (isInline)?", inline":"" ) );
                QFile file( QDir::temp().absoluteFilePath( part.name() ) );
                file.open( QIODevice::WriteOnly );
                // Funktioniert nur mit existierenden Dateien; darum Trick mit QFile
                item->setIcon( HeraldApp::inst()->getIconFromPath( file.fileName() ) );
                item->setData( Qt::UserRole, i );
                file.remove();
                if( i < disposed.size() )
                {
                    if( disposed[i] )
                        item->setFont( strikeout );
                    item->setData( Qt::UserRole + 1, disposed.testBit(i) );
                }
                count++;
            }
        }
    }else if( !d_msgB.isNull() )
    {
        MailObj mailObj = d_msgB;
        MailObj::Attachments atts = mailObj.getAttachments();
        QFont strikeout = d_attachments->font();
        strikeout.setStrikeOut( true );
        for( int i = 0; i < atts.size(); i++ )
        {
            AttachmentObj att = atts[i];
            const bool isInline = att.isInline();
            if( !isInline || d_listInlines )
            {
                QFile file( att.getDocumentPath() );
                QListWidgetItem* item = new QListWidgetItem( d_attachments );
                item->setText( tr("%1 (%2 k%3)").arg( att.getString( AttrText ) )
                               .arg( loc.toString( ( file.size() * 10 / 1024 ) * 0.1 ) ).
                               arg( (isInline)?", inline":"" ) );
                item->setIcon( HeraldApp::inst()->getIconFromPath( file.fileName() ) );
                item->setData( Qt::UserRole, att.getOid() );
                if( !file.exists() )
                    item->setFont( strikeout );
                count++;
            }
        }
    }
    d_attachments->parentWidget()->setVisible( count > 0 );

}

void MailView::gotoAttachment(const Udb::Obj &att)
{
    for( int i = 0; i < d_attachments->count(); i++ )
    {
        if( d_attachments->item(i)->data(Qt::UserRole).toULongLong() == att.getOid() )
        {
            d_attachments->setFocus();
            d_attachments->setCurrentItem( d_attachments->item(i), QItemSelectionModel::ClearAndSelect );
            d_attachments->scrollToItem( d_attachments->item(i) );
            return;
        }
    }
}

QString MailView::formatAddress(const QString &addr)
{
    QString namePart;
    QByteArray addrPart;
    MailMessage::parseEmailAddress( addr, namePart, addrPart ); // Hier ist Unquote Name ok
    if( namePart.isEmpty() )
        namePart = addrPart;
    return tr("<a href=\"mailto:%1\">%2</a>").arg(addrPart.data()).arg(namePart);
}

void MailView::formatAddressList(QStringList & l)
{
    for( int i = 0; i < l.size(); i++ )
    {
        l[i] = formatAddress( l[i] );
    }
}

void MailView::onShowSource()
{
    ENABLED_IF( d_msgA != 0 || !d_msgB.isNull() );

    QTextBrowser* b = new QTextBrowser();
    //b->setLineWrapMode( QTextEdit::NoWrap );
    b->setAttribute( Qt::WA_DeleteOnClose );
    b->setWindowTitle( tr("Source '%1' - Herald").arg( d_subject->text() ) );
    if( d_msgA != 0 )
    {
        QString body = d_msgA->htmlBody();
        if( body.isEmpty() )
            body = d_msgA->plainTextBody();
        b->setPlainText( QString::fromLatin1( d_msgA->rawHeaders() ) + QLatin1String("\n\n") + body );
    }else
        b->setPlainText( d_msgB.getString( AttrRawHeaders ) + QLatin1String("\n\n") +
                         d_msgB.getString( AttrBody ) );
    b->show();
    HeraldApp::inst()->setWindowGeometry( b );
}

void MailView::onPlainText()
{
    ENABLED_IF( d_msgA != 0 || !d_msgB.isNull() );

    if( d_msgA != 0 )
    {
        if( !d_msgA->plainTextBody().isEmpty() )
            d_body->showPlainText( d_msgA->plainTextBody() );
        else
            d_body->showPlainText( Stream::DataCell::stripMarkup( d_msgA->htmlBody(), true ) );
    }else
    {
        QString body = d_msgB.getString( AttrBody );
        d_body->showPlainText( Stream::DataCell::stripMarkup( body ) );
    }
}

void MailView::onSaveAs()
{
    ENABLED_IF( d_msgA != 0 || !d_msgB.isNull() );

    QString to = QFileDialog::getSaveFileName( this, tr("Save Message - Herald"),
        QString(), tr("Email RfC822 (*.eml)") );
    if( to.isEmpty() )
        return;

    QFile f( to );
    if( !f.open( QIODevice::WriteOnly ) )
    {
        QMessageBox::critical( this, tr("Save Message - Herald"),
                               tr( "Cannot open file for writing: %1" ).arg( to ) );
        return;
    }
	// TODO: verschlüsseln falls nötig
    if( d_msgA != 0 )
    {
        d_msgA->toRFC822Stream( &f );
    }else
    {
        MailMessage m;
        UploadManager::renderMessageTo( d_msgB, &m, false );
        m.toRFC822Stream( &f );
    }
}
