#ifndef __He_TimelineView__
#define __He_TimelineView__

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

#include <QFrame>
#include <QDate>

namespace He
{
	/* Zeigt einen Ruler an bestehend aus den drei Zeilen
		Monat
		Kalenderwoche
		Tag
	*/
	class TimelineView : public QFrame
	{
		Q_OBJECT
	public:
		TimelineView( QWidget* p );

		virtual QSize sizeHint () const;
		int getDayWidth() const { return d_dw; }
		QDate getDate() const { return d_left; }
		void setDate( QDate, bool notify = false );
		void setEndDate( QDate, bool notify = false );
		void setDateAndWidth( QDate, int days, bool notify = false );
		void setWeekendColor( const QColor& c ) { d_weekend = c; }
		const QColor& getWeekendColor() const { return d_weekend; }
	public slots:
		void gotoToday();
		void gotoDate( QDate );
		void nextPage();
		void prevPage();
		void nextDay();
		void prevDay();
		void zoomIn();
		void zoomOut();
		void dayResolution();
		void weekResolution();
		void monthResolution();
	signals:
		void dayWidthChanged( bool intermediate );
		void dateChanged( bool intermediate );
	protected:
		void paintEvent( QPaintEvent * event );
		void mousePressEvent ( QMouseEvent * event );
		void mouseMoveEvent ( QMouseEvent * event );
		void mouseReleaseEvent ( QMouseEvent * event );
		void mouseDoubleClickEvent ( QMouseEvent * event );
	private:
		QDate d_left;
		int d_dw; // day width
		int d_pos; 
		QColor d_weekend;
		bool d_dragging;
	};
}

#endif
