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

#include "ScheduleObj.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include "ScheduleItemObj.h"
#include <Oln2/OutlineUdbMdl.h>
#include <Udb/Transaction.h>
#include <Udb/Idx.h>
#include "IcsDocument.h"
using namespace He;
using namespace Stream;

// 1:1 von MasterPlan

const QUuid ScheduleObj::s_scheduleSets = "{2BDB2A44-BC6D-435c-B099-46488CEEA850}";
const QUuid ScheduleObj::s_schedules = "{EE6BB022-3337-4fce-AC70-7EE395D70040}";

QList<Udb::Obj> ScheduleObj::getScheduleSets(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
	QList<Udb::Obj> res;
	Obj cl = txn->getOrCreateObject( s_scheduleSets );
	Obj l = cl.getFirstObj();
	if( l.isNull() )
	{
		l = cl.createAggregate( TypeScheduleSet );
		l.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
		l.setValue( AttrText, DataCell().setString( QLatin1String("<default>") ) );
		res.append( l );
		return res;
	}else do
	{
		if( l.getType() == TypeScheduleSet )
			res.append( l );
	}while( l.next() );
	return res;
}

Udb::Obj ScheduleObj::createScheduleSet( Udb::Transaction* txn, const QString& name )
{
    Q_ASSERT( txn != 0 );
	Obj cl = txn->getOrCreateObject( s_scheduleSets );
	Obj l = cl.createAggregate( TypeScheduleSet );
	l.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
	if( !name.isEmpty() )
		l.setValue( AttrText, DataCell().setString( name ) );
	return l;
}

Udb::Obj ScheduleObj::createEvent( QDate start, QDate end, quint8 thread )
{
    if( !start.isValid() )
        return Udb::Obj();
	checkNull();
    if( !end.isValid() )
        end = start;
	Udb::Obj o = createAggregate( TypeEvent );
	o.setValue( AttrStartDate, DataCell().setDate( start ) );
	o.setValue( AttrEndDate, DataCell().setDate( end ) );
	o.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );

	setItemThread( o, thread );
	return o;
}

Udb::Obj ScheduleObj::createAppointment( QDate date, const Stream::TimeSlot& s, quint8 thread )
{
    if( !date.isValid() || !s.isValid() )
        return Udb::Obj();
	checkNull();
	Udb::Obj o = createAggregate( TypeAppointment );
	o.setValue( AttrStartDate, DataCell().setDate( date ) );
	o.setValue( AttrTimeSlot, DataCell().setTimeSlot( s ) );
	o.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );

	setItemThread( o, thread );
	return o;
}

Udb::Obj ScheduleObj::createDeadline( QDate date, const Stream::TimeSlot& s, quint8 thread )
{
	checkNull();
	Udb::Obj o = createAggregate( TypeDeadline );
	o.setValue( AttrStartDate, DataCell().setDate( date ) );
	o.setValue( AttrTimeSlot, DataCell().setTimeSlot( Stream::TimeSlot( s.d_start ) ) );
	o.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );

	setItemThread( o, thread );
	return o;
}

void ScheduleObj::setItemThread( Udb::Obj& o, quint8 thread )
{
	Udb::Obj::KeyList k(1);
	k[0] = o;
	setCell( k, DataCell().setUInt8( thread ) );
}

int ScheduleObj::getItemThread( Udb::Obj& o ) const
{
	Udb::Obj::KeyList k(1);
	k[0] = o;
	DataCell v = getCell( k );
	if( v.getType() == DataCell::TypeUInt8 )
		return v.getUInt8();
	else
		return -1;
}

void ScheduleObj::removeItem( Udb::Obj& o, bool erase )
{
	Udb::Obj::KeyList k(1);
	k[0] = o;
	setCell( k, DataCell().setNull() );
	if( erase && o.getParent().getOid() == getOid() )
        o.erase();
}

bool ScheduleObj::acquireItem(Udb::Obj &o, quint8 thread )
{
    if( !HeTypeDefs::isValidAggregate( o.getType(), TypeSchedule ) )
        return false;
    ScheduleObj oldCal = o.getParent();
    oldCal.removeItem( o, false );
    o.aggregateTo( *this );
    setItemThread( o, thread );
    return true;
}

void ScheduleObj::removeThreads()
{
	Udb::Mit i = findCells( KeyList() );
	if( !i.isNull() ) do
	{
		Udb::Mit::KeyList k = i.getKey();
		if( !k.isEmpty() && k[0].isOid() )
			setCell( k, Stream::DataCell().setNull() );
	}while( i.nextKey() );
}

