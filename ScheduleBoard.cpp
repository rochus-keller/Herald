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

#include "ScheduleBoard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QInputDialog>
#include <QComboBox>
#include <QFileDialog>
#include <QScrollBar>
#include <QTreeView>
#include <QHeaderView>
#include <QSplitter>
#include <QItemSelectionModel>
#include <QColorDialog>
#include <QStatusBar>
#include <QShortcut>
#include <QFontDialog>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QResizeEvent>
#include <QPainter>
#include <QSettings>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <QDesktopWidget>
#include <QIcon>
#include <QToolBar>
#include <QtDebug>
#include <QBitArray>
#include <GuiTools/AutoMenu.h>
#include <GuiTools/AutoToolBar.h>
#include <cassert>
#include "TimelineView.h"
#include "ScheduleBoardDeleg.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include "ScheduleListMdl.h"
#include "ScheduleObj.h"
#include "ScheduleItemObj.h"
#include "IcalExport.h"
#include "CalendarPopup.h"
#include "ScheduleSelectorDlg.h"
#include "ScheduleItemPropsDlg.h"
#include "PersonListView.h"
#include "MailObj.h"
#include "ObjectHelper.h"
#include <QItemDelegate>
#include <QToolTip>
#include <Udb/Transaction.h>
#include <Oln2/OutlineStream.h>
#include "IcsDocument.h"
using namespace He;
using namespace Udb;

// adaptiert aus MasterPlan

class ScheduleBoardTree : public QTreeView
{
public:
    ScheduleBoardTree( QWidget* p ):QTreeView(p){}

    void drawRow( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
    {
        QStyleOptionViewItem o2( option );
        o2.rect.adjust( 0, 0, 0, -1 );
        QTreeView::drawRow( painter, o2, index );
        painter->setPen( QColor::fromRgb( 160, 160, 164 ) );
        painter->drawLine( option.rect.bottomLeft(), option.rect.bottomRight() );
    }
};

class ScheduleBoardStrut : public QWidget
{
public:
	ScheduleBoardStrut( QWidget* p ):QWidget(p) 
	{
		setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
	}
	bool eventFilter ( QObject * watched, QEvent * e ) 
	{
		if( e->type() == QEvent::Resize )
		{
			QResizeEvent* re = static_cast<QResizeEvent*>( e );
			if( re->oldSize().height() != re->size().height() )
				setFixedHeight( re->size().height() );
		}
		return false;
	}
};

class _SetSelectorCombo : public QComboBox
{
public:
	_SetSelectorCombo( QWidget* p ):QComboBox(p){}

