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

#include "EmailMainWindow.h"
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDockWidget>
#include <QCloseEvent>
#include <QTreeView>
#include <QApplication>
#include <QFontDialog>
#include <QtCore/QFileInfo>
#include <QMessageBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QDesktopWidget>
#include <QInputDialog>
#include <QDir>
#include <QDialogButtonBox>
#include <Oln2/OutlineUdbCtrl.h>
#include <Udb/Database.h>
#include <GuiTools/AutoShortcut.h>
#include <QtDebug>
//#include <Txt/TextHtmlImporter.h>
#include <Mail/MailMessage.h>
#include "HeraldApp.h"
#include "PersonListView.h"
#include "DownloadManager.h"
#include "HeTypeDefs.h"
#include "InboxCtrl.h"
#include "MailView.h"
#include "AttrViewCtrl.h"
#include "MailObj.h"
#include "ImportManager.h"
#include "AddressIndexer.h"
#include "AddressListCtrl.h"
#include "MailListCtrl.h"
#include "MailHistoCtrl.h"
#include "TextViewCtrl.h"
#include "MailEdit.h"
#include "ConfigurationDlg.h"
#include "UploadManager.h"
#include "SearchView.h"
#include "RefViewCtrl.h"
#include "ResendToDlg.h"
#include "CalMainWindow.h"
using namespace He;

class _MyDockWidget : public QDockWidget
{
public:
	_MyDockWidget( const QString& title, QWidget* p ):QDockWidget(title,p){}
	virtual void setVisible( bool visible )
	{
		QDockWidget::setVisible( visible );
		if( visible )
			raise();
	}
};

QDockWidget* EmailMainWindow::createDock( QMainWindow* parent, const QString& title, quint32 id, bool visi )
{
	QDockWidget* dock = new _MyDockWidget( title, parent );
	if( id )
		dock->setObjectName( QString( "{%1" ).arg( id, 0, 16 ) );
	else
		dock->setObjectName( QChar('[') + title );
	dock->setAllowedAreas( Qt::AllDockWidgetAreas );
	dock->setFeatures( QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
		QDockWidget::DockWidgetFloatable );
	dock->setFloating( false );
	dock->setVisible( visi );
	return dock;
}

