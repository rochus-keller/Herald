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

#include "CalMainWindow.h"
#include <QFileInfo>
#include <QVBoxLayout>
#include <QDockWidget>
#include <QInputDialog>
#include <QTreeView>
#include <QMessageBox>
#include <QApplication>
#include <QShortcut>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/Idx.h>
#include <GuiTools/AutoShortcut.h>
#include "EmailMainWindow.h"
#include "ScheduleBoard.h"
#include "TimelineView.h"
#include "HeraldApp.h"
#include "CalendarView.h"
#include "TextViewCtrl.h"
#include "AttrViewCtrl.h"
#include "RefViewCtrl.h"
#include "HeTypeDefs.h"
#include "ScheduleItemObj.h"
#include "PersonPropsDlg.h"
#include "ScheduleItemPropsDlg.h"
#include "ObjectHelper.h"
#include "IcsDocument.h"
#include "BrowserCtrl.h"
#include "ScheduleObj.h"
#include "MailObj.h"
using namespace He;

CalMainWindow::CalMainWindow(EmailMainWindow *email) :
    QMainWindow(0),d_email(email),d_pushBackLock(false)
{
    Q_ASSERT( email != 0 );

    setCaption();
    QIcon icon;
    icon.addFile( ":/images/herald_cal64.png" );
    icon.addFile( ":/images/herald_cal32.png" );
    icon.addFile( ":/images/herald_cal16.png" );
    setWindowIcon( icon );

	d_sb = new ScheduleBoard( this, d_email->getTxn() );
    setCentralWidget( d_sb );

	connect( d_sb, SIGNAL( showStatusMessage( QString ) ), this, SLOT( onStatusMessage( QString ) ) );
	connect( d_sb, SIGNAL( objectSelected( quint64 ) ), this, SLOT( onSelectSchedule( quint64 ) ) );
	connect( d_sb, SIGNAL( objectDblClicked( quint64 ) ), this, SLOT( onEdit() ) );
	connect( d_sb->getTimeline(), SIGNAL( dateChanged(bool) ), this, SLOT( onDateChanged(bool) ) );

	setDockNestingEnabled(true);
	/* Nein, Default ist besser, da wir in Breite viel sehen wollen */
    setCorner( Qt::TopLeftCorner, Qt::LeftDockWidgetArea );
	setCorner( Qt::BottomLeftCorner, Qt::LeftDockWidgetArea );
    setCorner( Qt::TopRightCorner, Qt::RightDockWidgetArea );
    setCorner( Qt::BottomRightCorner, Qt::BottomDockWidgetArea );

    setupCalendar();
    setupTextView();
    setupAttrView();
    setupRefView();
    setupBrowserView();

    Udb::Obj olns = d_email->getTxn()->getOrCreateObject( HeraldApp::s_outlines );
    Udb::Obj oln = olns.getFirstObj();
    if( !oln.isNull() ) do
    {
        showAsDock( oln, false );
    }while( oln.next() );
    olns.commit();

    setupMenus();
}

void CalMainWindow::showObj(const Udb::Obj & o)
{
    if( o.isNull() )
        return;
    show();
    activateWindow();
    raise();
    followObject( o );
}

