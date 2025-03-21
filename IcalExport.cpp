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

#include "IcalExport.h"
#include <QTextStream>
#include "ScheduleItemObj.h"
#include "ScheduleObj.h"
#include "HeTypeDefs.h"
using namespace He;

// 1:1 von MasterPlan

static const char* CRLF = "\r\n";

bool IcalExport::startCalendar( QTextStream& out )
{
	out << "BEGIN:VCALENDAR" << CRLF;
	out << "VERSION:2.0" << CRLF;
	out << "PRODID:MasterPlan" << CRLF;
	out << "CALSCALE:GREGORIAN" << CRLF;
	out << "METHOD:PUBLISH" << CRLF;
	return true;
}

void IcalExport::writeEncoded( const QString& str, QTextStream& out )
{
	static const int lineLen = 75;
	for( int i = 0; i < str.size(); i++ )
	{
		if( i != 0 && i % lineLen == 0 )
			out << CRLF << ' ';
		if( str[i] == QLatin1Char( '\\' ) )
			out << "\\\\";
		else if( str[i] == QLatin1Char( ';' ) )
			out << "\\;";
		else if( str[i] == QLatin1Char( ',' ) )
			out << "\\,";
		else if( str[i] == QLatin1Char( '\r' ) )
			out << " ";
		else if( str[i] == QLatin1Char( '\n' ) )
			out << "\\n";
		else
			out << str[i];
	}
}

bool IcalExport::exportScheduleItem( const QString& category, const Udb::Obj& o, QTextStream& out )
{
	ScheduleItemObj e = o;
	out << "BEGIN:VEVENT" << CRLF;
	const QDate start = e.getStart();
	if( e.getValue( AttrTimeSlot ).isTimeSlot() )
	{
		Stream::TimeSlot ts = e.getValue( AttrTimeSlot ).getTimeSlot(); // 19970714T133000
		out << "DTEND:" << start.toString( "yyyyMMdd" ) << "T" << ts.getEndTime().toString( "hhmmss" ) << CRLF;
		out << "DTSTART:" << start.toString( "yyyyMMdd" ) << "T" << 
			ts.getStartTime().toString( "hhmmss" ) << CRLF; 
	}else
	{
		out << "DTSTART;VALUE=DATE:" << start.toString( "yyyyMMdd" ) << CRLF; // 20100111
		out << "DURATION:P" << start.daysTo( e.getEnd() ) + 1 << "D" << CRLF;
		//out << "DTEND;VALUE=DATE:" << e.getEnd().toString( "yyyyMMdd" ) << CRLF; // 20100116
	}
	out << "SUMMARY:";
    writeEncoded( e.getString(AttrText), out );
	out << CRLF;
	out << "CATEGORIES:";
	writeEncoded( category, out );
	out << CRLF;
	out << "TRANSP:OPAQUE" << CRLF;
	out << "SEQUENCE:0" << CRLF;
	out << "UID:" << e.getUuid().toString() << CRLF; // {558500A3-3C04-4DA4-B322-8590AA9860E2}
	out << "DTSTAMP:" << e.getValue(AttrModifiedOn).getDateTime().toString( "yyyyMMddThhmmss" ) << CRLF; // 20100128T091505
	out << "END:VEVENT" << CRLF;
	if( !e.getValue( AttrTimeSlot ).isTimeSlot() )
	{
		Udb::Obj sub = e.getFirstObj();
		if( !sub.isNull() ) do
		{
			if( sub.getValue( AttrTimeSlot ).isTimeSlot() )
				exportScheduleItem( category + QChar(':') + e.getString(AttrText), sub, out );
		}while( sub.next() );
	}
	return true;
}

bool IcalExport::exportSchedule( const Udb::Obj& cal, QTextStream& out )
{
	if( cal.isNull() )
		return false;

	Udb::Mit m = cal.findCells( Udb::Obj::KeyList() );
	if( !m.isNull() ) do
	{
		Udb::Mit::KeyList l = m.getKey();
		if( !l.isEmpty() && l.first().isOid() )
		{
			Udb::Obj e = cal.getObject( l.first().getOid() );
			if( !e.isNull() )
				exportScheduleItem( cal.getValue( AttrText ).toString(), e, out );
		}
	}while( m.nextKey() );
	return true;
}

bool IcalExport::endCalendar( QTextStream& out )
{
	out << "END:VCALENDAR" << CRLF;
	return true;
}