	void showPopup ()
	{
		QComboBox::showPopup();
		emit activated( currentIndex() );
	}
};

class _ListItemDeleg : public QItemDelegate
{
public:
    _ListItemDeleg( QObject* p, Udb::Transaction* txn ):QItemDelegate(p),d_txn(txn){}
	QPoint d_pos;
    Udb::Transaction* d_txn;
	bool editorEvent(QEvent * e, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
	{
		switch( e->type() )
		{
		case QEvent::MouseButtonPress:
			{
				QMouseEvent* me = static_cast<QMouseEvent*>( e );
				if( me->button() == Qt::LeftButton )
				{
					d_pos = me->pos();
					ScheduleObj o = d_txn->getObject(
						index.data( ScheduleListMdl::ScheduleOid ).toULongLong() );
					QToolTip::showText(me->globalPos(), HeTypeDefs::toolTip(o) );
				}
			}
			break;
		case QEvent::MouseMove:
			if( !d_pos.isNull() )
			{
				QMouseEvent* me = static_cast<QMouseEvent*>( e );
				if( ( me->pos() - d_pos ).manhattanLength() > 5 )
					QToolTip::hideText();
			}
			break;
		case QEvent::MouseButtonRelease:
			d_pos = QPoint();
			QToolTip::hideText();
			break;
		}
		return QItemDelegate::editorEvent( e, model, option, index );
	}
};


ScheduleBoard::ScheduleBoard( QWidget* p, Transaction * txn ):
    QWidget(p),d_block(false),d_txn( txn )
{
    Q_ASSERT( txn != 0 );
	QHBoxLayout* topl = new QHBoxLayout( this );
	topl->setMargin(0);
	topl->setSpacing(0);

	d_splitter = new QSplitter( this );
	d_splitter->setHandleWidth( 4 );
	topl->addWidget( d_splitter );
	connect( d_splitter, SIGNAL( splitterMoved ( int, int ) ), this, SLOT( onSplitterMoved(int,int) ) );

	QWidget* lhs = new QWidget( d_splitter );
	QVBoxLayout* lhsl = new QVBoxLayout( lhs );
	lhsl->setMargin(0);
	lhsl->setSpacing(2);

	//hbox->addWidget(  );
	ScheduleBoardStrut* strut = new ScheduleBoardStrut( lhs );
	QVBoxLayout* svbox = new QVBoxLayout( strut );
	svbox->setMargin( 0 );
	svbox->setSpacing( 2 );

    Gui::AutoToolBar* tb = new Gui::AutoToolBar( strut );
	tb->setIconSize( QPixmap( ":/images/goto_today.png" ).size() );
	tb->setToolButtonStyle( Qt::ToolButtonIconOnly );
	svbox->addWidget( tb );

	d_startSelector = new CalendarPopup( strut );
	d_startSelector->calendarWidget()->setVerticalHeaderFormat( QCalendarWidget::ISOWeekNumbers );
	d_startSelector->calendarWidget()->setGridVisible(true);
	d_startSelector->calendarWidget()->setFirstDayOfWeek( Qt::Monday );
	connect( d_startSelector, SIGNAL( activated ( const QDate & ) ), this, SLOT( onLeftSel( const QDate & ) ) );
	svbox->addWidget( d_startSelector );
	d_setSelector = new _SetSelectorCombo( strut );
	d_setSelector->setToolTip( tr("Select Schedule Set") );
	d_setSelector->setInsertPolicy( QComboBox::InsertAlphabetically );
	connect( d_setSelector, SIGNAL( currentIndexChanged ( int ) ), this, SLOT( onSelectScheduleSet(int) ) );
	connect( d_setSelector, SIGNAL( activated ( int ) ), this, SLOT( onVsel(int) ) );
	svbox->addWidget( d_setSelector );
	lhsl->addWidget( strut );


	d_mdl = new ScheduleListMdl( this, d_txn );
    d_mdl->setMinRowHeight( QPixmap(":/images/calendar.png").height() + 2 );

    QHBoxLayout* lhbox = new QHBoxLayout();
    lhbox->setMargin(0);
    lhbox->setSpacing(0);

    d_vh = new QHeaderView( Qt::Vertical, lhs );
    d_vh->setFixedWidth( 15 );
    d_vh->setModel( d_mdl );
    d_vh->setSectionsClickable( true );
    d_vh->setSectionsMovable( true );
    d_vh->setSectionResizeMode( QHeaderView::Interactive );
    connect( d_vh, SIGNAL( sectionResized ( int, int, int ) ),
             this, SLOT( onSectionResized ( int, int, int ) ) );
	connect( d_vh, SIGNAL(sectionMoved(int,int,int)),this, SLOT(onRowMoved(int,int,int)));
    lhbox->addWidget( d_vh );

	d_list = new ScheduleBoardTree( lhs );
    d_list->setRootIsDecorated( false );
	d_list->setModel( d_mdl );
	d_list->setItemDelegate( new _ListItemDeleg( d_list, d_txn ) );
    d_list->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	d_list->header()->hide();
	d_list->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	d_list->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	d_list->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
	d_list->setSelectionMode( QAbstractItemView::ExtendedSelection );
	d_list->setSelectionBehavior( QAbstractItemView::SelectRows );
	d_list->setAlternatingRowColors( true );
	d_list->setAcceptDrops( true );
	// Wir implementieren Drag&Drop selber, also keine Qt-Automatismen
	connect( d_list->verticalScrollBar(), SIGNAL( rangeChanged( int, int ) ), this, SLOT( onAdjustScrollbar() ) );
	connect( d_mdl, SIGNAL( rowsInserted ( const QModelIndex &, int, int ) ), 
		this, SLOT( onRowsInserted ( const QModelIndex &, int, int ) ), Qt::QueuedConnection );
	connect(d_list->selectionModel(), SIGNAL(  selectionChanged ( const QItemSelection &, const QItemSelection & ) ), 
		this, SLOT( onSelectSchedule() ) );
    lhbox->addWidget( d_list );
	lhsl->addLayout( lhbox );

	QWidget* rhs = new QWidget( d_splitter );
	QVBoxLayout* rhsl = new QVBoxLayout( rhs );
	rhsl->setMargin(0);
	rhsl->setSpacing(2);

	d_timeLine = new TimelineView( rhs );
	d_timeLine->installEventFilter( strut );
	d_timeLine->setWeekendColor( QColor::fromRgb( 204, 232, 187 ) );
	connect( d_timeLine, SIGNAL( dayWidthChanged( bool ) ), this, SLOT( onDayWidthChanged( bool ) ) );
	connect( d_timeLine, SIGNAL( dateChanged( bool ) ), this, SLOT( onDateChanged( bool ) ) );
	rhsl->addWidget( d_timeLine );

	tb->addCommand( QIcon( ":/images/prev_page.png" ), tr("Backward Page"), d_timeLine, SLOT( prevPage() ) );
	tb->addCommand( QIcon( ":/images/prev_day.png" ), tr("Backward Day"), d_timeLine, SLOT( prevDay() ) );
	tb->addCommand( QIcon( ":/images/goto_today.png" ), tr("Goto Today"), d_timeLine, SLOT( gotoToday() ) );
	tb->addCommand( QIcon( ":/images/select_date.png" ), tr("Select Date"), this, SLOT( onSelectDate() ) );
	tb->addCommand( QIcon( ":/images/next_day.png" ), tr("Forward Day"), d_timeLine, SLOT( nextDay() ) );
	tb->addCommand( QIcon( ":/images/next_page.png" ), tr("Forward Page"), d_timeLine, SLOT( nextPage() ) );

	d_board = new ScheduleBoardTree( rhs );
    d_board->setRootIsDecorated( false );
	d_board->setModel( d_mdl );
	d_boardDeleg = new ScheduleBoardDeleg( d_board, d_txn );
	connect( d_boardDeleg, SIGNAL( itemSelected( quint64 ) ), this, SLOT( onSelected( quint64 ) ) );
	d_boardDeleg->setWeekendColor( d_timeLine->getWeekendColor() );
	d_boardDeleg->setDate( d_timeLine->getDate() );
	d_boardDeleg->setDayWidth( d_timeLine->getDayWidth() );
	connect( d_boardDeleg, SIGNAL( showStatusMessage( QString ) ), this, SLOT( onStatusMessage( QString ) ) );
	connect( d_boardDeleg, SIGNAL( doubleClicked() ), this, SLOT( onDoubleClicked() ) );
	d_board->setItemDelegate( d_boardDeleg );
	d_board->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    d_board->header()->setSectionResizeMode( 0, QHeaderView::Stretch );
	d_board->header()->hide();
	d_board->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	d_board->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	d_board->setSelectionMode( QAbstractItemView::NoSelection );
	d_board->setSelectionBehavior( QAbstractItemView::SelectRows );
	d_board->setEditTriggers( QAbstractItemView::AllEditTriggers );
	d_board->setAcceptDrops( true );
	d_board->setAlternatingRowColors( true );

	QPalette pal = d_board->palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 241, 241, 241 ) );
	d_list->setPalette( pal );
	d_board->setPalette( pal );
	d_timeLine->setPalette( pal );

	rhsl->addWidget( d_board );

	d_scrollBar = new QScrollBar( this );
	connect( d_scrollBar, SIGNAL( valueChanged( int ) ), d_list->verticalScrollBar(), SLOT( setValue(int) ) );
	connect( d_scrollBar, SIGNAL( valueChanged( int ) ), d_board->verticalScrollBar(), SLOT( setValue(int) ) );
    connect( d_scrollBar, SIGNAL( valueChanged( int ) ), d_vh, SLOT( setOffset(int) ) );
	connect( d_list->verticalScrollBar(), SIGNAL( valueChanged( int ) ), d_scrollBar, SLOT( setValue(int) ) );
	connect( d_board->verticalScrollBar(), SIGNAL( valueChanged( int ) ), d_scrollBar, SLOT( setValue(int) ) );
	topl->addWidget( d_scrollBar );

	d_splitter->setStretchFactor( 1, 1000 );
	d_list->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Ignored );
	strut->setMinimumWidth( tb->sizeHint().width() ); // mit diesem Trick ist die Liste mindestens so breit, dass
														// alle Buttons Platz haben

	fillScheduleListMenu();
	d_txn->commit(); // Es sind bis hier ev. neue Objekte angelegt worden.

	d_setSelector->setCurrentIndex( 0 ); // TODO: memorieren
	
	d_startSelector->setDate( d_timeLine->getDate() );

    QMetaObject::invokeMethod( d_timeLine, "gotoToday", Qt::QueuedConnection );
}

QPair<QDate, QDate> ScheduleBoard::getCursor() const
{
    return d_boardDeleg->getRange();
}

Obj ScheduleBoard::getSelectedCalendar() const
{
   return d_txn->getObject( d_boardDeleg->getSelCal() );
}

void ScheduleBoard::onStatusMessage( QString str )
{
	emit showStatusMessage( str );
}

void ScheduleBoard::fillScheduleListMenu()
{
	d_setSelector->clear();
	QList<Udb::Obj> l = ScheduleObj::getScheduleSets( d_txn );
	for( int i = 0; i < l.size(); i++ )
	{
		d_setSelector->addItem( l[i].getValue( AttrText ).toString(), l[i].getOid() );
    }
}

