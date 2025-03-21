#ifndef __He_ScheduleObj__
#define __He_ScheduleObj__
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


#include <Udb/Obj.h>
#include <Stream/TimeSlot.h>

namespace He
{
    class IcsDocument;

    struct ScheduleItemData
    {
        QDate d_start;
        QDate d_end;
        Stream::TimeSlot d_slot;
        QString d_location;
        QString d_summary;
        QString d_description;
        QString d_html;
        QByteArray d_uid;
        bool isAppointment() const { return d_slot.isValid(); }
        enum Reason { Other, Publish, Request, Cancel };
        Reason d_reason;
        ScheduleItemData():d_reason(Other){}
    };

	class ScheduleObj : public Udb::Obj
	{
	public:
		static const QUuid s_scheduleSets;
		static const QUuid s_schedules;

		ScheduleObj( const Udb::Obj& o ):Obj( o ) {}

		quint8 getRowCount() const;
		void setRowCount( quint8 );

		Udb::Obj createEvent( QDate start, QDate end, quint8 thread = 0 );
		Udb::Obj createAppointment( QDate date, const Stream::TimeSlot&, quint8 thread = 0 );
		Udb::Obj createDeadline( QDate date, const Stream::TimeSlot&, quint8 thread = 0 );
		void removeItem( Udb::Obj& o, bool erase = true );
        bool acquireItem( Udb::Obj& o, quint8 thread = 0 );
		void setItemThread( Udb::Obj& o, quint8 thread );
		int getItemThread( Udb::Obj& o ) const; // -1 falls nicht vorhanden
		void removeThreads();
		quint8 getMaxThread() const;

		// Bringe alle ScheduleItems in eine nicht-überdeckende Ordnung. Returns RowCount
		quint8 layoutItems(const QList<Udb::Obj>& addToThread = QList<Udb::Obj>() ); 

		typedef QList< QPair<Udb::OID,quint8> > ItemList; // Liste von CalendarItems und ThreadRow
		ItemList findItems( QDate start, QDate end ) const;
		ItemList findAllItems() const;
		QDate earliestStart() const;
		QDate latestEnd() const;

        Udb::Obj acceptScheduleItem( const ScheduleItemData& );

		static QList<Udb::Obj> getScheduleSets(Udb::Transaction*);
		static Udb::Obj createScheduleSet( Udb::Transaction*, const QString& name );
		static Udb::Obj createSchedule( Udb::Transaction*, const QString& name );
        static void setDefaultSchedule( Udb::Transaction*, const Udb::Obj& );
        static Udb::Obj getDefaultSchedule(Udb::Transaction*);
        static QList<ScheduleItemData> readEvents( const IcsDocument&, bool onlyFromCurrentYear );
        static Udb::Obj findScheduleFromAddress( const Udb::Obj& address );
	};
}

#endif
