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

#include "ScheduleBoardDeleg.h"
#include "ScheduleListMdl.h"
#include <QPainter>
#include <QApplication>
#include <QHeaderView>
#include <QTreeView>
#include <QMouseEvent>
#include <QDrag>
#include <QToolTip>
#include <QtDebug>
#include <Stream/DataWriter.h>
#include <Stream/DataReader.h>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include "ScheduleObj.h"
#include "HeraldApp.h"
#include "ScheduleItemObj.h"
#include "HeTypeDefs.h"
using namespace He;

// 1:1 aus MasterPlan

static const int s_dc = 7;
static const char* s_mime = "application/masterplan/schedule-item";



static inline QString _format( QDate d, int a, int b )
{
	const QDate from = (a < b)?d.addDays( a ):d.addDays( b );
	const QDate to = (a < b)?d.addDays( b ):d.addDays( a );
	if( from == to )
		return ScheduleBoardDeleg::tr( "Selected: %1" ).arg( HeTypeDefs::prettyDate( from ) );
	else
		return ScheduleBoardDeleg::tr( "Selected: %1 to %2" ).arg( HeTypeDefs::prettyDate( from ) ).arg( HeTypeDefs::prettyDate( to ) );
}

static inline QString _format( QDate d, int off )
{
	return ScheduleBoardDeleg::tr( "Drop point: %1" ).arg( HeTypeDefs::prettyDate( d.addDays( off ) ) );
}

static inline QString _format( const ScheduleItemObj& e )
{
	if( e.isNull() )
		return QString();
	const QDate from = e.getStart();
	const QDate to = e.getEnd();
	if( from == to )
		return ScheduleBoardDeleg::tr( "Selected: %1 | %2" ).arg( e.getString(AttrText) ).
			arg( HeTypeDefs::prettyDate( from ) );
	else
		return ScheduleBoardDeleg::tr( "Selected: %1 | %2 to %3" ).arg( e.getString(AttrText) ).
			arg( HeTypeDefs::prettyDate( from ) ).
			arg( HeTypeDefs::prettyDate( to ) );
}

ScheduleBoardDeleg::ScheduleBoardDeleg( QTreeView* p, Udb::Transaction * txn ):
    QAbstractItemDelegate( p ),d_dragging(false),d_txn(txn),
    d_selRow( -1 ), d_selStart( -1 ), d_selEnd(-1), d_picked( 0 ), d_showCalendarItems( true )
{
    Q_ASSERT( txn != 0 );
	d_dw = 10;
	d_left = QDate::currentDate();
	txn->getDb()->addObserver( this, SLOT( onDatabaseUpdate( Udb::UpdateInfo ) ) );
	connect( p->model(), SIGNAL( modelReset () ), this, SLOT( clearCache () ) );
	connect( p->model(), SIGNAL( rowsAboutToBeInserted ( const QModelIndex &, int, int ) ), this, SLOT( clearCache() ) );
	connect( p->model(), SIGNAL( rowsAboutToBeRemoved ( const QModelIndex &, int, int ) ), this, SLOT( clearCache() ) );
	p->installEventFilter( this ); // wegen Drag/Drop und Resize
	// NOTE: funktioniert auch mit viewport()
	d_weekend = p->palette().color( QPalette::AlternateBase );
}


void ScheduleBoardDeleg::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
	QBrush bg = option.palette.brush(QPalette::Base);
	//if( option.state & QStyle::State_Selected ) // QStyle::State_HasFocus 
	//	bg = Qt::lightGray;

	if( getView()->alternatingRowColors() && index.row() & 1 ) 
		//bg = QColor::fromRgb( 255, 254, 225 ); // QColor::fromRgb( 255, 254, 225 ); // option.palette.brush(QPalette::AlternateBase);
		bg = option.palette.color( QPalette::Normal, QPalette::AlternateBase );
	painter->fillRect( option.rect, bg );

	drawGrid( *painter, option, index );
	drawBars( *painter, option, index );
}

QTreeView* ScheduleBoardDeleg::getView() const
{
	return static_cast<QTreeView*>( parent() );
}