EmailMainWindow::EmailMainWindow(Udb::Transaction * txn)
    : d_txn( txn ),d_pushBackLock(false),d_calendar(0),d_fullScreen(false)
{
    setAttribute( Qt::WA_DeleteOnClose );
    QIcon icon;
    icon.addFile( ":/images/herald_mail64.png" );
    icon.addFile( ":/images/herald_mail32.png" );
    icon.addFile( ":/images/herald_mail16.png" );
    setWindowIcon( icon );

//    d_tab = new Mp::DocTabWidget( this, false );
//	d_tab->setCloserIcon( ":/images/close.png" );
//	setCentralWidget( d_tab );

    setDockNestingEnabled(true);
    setCorner( Qt::BottomRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
	setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );

    d_dmgr = new DownloadManager( d_txn->getOrCreateObject( HeraldApp::s_inboxUuid ), this );
    connect( d_dmgr, SIGNAL(sigError(QString)), this, SLOT(onDownloadError(QString)) );
    connect( d_dmgr, SIGNAL(sigStatus(QString)), this, SLOT(onDownloadStatus(QString)) );
    d_aidx = new AddressIndexer( txn, this );

    d_umgr = new UploadManager( d_txn, this );
    connect( d_umgr, SIGNAL(sigError(QString)), this, SLOT(onUploadError(QString)) );
    connect( d_umgr, SIGNAL(sigStatus(QString)), this, SLOT(onUploadStatus(QString)) );

    setupMailView();
    setupInbox();
    setupAttrView();
    setupAddressList();
    setupMailList();
    setupMailHisto();
    setupTextView();
    setupLog();
    setupSearchView();
    setupRefView();

    QVariant state = HeraldApp::inst()->getSet()->value( "MainFrame/State/" +
		d_txn->getDb()->getDbUuid().toString() ); // Da DB-individuelle Docks
	if( !state.isNull() )
		restoreState( state.toByteArray() );

    d_calendar = 0;

    if( HeraldApp::inst()->getSet()->value( "MainFrame/State/FullScreen" ).toBool() )
    {
        toFullScreen( this );
        d_fullScreen = true;
    }else
    {
        showNormal();
        d_fullScreen = false;
    }

    setCaption();

    Gui::AutoMenu* pop;

    // Tab Menu
//	pop = new Gui::AutoMenu( d_tab, true );
//	pop->addCommand( tr("Forward Tab"), d_tab, SLOT(onDocSelect()), tr("CTRL+TAB") );
//	pop->addCommand( tr("Backward Tab"), d_tab, SLOT(onDocSelect()), tr("CTRL+SHIFT+TAB") );
//	pop->addCommand( tr("Close Tab"), d_tab, SLOT(onCloseDoc()), tr("CTRL+W") );
//    addTopCommands( pop );
//	new Gui::AutoShortcut( tr("CTRL+TAB"), this, d_tab, SLOT(onDocSelect()) );
//	new Gui::AutoShortcut( tr("CTRL+SHIFT+TAB"), this, d_tab, SLOT(onDocSelect()) );
//	new Gui::AutoShortcut( tr("CTRL+W"), this, d_tab, SLOT(onCloseDoc()) );

    pop = new Gui::AutoMenu( d_mailView->getBody(), true );
    pop->addCommand( tr("Reply..."), this,SLOT(onReply()) );
    pop->addCommand( tr("Reply All..."), this,SLOT(onReplyAll()) );
    pop->addCommand( tr("Forward..."), this,SLOT(onForward()) );
    pop->addCommand( tr("Resend..."), this,SLOT(onResend()) );
    pop->addSeparator();
    d_mailView->addCommands( pop );
    addTopCommands( pop );

    new Gui::AutoShortcut( tr("F11"), this, this, SLOT(onFullScreen()) );
    new Gui::AutoShortcut( tr("F10"), this, this, SLOT(onShowCalendar()) );
    new Gui::AutoShortcut( tr("F9"), this, this, SLOT(onFetchMail()) );
    new Gui::AutoShortcut( tr("F5"), this, this, SLOT(onShowDraft()) );
	new QShortcut( tr("CTRL+Q"), this, SLOT(close()) );
    new Gui::AutoShortcut( tr("CTRL+N"), this, this, SLOT( onNewMessage()) );
    new Gui::AutoShortcut( tr("CTRL+G"), this, this, SLOT(onSelectById()) );
    new Gui::AutoShortcut( tr("ALT+LEFT"), this, this,  SLOT(onGoBack()) );
    new Gui::AutoShortcut( tr("ALT+RIGHT"), this, this,  SLOT(onGoForward()) );
    new Gui::AutoShortcut( tr("ALT+UP"), this, this,  SLOT(onGoUpward()) );
    new Gui::AutoShortcut( tr("CTRL+F"), this, this, SLOT(onSearch()) );

    new Gui::AutoShortcut( tr("CTRL+T"), this, this, SLOT(onTest()) ); // TEST

    d_txn->commit(); // wegen getOrCreate

    //ImportManager::fixOutboundAttachments( d_txn );
    // ImportManager::checkLocalOutboundFiles(d_txn);
    // ImportManager::checkOutboundDocumentAvailability(d_txn);
    // ImportManager::checkDocumentHash( d_txn );
    // ImportManager::fixDocumentHash(d_txn);
    // ImportManager::checkDocumentAvailability(d_txn);
    //ImportManager::fixDocRedundancy(d_txn);

}

EmailMainWindow::~EmailMainWindow()
{
    if( d_txn )
        delete d_txn;
}

void EmailMainWindow::showOid(quint64 oid)
{
    Udb::Obj o = d_txn->getObject( oid );
    activateWindow();
    raise();
    onFollowObject( o );
}

void EmailMainWindow::setCaption()
{
	QFileInfo info( d_txn->getDb()->getFilePath() );
    setWindowTitle( tr("%1 - Email - Herald").arg( info.baseName() ) );
}

void EmailMainWindow::closeEvent(QCloseEvent *event)
{
    foreach( MailEdit* e, d_editors )
    {
        if( e != 0 && !e->close() )
        {
            event->ignore();
            return;
        }
    }
    if( d_calendar )
    {
        HeraldApp::inst()->getSet()->setValue("Calendar/State/" +
                                              d_txn->getDb()->getDbUuid().toString(), d_calendar->saveState() );
        d_calendar->close();
        d_calendar->deleteLater();
    }
    d_calendar = 0;

    HeraldApp::inst()->getSet()->setValue("MainFrame/State/" +
		d_txn->getDb()->getDbUuid().toString(), saveState() );
	QMainWindow::closeEvent( event );
	if( event->isAccepted() )
        emit closing();
}