quint8 ScheduleObj::getMaxThread() const
{
	quint8 res = 0;
	Udb::Mit i = findCells( KeyList() );
	if( !i.isNull() ) do
	{
		Udb::Mit::KeyList k = i.getKey();
		if( k.size() == 1 && k[0].isOid() )
			res = qMax( res, i.getValue().getUInt8() );
	}while( i.nextKey() );
	return res;
}

ScheduleObj::ItemList ScheduleObj::findItems( QDate start, QDate end ) const
{
	ItemList res;
	// Da per Definition jedes Item im Calendar einen Eintrag als Cell hat, können wir einfach die Cells durchgehen
	Udb::Mit m = findCells( KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			ScheduleItemObj e = getObject( l.first().getOid() );
			if( e.intersects( start, end ) )
				res.append( qMakePair( e.getOid(), m.getValue().getUInt8() ) );
		}
	}while( m.nextKey() );
	return res;
}

ScheduleObj::ItemList ScheduleObj::findAllItems() const
{
	ItemList res;
	// Da per Definition jedes Item im Calendar einen Eintrag als Cell hat, können wir einfach die Cells durchgehen
	Udb::Mit m = findCells( KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			ScheduleItemObj e = getObject( l.first().getOid() );
			res.append( qMakePair( e.getOid(), m.getValue().getUInt8() ) );
		}
	}while( m.nextKey() );
	return res;
}

QDate ScheduleObj::earliestStart() const
{
	QDate res;
	Udb::Mit m = findCells( KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			ScheduleItemObj e = getObject( l.first().getOid() );
			QDate start = e.getStart();
			if( !res.isValid() ) 
				res = start;
			else if( start.isValid() && start < res )
				res = start;
		}
	}while( m.nextKey() );
	return res;
}

QDate ScheduleObj::latestEnd() const
{
	QDate res;
	Udb::Mit m = findCells( KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			ScheduleItemObj e = getObject( l.first().getOid() );
			QDate end = e.getEnd();
			if( !res.isValid() || end.isValid() && end > res )
				res = end;
		}
	}while( m.nextKey() );
	return res;
}

Udb::Obj ScheduleObj::createSchedule( Udb::Transaction * txn, const QString &name )
{
    Q_ASSERT( txn != 0 );
	Obj l = txn->getOrCreateObject( s_schedules );
	Obj s = l.createAggregate( TypeSchedule );
	s.setValue( AttrCreatedOn, DataCell().setDateTime( QDateTime::currentDateTime() ) );
	s.setValue( AttrText, DataCell().setString( name ) );
	//s.setValue( AttrItemPool, s ); // Items werden direkt in s gespeichert
	s.setValue( AttrRowCount, DataCell().setUInt8( 1 ) );
    return s;
}

void ScheduleObj::setDefaultSchedule(Udb::Transaction* txn, const Udb::Obj & s)
{
    Q_ASSERT( txn != 0 );
    Obj l = txn->getOrCreateObject( s_schedules );
    l.setValueAsObj( AttrDefaultSchedule, s );
}

Udb::Obj ScheduleObj::getDefaultSchedule(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    Obj l = txn->getOrCreateObject( s_schedules );
    return l.getValueAsObj( AttrDefaultSchedule );
}

static inline ScheduleItemData::Reason _toReason( int methodCode )
{
    switch( methodCode )
    {
    case IcsDocument::CANCEL:
        return ScheduleItemData::Cancel;
    case IcsDocument::PUBLISH:
        return ScheduleItemData::Publish;
    case IcsDocument::REQUEST:
        return ScheduleItemData::Request;
    default:
        return ScheduleItemData::Other;
    }
}