void ScheduleBoard::readItems(Stream::DataReader &in, Obj & o)
{
    Stream::DataReader::Token t = in.nextToken();
	while( Stream::DataReader::isUseful( t ) )
	{
        const Stream::NameTag name = in.getName().getTag();
		switch( t )
		{
		case Stream::DataReader::Slot:
            if( name.equals( "tx" ) )
                o.setValue( AttrText, in.readValue() );
            else if( name.equals( "sd" ) )
                o.setValue( AttrStartDate, in.readValue() );
            else if( name.equals( "ed" ) )
                o.setValue( AttrEndDate, in.readValue() );
            else if( name.equals( "ts" ) )
                o.setValue( AttrTimeSlot, in.readValue() );
            else if( name.equals( "lo" ) )
                o.setValue( AttrLocation, in.readValue() );
            //                else if( name.equals( "ci" ) )
            //                    o.setValue( AttrCiClass, in.readValue() );
            break;
		case Stream::DataReader::BeginFrame:
            if( name.equals( "e" ) || name.equals( "a" ) || name.equals( "d" ) )
            {
                ScheduleObj cal = o;
                quint32 type = 0;
                if( name.equals( "e" ) )
                    type = TypeEvent;
                else if( name.equals( "a" ) )
                    type = TypeAppointment;
                else if( name.equals( "d" ) )
                    type = TypeDeadline;
                Udb::Obj item = ObjectHelper::createObject( type, o );
                cal.setItemThread( item, 0 );
                readItems( in, item );
            }else if( name.equals( "t" ) || name.equals( "m" ) || name.equals( "p" ) )
                in.skipToEndFrame();
            else if( name.equals( Oln::OutlineStream::s_olnFrameTag ) )
            {
                Oln::OutlineStream str;
				Udb::Obj oln = str.readFrom( in, o.getTxn(), o.getOid(), false );
                oln.aggregateTo( o );
            }
			break;
		case Stream::DataReader::EndFrame:
			return; // Ok, gutes Ende
		}
		t =in.nextToken();
    }
}

void ScheduleBoard::onDateChanged( bool intermediate )
{
	d_boardDeleg->setDate( d_timeLine->getDate(), !intermediate );
	d_board->viewport()->update();
	d_startSelector->setDate( d_timeLine->getDate() );
}

void ScheduleBoard::onLeftSel( const QDate & d )
{
	if( d == d_timeLine->getDate() )
		return;
	d_timeLine->setDate( d, true );
}

void ScheduleBoard::onDayWidthChanged( bool intermediate )
{
	d_boardDeleg->setDayWidth( d_timeLine->getDayWidth() );
	if( !intermediate )
		d_boardDeleg->refill();
	d_board->viewport()->update();
}

void ScheduleBoard::onSectionResized( int row, int , int size )
{
	if( d_block )
		return;
	QModelIndex i = d_mdl->index( row, 0 );
    const int n = size / float( d_mdl->getMinRowHeight() ) + 0.5;
	if( i.data( ScheduleListMdl::ThreadCountRole ).toInt() != n )
	{
		d_mdl->setData( i, n, ScheduleListMdl::ThreadCountRole );
		d_txn->commit();
	}
	onRowsInserted( QModelIndex(), row, row );
}

void ScheduleBoard::onRowsInserted( const QModelIndex & parent, int start, int end )
{
	for( int row = start; row <= end; row++ )
	{
		QModelIndex i = d_mdl->index( row, 0, parent );
		const int h = d_mdl->getMinRowHeight() * i.data( ScheduleListMdl::ThreadCountRole ).toInt();
		d_block = true;
		d_vh->resizeSection( row, h );
		d_block = false;
	}
}

void ScheduleBoard::onAdjustScrollbar()
{
	QScrollBar* sb = d_list->verticalScrollBar();
	assert( sb );
	d_scrollBar->setRange( sb->minimum(), sb->maximum() );
	d_scrollBar->setSingleStep( d_mdl->getMinRowHeight() ); // sb->singleStep() );
	d_scrollBar->setPageStep( sb->pageStep() );
	d_scrollBar->setVisible( sb->maximum() != 0 );
}

void ScheduleBoard::onCreateSchedule()
{
	ENABLED_IF( true );

	QString name = QInputDialog::getText( this, tr("Create Schedule"), tr("Enter name:") );
	if( name.isEmpty() )
		return;
	Udb::Obj l = d_mdl->getList();
	Udb::Obj before;
	QModelIndex i = d_list->currentIndex();
	if( i.isValid() )
		before = l.getObject( i.data( ScheduleListMdl::ElementOid ).toULongLong() );
	if( !before.isNull() )
		before = before.getNext(); // Noch eins davor
	Udb::Obj elem = l.createAggregate( TypeSchedSetElem, before );
	Udb::Obj cal = ScheduleObj::createSchedule( d_txn, name );
	elem.setValue( AttrElementValue, cal );
	elem.setTimeStamp( AttrModifiedOn );
	d_txn->commit();
	i = d_mdl->getIndex( elem );
	assert( i.isValid() );
	d_list->setCurrentIndex( i );
	d_list->scrollTo( i );
}

void ScheduleBoard::onRenameSchedule()
{
	ENABLED_IF( d_list->currentIndex().isValid() );

	QModelIndex i = d_list->currentIndex();
	ScheduleObj cal = d_txn->getObject( i.data( ScheduleListMdl::ScheduleOid ).toULongLong() );
	bool ok;
	QString name = QInputDialog::getText( this, tr("Rename Schedule"), tr("Enter name:"), QLineEdit::Normal,
		cal.getString(AttrText), &ok );
	if( !ok )
		return;
	cal.setString( AttrText, name );
	cal.setTimeStamp( AttrModifiedOn );
	d_txn->commit();
}

void ScheduleBoard::onShowFirstItem()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows();
	QDate start;
	for( int i = 0; i < l.size(); i++ )
	{
		ScheduleObj cal = d_txn->getObject( l[i].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		QDate d = cal.earliestStart();
		if( !start.isValid() || d < start )
			start = d;
	}
	if( start.isValid() )
	{
		start = start.addDays( -( start.dayOfWeek() - 1 ) );
		d_timeLine->setDate( start, true );
	}
}

void ScheduleBoard::onShowLastItem()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows();
	QDate end;
	for( int i = 0; i < l.size(); i++ )
	{
		ScheduleObj cal = d_txn->getObject( l[i].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		QDate d = cal.latestEnd();
		if( !end.isValid() || d > end )
			end = d;
	}
	if( end.isValid() )
	{
		end = end.addDays( ( 7 - end.dayOfWeek() ) );
		d_timeLine->setEndDate( end, true );
	}
}