bool ScheduleBoardDeleg::editorEvent(QEvent * e, QAbstractItemModel *model, const QStyleOptionViewItem &option, 
									 const QModelIndex &index)
{
	switch( e->type() )
	{
	case QEvent::MouseButtonPress:
		{
			QMouseEvent* me = static_cast<QMouseEvent*>( e );
			if( me->button() == Qt::LeftButton )
			{
				if( d_edit != index )
				{
					d_selRow = -1;
					d_edit = index;
				}
				const quint32 oid = pickBar( me->pos(), option.rect, index );
				d_pos = me->pos();
				if( oid != 0 )
				{
					d_selRow = -1;
					d_picked = oid;
					ScheduleItemObj o = d_txn->getObject( oid );
					emit showStatusMessage( _format( o ) );
					QToolTip::showText(me->globalPos(), HeTypeDefs::toolTip(o), getView() );
				}else
				{
					d_picked = 0;
					d_dragging = true; 
					d_selRow = ( me->y() - option.rect.y() ) / index.data( ScheduleListMdl::MinRowHeight ).toInt();
					d_selStart = ( me->x() - option.rect.x() ) / d_dw;
					d_selEnd = d_selStart;
					emit showStatusMessage( _format( d_left, d_selStart, d_selEnd ) );
					QToolTip::showText(me->globalPos(), HeTypeDefs::dateRangeToolTip( d_left.addDays( d_selStart ), d_left.addDays( d_selEnd ) ), getView() );
				}
				emit itemSelected( oid );
				getView()->viewport()->update();
				return true;
			}
		}
		break;
	case QEvent::MouseButtonDblClick:
		{
			QMouseEvent* me = static_cast<QMouseEvent*>( e );
			if( me->button() == Qt::LeftButton && d_picked )
			{
				emit doubleClicked();
				return true;
			}
		}
		break;
	case QEvent::MouseButtonRelease:
		{
			d_dragging = false; 
			getView()->viewport()->update();
			if( d_selStart > d_selEnd )
			{
				const int tmp = d_selEnd;
				d_selEnd = d_selStart;
				d_selStart = tmp;
			}
			QToolTip::hideText();
			return true;
		}
		break;
	case QEvent::MouseMove:
		// index ist immer von der Zeile unter der Maus, auch während drag. Darum currentIndex verwenden
		return handleMouseMove( static_cast<QMouseEvent*>( e ), option, index.data( ScheduleListMdl::MinRowHeight ).toInt() );
	case QEvent::ToolTip:
		break;
	}
	return false;
}

bool ScheduleBoardDeleg::eventFilter(QObject *watched, QEvent *e)
{
	bool res = QAbstractItemDelegate::eventFilter( watched, e );
	switch( e->type() )
	{
	case QEvent::Resize:
		{
			QResizeEvent* re = static_cast<QResizeEvent*>( e );
			if( re->size().width() != re->oldSize().width() )
				clearCache();
		}
		break;
	}
	return res;
}

static bool _canChangeDuration( quint32 t )
{
	switch( t )
	{
	case TypeEvent:
		return true;
	}
	return false;
}