void EmailMainWindow::onFullScreen()
{
    CHECKED_IF( true, d_fullScreen );
    if( d_fullScreen )
    {
#ifndef _WIN32
        if( d_calendar )
            d_calendar->setWindowFlags( Qt::Window );
        setWindowFlags( Qt::Window );
#endif
        if( d_calendar )
            d_calendar->showNormal();
		showNormal();
        d_fullScreen = false;
    }else
    {
        if( d_calendar )
            toFullScreen( d_calendar );
        toFullScreen( this );
        d_fullScreen = true;
    }
    HeraldApp::inst()->getSet()->setValue( "MainFrame/State/FullScreen", d_fullScreen );
}

void EmailMainWindow::onFetchMail()
{
    ENABLED_IF( d_dmgr->isIdle() );
    d_dmgr->fetchMail();
    d_umgr->sendAll();
}

void EmailMainWindow::onAbout()
{
	ENABLED_IF( true );

    raise();

	QMessageBox::about( this,
		tr("About Herald"),
		tr("Release: %1   Date: %2\n\n"

		"Herald can be used to manage email.\n\n"

        "Author: Rochus Keller, me@rochus-keller.ch\n"
        "Copyright (c) 2014-%3\n\n"

		"Terms of use:\n"
		"This version of Herald is freeware, i.e. it can be used for free by anyone. "
		"The software and documentation are provided \"as is\", without warranty of any kind, "
		"expressed or implied, including but not limited to the warranties of merchantability, "
		"fitness for a particular purpose and noninfringement. In no event shall the author or copyright holders be "
		"liable for any claim, damages or other liability, whether in an action of contract, tort or otherwise, "
		"arising from, out of or in connection with the software or the use or other dealings in the software." )
                        .arg( HeraldApp::s_version ).arg( HeraldApp::s_date ).arg( QDate::currentDate().year() ) );
}

void EmailMainWindow::onSetFont()
{
	ENABLED_IF( true );
	QFont f1 = QApplication::font();
	bool ok;
	QFont f2 = QFontDialog::getFont( &ok, f1, this, tr("Select Application Font - Herald" ) );
	if( !ok )
		return;
	f1.setFamily( f2.family() );
	f1.setPointSize( f2.pointSize() );
	QApplication::setFont( f1 );
	HeraldApp::inst()->setAppFont( f1 );
	// TODO d_dv->adjustFont();
    HeraldApp::inst()->getSet()->setValue( "App/Font", f1 );
}

void EmailMainWindow::onInboxSelected()
{
    // Wird nur bei Single Selection aufgerufen
    Udb::Obj mail = d_inbox->getSelectedObject();
    if( !mail.isNull() )
    {
        pushBack( mail );
        d_mailView->showMail( mail );
        d_refView->setObj( mail );
        d_attrView->setObj( mail );
        d_textView->setObj( mail );
        return;
    } // else
    QString fileName = d_inbox->getSelectedFile();
    if( !fileName.isEmpty() )
    {
        d_mailView->showMail( fileName );
        d_inbox->markSelectedAsRead();
    }
}

void EmailMainWindow::onMailListSelected()
{
    Udb::Obj mail = d_mailList->getSelectedMail();
    if( !mail.isNull() )
    {
        if( HeTypeDefs::isParty( mail.getType() ) )
            mail = mail.getParent();
        pushBack( mail );
        d_mailView->showMail( mail );
        d_refView->setObj( mail );
        d_attrView->setObj( mail );
        d_textView->setObj( mail );
    }
}

void EmailMainWindow::onMailHistoSelected()
{
    Udb::Obj mail = d_mailHisto->getSelectedMail();
    if( !mail.isNull() )
    {
        pushBack( mail );
        d_mailView->showMail( mail );
        d_refView->setObj( mail );
        d_attrView->setObj( mail );
        d_textView->setObj( mail );
    }
}

void EmailMainWindow::onFollowObject(const Udb::Obj & o)
{
    if( o.isNull( true, true ) )
        return;
    if( HeTypeDefs::isScheduleItem( o.getType() ) ||
						HeTypeDefs::isScheduleItem( o.getParent().getType() ) ||
                        o.getType() == TypeSchedule )
    {
        showCalendar();
        d_calendar->showObj( o );
        return;
    }
    pushBack( o );
    d_attrView->setObj( o );
    d_mailList->setObj( o );
    d_refView->setObj( o );
    d_textView->setObj( o );
    if( HeTypeDefs::isEmail( o.getType() ) || HeTypeDefs::isEmail( o.getParent().getType() ) )
        d_mailView->showMail( o );
}