void ScheduleBoard::onCreateEvent()
{
	ScheduleObj cal( d_txn->getObject( d_boardDeleg->getSelCal() ) );
	ENABLED_IF( d_boardDeleg->hasRange() && HeTypeDefs::isValidAggregate( TypeEvent, cal.getType() ) );

	ScheduleItemPropsDlg dlg( this ); 
	if( dlg.createEvent( d_boardDeleg->getRange().first, d_boardDeleg->getRange().second ) )
	{
		ScheduleItemObj e = cal.createEvent( d_boardDeleg->getRange().first, d_boardDeleg->getRange().second, d_boardDeleg->getRangeRow() );
		if( dlg.saveTo( e ) )
		{
			e.commit();
			d_boardDeleg->setSelItem( e.getOid(), true );
		}else
			d_txn->rollback();
	}
}

void ScheduleBoard::onCreateAppointment()
{
	ScheduleObj cal( d_txn->getObject( d_boardDeleg->getSelCal() ) );
	ENABLED_IF( d_boardDeleg->hasRange() && d_boardDeleg->getRange().first == d_boardDeleg->getRange().second &&
		HeTypeDefs::isValidAggregate( TypeAppointment, cal.getType() ) );

	ScheduleItemPropsDlg dlg( this ); 
	if( dlg.createAppointment( d_boardDeleg->getRange().first ) )
	{
        ScheduleItemObj e = cal.createAppointment( d_boardDeleg->getRange().first, Stream::TimeSlot(0,0), d_boardDeleg->getRangeRow() );
		if( dlg.saveTo( e ) )
		{
			e.commit();
			d_boardDeleg->setSelItem( e.getOid(), true );
		}else
			d_txn->rollback();
	}
}

void ScheduleBoard::onCreateDeadline()
{
	ScheduleObj cal( d_txn->getObject( d_boardDeleg->getSelCal() ) );
	ENABLED_IF( d_boardDeleg->hasRange() && d_boardDeleg->getRange().first == d_boardDeleg->getRange().second &&
		HeTypeDefs::isValidAggregate( TypeDeadline, cal.getType() ) );

	ScheduleItemPropsDlg dlg( this ); 
	if( dlg.createDeadline( d_boardDeleg->getRange().first ) )
	{
		ScheduleItemObj e = cal.createDeadline( d_boardDeleg->getRange().first, Stream::TimeSlot(), d_boardDeleg->getRangeRow() );
		if( dlg.saveTo( e ) )
		{
			e.commit();
			d_boardDeleg->setSelItem( e.getOid(), true );
		}else
			d_txn->rollback();
	}
}

void ScheduleBoard::onRemoveItem()
{
	ENABLED_IF( d_boardDeleg->getSelItem() != 0 );

	ScheduleObj cal( d_txn->getObject( d_boardDeleg->getSelCal() ) );
	ScheduleItemObj e = cal.getObject( d_boardDeleg->getSelItem() );
	cal.removeItem( e );
	d_txn->commit();
}

void ScheduleBoard::onEraseItem()
{
	ENABLED_IF( d_boardDeleg->getSelItem() != 0 );
	ScheduleObj cal( d_txn->getObject( d_boardDeleg->getSelCal() ) );
	ScheduleItemObj e = cal.getObject( d_boardDeleg->getSelItem() );
	cal.removeItem( e );
	cal.setTimeStamp( AttrModifiedOn );
	e.erase();
	d_txn->commit();
}

void ScheduleBoard::onRowMoved( int logicalIndex, int oldVisualIndex, int newVisualIndex )
{
//	d_vh->blockSignals( true );
//	d_vh->moveSection( newVisualIndex, oldVisualIndex );
//	// Mache den Move rückgängig, so dass logische von visuellen Indizes nicht abweichen.
//	d_vh->blockSignals( false );

	Udb::Obj list = d_mdl->getList();
	QModelIndex index = d_mdl->index( oldVisualIndex, 0 );
	Udb::Obj eOld = list.getObject( index.data( ScheduleListMdl::ElementOid ).toULongLong() );
	index = d_mdl->index( newVisualIndex, 0 );
	Udb::Obj eNew = list.getObject( index.data( ScheduleListMdl::ElementOid ).toULongLong() );
	if( newVisualIndex == ( d_vh->count() - 1 ) && ( oldVisualIndex + 1 ) == newVisualIndex )
		eNew = eNew.getNext();
	Udb::Obj p = eOld.getParent();
	eOld.deaggregate();
	eOld.aggregateTo( p, eNew );
	d_txn->commit();
}

void ScheduleBoard::onZoom()
{
	ENABLED_IF( d_boardDeleg->hasRange() );

	d_timeLine->setDateAndWidth( d_boardDeleg->getRange().first, d_boardDeleg->getRange().first.daysTo( d_boardDeleg->getRange().second ), true );
}

void ScheduleBoard::changeEvent ( QEvent * e )
{
	QWidget::changeEvent( e );
	if( e->type() == QEvent::FontChange )
	{
		onRowsInserted( QModelIndex(), 0, d_mdl->rowCount() - 1 );
	}
}

void ScheduleBoard::onSplitterMoved(int pos,int index)
{
	QSplitter* d_splitter = static_cast<QSplitter*>( sender() );
	HeraldApp::inst()->getSet()->setValue("ScheduleBoard/Splitter", d_splitter->saveState() );
}

void ScheduleBoard::onExportAll()
{
	ENABLED_IF( true );

	QString path = QFileDialog::getSaveFileName( this, tr("Export iCalendar"), 
		QString(), "iCalendar File (*.ics)" );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".ics" ) ) )
		path += QLatin1String( ".ics" );
	QFile file( path );
	if( !file.open( QIODevice::WriteOnly ) )
	{
		QMessageBox::critical( this, tr("Export iCalendar"), tr("Cannot open file for writing" ) );
		return;
	}
	QTextStream out( &file );
	out.setCodec( "UTF-8" );
	IcalExport::startCalendar( out );
	Udb::Obj e = d_mdl->getList().getFirstObj();
	if( !e.isNull() ) do
	{
		assert( e.getType() == TypeSchedSetElem );
		Obj cal = e.getValueAsObj( AttrElementValue );
		if( !cal.isNull() )
		{
			IcalExport::exportSchedule( cal, out );
		}
	}while( e.next() );
	IcalExport::endCalendar( out );
	d_txn->commit(); // Da ev. UUID generiert
}

void ScheduleBoard::onExportSel()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QString path = QFileDialog::getSaveFileName( this, tr("Export iCalendar"), 
		QString(), "iCalendar File (*.ics)" );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".ics" ) ) )
		path += QLatin1String( ".ics" );
	QFile file( path );
	if( !file.open( QIODevice::WriteOnly ) )
	{
		QMessageBox::critical( this, tr("Export iCalendar"), tr("Cannot open file for writing" ) );
		return;
	}
	QTextStream out( &file );
	out.setCodec( "UTF-8" );
	IcalExport::startCalendar( out );
	QModelIndexList l = d_list->selectionModel()->selectedRows( 0 );
	for( int row = 0; row < l.size(); row++ )
	{
		Udb::Obj cal = d_txn->getObject( l[row].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		if( !cal.isNull() )
			IcalExport::exportSchedule( cal, out );
	}
	IcalExport::endCalendar( out );
	d_txn->commit(); // Da ev. UUID generiert
}