QList<ScheduleItemData> ScheduleObj::readEvents(const IcsDocument & ics, bool onlyFromCurrentYear)
{
    QList<ScheduleItemData> res;
    const int startYear = QDate::currentDate().year();
    foreach( IcsDocument::Component c, ics.getComps() )
    {
        if( c.d_type == IcsDocument::VEVENT )
        {
            ScheduleItemData item;
            item.d_reason = _toReason( ics.getMethod() );

            const IcsDocument::Property& subject = c.findProp( IcsDocument::SUMMARY );
            const IcsDocument::Property& start = c.findProp( IcsDocument::DTSTART );
            const IcsDocument::Property& end = c.findProp( IcsDocument::DTEND );
            const IcsDocument::Property& dur = c.findProp( IcsDocument::DURATION );
            const IcsDocument::Property& loc = c.findProp( IcsDocument::LOCATION );
            const IcsDocument::Property& uid = c.findProp( IcsDocument::UID );
            const IcsDocument::Property& method = c.findProp( IcsDocument::METHOD );
            const IcsDocument::Property& desc = c.findProp( IcsDocument::DESCRIPTION );
            // const IcsDocument::Property& org = c.findProp( IcsDocument::ORGANIZER );

            if( !subject.isValid() || !start.isValid() )
                continue;
            if( method.isValid() )
                item.d_reason = _toReason( method.d_value.toInt() );

            if( start.d_value.type() == QVariant::Date )
            {
                // Event
                QDate startDate = start.d_value.toDate();
                QDate endDate = startDate;
                if( !end.d_value.isNull() )
                    endDate = end.d_value.toDate().addDays( - 1 ); // In iCal hat ein Event mit Dauer 1 Tag ein Enddatum des Folgetags
                else if( !dur.d_value.isNull() )
                {
                    // dur enthält Sekunden als qint64
                    qint64 days = qMax( qint64(1), dur.d_value.toLongLong() / ( 60 * 60 * 24 ) );
                    endDate = endDate.addDays( days - 1 );
                }else
                    continue;
                if( endDate < startDate )
                {
                    QDate tmp = endDate;
                    endDate = startDate;
                    startDate = tmp;
                }
                if( onlyFromCurrentYear && startDate.year() < startYear && endDate.year() < startYear )
                    continue;
                item.d_start = startDate;
                item.d_end = endDate;
            }else if( start.d_value.type() == QVariant::DateTime )
            {
                // Appointment
                QDateTime startDt = start.d_value.toDateTime();
                QDateTime endDt = startDt;
                if( !end.d_value.isNull() )
                    endDt = end.d_value.toDateTime();
                else if( !dur.d_value.isNull() )
                    endDt = startDt.addSecs( dur.d_value.toLongLong() );
                else
                    continue;
                if( endDt < startDt )
                {
                    QDateTime tmp = endDt;
                    endDt = startDt;
                    startDt = tmp;
                }
                if( onlyFromCurrentYear && startDt.date().year() < startYear && endDt.date().year() < startYear )
                    continue;
                item.d_start = startDt.date();
                if( endDt.date() > startDt.date() && endDt.time() > QTime(0,0) )
                    item.d_end = endDt.date().addDays( -1 );
                else
                    item.d_slot = Stream::TimeSlot( startDt.time(), startDt.secsTo( endDt ) / 60 ); // Minuten
            }
            item.d_summary = subject.d_value.toString();
            if( loc.isValid() )
                item.d_location = loc.d_value.toString();
            if( uid.isValid() )
                item.d_uid = uid.d_value.toByteArray();
            if( desc.isValid() )
                item.d_description = desc.d_value.toByteArray();
            QTextStream out( &item.d_html );
            ics.writeHtml( out, c, false );
            res.append( item );
        }
    }
    return res;
}

Udb::Obj ScheduleObj::findScheduleFromAddress(const Udb::Obj &address)
{
    if( address.isNull() )
        return Udb::Obj();
    Udb::Idx idx( address.getTxn(), IndexDefs::IdxSchedOwner );
    if( idx.seek( address ) )
        return address.getObject( idx.getOid() );
    else
        return getDefaultSchedule( address.getTxn() );
}

