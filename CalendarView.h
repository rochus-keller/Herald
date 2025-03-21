#ifndef __He_CalendarView__
#define __He_CalendarView__

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
#include <Stream/TimeSlot.h>
#include <Udb/Obj.h>
#include <QVector>

namespace He
{
	class CalendarView : public QFrame
	{
		Q_OBJECT
	public:
		CalendarView(QWidget*, Udb::Transaction*);
		void setDate( QDate, bool align = false );
		QDate getDate() const { return d_left; }
		void setNumOfDays( quint8 ); // 1..28
		int getNumOfDays() const { return d_days.size(); }
		const Stream::TimeSlot& getAperture() const { return d_aperture; }
		void setNumOfHours(quint8); // 1..24
		Stream::TimeSlot getSelection() const;
        void setSelection( const QDate&, const Stream::TimeSlot& );
		QDate getSelectedDay() const;
		const Udb::Obj& getSelItem() const { return d_selItem; }
		void show( const Udb::Obj& filter, QDate = QDate(), bool align = true );
        void showAll( QDate d = QDate(), bool align = true );
		void clearSelection(); 
		void setSelectedItem( const Udb::Obj& );
		const Udb::Obj& getFilter() const { return d_filter; }
	public slots:
		void onSetNumOfDays();
		void onSetNumOfHours();
		void onCreateAppointment();
		void onCreateDeadline();
		void onDeleteItem();
        void onCopyLink();
        void onMoveToCalendar();
	signals:
		void dateChanged( bool intermediate );
		void objectSelected( quint64 );
		void objectDoubleClicked( quint64 );
	protected:
		void paintEvent ( QPaintEvent * event );
		QSize sizeHint() const;
		void mousePressEvent ( QMouseEvent * event );
		void mouseMoveEvent ( QMouseEvent * event );
		void mouseReleaseEvent ( QMouseEvent * event );
		void mouseDoubleClickEvent ( QMouseEvent * event );
	protected:
		int dayHeaderHeight() const;
		int rowHeaderWidth() const;
		int dayWidth() const;
		int rowHeight() const;
		int slotHeight() const;
		int rowCount() const;
		int yToSlot( int y ) const;
		int slotToY( int slot ) const;
		int yToMinute( int y ) const;
		int xToDay( int x ) const;
        void alignDate();
        bool grayOut( const Udb::Obj& ) const;
		typedef QList<Udb::Obj> SlotObjs;
		void layoutObjs( SlotObjs, quint8 day );
		Udb::Obj pick( const QPoint& pos ) const;
		void reloadContents();
	private:
        Udb::Transaction* d_txn;
		Stream::TimeSlot d_aperture;
		QDate d_left;
		struct Slot
		{
			Udb::Obj d_item;
			quint8 d_start; // Slot-Nummer
			quint8 d_end; // Slot-Nummer
			quint8 d_track; // Vertikale Bahn nach Layout
			Slot():d_start(0),d_end(0),d_track(0) {}
		};
		typedef QList<Slot> Slots;
		struct Day
		{
			Slots d_slots;
			int d_trackCount;
			Day():d_trackCount(1){}
		};
		typedef QVector<Day> Days;
		Days d_days;
		int d_pos; 
		enum State { Idle, DragDate, DragTime, Select };
		quint8 d_state;
		qint8 d_selDay;
		qint8 d_selStart; // Slot-Nummer
		qint8 d_selEnd; // Slot-Nummer
		Udb::Obj d_selItem;
		Udb::Obj d_filter;
        bool d_filterExclusive;
	};
}

#endif