void EmailMainWindow::onFollowEmail(const QByteArray & emailAddr )
{
    Udb::Obj o = MailObj::getEmailAddress( d_txn, emailAddr );
    onFollowObject( o );
}

void EmailMainWindow::onImportMail()
{
    ENABLED_IF(true);

#ifdef _mbx
    QString path = QFileDialog::getOpenFileName( this, tr("Import Mailbox - Herald"),
                                                 QString(),
                                                 tr("Mailbox (*.mbx)") );
    if( path.isEmpty() )
        return;
    ImportManager mgr( d_txn );
    mgr.importMbx( path );
#endif
    QString path = QFileDialog::getExistingDirectory( this, tr("Import Mailbox - Herald"),
                                                 QString(),
                                                 QFileDialog::ShowDirsOnly );
    if( path.isEmpty() )
        return;
    const int res = QMessageBox::question( this, tr("Import Mailbox - Herald"),
                           tr("What kind of emails are these?" ), tr("Inbound"), tr("Outbound"),
                           tr("Cancel"), 2, 2 );
    if( res == 2 )
        return; // cancel
    ImportManager mgr( d_txn );
    connect( &mgr,SIGNAL(sigError( const QString&)), this, SLOT(onImportError(QString)) );
    connect( &mgr,SIGNAL(sigStatus( const QString&)), this, SLOT(onImportStatus(QString)) );
    mgr.importEmlDir( path, res == 0 );

}

void EmailMainWindow::onConfig()
{
    ENABLED_IF(true);

    ConfigurationDlg dlg( d_txn, this );
    dlg.exec();
}

void EmailMainWindow::onDownloadStatus(const QString &msg)
{
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::blue );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 RCV OK %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
}

void EmailMainWindow::onDownloadError(const QString &msg)
{
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::red );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 RCV ERR %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
    const int res = QMessageBox::critical( this, tr("Mail Manager Error"),
                                       msg, tr("Continue"), tr("Cancel") );
    if( res == 0 )
        return;
    else
        d_dmgr->quit();
}

void EmailMainWindow::onNewMessage()
{
    ENABLED_IF( true );

    MailEdit* e = createMailEdit();
    e->show();

}

void EmailMainWindow::onReply()
{
    ENABLED_IF( d_mailView->getMail().getType() == TypeInboundMessage );
    MailEdit* e = createMailEdit();
    e->replyTo( d_mailView->getMail(), false );
    e->show();
}

void EmailMainWindow::onReplyAll()
{
    ENABLED_IF( d_mailView->getMail().getType() == TypeInboundMessage );
    MailEdit* e = createMailEdit();
    e->replyTo( d_mailView->getMail(), true );
    e->show();
}

void EmailMainWindow::onForward()
{
    ENABLED_IF( !d_mailView->getMail().isNull() );
    MailEdit* e = createMailEdit();
    e->forward( d_mailView->getMail() );
    e->show();
}

void EmailMainWindow::onResend()
{
    ENABLED_IF( !d_mailView->getMail().isNull() );

    ResendToDlg dlg( d_aidx, this );
    MailObj mail = d_mailView->getMail();
    dlg.setFrom( mail.findIdentity() );
    if( dlg.select() )
    {
		if( d_mailView->getMail().getValue(AttrIsEncrypted).getBool() )
		{
			QStringList addrs = dlg.getTo();
			foreach( const QString& addr, addrs )
			{
				QString name;
				QByteArray a;
				MailMessage::parseEmailAddress( addr, name, a );
				if( MailObj::findCertificate( mail.getTxn(), a ).isEmpty() )
				{
					QMessageBox::critical( this, tr("Resend Email"),
										   tr("Cannot send encrypted Email because %1 has no certificate" )
														  .arg( name ) );
					return;
				}
			}
		}
        Udb::Obj draftParty = mail.createAggregate( TypeResentPartyDraft );
        draftParty.setValueAsObj( AttrDraftFrom, dlg.getIdentity() );
        draftParty.setValue( AttrDraftTo, Stream::DataCell().setBml( dlg.getToBml() ) );
        draftParty.setValue( AttrDraftScheduled, Stream::DataCell().setBool( true ) );
        draftParty.commit();
        d_umgr->resendTo( draftParty );
    }
}