Udb::Obj ScheduleObj::acceptScheduleItem(const ScheduleItemData & item)
{
    checkNull();
    Udb::Obj schedItem;
    Udb::Idx idx( getTxn(), IndexDefs::IdxMessageId );
    if( idx.seek( Stream::DataCell().setLatin1( item.d_uid ) ) ) do
    {
        Udb::Obj o = getObject( idx.getOid() );
        if( HeTypeDefs::isScheduleItem( o.getType() ) )
        {
            schedItem = o;
            break;
            // RISK: was ist, wenn es mehrere ScheduleItems mit derselben UID gibt?
        }
    }while( idx.nextKey() );

    if( item.d_reason == ScheduleItemData::Request )
    {
        if( schedItem.isNull() )
        {
            // Neu anlegen
            if( item.isAppointment() )
                schedItem = createAppointment( item.d_start, item.d_slot );
            else
                schedItem = createEvent( item.d_start, item.d_end );
            if( schedItem.isNull() )
                return Udb::Obj();
            schedItem.setString( AttrText, item.d_summary );
            schedItem.setString( AttrLocation, item.d_location );
            schedItem.setLatin1( AttrMessageId, item.d_uid );
        }else
        {
            // Bestehendes aktualisieren
            if( item.isAppointment() )
            {
                if( schedItem.getType() != TypeAppointment )
                {
                    schedItem.clearValue( AttrEndDate );
                    schedItem.setType( TypeAppointment );
                }
                if( item.d_slot.isValid() )
                    schedItem.setValue( AttrTimeSlot, Stream::DataCell().setTimeSlot( item.d_slot ) );
            }else
            {
                if( schedItem.getType() != TypeEvent )
                {
                    schedItem.clearValue( AttrTimeSlot );
                    schedItem.setType( TypeAppointment );
                }
                if( item.d_end.isValid() )
                    schedItem.setValue( AttrEndDate, Stream::DataCell().setDate( item.d_start ) );
            }
            if( item.d_start.isValid() )
                schedItem.setValue( AttrStartDate, Stream::DataCell().setDate( item.d_start ) );
            schedItem.setString( AttrText, item.d_summary );
            schedItem.setString( AttrLocation, item.d_location );
            schedItem.setTimeStamp( AttrModifiedOn );
            schedItem.clearValue(Stat_Planned); // Damit man einmal gecancelte mit REQUEST wiederbeleben kann
        }
        // TODO: item.d_description
    }else if( item.d_reason == ScheduleItemData::Cancel && !schedItem.isNull() )
    {
        schedItem.setValue( AttrStatus, Stream::DataCell().setUInt8( Stat_Canceled ) );
        schedItem.setTimeStamp( AttrModifiedOn );
    } // else
    return schedItem;
}

quint8 ScheduleObj::getRowCount() const
{
	return qMax( quint8(1), getValue( AttrRowCount ).getUInt8() );
}

void ScheduleObj::setRowCount( quint8 r )
{
	setValue( AttrRowCount, DataCell().setUInt8( r ) );
}

static inline bool _LessThan(const ScheduleItemObj &p1, const ScheduleItemObj &p2)
{
	return p1.getStart() < p2.getStart() || ( p2.getStart() == p1.getStart() && p1.getEnd() < p2.getEnd() ) ||
		( p2.getStart() == p1.getStart() && p1.getEnd() == p2.getEnd() && p1.getOid() < p2.getOid() );
	// NOTE: als drittes Kriterium nach OID sortieren, falls verschiedene OID gleiches Start UND End haben.
}

quint8 ScheduleObj::layoutItems( const QList<Udb::Obj>& addToThread )
{
	QList<ScheduleItemObj> items;
	ScheduleItemObj sub = getFirstObj();
	// Hole zuerst alle Objekte, deren Parent dieser Schedule ist
	if( !sub.isNull() ) do
	{
		if( HeTypeDefs::isScheduleItem( sub.getType() ) )
			items.append( sub );
	}while( sub.next() );
	// Hole dann alle Items aus den Threads. 
	Udb::Mit m = findCells( KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			Udb::Obj o = getObject( l.first().getOid() );
			if( !o.isNull() && HeTypeDefs::isScheduleItem( o.getType() ) )
				items.append( o );
		}
	}while( m.nextKey() );
	for( int k = 0; k < addToThread.size(); k++ )
	{
		if( HeTypeDefs::isScheduleItem( addToThread[k].getType() ) )
			items.append( addToThread[k] );
	}
	qSort( items.begin(), items.end(), _LessThan );
	// Die Liste darf Items mehrfach enthalten.
	QList<QDate> threads;
	int cur;
	int rows = 1;
	ScheduleItemObj last;
	for( int i = 0; i < items.size(); i++ )
	{
		if( !last.equals( items[i] ) )
		{
			cur = -1;
			for( int j = 0; j < threads.size(); j++ )
			{
				if( threads[j] < items[i].getStart() )
				{
					cur = j;
					break;
				}
			}
			if( cur == -1 )
			{
				threads.append( QDate() );
				rows = threads.size();
				cur = rows - 1;
			}
			threads[cur] = items[i].getEnd();
			setItemThread( items[i], cur );
			last = items[i];
		}
	}
	return rows;
}
