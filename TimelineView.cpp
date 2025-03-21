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

#include "TimelineView.h"
#include <QPainter>
#include <QMouseEvent>
#include <QApplication>
#include <GuiTools/AutoMenu.h>
using namespace He;

// 1:1 aus MasterPlan

static const char* s_dpat = "00,";
static const char* s_wpat = "CW000";
static const int s_dc = 7;

TimelineView::TimelineView( QWidget* p ):QFrame(p),d_dragging(false)
{
	d_dw = fontMetrics().width( QLatin1String(s_dpat) );
	d_left = QDate::currentDate();
	setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Fixed );
	setCursor( Qt::OpenHandCursor );
    setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
	d_weekend = p->palette().color( QPalette::Normal, QPalette::AlternateBase );
}

static QDate _workWeek( const QDate& cur )
{
	switch( cur.dayOfWeek() )
	{
	case 5:
		return cur.addDays( 3 );
	case 6:
		return cur.addDays( 2 );
	default:
		return cur.addDays( 1 );
	}
}

void TimelineView::paintEvent( QPaintEvent * e )
{
	QFrame::paintEvent( e );
	QPainter p( this );
	const int h = fontMetrics().height();
	const QRect cr = contentsRect();

	p.setClipRect( cr );
	p.fillRect( cr.x(), cr.y(), cr.width(), h * 3, palette().color( QPalette::Normal, QPalette::Base ) );
	p.fillRect( cr.x(), cr.y() + h, cr.width(), h, palette().color( QPalette::Normal, QPalette::AlternateBase ) );

	int x;
	QString str;
	QDate cur;

	// Zeichne Tag
	// Zeichne Woche
	const int w = fontMetrics().width( QLatin1String(s_dpat) );
	cur = d_left;
	x = cr.x() -( cur.dayOfWeek() - 1 ) * d_dw;
	cur = cur.addDays( -( cur.dayOfWeek() - 1 ) );
	int dnr = 0;
	int n = 1;
	if( d_dw < w )
		n = w / float(d_dw) + 1.0;
	const QColor ab = palette().color( QPalette::Normal, QPalette::AlternateBase );
	while( x < cr.width() )
	{
		if( dnr == 0 )
		{
			str = QString( "W%1" ).arg( cur.weekNumber(), 2, 10, QChar( '0' ) );
			int flags = Qt::AlignVCenter;
			if( fontMetrics().width( str ) > s_dc * d_dw )
				flags |= Qt::AlignRight;
			else
				flags |= Qt::AlignCenter;
			p.drawText( x, cr.y() + h, s_dc * d_dw, cr.y() + h, flags, str );
			p.drawLine( x, cr.y() + h, x, cr.y() + 2 * h - 1 );
		}
		if( dnr % n == 0 && ( dnr - 1 ) * n < s_dc )
		{
			if( cur.dayOfWeek() > 5 )
				p.fillRect( x, cr.y() + 2 * h, n * d_dw, h, d_weekend );
			p.drawLine( x, cr.y() + 2 * h, x, cr.y() + 3 * h - 1 );
			str = cur.toString( "dd" );
			p.drawText( x, cr.y() + 2 * h, n * d_dw, h, Qt::AlignVCenter | Qt::AlignCenter, str );
		}
		x += d_dw;
		cur = cur.addDays( 1 );
		dnr = ( dnr + 1 ) % s_dc;
	}

	// Zeichne Monat
	cur = d_left;
	x = cr.x() -( cur.day() - 1 ) * d_dw;
	if( cur.day() == 1 )
		p.drawLine( cr.x(), cr.y(), cr.x(), cr.y() + h );
	while( x < cr.width() )
	{
		int mw = cur.daysInMonth() * d_dw;
		str = fontMetrics().elidedText( cur.toString( "MMMM yyyy" ), Qt::ElideMiddle, mw - 2 );
		p.drawText( x + 1, cr.y(), mw - 1, h, Qt::AlignVCenter | Qt::AlignCenter, str );
		x += mw;
		p.drawLine( x, cr.y(), x, cr.y() + h );
		cur = cur.addMonths( 1 );
	}

	p.drawLine( cr.x(), cr.y() + h, cr.width(), cr.y() + h ); // Month
	p.drawLine( cr.x(), cr.y() + 2 * h, cr.width(), cr.y() + 2 * h ); // Week
	// p.drawLine( cr.x(), 3 * h, cr.width(), 3 * h ); // Day
}

void TimelineView::mousePressEvent ( QMouseEvent * e )
{
	if( e->button() == Qt::LeftButton )
	{
		const int h = fontMetrics().height();
		if( e->y() < ( 3 * h ) )
		{
			d_pos = e->x();
			d_dragging = true;
			if( e->modifiers() == Qt::ControlModifier )
				QApplication::setOverrideCursor( Qt::SizeHorCursor );
			else
				QApplication::setOverrideCursor( Qt::ClosedHandCursor );
		}
	}
}