void EmailMainWindow::onShowDraft()
{
    ENABLED_IF( true );

    QDialog dlg( this );
    dlg.setWindowTitle( tr("Select Draft Message - Herald") );
    dlg.resize( 800, 600 );
    QVBoxLayout vbox( &dlg );
    QListWidget list( &dlg );
    vbox.addWidget( &list );
    QDialogButtonBox bb(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, &dlg );
    vbox.addWidget( &bb );
    connect(&bb, SIGNAL(accepted()), &dlg, SLOT(accept()));
    connect(&bb, SIGNAL(rejected()), &dlg, SLOT(reject()));
    connect( &list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), &dlg, SLOT(accept()) );

    Udb::Obj drafts = d_txn->getOrCreateObject( HeraldApp::s_drafts );
    Udb::Obj draft = drafts.getLastObj();
    if( !draft.isNull() ) do
    {
        QListWidgetItem* item = new QListWidgetItem( &list );
        item->setData( Qt::UserRole, draft.getOid() );
        QString scheduled;
        if( draft.hasValue(AttrDraftCommitted) )
            scheduled = tr(", commited %1").arg( HeTypeDefs::prettyDateTime(
                draft.getValue(AttrDraftCommitted).getDateTime() ) );
        else if( draft.hasValue(AttrDraftSubmitted) )
            scheduled = tr(", submitted %1").arg( HeTypeDefs::prettyDateTime(
                draft.getValue(AttrDraftSubmitted).getDateTime() ) );
        else if( draft.getValue(AttrDraftScheduled).getBool() )
            scheduled = QString::fromLatin1(", scheduled");
        item->setText( tr("%4 %1 (created %2%3)").arg( draft.getString( AttrText ) ).
        arg( HeTypeDefs::prettyDateTime( draft.getValue( AttrCreatedOn ).getDateTime() ) ).
        arg( scheduled ).arg( draft.getString( AttrInternalId ) ) );
        item->setToolTip( item->text() );
        item->setIcon( QIcon(":/images/mail-draft.png" ) );
    }while( draft.prev() );

    if( dlg.exec() == QDialog::Accepted && list.currentItem() )
    {
        draft = d_txn->getObject( list.currentItem()->data(Qt::UserRole).toULongLong() );
        MailEdit* e = createMailEdit();
        e->loadFrom( draft );
        e->show();
    }

}

void EmailMainWindow::onEditorDestroyed(QObject * o)
{
    int pos = d_editors.indexOf( static_cast<MailEdit*>(o) );
    if( pos != -1 )
        d_editors[pos] = 0;
}

void EmailMainWindow::onUploadStatus(const QString &msg)
{
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::blue );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 SND OK %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
}

void EmailMainWindow::onUploadError(const QString &msg)
{
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::red );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 SND ERR %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
}

void EmailMainWindow::onSelectById()
{
    ENABLED_IF( true );

    QString id = QInputDialog::getText( this, tr("Select by ID"),
                           tr("Enter an ID") ).toUpper();
    if( id.startsWith( "OID" ) )
    {
        Udb::Obj obj = d_txn->getObject( id.mid(3).toULongLong() );
        if( !obj.isNull(true,true ) )
            onFollowObject(obj);
    }else
    {
        Udb::Idx idx( d_txn, IndexDefs::IdxInternalId );
        if( idx.seek( Stream::DataCell().setString(id) ) )
        {
            Udb::Obj obj = d_txn->getObject( idx.getOid() );
            if( !obj.isNull(true,true ) )
                onFollowObject(obj);
        }
    }
}

void EmailMainWindow::onImportStatus(const QString &msg)
{
    qDebug() << "IMP OK" << msg;
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::blue );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 IMP OK %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
}

void EmailMainWindow::onImportError(const QString &msg)
{
    qDebug() << "IMP ERR" << msg;
    QTextCharFormat f = d_log->currentCharFormat();
    f.setForeground( Qt::red );
    d_log->setCurrentCharFormat( f );
    d_log->append( tr("%1 SND ERR %2").arg( QDateTime::currentDateTime().toString(Qt::ISODate ) ).arg( msg ));
}

void EmailMainWindow::addTopCommands(Gui::AutoMenu * pop)
{
    Q_ASSERT( pop != 0 );
    pop->addSeparator();
	pop->addCommand( tr("Go back"),  this, SLOT(onGoBack()), tr("ALT+LEFT") );
	pop->addCommand( tr("Go forward"), this, SLOT(onGoForward()), tr("ALT+RIGHT") );
    pop->addCommand( tr("Search..."),  this, SLOT(onSearch()), tr("CTRL+F") );
    pop->addSeparator();
    QMenu* sub2 = createPopupMenu();
	sub2->setTitle( tr("Show Window") );
	pop->addMenu( sub2 );

    Gui::AutoMenu* sub = new Gui::AutoMenu( tr("Configuration" ), pop );
	pop->addMenu( sub );
	sub->addCommand( tr("Full Screen"), this, SLOT(onFullScreen()), tr("F11") );
    sub->addCommand( tr("Calendar"), this, SLOT(onShowCalendar()), tr("F10") );
	sub->addCommand( tr("Configuration..."), this, SLOT(onConfig()) );
	sub->addCommand( tr("Set Font..."), this, SLOT(onSetFont()) );

    pop->addCommand( tr("About Herald..."), this, SLOT(onAbout()) );
    pop->addSeparator();
    pop->addAction( tr("Quit"), this, SLOT(close()), tr("CTRL+Q") );

}