bool ScheduleBoardDeleg::handleMouseMove( QMouseEvent* me, const QStyleOptionViewItem &option, int th )
{
	if( d_dragging ) 
	{
		// Ändere Selektion von freier Fläche
		d_selEnd = ( me->x() - option.rect.x() ) / d_dw;
		emit showStatusMessage( _format( d_left, d_selStart, d_selEnd ) );
		QToolTip::showText(me->globalPos(), HeTypeDefs::dateRangeToolTip( d_left.addDays( d_selStart ),
			d_left.addDays( d_selEnd ) ), getView() );
		getView()->viewport()->update();
		return true;
	}else if( me->buttons() == Qt::LeftButton && d_picked )
	{
		if( me->modifiers() == Qt::ShiftModifier )
		{
			// Länge mit der Maus verändern
			const int days = ( me->pos().x() - d_pos.x() ) / d_dw;
			if( days != 0 )
			{
				ScheduleItemObj e = d_txn->getObject( d_picked );
				if( _canChangeDuration( e.getType() ) && e.moveEnd( days ) )
				{
					d_txn->commit();
					d_rowCache[ qMakePair( d_itemCache[d_picked], d_picked ) ].end =
                            d_left.daysTo( e.getEnd() );
					emit showStatusMessage( _format( e ) );
					QToolTip::showText(me->globalPos(), HeTypeDefs::toolTip(e), getView() );
					getView()->viewport()->update();
				}
				d_pos = me->pos();
			}
		}else if( me->modifiers() == Qt::ControlModifier )
		{
			// Position mit der Maus verändern
			const int days = ( me->pos().x() - d_pos.x() ) / d_dw;
			const int tracks = ( me->pos().y() - d_pos.y() ) / th;
			if( days != 0 || tracks != 0 )
			{
				ScheduleItemObj e = d_txn->getObject( d_picked );
				if( e.moveStartEnd( days ) )
				{
					d_txn->commit();
					d_rowCache[ qMakePair( d_itemCache[d_picked], d_picked ) ] = 
                        Slot( d_left.daysTo( e.getStart() ), d_left.daysTo( e.getEnd()  ) );
					emit showStatusMessage( _format( e ) );
					QToolTip::showText(me->globalPos(), HeTypeDefs::toolTip(e), getView() );
					getView()->viewport()->update();
				}
				if( tracks != 0 )
				{
					ScheduleObj cal = e.getObject(
                            getView()->currentIndex().data( ScheduleListMdl::ScheduleOid ).toULongLong() );
					if( !cal.isNull() )
					{
						QPair<int,quint8> p = d_itemCache.value( d_picked );
						const int thread = tracks + int( p.second );
						if( thread != int( p.second ) && thread >= 0 && thread < cal.getRowCount() && 
							e.getParent().equals( cal ) )
						{
							// NOTE: wenn man über den unteren Rand fährt, ist cal plötzlich ein anderer.
							// index scheint immer derjenige unter der Maus zu sein
							d_rowCache.remove( qMakePair( p, d_picked ) );
							d_itemCache.remove( d_picked );
							cal.setItemThread( e, thread );
							d_txn->commit();
							p.second = thread;
							d_itemCache[ d_picked ] = p;
							d_rowCache[ qMakePair( p, d_picked ) ] = 
								Slot( d_left.daysTo( e.getStart() ), d_left.daysTo( e.getEnd() ) );
							getView()->viewport()->update();
						}
					}
				}
				d_pos = me->pos();
			}
		}
	}else
		QToolTip::hideText();
	return false;
}

void ScheduleBoardDeleg::drawGrid(QPainter& p, const QStyleOptionViewItem &option, const QModelIndex & index) const
{
	const QRect& r = option.rect; 
    const int th = index.data( ScheduleListMdl::MinRowHeight ).toInt();
	QDate cur = d_left;
	int x = r.x() -( cur.dayOfWeek() - 1 ) * d_dw;
	int col = ( x - r.x() ) / d_dw;
	cur = cur.addDays( -( cur.dayOfWeek() - 1 ) );
	int dnr = 0;
	int n = 1;
	const int w = option.fontMetrics.width( QLatin1String("00,") );
	if( d_dw < w )
		n = w / float(d_dw) + 1.0;
	ScheduleObj cal = d_txn->getObject( index.data( ScheduleListMdl::ScheduleOid ).toULongLong() );
	const int rowCount = cal.getRowCount();
	QColor clr = option.palette.color( QPalette::Normal, QPalette::AlternateBase );
	if( getView()->alternatingRowColors() && index.row() & 1 ) 
		clr = option.palette.color( QPalette::Normal, QPalette::Base );
	const QDate today = QDate::currentDate();
	while( x < r.width() )
	{

		if( dnr % n == 0 && ( dnr - 1 ) * n < s_dc )
		{
			if( cur.dayOfWeek() > 5 )
				p.fillRect( x, r.y(), n * d_dw, r.height(), d_weekend );
			// Tageslinie
			if( cur.dayOfWeek() >= 6 )
				p.setPen( Qt::white );
			else
				p.setPen( clr );
			p.drawLine( x, r.y(), x, r.y() + r.height() - 1 );
		}
		if( dnr == 0 )
		{
			// Wochenlinie
			p.setPen( Qt::lightGray );
			p.drawLine( x, r.y(), x, r.y() + r.height() - 1 );
		}
		if(  d_selRow >= 0  && ( col >= d_selStart && col <= d_selEnd ||
                                 col >= d_selEnd && col <= d_selStart ) )
		{
			if( col == d_selStart || col == d_selEnd )
				p.fillRect( x + 1, r.y(), d_dw - 1, rowCount * th - 1, 
					option.palette.color(QPalette::Normal, QPalette::Highlight).lighter( 300 ) );
			if( index == d_edit && d_selRow < rowCount )
				p.fillRect( x + 1, r.y() + d_selRow * th + 1, d_dw - 1, th - 3,
                            option.palette.brush(QPalette::Normal,QPalette::Highlight) );
		}
		if( today == cur )
		{
            // Linie für aktuellen Tag
			p.setPen( Qt::red );
			p.drawLine( x + d_dw / 2, r.y(), x + d_dw / 2, r.y() + r.height() - 1 );
		}
		x += d_dw;
		col++;
		cur = cur.addDays( 1 );
		dnr = ( dnr + 1 ) % s_dc;
	}
}