void CalMainWindow::showIcs(const QString &path)
{
    const QString title = tr("Show iCalendar File");
    IcsDocument doc;
    if( !doc.loadFrom( path ) )
    {
        QMessageBox::critical( this, title, doc.getError() );
        return;
    }
    QList<ScheduleItemData> l = ScheduleObj::readEvents( doc, false );
    int which = 0;
    if( l.isEmpty() )
    {
        QMessageBox::critical( this, tr("Show iCalendar file"), tr("The iCalendar File contains no events") );
        return;
    }else if( l.size() > 1 )
    {
        QStringList items;
        foreach( const ScheduleItemData& d, l )
            items.append( d.d_description );
        QString str = QInputDialog::getItem( this, title,
                                             tr("The iCalendar file contains more than one Event.\n"
                                                "Select the one to show."), items );
        if( str.isEmpty() )
            return;
        which = items.indexOf( str ); // RISK: was bei mehreren gleichen Namen?
    }
    d_browser->getWidget()->parentWidget()->show();
	d_browser->getWidget()->parentWidget()->raise();
    d_browser->setTitleText( l.first().d_summary, l.first().d_html );
    Udb::Obj schedItem;
    Udb::Idx idx( d_email->getTxn(), IndexDefs::IdxMessageId );
    if( idx.seek( Stream::DataCell().setLatin1( l[which].d_uid ) ) ) do
    {
        Udb::Obj o = d_email->getTxn()->getObject( idx.getOid() );
        if( HeTypeDefs::isScheduleItem( o.getType() ) )
        {
            if( !schedItem.isNull() )
            {
                QMessageBox::information( this, title, tr("There is more than one event with the same UID.\n"
                                                          "Using the first one.") );
                break;
            }
            schedItem = o;
        }
    }while( idx.nextKey() );
    if( !schedItem.isNull() )
        followObject( schedItem );
    else
    {
        d_sb->gotoDate( l[which].d_start, l[which].d_end );
        d_cv->setFocus();
        d_cv->setSelection( l[which].d_start, l[which].d_slot );
    }
    // TODO Anzeige der Unterschiede zu diesen Objekten
}

void CalMainWindow::restore()
{
    QVariant state = HeraldApp::inst()->getSet()->value( "Calendar/State/" +
		d_email->getTxn()->getDb()->getDbUuid().toString() ); // Da DB-individuelle Docks
	if( !state.isNull() )
		restoreState( state.toByteArray() );
    d_sb->restoreSplitter();
}

void CalMainWindow::setCaption()
{
    QFileInfo info( d_email->getTxn()->getDb()->getFilePath() );
    setWindowTitle( tr("%1 - Calendar - Herald").arg( info.baseName() ) );
}

void CalMainWindow::setupCalendar()
{
    QDockWidget* dock = EmailMainWindow::createDock( this, tr("Calendar" ), 0, true );
	QWidget* outer = new QWidget( this );
	QVBoxLayout* vbox = new QVBoxLayout( outer );
	vbox->setMargin( 2 );
	vbox->setSpacing( 2 );
    d_cv = new CalendarView( outer, d_email->getTxn() );
	vbox->addWidget( d_cv );
	dock->setWidget( outer );
	addDockWidget( Qt::BottomDockWidgetArea, dock );
	connect( d_cv, SIGNAL( dateChanged( bool ) ), this, SLOT( onCalendarMoved(bool) ) );
	connect( d_cv, SIGNAL( objectSelected( quint64 ) ), this, SLOT( onSelectCalendar( quint64 ) ) );
	connect( d_cv, SIGNAL( objectDoubleClicked( quint64 ) ), this, SLOT( onEdit() ) );

    Gui::AutoMenu* pop = new Gui::AutoMenu( d_cv, true );
	pop->addCommand( tr("Create Appointment..."), d_cv, SLOT(onCreateAppointment()) );
	pop->addCommand( tr("Create Deadline..."), d_cv, SLOT(onCreateDeadline()) );
	pop->addCommand( tr("Delete Item" ), d_cv, SLOT( onDeleteItem() ) );
    pop->addCommand( tr("Move to..." ), d_cv, SLOT( onMoveToCalendar() ) );
    pop->addCommand( tr("Copy Ref."), d_cv, SLOT(onCopyLink()), tr("CTRL+C"), true );
	pop->addSeparator();
	pop->addCommand( tr("Set Number of Days..."), d_cv, SLOT(onSetNumOfDays()) );
    pop->addCommand( tr("Set Number of Hours..."), d_cv, SLOT(onSetNumOfHours()) );
    addTopCommands( pop );
}