void TimelineView::mouseMoveEvent ( QMouseEvent * e )
{
	if( d_dragging )
	{
		if( e->modifiers() == Qt::ControlModifier )
		{
			const int w = fontMetrics().width( QLatin1String(s_dpat) );
			const int d = ( e->x() - d_pos );
			if( ::abs( d ) > 3 )
			{
				d_dw = qMax( d_dw + d / 3, w / s_dc );
				if( d_dw > w )
					d_dw = w;
				emit dayWidthChanged( true );
				d_pos = e->x();
			}
		}else
		{
			int n = ( d_pos - e->x() ) / d_dw;
			if( n != 0 )
			{
				if( e->modifiers() == Qt::ShiftModifier )
					n *= s_dc;
				d_left = d_left.addDays( n );
				emit dateChanged( true );
				d_pos = e->x();
			}
		}
		update();
	}
}

void TimelineView::setDate( QDate d, bool notify )
{
	d_left = d;
	update();
	if( notify )
		emit dateChanged( false );
}

void TimelineView::setEndDate( QDate d, bool notify )
{
	d = d.addDays( - contentsRect().width() / d_dw );
	setDate( d, notify );
}

void TimelineView::setDateAndWidth( QDate d, int days, bool notify )
{
	if( days < 1 )
		return;
	d_left = d;
	const int w = fontMetrics().width( QLatin1String(s_dpat) );
	d_dw = qMax( contentsRect().width() / int(days), w / s_dc );
	if( d_dw > w )
		d_dw = w;
	const int ist = contentsRect().width() / d_dw;
	if( false ) // Sollen wir in die Mitte zoomen? Nicht sinnvoll. ist > days )
	{
		const int off = ( ist - days ) / 2;
		d_left = d_left.addDays( -off );
	}

	update();
	if( notify )
	{
		emit dateChanged( false );
		emit dayWidthChanged( false );
	}
}

void TimelineView::mouseDoubleClickEvent ( QMouseEvent * e )
{
	if( e->button() == Qt::LeftButton )
	{
		const int h = fontMetrics().height();
		if( e->y() > 0 && e->y() < h )
		{
			monthResolution();
		}else if( e->y() > h && e->y() < 2 * h )
		{
			weekResolution();
		}else if( e->y() > 2 * h && e->y() < 3 * h )
		{
			dayResolution();
		}
	}
}

void TimelineView::dayResolution()
{
    ENABLED_IF( true );
	d_dw = fontMetrics().width( QLatin1String(s_dpat) );
	emit dayWidthChanged( false );
	update();
}

void TimelineView::weekResolution()
{
    ENABLED_IF( true );
	const int w = fontMetrics().width( QLatin1String(s_wpat) );
	d_dw = w / s_dc;
	emit dayWidthChanged( false );
	update();
}

void TimelineView::monthResolution()
{
    ENABLED_IF( true );
	const int w = fontMetrics().width( QLatin1String(s_dpat) );
	d_dw = w / s_dc;
	emit dayWidthChanged( false );
	update();
}

void TimelineView::mouseReleaseEvent ( QMouseEvent * e )
{
	if( d_dragging )
	{
		d_dragging = false;
		if( e->modifiers() == Qt::ControlModifier )
			emit dayWidthChanged( false );
		else
			emit dateChanged( false );
		QApplication::restoreOverrideCursor();
	}
}

QSize TimelineView::sizeHint () const
{
	const int h = fontMetrics().height();
	QSize s;
	s.setHeight( 3 * h + 2 * frameWidth() );
	return s;
}

void TimelineView::gotoToday()
{
    ENABLED_IF( true );
	gotoDate( QDate::currentDate() );
}

void TimelineView::gotoDate( QDate d )
{
    ENABLED_IF( true );
	d_left = d;
	const int days = contentsRect().width() / d_dw;
	d_left = d_left.addDays( -days / 10 ); // RISK
	update();

	emit dateChanged( false );
}

void TimelineView::nextPage()
{
    ENABLED_IF( true );
	const int days = contentsRect().width() / d_dw;
	d_left = d_left.addDays( days * 0.9 ); 
	update();

	emit dateChanged( false );
}

void TimelineView::prevPage()
{
    ENABLED_IF( true );
	const int days = contentsRect().width() / d_dw;
	d_left = d_left.addDays( -days * 0.9 ); 
	update();

	emit dateChanged( false );
}

void TimelineView::nextDay()
{
    ENABLED_IF( true );
	d_left = d_left.addDays( 1 );
	update();

	emit dateChanged( false );
}

void TimelineView::prevDay()
{
    ENABLED_IF( true );
	d_left = d_left.addDays( -1 );
	update();

	emit dateChanged( false );
}

void TimelineView::zoomIn()
{
    ENABLED_IF( true );
	const int w = fontMetrics().width( QLatin1String(s_dpat) );
	d_dw = qMin( d_dw + 1, w );
	update();
	emit dayWidthChanged( false );
}

void TimelineView::zoomOut()
{
    ENABLED_IF( true );
	const int w = fontMetrics().width( QLatin1String(s_dpat) );
	d_dw = qMax( d_dw - 1, w / s_dc );
	update();
	emit dayWidthChanged( false );
}
