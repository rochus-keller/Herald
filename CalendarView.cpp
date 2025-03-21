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

#include "CalendarView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QtDebug>
#include <QToolTip>
#include <QBitArray>
#include <QSet>
#include <QStyleOptionFocusRect>
#include "HeTypeDefs.h"
#include "HeraldApp.h"
#include "ScheduleItemObj.h"
#include "ScheduleObj.h"
#include "ScheduleItemPropsDlg.h"
#include <QInputDialog>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <GuiTools/AutoMenu.h>
#include <QSettings>
#include "ScheduleSelectorDlg.h"
#include <Udb/Database.h>
#include <cassert>
using namespace He;

// 1:1 von MasterPlan

const int s_fraction = 4; // RISK Auflösung Viertelstunde
const int s_minTw = 5;

CalendarView::CalendarView( QWidget* p, Udb::Transaction * txn ):QFrame(p),
	d_days( 10 ), d_aperture( QTime( 8,0 ), QTime( 18,0 ) ), d_state(Idle ), d_selDay(-1),
    d_filterExclusive( false ),d_txn( txn )
{
    Q_ASSERT( txn != 0 );
	setFocusPolicy( Qt::StrongFocus );
	setFrameStyle( QFrame::Panel | QFrame::StyledPanel );
	setLineWidth( 1 );
	QVariant v = HeraldApp::inst()->getSet()->value( "Calendar/NumOfDays" );
	if( !v.isNull() )
		setNumOfDays( v.toInt() );
	v = HeraldApp::inst()->getSet()->value( "Calendar/NumOfHours" );
	if( !v.isNull() )
		setNumOfHours( v.toInt() );
}

void CalendarView::setDate( QDate d, bool align )
{
	d_left = d;
    if( align )
        alignDate();
	d_selDay = -1;
	reloadContents(); 
	update();
}

int CalendarView::dayWidth() const
{
	return 1 + qMax( fontMetrics().width( "00.00" ), ( contentsRect().width() - rowHeaderWidth() ) / d_days.size() );
}

int CalendarView::rowCount() const
{
	return d_aperture.getDuration() / 60.0 + 0.5;
}

int CalendarView::slotHeight() const 
{
	const int rc = d_aperture.d_duration / 60.0 + 0.5;
	int sh = qMax( fontMetrics().height(), ( ( contentsRect().height() - dayHeaderHeight() - rc - 1 ) / rc ) ) / s_fraction + 0.5;
	if( sh == 0 )
		sh = 1;
	return sh;
}

int CalendarView::rowHeight() const
{

	return 1 + slotHeight() * s_fraction; 
}

bool CalendarView::grayOut(const Udb::Obj & o) const
{
    return !d_filterExclusive && !d_filter.isNull() &&
            !( d_filter.equals( o ) || d_filter.equals( o.getParent() ) );
}

