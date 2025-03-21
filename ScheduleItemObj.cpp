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

#include "ScheduleItemObj.h"
#include "HeTypeDefs.h"
#include <Oln2/OutlineUdbMdl.h>
using namespace He;

// 1:1 von MasterPlan

bool ScheduleItemObj::intersects( QDate start, QDate end ) const
{
	const QDate a1 = getStart();
	const QDate b1 = getEnd();
	const QDate a2 = start;
	const QDate b2 = end;

	return a2 <= a1 && b2 >= a1 ||
		a2 <= b1 && b2 >= b1 ||
		a2 >= a1 && b2 <= b1;

}

QDate ScheduleItemObj::getStart() const
{
	return getValue( AttrStartDate ).getDate();
}

QDate ScheduleItemObj::getEnd() const
{
	Stream::DataCell v = getValue( AttrEndDate );
	if( v.isDate() )
		return v.getDate();
	else
		return getStart();
}

void ScheduleItemObj::setStart( QDate d )
{
	setValue( AttrStartDate, Stream::DataCell().setDate( d ) );
	setTimeStamp( AttrModifiedOn );
}

void ScheduleItemObj::setEnd( QDate d )
{
	setValue( AttrEndDate, Stream::DataCell().setDate( d ) );
	setTimeStamp( AttrModifiedOn );
}

bool ScheduleItemObj::moveStartEnd( int days )
{
	if( days == 0 )
		return false;
	Stream::DataCell v = getValue( AttrStartDate );
	if( v.isDate() )
	{
		v.setDate( v.getDate().addDays( days ) );
		setValue( AttrStartDate, v );
	}else
		return false;
	v = getValue( AttrEndDate );
	if( v.isDate() )
	{
		v.setDate( v.getDate().addDays( days ) );
		setValue( AttrEndDate, v );
	}
	setTimeStamp( AttrModifiedOn );
	return true;
}

bool ScheduleItemObj::moveEnd( int days )
{
	Stream::DataCell v = getValue( AttrStartDate );
	if( !v.isDate() )
		return false;
	QDate start = v.getDate();
	v = getValue( AttrEndDate );
	if( !v.isDate() )
		return false;
	QDate end = v.getDate();
	end = end.addDays( days );
	if( end < start )
		end = start;
	setEnd( end );
	setTimeStamp( AttrModifiedOn );
	return true;
}

QString ScheduleItemObj::getLocation() const
{
	return getValue( AttrLocation ).toString();
}