void ScheduleBoardDeleg::clearRow( int row ) const
{
	RowCache::iterator i = d_rowCache.lowerBound( qMakePair( qMakePair( row, quint8(0) ), quint32(0) ) );
	while( i != d_rowCache.end() && i.key().first.first == row )
	{
		d_passed.remove( i.key().second );
		d_itemCache.remove( i.key().second );
		i = d_rowCache.erase( i );
	}
}

void ScheduleBoardDeleg::drawBars(QPainter& p, const QStyleOptionViewItem &option, const QModelIndex & index) const
{
	const QRect& r = option.rect; 
	const int th = index.data( ScheduleListMdl::MinRowHeight ).toInt();

	RowCache::const_iterator i = d_rowCache.lowerBound(
                                     qMakePair( qMakePair( index.row(), quint8(0) ), quint32(0) ) );
	if( i == d_rowCache.end() || i.key().first.first != index.row() )
	{
		refillRow( index.row() );
		i = d_rowCache.lowerBound( qMakePair( qMakePair( index.row(), quint8(0) ), quint32(0) ) );
	}
	ScheduleObj cal = d_txn->getObject( index.data( ScheduleListMdl::ScheduleOid ).toULongLong() );
	int thread = 0;
	const int rowCount = cal.getRowCount();
	while( i != d_rowCache.end() && i.key().first.first == index.row() )
	{
		ScheduleItemObj e = cal.getObject( i.key().second );
        if( !e.isNull() )
		{
            // Zeichne das Item als Bar
			if( i.key().first.second > thread )
				thread = i.key().first.second;
			if( i.key().first.second < rowCount )
			{
				const bool passed = d_filter.isEmpty() || d_passed.contains( e.getOid() );
				QRect bar( r.x() + d_dw * i.value().start, r.y() + i.key().first.second * th,
					( i.value().end - i.value().start + 1 ) * d_dw, th );
				bar.adjust( 0, 1, 1, -2 );
                QColor barPenClr;
                QColor barBrushClr;
                QColor fontClr;
				if( i.key().second == d_picked )
				{
                    // Selektierte Darstellung
                    barPenClr = option.palette.color(QPalette::Normal,QPalette::Highlight);
                    barBrushClr = option.palette.color(QPalette::Normal,QPalette::Highlight);
					fontClr = option.palette.color(QPalette::Normal,QPalette::HighlightedText);
				}else
				{
                    // Normale Darstellung
					barPenClr = QColor::fromRgb( 170, 119, 68 ); // braun
                    switch( e.getType() )
                    {
                    case TypeAppointment:
                    case TypeDeadline:
                        barPenClr = Qt::darkYellow;
                        // barPenClr = QColor::fromRgb( 0, 128, 102 );
                        break;
                    }
                    if( !e.getParent().equals( cal ) )
						// barPenClr = QColor::fromRgb( 175, 158, 63 ); // gelb, für Aliasse
                        barPenClr = barPenClr.lighter( 150 );
					if( !passed )
						barPenClr = QColor::fromRgb( 120, 120, 120 );
                    barBrushClr = barPenClr.lighter( 190 );
                    fontClr = Qt::black;
				}
                p.setPen( barPenClr );
                p.setBrush( barBrushClr );
                switch( e.getType() )
                {
                case TypeAppointment:
                    {
                        QRect r = bar.adjusted( 1, 0, -2, -1 );
                        if( r.width() < 1 )
                            r.setWidth( 1 );
                        p.drawEllipse( r );
                    }
                    break;
                case TypeDeadline:
                    if( bar.width() > 5 )
                    {
                        QPolygon triangle;
                        QRect r = bar.adjusted( 1, 0, -2, -1 );
                        triangle.append( r.topLeft() );
                        triangle.append( r.topRight() );
                        triangle.append( r.bottomLeft() + QPoint( r.width() / 2, 0 ) );
                        p.drawPolygon( triangle );
                    }else
                        p.drawRect( bar.adjusted( 0, 0, -3, -1 ) );
                    break;
                default:
                    p.drawRect( bar.adjusted( 0, 0, -1, -1 ) );
                    break;
                }
                if( e.getValue( AttrStatus ).getUInt8() == Stat_Canceled )
                {
                    p.setPen( Qt::red );
                    p.drawLine(bar.topLeft(),bar.bottomRight());
                    p.drawLine(bar.topRight(), bar.bottomLeft());
                }

                p.setPen( fontClr );
				if( r.x() > bar.left() )
					bar.adjust( r.x() - bar.left(), 0, 0, 0 ); // Sorge dafür, dass Beschriftung nicht links verschwindet.
                if( !HeTypeDefs::isCalendarItem( e.getType() ) )
                    p.drawText( bar.adjusted( 2, 0, -2, -1 ), Qt::AlignVCenter | Qt::AlignLeft,
                        option.fontMetrics.elidedText( e.getString(AttrText), Qt::ElideRight, bar.width() - 4 ) );
			}
		}
		++i;
	}
	if( thread >= rowCount )
	{
		const int w = th * 0.7;
		QStyleOption opt;
		opt.init( getView() );
		opt.rect = QRect( option.rect.left() + 1, option.rect.bottom() - w + 2, w, w ); // PE_IndicatorSpinPlus
		getView()->style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, &p );
		opt.rect.moveLeft( option.rect.width() - w );
		getView()->style()->drawPrimitive(QStyle::PE_IndicatorArrowDown, &opt, &p );
	}
	if( index == d_edit && getView()->hasFocus() ) // option.state & QStyle::State_Selected )
	{
		QStyleOptionFocusRect o;
		o.init( getView() );
		o.rect = option.rect.adjusted( 1, 0, 0, 0 );
		o.state |= QStyle::State_KeyboardFocusChange;
		getView()->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, &p );
	}
}