void CalendarView::paintEvent( QPaintEvent * event )
{
	QFrame::paintEvent( event );

	QPainter p( this );

	const QRect cr = contentsRect();
	const int rhw = rowHeaderWidth(); // Row Header Width + Line
	const int dhh = dayHeaderHeight(); // Day Header Height + Line
	const int dw = dayWidth(); // day width
	const QTime st = d_aperture.getStartTime();
	const int rc = rowCount(); // row count
	const int sh = slotHeight();
	const int rh = 1 + sh * s_fraction; // row height

	p.fillRect( cr, Qt::white );
	p.setPen( Qt::black );

	// paint row header and background
	int r;
	for( r = 0; r < rc; r++ )
	{
		const int y = dhh + cr.top() + r * rh;
		if( r % 2 != 0 )
			p.fillRect( cr.left(), y, cr.width(), rh - 1, QColor::fromRgb( 241, 241, 241 ) );
		p.setPen( Qt::black );
		p.drawText( cr.left() + 1, y - 1, rhw - 1, rh - 2,
			Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
			st.addSecs( r * 60 * 60 ).toString( "hh:mm" ) );
	}

	// paint day header 
	int d;
	for( d = 0; d < d_days.size(); d++ )
	{
		const int x = rhw + cr.left() + d * dw;
		QDate cur = d_left.addDays( d );
		if( cur.dayOfWeek() == Qt::Saturday || cur.dayOfWeek() == Qt::Sunday )
			p.fillRect( x, cr.top(), dw, cr.height(), QColor::fromRgb( 204, 232, 187 ) );
		p.setPen( Qt::black );
		p.drawText( x + 1, cr.top(), dw - 1, fontMetrics().height(),
			Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine, cur.toString( "dddd" ) );
		p.drawText( x + 1, cr.top() + fontMetrics().height(), dw - 1, fontMetrics().height(),
			Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine, cur.toString( "dd.MM.yy" ) ); // Qt::SystemLocaleShortDate
		if( false ) // cur.dayOfWeek() == Qt::Saturday ) // sieht komisch aus
			p.setPen( Qt::white );
		else
			p.setPen( Qt::darkGray );
		p.drawLine( x + dw - 1, cr.top(), x + dw - 1, cr.bottom() );
		if( cur == QDate::currentDate() )
		{
			p.setPen( Qt::red );
			p.drawRect( x - 1, cr.top(), dw, cr.height() - 1 );
		}
	}

	if( !d_filter.isNull() )
	{
		QPixmap pix( ":/images/filtered.png" );
		const int x = ( rhw - pix.width() ) / 2;
		const int y = ( dhh - pix.height() ) / 2;
		p.drawPixmap( cr.left() + x, cr.top() + y, pix );
	}

	// paint selection
	if( d_selDay >= 0 && d_selDay < d_days.size() )
	{
		p.setClipRect( cr.adjusted( rhw, dhh, 0, 0 ) );
		const qint8 start = ( d_selStart <= d_selEnd ) ? d_selStart : d_selEnd;
		const qint8 end = ( ( d_selStart <= d_selEnd ) ? d_selEnd : d_selStart ) + 1;

		const int x = rhw + cr.left() + d_selDay * dw;
		const int y1 = slotToY( start ); 
		const int y2 = slotToY( end ); 
        QPalette::ColorGroup cg = (hasFocus())?QPalette::Active:QPalette::Inactive;
		p.fillRect( x, y1, dw - 1, y2 - y1, palette().brush(cg,QPalette::Highlight) );
		p.setClipping( false );
	}

	for( r = 0; r < rc; r++ )
	{
		const int y = dhh + cr.top() + r * rh;
		p.setPen( Qt::gray );
		p.drawLine( cr.left(), y + rh - 1, cr.right(), y + rh - 1 );
	}

	// paint bars
	p.setClipRect( cr.adjusted( rhw, dhh, 0, 0 ) );
	for( d = 0; d < d_days.size(); d++ )
	{
		int tw = ( dw - 1 ) / d_days[d].d_trackCount;
		if( tw < s_minTw )
		{
			// Sonderprogramm: Tracks werden rechts abgeschnitten; TODO: entsprechende Markierung
			tw = s_minTw;
		}
		int x = rhw + cr.left() + d * dw;
		for( int s = 0; s < d_days[d].d_slots.size(); s++ )
		{
			const int y1 = slotToY( d_days[d].d_slots[s].d_start );
			const int y2 = slotToY( d_days[d].d_slots[s].d_end ); 
			const int h = ( d_days[d].d_slots[s].d_end - d_days[d].d_slots[s].d_start ) * sh;
			QRect bar( x + d_days[d].d_slots[s].d_track * tw + 1, y1 - 1, tw - 2, y2 - y1 + 1 ); 
			if( d_days[d].d_slots[s].d_item == d_selItem )
			{
				p.fillRect( bar, palette().color(QPalette::Highlight) );
				p.setPen( palette().color(QPalette::HighlightedText) );
			}else
			{
				if( d_days[d].d_slots[s].d_item.getType() == TypeDeadline )
				{
					QColor clr = QColor::fromRgb( 220, 119, 217 ); // violett
                    if( grayOut( d_days[d].d_slots[s].d_item ) )
                        clr = QColor::fromRgb( 120, 120, 120 );
					p.fillRect( bar, clr );
					p.setPen( Qt::darkMagenta );
				}else
				{
					QColor clr = QColor::fromRgb( 170, 119, 68 ); // braun, nicht schlecht
                    if( grayOut( d_days[d].d_slots[s].d_item ) )
                        clr = QColor::fromRgb( 120, 120, 120 );
					p.fillRect( bar, clr.lighter( 190 ) ); // mit blue und braun
					p.setPen( clr );
				}
				p.drawRect( bar.adjusted( 0, 0, -1, -1 ) );
				p.setPen( Qt::black );
			}
			if( bar.width() > p.fontMetrics().width( "Titel" ) &&
                    ( bar.height() - 4 ) >= p.fontMetrics().height() )
				p.drawText( bar.adjusted( 2, 2, -2, -2 ), Qt::AlignTop | Qt::AlignLeft | 
					Qt::TextWordWrap, d_days[d].d_slots[s].d_item.getValue( AttrText ).toString() );
                    // | Qt::TextWrapAnywhere
            if( d_days[d].d_slots[s].d_item.getValue( AttrStatus ).getUInt8() == Stat_Canceled )
            {
                p.setPen( Qt::red );
                p.drawLine(bar.topLeft(),bar.bottomRight());
                p.drawLine(bar.topRight(), bar.bottomLeft());
            }
		}
	}
	p.setClipping( false );

	p.setPen( Qt::black );
	// Row Header Grenzlinie
	p.drawLine( cr.left() + rhw - 1, cr.top(), cr.left() + rhw - 1, cr.bottom() );
	// Column Header Grenzlinie
	p.drawLine( cr.left(), cr.top() + dhh - 1, cr.right(), cr.top() + dhh - 1 );

	if( hasFocus() )
	{
		// style()->drawPrimitive( QStyle::PE_FrameFocusRect, ...) funktioniert nicht
		p.setPen( QPen( palette().brush( QPalette::Highlight ), 2 ) );
		p.drawRoundedRect( rect().adjusted( 1, 1, -1, -1 ), 3, 3 );
	}
}