void ScheduleBoard::onSelectDate()
{
	ENABLED_IF( true );

	QPoint pos = QCursor::pos();
    QSize size = d_startSelector->sizeHint();
    QRect screen = QApplication::desktop()->availableGeometry();
    //handle popup falling "off screen"
    if (pos.x()+size.width() > screen.right())
        pos.setX(screen.right()-size.width());
    pos.setX(qMax(pos.x(), screen.left()));
    if (pos.y() + size.height() > screen.bottom())
        pos.setY(pos.y() - size.height());
    else if (pos.y() < screen.top())
        pos.setY(screen.top());
    if (pos.y() < screen.top())
        pos.setY(screen.top());
    if (pos.y()+size.height() > screen.bottom())
        pos.setY(screen.bottom()-size.height());
    d_startSelector->move(pos);
	d_startSelector->show();
}

void ScheduleBoard::onSelectScheduleSet(int i)
{
	Udb::Obj o = d_txn->getObject( d_setSelector->itemData( i ).toULongLong() );
	if( !o.isNull() )
	{
		d_mdl->setList( o );
		onRowsInserted( QModelIndex(), 0, d_mdl->rowCount() - 1 );
		onAdjustScrollbar();
		objectSelected( o.getOid() );
		d_block = true;
		d_list->clearSelection();
		d_block = false;
		d_boardDeleg->clearSelection();
	}
}

void ScheduleBoard::onVsel(int i)
{
	Udb::Obj o = d_txn->getObject( d_setSelector->itemData( i ).toULongLong() );
	if( !o.isNull() )
	{
		emit objectSelected( o.getOid() );
		d_block = true;
		d_list->clearSelection();
		d_block = false;
		d_boardDeleg->clearSelection();
	}
}

void ScheduleBoard::onRenameScheduleSet()
{
	ENABLED_IF( d_setSelector->count() > 0 && d_setSelector->currentIndex() != -1 );

	const int i = d_setSelector->currentIndex();
	Udb::Obj o = d_txn->getObject( d_setSelector->itemData( i ).toULongLong() );
	if( o.isNull() )
		return;

	QString name = QInputDialog::getText( this, tr("Rename Schedule Set"), tr("Enter name:"), QLineEdit::Normal,
		o.getValue( AttrText ).toString() );
	if( name.isEmpty() )
		return;
	o.setValue( AttrText, Stream::DataCell().setString( name ) );
	o.setTimeStamp( AttrModifiedOn );
	o.commit();
	d_setSelector->setItemText( i, name );
}

void ScheduleBoard::onCreateScheduleSet()
{
	ENABLED_IF( true );

	QString name = QInputDialog::getText( this, tr("Add Schedule Set"), tr("Enter name:") );
	if( name.isEmpty() )
		return;
	Udb::Obj o = ScheduleObj::createScheduleSet( d_txn, name );
	o.commit();
	d_setSelector->addItem( name, o.getOid() );
	d_setSelector->setCurrentIndex( d_setSelector->count() - 1 );
}

struct _Item
{
	QString d_name;
	QDate d_start;
	QDate d_end;
	int d_track;
	_Item():d_track(0) {}
};
static inline bool _LessThan(const _Item &p1, const _Item &p2)
{
	return p1.d_start < p2.d_start || (!(p2.d_start < p1.d_start) && p1.d_end < p2.d_end );
}