void ScheduleBoardDeleg::setDate( const QDate& d, bool refill ) 
{ 
	const int diff = d_left.daysTo( d );
	if( diff == 0 )
		return;
	d_left = d; 
	if( d_selRow >= 0 )
	{
		d_selStart -= diff;
		d_selEnd -= diff;
	}
	if( true ) // TODO: funktioniert noch nicht refill )
		clearCache();
	//refill();
}

QPair<QDate,QDate> ScheduleBoardDeleg::getRange() const
{
	QPair<QDate,QDate> res;
    if( d_selStart >= 0 )
        res.first = d_left.addDays( d_selStart );
    if( d_selEnd >= 0 )
        res.second = d_left.addDays( d_selEnd );
    return res;
}

void ScheduleBoardDeleg::setRange(const QDate &start, const QDate &end)
{
    if( start.isValid() )
        d_selStart = d_left.daysTo( start );
    else
        d_selStart = -1;
    if( end.isValid() )
        d_selEnd = d_left.daysTo( end );
    else
        d_selEnd = d_selStart;
    d_selRow = 0;
    getView()->viewport()->update();
    if( d_picked != 0 )
        emit itemSelected( 0 );
    d_picked = 3;
}

Udb::OID ScheduleBoardDeleg::getSelCal() const
{
	if( d_edit.isValid() )
		return d_edit.data( ScheduleListMdl::ScheduleOid ).toULongLong();
	else
		return 0;
}

void ScheduleBoardDeleg::refill() const
{
	clearCache();
	// Gehe durch alle Zeilen des Modells
	for( int row = 0; row < getView()->model()->rowCount(); row++ )
		refillRow( row );
}