void CalMainWindow::setupTextView()
{
    QDockWidget* dock = EmailMainWindow::createDock( this, tr("Object Text"), 0, true );
	d_textView = TextViewCtrl::create( dock, d_email->getTxn() );
    Gui::AutoMenu* pop = d_textView->createPopup();
    addTopCommands( pop );
    dock->setWidget( d_textView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_textView,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void CalMainWindow::setupAttrView()
{
    QDockWidget* dock = EmailMainWindow::createDock( this, tr("Object Attributes"), 0, true );
    d_attrView = AttrViewCtrl::create( dock, d_email->getTxn() );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_attrView->getWidget(), true );
    d_attrView->addCommands(pop);
    addTopCommands( pop );
    dock->setWidget( d_attrView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_attrView,SIGNAL(signalFollowObject(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
}

void CalMainWindow::setupRefView()
{
    QDockWidget* dock = EmailMainWindow::createDock( this, tr("References"), 0, true );
    d_refView = RefViewCtrl::create( dock, d_email->getTxn() );
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_refView->getWidget(), true );
    d_refView->addCommands( pop );
    addTopCommands( pop );
    dock->setWidget( d_refView->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
    connect( d_refView,SIGNAL(sigDblClicked(Udb::Obj)), this,SLOT(onFollowObject(Udb::Obj)) );
    connect( d_refView,SIGNAL(sigSelected(Udb::Obj)), this,SLOT(onShowLocalObject(Udb::Obj)) );
}

void CalMainWindow::setupBrowserView()
{
    QDockWidget* dock = EmailMainWindow::createDock( this, tr("Browser"), 0, true );
    d_browser = BrowserCtrl::create( dock );
    connect( d_browser, SIGNAL(sigActivated(QByteArray)), this, SLOT(onFollowEmail(QByteArray)) );
    dock->setWidget( d_browser->getWidget() );
    addDockWidget( Qt::BottomDockWidgetArea, dock );
}

void CalMainWindow::setupMenus()
{
    Gui::AutoMenu* pop = new Gui::AutoMenu( d_sb->getList(), true );

	pop->addCommand( tr("Goto Earliest Date"), d_sb, SLOT(onShowFirstItem()) );
	pop->addCommand( tr("Goto Latest Date"), d_sb, SLOT(onShowLastItem()) );
	pop->addSeparator();
	pop->addCommand( tr("Select Schedules..."), d_sb, SLOT(onSelectSchedules()) );
	pop->addCommand( tr("Select Persons..."), d_sb, SLOT(onSelectPersons()) );
	pop->addCommand( tr("Create Schedule..."), d_sb, SLOT(onCreateSchedule()) );
	pop->addCommand( tr("Rename Schedule..."), d_sb, SLOT(onRenameSchedule()) );
	pop->addCommand( tr("Remove Schedules"), d_sb, SLOT(onRemoveSchedules()) );
    pop->addCommand( tr("Set Default Schedule"), d_sb, SLOT(onDefaultSched()) );
    pop->addCommand( tr("Set Owner..."), d_sb, SLOT(onSetOwner()) );
    Gui::AutoMenu* sub = new Gui::AutoMenu( tr("Sort Schedules" ), pop );
	pop->addMenu( sub );
	sub->addCommand( tr("By Type and Name"), d_sb, SLOT(onSortListByTypeName()) );
	sub->addCommand( tr("By Name"), d_sb, SLOT(onSortListByName()) );
	pop->addSeparator();
	pop->addCommand( tr("Fit Row Heights"), d_sb, SLOT(onAdjustSections()) );
	pop->addCommand( tr("Re-layout Rows"), d_sb, SLOT(onLayoutSections()) );
	pop->addCommand( tr("Collapse Rows"), d_sb, SLOT(onCollapseSections()) );
	pop->addSeparator();
	pop->addCommand( tr("Export all..."), d_sb, SLOT(onExportAll()) );
	pop->addCommand( tr("Export selection..."), d_sb, SLOT(onExportSel()) );
    pop->addCommand( tr("Import schedule..."), d_sb, SLOT(onImportSched()) );
	pop->addSeparator();
	pop->addCommand( tr("Create Schedule Set..."), d_sb, SLOT(onCreateScheduleSet()) );
	pop->addCommand( tr("Rename Schedule Set..."), d_sb, SLOT(onRenameScheduleSet()) );
	pop->addCommand( tr("Delete Schedule Set..."), d_sb, SLOT(onDeleteScheduleSet()) );
	pop->addSeparator();
	pop->addCommand( tr("Save Schedule Sets..."), d_sb, SLOT(onSaveScheduleSets()) );
	pop->addCommand( tr("Load Schedule Sets..."), d_sb, SLOT(onLoadScheduleSets()) );
    addTopCommands( pop );

    pop = new Gui::AutoMenu( d_sb->getBoard(), true );
	pop->addCommand( tr("Create Event..."), d_sb, SLOT(onCreateEvent()) );
	pop->addCommand( tr("Create Appointment..."), d_sb, SLOT(onCreateAppointment()) );
	pop->addCommand( tr("Create Deadline..."), d_sb, SLOT(onCreateDeadline()) );
	pop->addCommand( tr("Set Item Properties..."), this, SLOT(onEdit()) );
	pop->addCommand( tr("Remove Item"), d_sb, SLOT(onRemoveItem()) );
    pop->addCommand( tr("Delete Item"), d_sb, SLOT(onEraseItem()), tr("CTRL+D"), true );
    sub = new Gui::AutoMenu( tr("Change to" ), pop );
	pop->addMenu( sub );
	sub->addCommand( tr("Deadline"),  d_sb, SLOT(onToDeadline()) );
	sub->addCommand( tr("Schedule Event"),  d_sb, SLOT(onToEvent()) );
	sub->addCommand( tr("Appointment"),  d_sb, SLOT(onToAppointment()) );

	pop->addSeparator();
	pop->addCommand( tr("Copy Ref."), d_sb, SLOT(onCopyLink()), tr("CTRL+C"), true );
    addTopCommands( pop );

    pop = new Gui::AutoMenu( d_sb->getTimeline(), true );

	pop->addCommand( tr("Goto today"), d_sb->getTimeline(), SLOT(gotoToday()), tr( "CTRL+T" ), true );
	pop->addCommand( tr("Select Date..."), d_sb, SLOT(onSelectDate()), tr( "CTRL+D" ), true );
	pop->addCommand( tr("Forward Day"), d_sb->getTimeline(), SLOT(nextDay()), tr("CTRL+Right"), true );
	pop->addCommand( tr("Backward Day"), d_sb->getTimeline(), SLOT(prevDay()), tr("CTRL+Left"), true );
	pop->addCommand( tr("Forward Page"), d_sb->getTimeline(), SLOT(nextPage()), Qt::CTRL | Qt::Key_PageUp, true );
	pop->addCommand( tr("Backward Page"), d_sb->getTimeline(), SLOT(prevPage()), Qt::CTRL | Qt::Key_PageDown, true );
	pop->addSeparator();
	pop->addCommand( tr("Zoom in"), d_sb->getTimeline(), SLOT(zoomIn()), tr("CTRL+SHIFT+Right"), true );
	pop->addCommand( tr("Zoom out"), d_sb->getTimeline(), SLOT(zoomOut()), tr("CTRL+SHIFT+Left"), true );
	pop->addCommand( tr("Day Resolution"), d_sb->getTimeline(), SLOT(dayResolution()), tr("CTRL+1"), true );
	pop->addCommand( tr("Week Resolution"), d_sb->getTimeline(), SLOT(weekResolution()), tr("CTRL+2"), true );
	pop->addCommand( tr("Month Resolution"), d_sb->getTimeline(), SLOT(monthResolution()), tr("CTRL+3"), true );
    addTopCommands( pop );

    new Gui::AutoShortcut( tr("F11"), this, d_email, SLOT(onFullScreen()) );
    new Gui::AutoShortcut( tr("CTRL+Q"), this, d_email, SLOT(close()) );
    new Gui::AutoShortcut( tr("ALT+LEFT"), this, this,  SLOT(onGoBack()) );
    new Gui::AutoShortcut( tr("ALT+RIGHT"), this, this,  SLOT(onGoForward()) );
    new Gui::AutoShortcut( tr("CTRL+F"), this, d_email, SLOT(onSearch()) );
    new Gui::AutoShortcut( tr("F10"), this, this,  SLOT(close()) );

}

void CalMainWindow::onStatusMessage(const QString &)
{
    // NOP
}

void CalMainWindow::onSelectSchedule(quint64 id)
{
    Udb::Obj o = d_email->getTxn()->getObject( id );
    followObject( o, false );
    d_cv->clearSelection();
    if( HeTypeDefs::isScheduleItem( o.getType() ) || o.getType() == TypeSchedule )
	{
		ScheduleItemObj s( o );
		if( s.getStart() > d_sb->getTimeline()->getDate() )
			d_cv->show( s, s.getStart() );
		else
			d_cv->show( s, d_sb->getTimeline()->getDate(), false );
	}else
	{
		if( !o.isNull() )
		{
			// Click auf Schedule oder ScheduleSet
			onDateChanged( false );
            d_cv->showAll();
		}else
        {
            d_cv->show( d_sb->getSelectedCalendar(), d_sb->getCursor().first );
        }
	}
}

void CalMainWindow::onEdit()
{
    ENABLED_IF( true );

	Udb::Obj o = d_attrView->getObj();
	switch( o.getType() )
	{
	case TypePerson:
		{
			PersonPropsDlg dlg(this);
			dlg.edit( o );
			return;
		}
	case TypeEvent:
	case TypeAppointment:
	case TypeDeadline:
		{
			ScheduleItemPropsDlg dlg(this);
			dlg.edit(o);
			return;
		}// else fall through
	default:
		{
			bool ok;
			QString res = QInputDialog::getText( this, tr("Edit Object"), tr("Title/Text:"), QLineEdit::Normal,
				o.getString( AttrText ), &ok );
			if( ok && res != o.getString( AttrText ) )
			{
				o.setString( AttrText, res );
				o.setTimeStamp( AttrModifiedOn );
				o.commit();
			}
		}
		break;
	}
}

void CalMainWindow::onDateChanged(bool intermediate)
{
    if( !intermediate )
		d_cv->setDate( d_sb->getTimeline()->getDate() );
}

void CalMainWindow::onCalendarMoved(bool)
{
    // NOP
}

void CalMainWindow::onSelectCalendar(quint64 id)
{
    Udb::Obj o = d_email->getTxn()->getObject( id );
	followObject( o, false );
}

void CalMainWindow::onFollowObject(const Udb::Obj & o)
{
    if( o.isNull() )
        return;
	if( HeTypeDefs::isScheduleItem( o.getType() ) ||
		HeTypeDefs::isScheduleItem( o.getParent().getType() ) || o.getType() == TypeSchedule )
    {
        followObject( o, true );
    }else
        d_email->showOid( o.getOid() );
}

void CalMainWindow::onShowLocalObject(const Udb::Obj & o)
{
    if( o.getType() == TypeAttachment )
    {
        AttachmentObj att = o;
        showIcs( att.getDocumentPath() );
    }
}

void CalMainWindow::onUrlActivated(const QUrl & url)
{
    HeraldApp::openUrl( url );
}

void CalMainWindow::onSelectOutline(quint64 oid)
{
    Udb::Obj o = d_email->getTxn()->getObject( oid );
    onFollowObject( o ); // Hier muss onFollowObject stehen, da oid irgendwelche Typen sein kann
}

void CalMainWindow::onAddDock()
{
    ENABLED_IF(true);
    QString title = QInputDialog::getText( this, tr("New Outline - Herald"), tr("Enter title:") );
    if( title.isEmpty() )
        return;
    Udb::Obj oln = ObjectHelper::createObject( TypeOutline, d_email->getTxn()->
                                               getOrCreateObject( HeraldApp::s_outlines ) );
    oln.setString( AttrText, title );
    showAsDock( oln, true );
    oln.commit();
}

void CalMainWindow::onRemoveDock()
{
    int cur = -1;
	for( int i = 0; i < d_docks.size(); i++ )
		if( d_docks[i]->getTree() == QApplication::focusWidget() )
		{
			cur = i;
			break;
		}
	ENABLED_IF( cur != -1 );

    if( QMessageBox::warning( this, tr("Remove Outline - Herald"),
                                          tr("Do you really want to remove this outline? This cannot be undone."),
                                          QMessageBox::Ok | QMessageBox::Cancel,
                                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;

    Udb::Obj oln = d_docks[cur]->getOutline();
    QWidget* dock = d_docks[cur]->getTree()->parentWidget();
	removeDockWidget( static_cast<QDockWidget*>( dock ) );
    d_docks.removeAt( cur );
    oln.erase();
	dock->deleteLater();
    d_email->getTxn()->commit();
}

void CalMainWindow::showAsDock( const Udb::Obj& oln, bool visible )
{
	if( oln.isNull() )
		return;
	QDockWidget* dock = EmailMainWindow::createDock( this, oln.getValue( AttrText ).toString(),
                                                     oln.getOid(), visible );
	Oln::OutlineUdbCtrl* ctrl = Oln::OutlineUdbCtrl::create( dock, d_email->getTxn() );
	ctrl->getTree()->setShowNumbers( true ); // false
	ctrl->getTree()->setIndentation( 15 ); // 10
	ctrl->getTree()->setHandleWidth( 7 );
	ctrl->getDeleg()->setBiggerTitle( false );
	ctrl->getDeleg()->setShowIcons( true );
	ctrl->setOutline( oln );

	dock->setWidget( ctrl->getTree() );
    d_docks.append( ctrl );
	addDockWidget( Qt::LeftDockWidgetArea, dock );

    connect( ctrl, SIGNAL(sigLinkActivated(quint64) ), this, SLOT(onSelectOutline(quint64) ) );
    connect( ctrl, SIGNAL(sigUrlActivated(QUrl) ), this, SLOT(onUrlActivated(QUrl)) );

    Gui::AutoMenu* pop = new Gui::AutoMenu( ctrl->getTree(), true );
    Gui::AutoMenu* sub = new Gui::AutoMenu( tr("Item" ), pop );
	pop->addMenu( sub );
    ctrl->addItemCommands( sub );
    sub = new Gui::AutoMenu( tr("Text" ), pop );
	pop->addMenu( sub );
    ctrl->addTextCommands( sub );
	pop->addSeparator();
    pop->addCommand( tr("Remove Outline..."), this, SLOT(onRemoveDock() ) );
    ctrl->addOutlineCommands( pop );
    addTopCommands( pop );
}

void CalMainWindow::addTopCommands(Gui::AutoMenu * pop)
{
    Q_ASSERT( pop != 0 );
    pop->addSeparator();
	pop->addCommand( tr("Go back"),  this, SLOT(onGoBack()), tr("ALT+LEFT") );
	pop->addCommand( tr("Go forward"), this, SLOT(onGoForward()), tr("ALT+RIGHT") );
    pop->addCommand( tr("Search..."),  d_email, SLOT(onSearch()), tr("CTRL+F") );
    pop->addSeparator();
    QMenu* sub2 = createPopupMenu();
	sub2->setTitle( tr("Show Window") );
	pop->addMenu( sub2 );
    pop->addCommand( tr("Add Outline..."), this, SLOT(onAddDock()) );
    pop->addCommand( tr("Full Screen"), d_email, SLOT(onFullScreen()), tr("F11") );
    pop->addCommand( tr("About Herald..."), d_email, SLOT(onAbout()) );
    pop->addSeparator();
    pop->addAction( tr("Quit"), d_email, SLOT(close()), tr("CTRL+Q") );
}

void CalMainWindow::onGoBack()
{
    // d_backHisto.last() ist Current
    ENABLED_IF( d_backHisto.size() > 1 );

    d_pushBackLock = true;
    d_forwardHisto.push_back( d_backHisto.last() );
    d_backHisto.pop_back();
    followObject( d_email->getTxn()->getObject( d_backHisto.last() ) );
    d_pushBackLock = false;
}

void CalMainWindow::onGoForward()
{
    ENABLED_IF( !d_forwardHisto.isEmpty() );

    Udb::Obj cur = d_email->getTxn()->getObject( d_forwardHisto.last() );
    d_forwardHisto.pop_back();
    followObject( cur );
}

void CalMainWindow::onFollowEmail(const QByteArray &emailAddr)
{
    Udb::Obj o = MailObj::getEmailAddress( d_email->getTxn(), emailAddr );
    onFollowObject( o );
}

void CalMainWindow::pushBack(const Udb::Obj & o)
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

void CalMainWindow::followObject(const Udb::Obj & o, bool openDoc)
{
    pushBack( o );
    if( openDoc && d_sb->gotoItem( o.getOid() ) && HeTypeDefs::isCalendarItem( o.getType() ) )
    {
        d_cv->setSelectedItem( o );
        d_cv->parentWidget()->parentWidget()->show();
        d_cv->parentWidget()->parentWidget()->raise();
    }else
    {
        d_attrView->setObj( o );
        d_textView->setObj( o );
        d_refView->setObj( o );
    }
}