int CalendarView::dayHeaderHeight() const
{
	return fontMetrics().height() * 2 + 1;
}

int CalendarView::rowHeaderWidth() const
{
	return fontMetrics().width( "00:00" ) + 3;
}

QSize CalendarView::sizeHint() const
{
	const int rhw = rowHeaderWidth(); // Row Header Width + Line
	const int dhh = dayHeaderHeight(); // Day Header Height + Line
	const int w = 1 + fontMetrics().width( "01.02" ) * d_days.size(); // day width
	const int h = 1 + fontMetrics().height() * d_aperture.d_duration / 60; // row height

	return QSize( w + rhw + 2 * frameWidth(), h + dhh + 2 * frameWidth() );
}

int CalendarView::yToSlot( int y ) const
{
	const int dhh = dayHeaderHeight();
	const int top = contentsRect().top();
	if( y < ( dhh + top ) )
		return -1;
	const int sh = slotHeight();
	const int rh = sh * s_fraction + 1;
	y = y - dhh - top;
	const int row = y / rh;
	const int minPerSlot = 60 / s_fraction;
	return row * s_fraction + ( y - rh * row ) / sh + d_aperture.d_start / minPerSlot; 
}

int CalendarView::slotToY( int slot ) const
{
	// y bezeichnet die obere Kante, ab der der Slot gemalt wird, unter Berücksichtigung aller Trennlinien
	const int dhh = dayHeaderHeight();
	const int top = contentsRect().top();
	const int sh = slotHeight();
	const int minPerSlot = 60 / s_fraction;
	slot -= d_aperture.d_start / minPerSlot;
	return dhh + top + ( sh * slot ) + slot / s_fraction; // letzter Term ist Anz. Trennlinien
}