void EmailMainWindow::setupInbox()
{
    QDockWidget* dock = createDock( this, tr("Inbox"), 0, true );

    Udb::Obj root = d_txn->getOrCreateObject( HeraldApp::s_inboxUuid, TypeInbox );

	d_txn->commit();
    d_inbox = InboxCtrl::create( dock, root );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_inbox->getTree(), true );
    pop->addCommand( tr("Download Email"), this, SLOT(onFetchMail()), tr("F9") );
    pop->addCommand( tr("Resend to..."), this, SLOT(onResendInbox()) );
	pop->addCommand( tr("Open File..."), this, SLOT(onOpenFile()) );
	pop->addCommand( tr("Send from File..."), this, SLOT(onSendFromFile()) );
	d_inbox->addCommands( pop );
    pop->addCommand( tr("Import Mailbox..."), this, SLOT(onImportMail()) );
	addTopCommands( pop );
    connect( d_inbox, SIGNAL(sigSelectionChanged()),this,SLOT(onInboxSelected()));
    connect( d_inbox,SIGNAL(sigUnselect()), this, SLOT(onUnselect()) );
    dock->setWidget( d_inbox->getTree() );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
}

void EmailMainWindow::setupAttrView()
{
    QDockWidget* dock = createDock( this, tr("Object Attributes"), 0, true );
    d_attrView = AttrViewCtrl::create( dock, d_txn );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_attrView->getWidget(), true );
    d_attrView->addCommands(pop);
    addTopCommands( pop );
    dock->setWidget( d_attrView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_attrView,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void EmailMainWindow::setupAddressList()
{
    QDockWidget* dock = createDock( this, tr("Email Addresses"), 0, true );
    d_addressList = AddressListCtrl::create( dock, d_aidx );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_addressList->getWidget(), true );
    d_addressList->addCommands( pop );
	pop->addSeparator();
	pop->addCommand( tr("Send Certificate..."), this, SLOT(onSendCert1()) );
    addTopCommands( pop );
    dock->setWidget( d_addressList->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_addressList,SIGNAL(sigSelected(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void EmailMainWindow::setupMailList()
{
    QDockWidget* dock = createDock( this, tr("Message Selection"), 0, true );
    d_mailList = MailListCtrl::create( dock, d_txn );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_mailList->getWidget(), true );
    pop->addCommand( tr("Show Draft..."), this, SLOT(onShowDraft()), tr("F5") );
	pop->addSeparator();
    d_mailList->addCommands(pop);
    addTopCommands( pop );
    dock->setWidget( d_mailList->getWidget() );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
    connect( d_mailList, SIGNAL(sigSelectionChanged()),this,SLOT(onMailListSelected()));
}

void EmailMainWindow::setupMailHisto()
{
    QDockWidget* dock = createDock( this, tr("Message Log"), 0, true );
    d_mailHisto = MailHistoCtrl::create( dock, d_txn );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_mailHisto->getWidget(), true );
    pop->addCommand( tr("New Message..."), this, SLOT(onNewMessage()), tr("CTRL+N") );
    pop->addCommand( tr("Show Drafts..."), this, SLOT(onShowDraft()), tr("F5") );
    d_mailHisto->addCommands(pop);
    addTopCommands( pop );
    dock->setWidget( d_mailHisto->getWidget() );
    addDockWidget( Qt::LeftDockWidgetArea, dock );
    connect( d_mailHisto, SIGNAL(sigSelectionChanged()),this,SLOT(onMailHistoSelected()));
    connect( d_inbox, SIGNAL(sigAccepted(QList<Udb::Obj>)), d_mailHisto, SLOT(onSelect(QList<Udb::Obj>)) );
}

void EmailMainWindow::setupMailView()
{
	d_mailView = new MailView( d_txn, this );
    connect( d_mailView, SIGNAL(sigActivated(Udb::Obj)), this, SLOT(onFollowObject(Udb::Obj)) );
    connect( d_mailView,SIGNAL(sigActivated(QByteArray)),this,SLOT(onFollowEmail(QByteArray)) );
    connect( d_mailView, SIGNAL(sigShowIcd(QString)),this,SLOT(onShowIcd(QString)) );
    //d_tab->addFixed( d_mailView, tr("Mail Viewer") );
    setCentralWidget( d_mailView );
    d_txn->getDb()->addObserver( d_mailView, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );
}

void EmailMainWindow::setupTextView()
{
    QDockWidget* dock = createDock( this, tr("Object Text"), 0, true );
	d_textView = TextViewCtrl::create( dock, d_txn );
    Gui::AutoMenu* pop = d_textView->createPopup();
    addTopCommands( pop );
    dock->setWidget( d_textView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_textView,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void EmailMainWindow::setupRefView()
{
    QDockWidget* dock = createDock( this, tr("References"), 0, true );
    d_refView = RefViewCtrl::create( dock, d_txn );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_refView->getWidget(), true );
    d_refView->addCommands( pop );
    addTopCommands( pop );
    dock->setWidget( d_refView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_refView,SIGNAL(sigSelected(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void EmailMainWindow::setupSearchView()
{
    d_sv = new SearchView( d_txn, this );
	QDockWidget* dock = createDock( this, tr("Search" ), 0, false );
	dock->setWidget( d_sv );
	addDockWidget( Qt::RightDockWidgetArea, dock );

    connect( d_sv, SIGNAL(signalShowItem(Udb::Obj) ), this, SLOT(onFollowObject(Udb::Obj) ) );

    Gui::AutoMenu* pop = new Gui::AutoMenu( d_sv, true );
	pop->addCommand( tr("Show Item"), d_sv, SLOT(onGoto()), tr("CTRL+G"), true );
	pop->addCommand( tr("Clear"), d_sv, SLOT(onClearSearch()) );
	pop->addSeparator();
    pop->addCommand( tr("Copy"), d_sv, SLOT(onCopyRef()), tr("CTRL+C"), true );
    pop->addSeparator();
	pop->addCommand( tr("Update Index..."), d_sv, SLOT(onUpdateIndex()) );
	pop->addCommand( tr("Rebuild Index..."), d_sv, SLOT(onRebuildIndex()) );
    addTopCommands( pop );
}

void EmailMainWindow::setupLog()
{
    QDockWidget* dock = createDock( this, tr("System Log"), 0, true );
    d_log = new QTextBrowser( dock );
    d_log->setLineWrapMode(QTextEdit::NoWrap);
    dock->setWidget( d_log );
    addDockWidget( Qt::RightDockWidgetArea, dock );
}

void EmailMainWindow::showCalendar()
{
    if( d_calendar )
    {
        d_calendar->activateWindow();
        d_calendar->raise();
        return;
    }
    d_calendar = new CalMainWindow(this);

    if( d_fullScreen )
    {
        d_calendar->restore();
        toFullScreen(d_calendar);
    }else
    {
        d_calendar->restore();
        d_calendar->showNormal();
    }
}

MailEdit *EmailMainWindow::createMailEdit()
{
    MailEdit* e = new MailEdit( d_aidx );
    connect( e, SIGNAL(destroyed(QObject*)), this,SLOT(onEditorDestroyed(QObject*)) );
    e->show(); // setzt die Geometrie, darum zuerst aufrufen
    HeraldApp::inst()->setWindowGeometry( e );
    e->setAttribute( Qt::WA_DeleteOnClose );
    d_editors.append( e );
    connect( e, SIGNAL(sigSendMessage(quint64)), d_umgr, SLOT(onSendMessage(quint64)),
        Qt::QueuedConnection );
    return e;
}

void EmailMainWindow::pushBack(const Udb::Obj & o)
{
    if( d_pushBackLock )
        return;
    if( o.isNull( true, true ) )
        return;
    if( !d_backHisto.isEmpty() && d_backHisto.last() == o.getOid() )
        return; // o ist bereits oberstes Element auf dem Stack.
    d_backHisto.removeAll( o.getOid() );
    d_backHisto.push_back( o.getOid() );
}

void EmailMainWindow::toFullScreen(QMainWindow * w)
{
#ifdef _WIN32
    w->showFullScreen();
#else
    w->setWindowFlags( Qt::Window | Qt::FramelessWindowHint  );
    w->showMaximized();
    //w->setWindowIcon( qApp->windowIcon() );
#endif
}

void EmailMainWindow::onGoBack()
{
    // d_backHisto.last() ist Current
    ENABLED_IF( d_backHisto.size() > 1 );

    d_pushBackLock = true;
    d_forwardHisto.push_back( d_backHisto.last() );
    d_backHisto.pop_back();
    onFollowObject( d_txn->getObject( d_backHisto.last() ) );
    d_pushBackLock = false;
}

void EmailMainWindow::onGoForward()
{
    ENABLED_IF( !d_forwardHisto.isEmpty() );

    Udb::Obj cur = d_txn->getObject( d_forwardHisto.last() );
    d_forwardHisto.pop_back();
    onFollowObject( cur );
}

void EmailMainWindow::onGoUpward()
{
    ENABLED_IF( !d_attrView->getObj().isNull() && !d_attrView->getObj().getParent().isNull() );

    onFollowObject( d_attrView->getObj().getParent() );
}

void EmailMainWindow::onShowIcd(const QString &fileName)
{
    showCalendar();
	d_calendar->showIcs( fileName );
}

void EmailMainWindow::onOpenFile()
{
	ENABLED_IF(true);
	QString path = QFileDialog::getOpenFileName( this, tr("Open File - Herald"),
												 QString(),
												 tr("Email (*.eml);;All Files (*)") );
	if( path.isEmpty() )
		return;
	d_mailView->showMail(path);
}

void EmailMainWindow::onSendFromFile()
{
	ENABLED_IF(d_umgr->isIdle());
	QString path = QFileDialog::getOpenFileName( this, tr("Send from File - Herald"),
												 QString(),
												 tr("Email (*.eml)") );
	if( path.isEmpty() )
		return;
	d_umgr->sendFromFile( path );
}

void EmailMainWindow::onSendCert1()
{
	Udb::Obj addr = d_addressList->getSelected();
	ENABLED_IF( d_umgr->isIdle() && !addr.isNull() );

	SendCertificateDlg dlg( d_aidx, this );
	dlg.setTo( addr );
	do
	{
		if( !dlg.select() )
			return;
		Udb::Obj id = dlg.getIdentity();
		Q_ASSERT( !id.isNull() );
		Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
		Q_ASSERT( !addrObj.isNull() );
		const QString cert = MailObj::findCertificate( d_txn, addrObj.getValue(AttrEmailAddress).getArr() );
		const QString key = id.getString( AttrPrivateKey );
		if( cert.isEmpty() || key.isEmpty() )
		{
			QMessageBox::critical( this, dlg.windowTitle(),  tr("Selected identity has no certificate or private key!") );
			continue;
		}
		if( dlg.getMsg().isEmpty() || dlg.getSubject().isEmpty() )
		{
			QMessageBox::critical( this, dlg.windowTitle(),  tr("Subject or message is empty!") );
			continue;
		}
		MailMessage* msg = new MailMessage(this);
		msg->setFrom( dlg.getFrom() );
		msg->setTo( dlg.getTo() );
		msg->setSubject( dlg.getSubject() );
		msg->setPlainTextBody( dlg.getMsg() );
		msg->setDispositionNotificationTo(dlg.getFrom());
		msg->setCertPath( cert );
		msg->setKeyPath( key );
		msg->setSigned( true );
		d_umgr->sendMessage( msg );
		return;
    }while( true );
}

void EmailMainWindow::onShowCalendar()
{
    CHECKED_IF(true, d_calendar);
    if( d_calendar )
    {
        d_calendar->deleteLater();
        d_calendar = 0;
    }else
        showCalendar();
}

void EmailMainWindow::onSearch()
{
    ENABLED_IF( true );

    raise();
	d_sv->parentWidget()->show();
	d_sv->parentWidget()->raise();
	d_sv->onNew();
}

void EmailMainWindow::onTest()
{
	ENABLED_IF( true );
}

void EmailMainWindow::onResendInbox()
{
    QString path = d_inbox->getSelectedFile();
    ENABLED_IF( !path.isEmpty() && d_umgr->isIdle() );

    ResendToDlg dlg( d_aidx, this );

    Udb::Obj id;
//    id = d_txn->getObject( d_inbox->getSelectedPopAccount() );
//    dlg.setFrom( id );
    if( dlg.select() )
    {
        id = dlg.getIdentity();
        Q_ASSERT( !id.isNull() );
        d_umgr->resendMessage( path, id, dlg.getTo() );
    }
}

void EmailMainWindow::onUnselect()
{
    d_mailView->clear();
}