void ScheduleBoard::onPasteItems()
{
	ENABLED_IF( QApplication::clipboard()->mimeData()->hasText() && d_list->currentIndex().isValid() );

	//qDebug() << QApplication::clipboard()->mimeData()->formats();
	//return; // TEST

	QList<_Item> items;
	int failed = 0;
	{
		QDate start;
		QDate end;
		QStringList line = QApplication::clipboard()->text().split( QChar('\n') );
		int i;
		for( i = 0; i < line.size(); i++ )
		{
			QStringList fields = line[i].split( QChar('\t') );
			if( fields.size() == 3 )
			{
				_Item item;
				item.d_name = fields[0];
				item.d_start = QDate::fromString( fields[1], "d.M.yy" ); // yy führt zu 1900!!
				item.d_end = QDate::fromString( fields[2], "d.M.yy" );
				if( item.d_start.isValid() && item.d_end.isValid() )
				{
					item.d_start = item.d_start.addYears( 100 );
					item.d_end = item.d_end.addYears( 100 );
					if( item.d_end < item.d_start )
					{
						QDate tmp = item.d_end;
						item.d_end = item.d_start;
						item.d_start = tmp;
					}
					if( !start.isValid() || item.d_start < start )
						start = item.d_start;
					if( !end.isValid() || item.d_end > end )
						end = item.d_end;
					items.append( item );
				}else
				{
					qWarning() << "onPasteItems invalid dates: " << fields[1] << fields[2];
					failed++;
				}
			}else if( fields.size() != 0 )
			{
				failed++;
				qWarning() << "onPasteItems invalid line: " << line[i];
			}
		}
		qSort( items.begin(), items.end(), _LessThan );
		// Verwende eine Bit-Matrix mit einer Zelle pro Tag und Track, um das Layout zu machen
		// TODO: ineffizienter erster Wurf
		QList<QBitArray> tracks;
		const int dayCount = start.daysTo( end ) + 1;
		tracks.append( QBitArray( dayCount ) );
		for( i = 0; i < items.size(); i++ )
		{
			const int a = start.daysTo( items[i].d_start );
			const int b = start.daysTo( items[i].d_end );
			bool placed = false;
			for( int t = 0; t < tracks.size(); t++ )
			{
				bool occupied = false;
				for( int d = a; d <= b; d++ )
				{
					if( tracks[t].testBit( d ) )
					{
						occupied = true;
						break;
					}
				}
				if( !occupied )
				{
					tracks[t].fill( true, a, b + 1 );
					items[i].d_track = t;
					placed = true;
					break;
				}
			}
			if( !placed )
			{
				if( tracks.size() >= 20 )
				{
					QMessageBox::critical( this, tr("Paste Items"),
						tr("More than 20 tracks are needed to add the given items. "
						"Please divide surplus parallel items into different schedules." ) );
					return;
				}
				tracks.append( QBitArray( dayCount ) );
				tracks.last().fill( true, a, b + 1 );
				items[i].d_track = tracks.size() - 1;
			}
		}
	}
	if( items.isEmpty() )
	{
		QMessageBox::information( this, tr("Paste Items"),
			tr("There are no valid items to paste!" ) );
		return;
	}else
	{
		if( QMessageBox::warning( this, tr("Paste Items"),
			tr("Do you really want to insert %1 items? %2 items are invalid." ).
			arg( items.size() ).arg( failed ), 
			QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
			return;
	}

	ScheduleObj cal = d_txn->getObject(
		d_list->currentIndex().data( ScheduleListMdl::ScheduleOid ).toULongLong() );

	for( int i = 0; i < items.size(); i++ )
	{
		// qDebug() << items[i].d_name << items[i].d_start << items[i].d_end << items[i].d_track; 
		ScheduleItemObj e = cal.createEvent( items[i].d_start, items[i].d_end, items[i].d_track );
		e.setString( AttrText, items[i].d_name );
	}
	d_txn->commit();
}

void ScheduleBoard::onRemoveSchedules()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows( 0 );
	for( int row = 0; row < l.size(); row++ )
	{
		Udb::Obj e = d_txn->getObject( l[row].data( ScheduleListMdl::ElementOid ).toULongLong() );
		if( !e.isNull() )
			e.erase();
	}
	d_txn->commit();
}

void ScheduleBoard::onSelectSchedules()
{
	ENABLED_IF( true );

	ScheduleSelectorDlg dlg( this, d_txn );

	Udb::Obj l = d_mdl->getList();
	ScheduleSelectorDlg::Objs s;
	Udb::Obj e = l.getFirstObj();
	if( !e.isNull() ) do
	{
		if( e.getType() == TypeSchedSetElem )
			s.append( e.getValueAsObj( AttrElementValue ) );
	}while( e.next() );
	s = dlg.select( s );
	if( s.isEmpty() )
		return;
	Udb::Obj before;
	QModelIndex i = d_list->currentIndex();
	if( i.isValid() )
		before = l.getObject( i.data( ScheduleListMdl::ElementOid ).toULongLong() );
	if( !before.isNull() )
		before = before.getNext(); // Noch eins davor

	for( int j = 0; j < s.size(); j++ )
	{
		Udb::Obj elem = l.createAggregate( TypeSchedSetElem, before );
		elem.setValue( AttrElementValue, s[j] );
	}
	d_txn->commit();
}

void ScheduleBoard::addSchedules( const QList<quint64>& wps )
{
	Udb::Obj before;
	QModelIndex i = d_list->currentIndex();
	Udb::Obj l = d_mdl->getList();
	if( i.isValid() )
		before = l.getObject( i.data( ScheduleListMdl::ElementOid ).toULongLong() );
	if( !before.isNull() )
		before = before.getNext(); // Noch eins davor

	for( int j = 0; j < wps.size(); j++ )
	{
		Udb::Obj o = d_txn->getObject( wps[j] );
		switch( o.getType() )
		{
		case TypePerson:
		case TypeSchedule:
			{
				Udb::Obj elem = l.createAggregate( TypeSchedSetElem, before );
				elem.setValue( AttrElementValue, o );
			}
			break;
		}
	}
    d_txn->commit();
}

void ScheduleBoard::restoreSplitter()
{
    QVariant state = HeraldApp::inst()->getSet()->value( "ScheduleBoard/Splitter" );
	if( !state.isNull() )
		d_splitter->restoreState( state.toByteArray() );
}

void ScheduleBoard::onSelectPersons()
{
	ENABLED_IF( true );

	PersonSelectorDlg dlg( this, d_txn );

	QList<quint64> s = dlg.select();
	if( s.isEmpty() )
		return;
	Udb::Obj l = d_mdl->getList();
	Udb::Obj before;
	QModelIndex i = d_list->currentIndex();
	if( i.isValid() )
		before = l.getObject( i.data( ScheduleListMdl::ElementOid ).toULongLong() );
	if( !before.isNull() )
		before = before.getNext(); // Noch eins davor

	for( int j = 0; j < s.size(); j++ )
	{
		Udb::Obj elem = l.createAggregate( TypeSchedSetElem, before );
		elem.setValue( AttrElementValue, Stream::DataCell().setOid( s[j] ) );
	}
	d_txn->commit();
}

void ScheduleBoard::onDeleteScheduleSet()
{
	ENABLED_IF( d_setSelector->count() > 1 );

	const int i = d_setSelector->currentIndex();
	Udb::Obj o = d_txn->getObject( d_setSelector->itemData( i ).toULongLong() );
	if( o.isNull() )
		return;

	if( QMessageBox::question( this, tr("Delete Schedule Set - Herald"),
		tr("Are you sure you want to delete this Schedule Set? This cannot be undone."),
		QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel ) != QMessageBox::Yes )
		return;
	d_setSelector->removeItem( i );
	o.erase();
	o.commit();
}

void ScheduleBoard::onSelected( quint64 id )
{
	emit objectSelected( id );
	d_block = true;
	d_list->clearSelection();
	d_block = false;
}

void ScheduleBoard::onSelectSchedule()
{
	QModelIndexList l = d_list->selectionModel()->selectedRows();
	if( l.size() == 1 )
	{
		Udb::Obj e = d_mdl->getElem( l.first() );
		if( !e.isNull() )
		{
			emit objectSelected( e.getValue( AttrElementValue ).getOid() );
			d_boardDeleg->clearSelection();
		}
	}else if( !d_block )
		emit objectSelected( 0 );
}

void ScheduleBoard::onToDeadline()
{
	if( d_boardDeleg->getSelItem() == 0 )
		return;
	Udb::Obj obj = d_txn->getObject( d_boardDeleg->getSelItem() );
	ENABLED_IF( !obj.isNull() && ( obj.getType() == TypeEvent || obj.getType() == TypeAppointment ) );

	obj.setValue( AttrTimeSlot, Stream::DataCell().setTimeSlot( 
		Stream::TimeSlot( obj.getValue( AttrTimeSlot ).getTimeSlot().d_start, 0 ) ) );
	obj.setValue( AttrLocation, Stream::DataCell().setNull() );
	obj.setValue( AttrEndDate, Stream::DataCell().setNull() );
	obj.setType( TypeDeadline );
	obj.setTimeStamp( AttrModifiedOn );
	obj.commit();
}

void ScheduleBoard::onToEvent()
{
	if( d_boardDeleg->getSelItem() == 0 )
		return;
	Udb::Obj obj = d_txn->getObject( d_boardDeleg->getSelItem() );
	ENABLED_IF( !obj.isNull() && HeTypeDefs::isCalendarItem( obj.getType() ) );

	obj.setValue( AttrTimeSlot, Stream::DataCell().setNull() );
	obj.setValue( AttrLocation, Stream::DataCell().setNull() );
	obj.setType( TypeEvent );
	obj.setTimeStamp( AttrModifiedOn );
	obj.commit();
}

void ScheduleBoard::onToAppointment()
{
	if( d_boardDeleg->getSelItem() == 0 )
		return;
	Udb::Obj obj = d_txn->getObject( d_boardDeleg->getSelItem() );
	ENABLED_IF( !obj.isNull() && ( obj.getType() == TypeDeadline || obj.getType() == TypeEvent ) );

	obj.setValue( AttrEndDate, Stream::DataCell().setNull() );
	obj.setType( TypeAppointment );
	obj.setTimeStamp( AttrModifiedOn );
	obj.commit();
}

void ScheduleBoard::onDoubleClicked()
{
	Udb::OID id = d_boardDeleg->getSelItem();
	if( id )
		emit objectDblClicked( id );
}

static inline Udb::Obj inList( const Udb::Obj& l, const Udb::Obj& i )
{
	Udb::Obj e = l.getFirstObj();
	if( !e.isNull() ) do
	{
		assert( e.getType() == TypeSchedSetElem );
		if( e.getValue( AttrElementValue ).getOid() == i.getOid() )
			return e;
	}while( e.next() );
	return Udb::Obj();
}

bool ScheduleBoard::gotoItem( quint64 id )
{
	ScheduleItemObj item = d_txn->getObject( id );
	if( item.isNull() )
		return false; 
	Udb::Obj sched = item.getParent();
	if( HeTypeDefs::isScheduleItem( sched.getType() ) )
	{
		item = sched; // Das Item selber hat ein Item als Parent
		sched = item.getParent();
	}else if( item.getType() == TypeSchedule )
	{
		sched = item;
		item = Udb::Obj();
    }else if( !HeTypeDefs::isScheduleItem( item.getType() ) )
        return false;
	Udb::Obj sset;
	Udb::Obj elem = inList( d_mdl->getList(), sched );
	if( elem.isNull() )
	{
		// Der Schedule ist nicht Teil des angezeigten ScheduleSets
		QList<Udb::Obj> l = ScheduleObj::getScheduleSets(d_txn);
		for( int i = 0; i < l.size(); i++ )
		{
			elem = inList( l[i], sched );
			if( !l[i].equals( d_mdl->getList() ) && !elem.isNull() )
			{
				d_setSelector->setCurrentIndex( i );
				sset = l[i];
				break;
			}
		}
	}else
		sset = d_mdl->getList();
	if( sset.isNull() )
	{
		sset = d_mdl->getList();
		// Schedule ist in keinem Set vorhanden. Füge Schedule ans Ende des angezeigten Sets
		Udb::Obj l = d_mdl->getList();
		elem = l.createAggregate( TypeSchedSetElem );
		elem.setValue( AttrElementValue, sched );
		elem.commit();  
	}
	// Nun haben wir alles beieinander: Item, Schedule und Set, und Set ist angezeigt

	// Mache Schedule Current und zeige Zeile an
	d_block = true;
	d_list->selectionModel()->clearSelection(); // Es soll nur der Current selektiert sein.
	d_list->setCurrentIndex( d_mdl->getIndex( elem ) );
	d_block = false;
	d_list->scrollTo( d_list->currentIndex() );

	// Setze Datumsbereich auf Wochenstart
	if( !item.isNull() )
	{
		const QDate start = item.getStart();
		d_timeLine->gotoDate( start.addDays( -( start.dayOfWeek() - 1 ) ) );
		d_boardDeleg->setSelItem( item.getOid(), true );
	}

    return true;
}

void ScheduleBoard::gotoDate(const QDate & start, const QDate& end )
{
    d_timeLine->gotoDate( start );
    d_boardDeleg->setRange( start, end );
}

void ScheduleBoard::onSortListByTypeName()
{
	ENABLED_IF( true );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	d_mdl->sortListByTypeName();
	QApplication::restoreOverrideCursor();
}

void ScheduleBoard::onSortListByName()
{
	ENABLED_IF( true );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	d_mdl->sortListByName();
	QApplication::restoreOverrideCursor();
}

void ScheduleBoard::setFilter( const QList<Udb::Obj>& l )
{
	d_boardDeleg->setFilter( l );
}

void ScheduleBoard::onSaveScheduleSets()
{
	ENABLED_IF( true );

	ScheduleSetSelectorDlg dlg( this, d_txn );

	ScheduleSetSelectorDlg::Objs l = dlg.select();
	if( l.isEmpty() )
		return;
	QString path = QFileDialog::getSaveFileName( this, tr("Save Schedule Sets"), 
		QString(), "Schedule Set Definition File (*.mpsd)" );
	if( path.isNull() )
		return;
	if( !path.toLower().endsWith( QLatin1String( ".mpsd" ) ) )
		path += QLatin1String( ".mpsd" );
	QFile file( path );
	if( !file.open( QIODevice::WriteOnly ) )
	{
		QMessageBox::critical( this, tr("Save Schedule Sets"), tr("Cannot open file for writing" ) );
		return;
	}
	Stream::DataWriter out( &file );
	out.writeSlot( Stream::DataCell().setAscii("mpsd") );
	out.writeSlot( Stream::DataCell().setAscii( "1.0" ) );
	for( int i = 0; i < l.size(); i++ )
	{
		out.startFrame( Stream::NameTag("sset") );
		out.writeSlot( l[i].getValue( AttrText ), Stream::NameTag( "name" ) );
		Udb::Obj e = l[i].getFirstObj();
		if( !e.isNull() ) do
		{
			assert( e.getType() == TypeSchedSetElem );
			Udb::Obj o = e.getValueAsObj( AttrElementValue );
			assert( !o.isNull() );
			out.writeSlot( Stream::DataCell().setUuid( o.getUuid() ), Stream::NameTag( "sche" ) );
		}while( e.next() );
		out.endFrame();
	}
	d_txn->commit(); // wegen getUuid
}

void ScheduleBoard::onLoadScheduleSets()
{
	ENABLED_IF( true );

	QString path = QFileDialog::getOpenFileName( this, tr("Load Schedule Sets"), 
		QString(), "Schedule Set Definition File (*.mpsd)" );
	if( path.isNull() )
		return;
	QFile file( path );
	if( !file.open( QIODevice::ReadOnly ) )
	{
		QMessageBox::critical( this, tr("Load Schedule Sets"), tr("Cannot open file for reading" ) );
		return;
	}
	Stream::DataReader in( &file );
	if( in.nextToken() != Stream::DataReader::Slot || in.readValue().getArr() != "mpsd" )
	{
		QMessageBox::critical( this, tr("Load Schedule Sets"), tr("Wrong file format" ) );
		return;
	}
	if( in.nextToken() != Stream::DataReader::Slot || in.readValue().getArr() != "1.0" )
	{
		QMessageBox::critical( this, tr("Load Schedule Sets"), tr("Wrong file version" ) );
		return;
	}
	Stream::DataReader::Token t = in.nextToken();
	QList<Udb::Obj> sset;
	while( Stream::DataReader::isUseful( t ) )
	{
		switch( t )
		{
		case Stream::DataReader::BeginFrame:
			if( in.getName().getTag().equals( "sset" ) )
				sset.append( ScheduleObj::createScheduleSet( d_txn, "" ) );
			else
				goto err_file_format;
			break;
		case Stream::DataReader::Slot:
			{
				if( sset.isEmpty() )
					goto err_file_format;
				Stream::NameTag name = in.getName().getTag();
				if( name.equals( "name" ) )
					sset.last().setValue( AttrText, in.readValue() );
				else if( name.equals( "sche" ) )
				{
					Udb::Obj o = d_txn->getObject( in.readValue().getUuid() );
					if( !o.isNull() )
					{
						Udb::Obj elem = sset.last().createAggregate( TypeSchedSetElem );
						elem.setValue( AttrElementValue, o );
						elem.setTimeStamp( AttrModifiedOn );
					}
				}else
					goto err_file_format;
			}
			break;
		}
		t = in.nextToken();
	}
	d_txn->commit();
	for( int i = 0; i < sset.size(); i++ )
		d_setSelector->addItem( sset[i].getValue( AttrText ).toString(), sset[i].getOid() );
	d_setSelector->setCurrentIndex( d_setSelector->count() - 1 );
	return;
err_file_format:
	d_txn->rollback();
	QMessageBox::critical( this, tr("Load Schedule Sets"), tr("Wrong file format" ) );
}

void ScheduleBoard::onAdjustSections()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows( 0 );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	for( int row = 0; row < l.size(); row++ )
	{
		ScheduleObj o = d_txn->getObject( l[row].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		o.setRowCount( o.getMaxThread() + 1 );
	}
	d_txn->commit();
	onRowsInserted( QModelIndex(), 0, d_mdl->rowCount() - 1 );
	onAdjustScrollbar();
	QApplication::restoreOverrideCursor();
}

void ScheduleBoard::onLayoutSections()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows( 0 );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	for( int row = 0; row < l.size(); row++ )
	{
		ScheduleObj o = d_txn->getObject( l[row].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		o.layoutItems();
		d_boardDeleg->clearRow( l[row].row() );
	}
	d_txn->commit();
	d_board->viewport()->update();
	QApplication::restoreOverrideCursor();
}

void ScheduleBoard::onCollapseSections()
{
	ENABLED_IF( d_list->selectionModel()->hasSelection() );

	QModelIndexList l = d_list->selectionModel()->selectedRows( 0 );
	QApplication::setOverrideCursor( Qt::WaitCursor );
	for( int row = 0; row < l.size(); row++ )
	{
		ScheduleObj o = d_txn->getObject( l[row].data( ScheduleListMdl::ScheduleOid ).toULongLong() );
		o.setRowCount( 1 );
	}
	d_txn->commit();
	onRowsInserted( QModelIndex(), 0, d_mdl->rowCount() - 1 );
	onAdjustScrollbar();
	QApplication::restoreOverrideCursor();
}

void ScheduleBoard::onCopyLink()
{
    Udb::Obj o;
	if( d_list->hasFocus() )
	{
		ENABLED_IF( d_list->currentIndex().isValid() );
		o = d_mdl->getElem( d_list->currentIndex() );
	}else if( d_board->hasFocus() )
	{
		ENABLED_IF( d_boardDeleg->getSelItem() != 0 );
		o = d_txn->getObject( d_boardDeleg->getSelItem() );
	}else if( d_setSelector->hasFocus() )
	{
		ENABLED_IF( d_setSelector->count() > 0 && d_setSelector->currentIndex() != -1 );
		o = d_txn->getObject(
			d_setSelector->itemData( d_setSelector->currentIndex() ).toULongLong() );
    }
    if( o.isNull() )
        return;
    QMimeData* mimeData = new QMimeData();
    HeTypeDefs::writeObjectRefs( mimeData, QList<Udb::Obj>() << o );
    QApplication::clipboard()->setMimeData( mimeData );
}

void ScheduleBoard::onImportSched()
{
    QModelIndexList rows = d_list->selectionModel()->selectedRows();
    ENABLED_IF( rows.size() == 1 );

    const QString title = tr("Import Schedule Items - Herald");
    QString selectedFilter;
	QString path = QFileDialog::getOpenFileName( this, title,
		QString(), "iCalendar File (*.ics);;Schedule (*.mps)", &selectedFilter );
	if( path.isNull() )
		return;

    ScheduleObj cal = d_txn->getObject( rows.first().data( ScheduleListMdl::ScheduleOid ).toULongLong() );
    if( cal.isNull() )
        return;

    QFile f(path);
    if( !f.open(QIODevice::ReadOnly ) )
    {
        QMessageBox::critical( this, title, tr("Cannot open file for reading" ) );
        return;
    }
    if( selectedFilter == "Schedule (*.mps)" )
    {
        Stream::DataReader in( &f );
        readItems( in, cal );
        d_txn->commit();
    }else if( selectedFilter == "iCalendar File (*.ics)" )
    {
        QApplication::setOverrideCursor( Qt::WaitCursor );
        IcsDocument ics;
        if( !ics.loadFrom( f ) )
        {
            QApplication::restoreOverrideCursor();
            QMessageBox::critical( this, title, tr("Error loading iCalendar file: %1" ).arg( ics.getError() ) );
            return;
        }
        foreach( ScheduleItemData item, ScheduleObj::readEvents( ics, true ) )
        {
            Udb::Obj e;
            if( !item.d_slot.isValid() )
                e = cal.createEvent( item.d_start, item.d_end );
            else
                e = cal.createAppointment( item.d_start, item.d_slot );
            e.setString( AttrText, item.d_summary );
            e.setString( AttrLocation, item.d_location );
            // ignore reason and uid
        }
        cal.layoutItems();
        d_txn->commit();
        d_boardDeleg->clearRow( rows.first().row() );
        d_board->viewport()->update();
        QApplication::restoreOverrideCursor();
    }
}

void ScheduleBoard::onDefaultSched()
{
    QModelIndexList rows = d_list->selectionModel()->selectedRows();
    if( rows.size() != 1 )
        return;
    Udb::Obj cal = d_txn->getObject( rows.first().data( ScheduleListMdl::ScheduleOid ).toULongLong() );
    CHECKED_IF( !cal.isNull(), cal.equals( ScheduleObj::getDefaultSchedule(d_txn) ) );
    ScheduleObj::setDefaultSchedule( d_txn, cal );
    d_txn->commit();
}

void ScheduleBoard::onSetOwner()
{
    QModelIndexList rows = d_list->selectionModel()->selectedRows();
    if( rows.size() != 1 )
        return;
    Udb::Obj cal = d_txn->getObject( rows.first().data( ScheduleListMdl::ScheduleOid ).toULongLong() );
    ENABLED_IF( !cal.isNull() );

    cal.setValueAsObj( AttrSchedOwner, MailObj::simpleAddressEntry( this, d_txn,
                                                                    cal.getValueAsObj(AttrSchedOwner) ) );
    cal.setTimeStamp( AttrModifiedOn );
    d_txn->commit();

}