int CalendarView::yToMinute( int y ) const
{
	const int slot = yToSlot( y );
	if( slot < 0 )
		return -1;
	const int minPerSlot = 60 / s_fraction;
	return slot * minPerSlot;
}

int CalendarView::xToDay( int x ) const
{
	const int rhw = rowHeaderWidth();
	const int left = contentsRect().left();
	if( x < ( rhw + left ) )
		return -1;
	else
        return ( x - rhw - left ) / dayWidth();
}

void CalendarView::alignDate()
{
    // Korrigiere, damit immer Montag links, aber nur wenn dann noch sichtbar
    d_left = d_left.addDays( - qMin( d_left.dayOfWeek() - 1, getNumOfDays() - 1 ) );
}

void CalendarView::mousePressEvent ( QMouseEvent * e )
{
	if( e->button() == Qt::LeftButton )
	{
		const int dhh = dayHeaderHeight();
		const int rhw = rowHeaderWidth();
		if( e->y() < dhh && e->x() > rhw )
		{
			d_pos = e->x();
			d_state = DragDate;
			QApplication::setOverrideCursor( Qt::ClosedHandCursor );
		}else if( e->x() < rhw && e->y() > dhh )
		{
			d_pos = e->y();
			d_state = DragTime;
			QApplication::setOverrideCursor( Qt::ClosedHandCursor );
		}else if( e->x() > rhw && e->y() > dhh )
		{
			// Pick or Select
			d_selItem = pick( e->pos() );
			if( !d_selItem.isNull() )
			{
				d_selDay = -1;
				ScheduleItemObj o = d_selItem;
				QToolTip::showText(e->globalPos(), HeTypeDefs::toolTip(o), this );
				emit objectSelected( o.getOid() );
			}else
			{
				d_state = Select;
				d_selDay = xToDay( e->x() );
				d_selStart = yToSlot( e->y() );
				d_selEnd = d_selStart;
				QToolTip::showText(e->globalPos(), HeTypeDefs::timeSlotToolTip( d_left.addDays( d_selDay ), getSelection() ) );
				emit objectSelected( 0 );
			}
			update();
		}else
		{
			// Pick ist auf Filter-Symbol
			if( !d_filter.isNull() )
				showAll();
			else if( !d_selItem.isNull() )
			{
				Udb::Obj p = d_selItem.getParent();
				if( HeTypeDefs::isScheduleItem( p.getType() ) )
					show( p );
				else
					show( d_selItem );
			}
		}
	}
}

void CalendarView::mouseMoveEvent ( QMouseEvent * e )
{
	if( d_state == DragDate)
	{
		int n = ( d_pos - e->x() ) / dayWidth();
		if( n != 0 )
		{
			d_left = d_left.addDays( n );
			d_selDay = -1;
			emit dateChanged( true );
			d_pos = e->x();
			reloadContents(); 
		}
		update();
	}else if( d_state == DragTime )
	{
		int n = ( d_pos - e->y() ) / rowHeight();
		if( n != 0 )
		{
			const int maxTime = 60 * 23; // 23:00
			int start = d_aperture.d_start;
			start += n * 60;
			if( start < 0 )
				start = 0;
			else if( start > maxTime )
				start = maxTime;
			d_aperture.d_start = start;
			d_pos = e->y();
		}
		update();
	}else if( d_state == Select )
	{
		d_selEnd = yToSlot( e->y() );
		QToolTip::showText(e->globalPos(), HeTypeDefs::timeSlotToolTip( d_left.addDays( d_selDay ), getSelection() ) );
		update();
	}
}

Stream::TimeSlot CalendarView::getSelection() const
{
	if( d_selDay > -1 )
	{
		const int minPerSlot = 60 / s_fraction;
		qint16 start = ( ( d_selStart <= d_selEnd ) ? d_selStart : d_selEnd ) * minPerSlot;
		qint16 end = ( ( d_selStart <= d_selEnd ) ? d_selEnd : d_selStart ) * minPerSlot;
		if( start < 0 )
			start = 0;
		if( end >= 1440 )
			end = 1440 - 1;
		return Stream::TimeSlot( start, end - start + 60 / s_fraction );
	}else if( !d_selItem.isNull() )
		return d_selItem.getValue( AttrTimeSlot ).getTimeSlot();
	else
        return Stream::TimeSlot();
}