void ScheduleBoardDeleg::refillRow( int row ) const
{
	const QDate right = d_left.addDays( getView()->childrenRect().width() / d_dw );
	QModelIndex index = getView()->model()->index( row, 0 );
	ScheduleObj cal = d_txn->getObject(
                          index.data( ScheduleListMdl::ScheduleOid ).toULongLong() );
	d_calCache[ cal.getOid() ] = row;
	ScheduleObj::ItemList l = cal.findItems( d_left, right );
	bool found = false;
	for( int i = 0; i < l.size(); i++ )
	{
		ScheduleItemObj e = cal.getObject( l[i].first );
        if( !e.isNull() && ( d_showCalendarItems || !HeTypeDefs::isCalendarItem( e.getType() ) ) )
		{
			d_itemCache[ e.getOid() ] = qMakePair( row, quint8(l[i].second) );
			d_rowCache[ qMakePair( qMakePair( row, quint8(l[i].second) ), quint32(e.getOid()) ) ] = 
				Slot( d_left.daysTo( e.getStart() ), d_left.daysTo( e.getEnd() ) );
			found = true;
			for( int j = 0; j < d_filter.size(); j++ )
			{
				if( cal.equals( d_filter[j] ) )
					d_passed.insert( e.getOid() );
			}
		}
	}
	if( !found )
		d_rowCache[ qMakePair( qMakePair( row, quint8(0) ), quint32(0) ) ] = Slot( 0, 0 );
}

quint32 ScheduleBoardDeleg::pickBar( const QPoint& p, const QRect& r, const QModelIndex &index )
{
	const int th = index.data( ScheduleListMdl::MinRowHeight ).toInt();
	RowCache::const_iterator i = d_rowCache.lowerBound( qMakePair( qMakePair( index.row(), quint8(0) ), quint32(0) ) );
	while( i != d_rowCache.end() && i.key().first.first == index.row() )
	{
		if( i.key().second != 0 )
		{
			QRect bar( r.x() + d_dw * i.value().start, r.y() + i.key().first.second * th,
				( i.value().end - i.value().start + 1 ) * d_dw, th );
			bar.adjust( 0, 1, 0, -2 );
			if( bar.contains( p ) )
				return i.key().second;
		}
		++i;
	}
	return 0;
}

void ScheduleBoardDeleg::onDatabaseUpdate( Udb::UpdateInfo info )
{
	switch( info.d_kind )
	{
	case Udb::UpdateInfo::Deaggregated:
		{
			ItemCache::iterator i = d_itemCache.find( info.d_id );
			if( i != d_itemCache.end() )
			{
				d_rowCache.remove( qMakePair( i.value(), info.d_id ) );
				d_itemCache.erase( i );
				getView()->viewport()->update();
			}
		}
		break;
	case Udb::UpdateInfo::Aggregated:
		{
			CalCache::const_iterator ci = d_calCache.find( info.d_parent );
			if( ci != d_calCache.end() )
			{
				clearRow( *ci );
				getView()->viewport()->update();
			}
		}
		break;
	case Udb::UpdateInfo::MapChanged:
		{
			// Wird eigentlich nur getriggert, wenn Thread ändert.

		}
		break;
	}
}

void ScheduleBoardDeleg::onModelReset ()
{
	clearCache();
	//refill();
}

void ScheduleBoardDeleg::onRowsAboutToBeRemoved ( const QModelIndex & parent, int start, int end )
{
	//assert( !parent.isValid() );
	clearCache();

}

void ScheduleBoardDeleg::clearCache() const
{
	d_itemCache.clear();
	d_rowCache.clear();
	d_calCache.clear();
	d_passed.clear();
}

void ScheduleBoardDeleg::clearSelection()
{
	d_picked = 0;
	d_selRow = -1;
	getView()->viewport()->update();
}

void ScheduleBoardDeleg::setSelItem( Udb::OID id, bool notify )
{
	d_picked = id;
	d_selRow = -1;
	getView()->viewport()->update();
	if( notify )
		emit itemSelected( id );
}

void ScheduleBoardDeleg::setFilter( const QList<Udb::Obj>& f )
{
	d_filter = f;
	clearCache();
    getView()->viewport()->update();
}

QSize ScheduleBoardDeleg::sizeHint(const QStyleOptionViewItem &, const QModelIndex & i) const
{
    return i.data(Qt::SizeHintRole).toSize();
}