void CalendarView::setSelection(const QDate& d, const Stream::TimeSlot & s)
{
    if( d.isValid() && s.isValid() )
    {
        d_selDay = d_left.daysTo( d );
        const int minPerSlot = 60 / s_fraction;
        d_selStart = s.d_start / minPerSlot;
        d_selEnd = ( s.d_start + s.d_duration - 60 / s_fraction ) / minPerSlot;
        if( !d_selItem.isNull() )
            emit objectSelected(0);
        d_selItem = Udb::Obj();
        update();
    }
}

void CalendarView::mouseReleaseEvent ( QMouseEvent * e )
{
	if( d_state == DragDate )
	{
		d_state = Idle;
		emit dateChanged( false );
		QApplication::restoreOverrideCursor();
	}else if( d_state == DragTime )
	{
		d_state = Idle;
		QApplication::restoreOverrideCursor();
	}else 
	{
		d_state = Idle;
		QToolTip::hideText();
	}
}

void CalendarView::setNumOfDays( quint8 d )
{
	if( d < 1 )
		d_days.resize( 1 ); // RISK
	else if( d > 28 ) // RISK
		d_days.resize( 28 );
	else
		d_days.resize( d );
	update();
	HeraldApp::inst()->getSet()->setValue( "Calendar/NumOfDays", d );
}

void CalendarView::show( const Udb::Obj& filter, QDate d, bool align )
{
    if( filter.isNull() )
    {
        showAll( d, align );
        return;
    }
    if( !d.isNull() )
    {
		d_left = d;
        if( align )
            alignDate();
    }
	d_filter = filter;
	reloadContents();
}

void CalendarView::showAll( QDate d, bool align )
{
    if( !d.isNull() )
    {
		d_left = d;
        if( align )
            alignDate();
    }
    d_filter = Udb::Obj();
	for( int i = 0; i < d_days.size(); i++ )
	{
		QDate cur = d_left.addDays( i );
		Udb::Idx idx( d_txn, d_txn->getDb()->findIndex(
                          IndexDefs::IdxStartDate ) );
		SlotObjs objs;
		if( idx.seek( QList<Stream::DataCell>() << Stream::DataCell().setDate( cur ) ) ) do
		{
			ScheduleItemObj o = d_txn->getObject( idx.getOid() );
            // übernehme alle Objekte, welche AttrTimeSlot haben.
			if( !o.isNull() && o.getValue( AttrTimeSlot ).isTimeSlot() )
				objs.append( o );
		}while( idx.nextKey() );
		layoutObjs( objs, i );
	}
	update();
}

static inline bool _LessThan(const Udb::Obj &p1, const Udb::Obj  &p2)
{
	Stream::TimeSlot a = p1.getValue( AttrTimeSlot ).getTimeSlot();
	Stream::TimeSlot b = p2.getValue( AttrTimeSlot ).getTimeSlot();
	return a < b;
}

void CalendarView::layoutObjs( SlotObjs objs, quint8 day )
{
	assert( day < d_days.size() );
	d_days[day].d_slots.clear();
	d_days[day].d_trackCount = 1;

	qSort( objs.begin(), objs.end(), _LessThan );
	// TODO: ineffizienter erster Wurf
	QList<QBitArray> tracks;
	const int slotCount = 24 * s_fraction; // Jede Stunde des Tages
	const int minPerSlot = 60 / s_fraction;
	tracks.append( QBitArray( slotCount ) );
	for( int i = 0; i < objs.size(); i++ )
	{
		Slot s;
		s.d_item = objs[i];
		const Stream::TimeSlot ts = objs[i].getValue( AttrTimeSlot ).getTimeSlot();
		s.d_start = ts.d_start / minPerSlot;
		s.d_end = ( ts.d_start + ts.getDuration() ) / minPerSlot;
		if( s.d_end == s.d_start )
			s.d_end = s.d_start + 1; // minimale Höhe ist ein Slot
		bool placed = false;
		for( int t = 0; t < tracks.size(); t++ )
		{
			bool occupied = false;
			for( int d = s.d_start; d < s.d_end; d++ )
			{
				if( tracks[t].testBit( d ) )
				{
					occupied = true;
					break;
				}
			}
			if( !occupied )
			{
				tracks[t].fill( true, s.d_start, s.d_end );
				s.d_track = t;
				d_days[day].d_slots.append( s );
				if( d_days[day].d_trackCount <= t ) 
					d_days[day].d_trackCount = t + 1;
				placed = true;
				break;
			}
		}
		if( !placed )
		{
			tracks.append( QBitArray( slotCount ) );
			tracks.last().fill( true, s.d_start, s.d_end );
			s.d_track = tracks.size() - 1;
			d_days[day].d_slots.append( s );
			if( d_days[day].d_trackCount <= s.d_track ) 
				d_days[day].d_trackCount = s.d_track + 1;
		}
	}
}

Udb::Obj CalendarView::pick( const QPoint& pos ) const
{
	// TODO: ev. effizienter
	const int day = xToDay( pos.x() );
	assert( day != -1 );
	const int hitSlot = yToSlot( pos.y() );
	assert( hitSlot != -1 );
	const int dw = dayWidth(); 
	const int tw = qMax( s_minTw, ( dw - 1 ) / d_days[day].d_trackCount );
	const int rhw = rowHeaderWidth();
	const int x = rhw + contentsRect().left() + day * dw;
	const int t = ( pos.x() - x ) / tw;
	for( int i = 0; i < d_days[day].d_slots.size(); i++ )
	{
		if( hitSlot >= d_days[day].d_slots[i].d_start && 
			hitSlot < d_days[day].d_slots[i].d_end && 
			t == d_days[day].d_slots[i].d_track )
			return d_days[day].d_slots[i].d_item;
	}
	return Udb::Obj();
}

void CalendarView::clearSelection()
{
	d_selItem = Udb::Obj();
	update();
}

void CalendarView::setSelectedItem( const Udb::Obj& o )
{
	d_selItem = o;
	update();
}

void CalendarView::mouseDoubleClickEvent ( QMouseEvent * e )
{
	if( e->button() == Qt::LeftButton )
	{
		if( !d_selItem.isNull() )
			emit objectDoubleClicked( d_selItem.getOid() );
	}
}

void CalendarView::reloadContents()
{
	QSet<quint64> items;
	int i;
    if( !d_filter.isNull() )
	{
		if( d_filter.getValue( AttrTimeSlot ).isTimeSlot() )
		{
			// Das Aggregat ist gleich selber das Kalenderobjekt
			items.insert( d_filter.getOid() );
		}else
		{
			Udb::Obj sub = d_filter.getFirstObj();
			if( !sub.isNull() ) do
			{
				if( sub.getValue( AttrTimeSlot ).isTimeSlot() )
					items.insert( sub.getOid() );
			}while( sub.next() );
		}
	}
	for( i = 0; i < d_days.size(); i++ )
	{
		QDate cur = d_left.addDays( i );
		Udb::Idx idx( d_txn, d_txn->getDb()->findIndex( IndexDefs::IdxStartDate ) );
		SlotObjs objs;
		if( idx.seek( QList<Stream::DataCell>() << Stream::DataCell().setDate( cur ) ) ) do
		{
            // Alle Objekte mit einem AttrTimeSlot werden im Kalender angezeigt.
			ScheduleItemObj o = d_txn->getObject( idx.getOid() );
			if( !o.isNull() && o.getValue( AttrTimeSlot ).isTimeSlot() &&
                    ( !d_filterExclusive || items.contains( o.getOid() ) ) )
				objs.append( o );
		}while( idx.nextKey() );
		layoutObjs( objs, i );
	}
	update();
}

void CalendarView::onSetNumOfDays()
{
	ENABLED_IF( true );

	bool ok;
    const int n = QInputDialog::getInt( this, tr("Number of Days shown"),
		tr("Number 1..28:"), d_days.size(), 1, 28, 1, &ok );
	if( !ok )
		return;
	setNumOfDays( n );
}

void CalendarView::onSetNumOfHours()
{
	ENABLED_IF( true );

	bool ok;
    const int n = QInputDialog::getInt( this, tr("Number of Hours shown"),
		tr("Number 1..24:"), d_aperture.d_duration / 60, 1, 24, 1, &ok );
	if( !ok )
		return;
	setNumOfHours( n );
}

void CalendarView::setNumOfHours(quint8 h)
{
	if( h < 1 )
		h = 1;
	if( h > 24 )
		h = 24;
	d_aperture.d_duration = h * 60;
	update();
	HeraldApp::inst()->getSet()->setValue( "Calendar/NumOfHours", h );
}

void CalendarView::onCreateAppointment()
{
    // TODO: dieser Code ist fast identisch mit ScheduleBoard::onCreateAppointment
    ENABLED_IF( !d_filter.isNull() && HeTypeDefs::isValidAggregate( TypeAppointment, d_filter.getType() ) &&
		getSelection().isValid() );

	ScheduleItemPropsDlg dlg( this ); 
	if( dlg.createAppointment( getSelectedDay(), getSelection() ) )
	{
        ScheduleItemObj o = ScheduleObj( d_filter ).createAppointment( getSelectedDay(), Stream::TimeSlot(0), 0 );
		if( dlg.saveTo( o ) )
		{
			o.commit();
			d_selDay = -1;
			d_selItem = o;
			emit objectSelected( o.getOid() );
			reloadContents();
		}else
			d_txn->rollback();
	}
}

void CalendarView::onCreateDeadline()
{
    // TODO: dieser Code ist fast identisch mit ScheduleBoard::onCreateDeadline
    ENABLED_IF( !d_filter.isNull() && HeTypeDefs::isValidAggregate( TypeDeadline, d_filter.getType() ) &&
		getSelection().isValid() );

	ScheduleItemPropsDlg dlg( this ); 
	if( dlg.createDeadline( getSelectedDay(), getSelection() ) )
	{
        ScheduleItemObj o = ScheduleObj( d_filter ).createDeadline( getSelectedDay(), Stream::TimeSlot(), 0 );
		if( dlg.saveTo( o ) )
		{
			o.commit();
			d_selDay = -1;
			d_selItem = o;
			emit objectSelected( o.getOid() );
			reloadContents();
		}else
			d_txn->rollback();
	}
}

QDate CalendarView::getSelectedDay() const
{
	if( d_selDay > -1 )
		return d_left.addDays( d_selDay );
	else if( !d_selItem.isNull() )
		return d_selItem.getValue( AttrStartDate ).getDate();
	else
		return QDate();
}

void CalendarView::onDeleteItem()
{
	ScheduleItemObj e = getSelItem();
	ENABLED_IF( !e.isNull() );
	e.erase();
	emit objectSelected( 0 );
	d_txn->commit();
    reloadContents();
}

void CalendarView::onCopyLink()
{
     ENABLED_IF( !d_selItem.isNull() );
     QMimeData* mimeData = new QMimeData();
     HeTypeDefs::writeObjectRefs( mimeData, QList<Udb::Obj>() << d_selItem );
     QApplication::clipboard()->setMimeData( mimeData );
}

void CalendarView::onMoveToCalendar()
{
    ScheduleItemObj e = getSelItem();
    ENABLED_IF( !e.isNull() && HeTypeDefs::isValidAggregate( e.getType(), TypeSchedule ) );
    ScheduleSelectorDlg dlg( this, d_txn );
    QList<Udb::Obj> cals = dlg.select( QList<Udb::Obj>(), false );
    if( !cals.isEmpty() && !cals.first().equals( e.getParent() ) )
    {
        ScheduleObj newCal = cals.first();
        newCal.acquireItem( e );
        e.commit();
    }
}
