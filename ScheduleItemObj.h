#ifndef __He_ScheduleItemObj__
#define __He_ScheduleItemObj__

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

namespace He
{
	class ScheduleItemObj : public Udb::Obj
	{
	public:
		ScheduleItemObj(){}
		ScheduleItemObj( const Udb::Obj& o ):Obj( o ) {}

		bool intersects( QDate start, QDate end ) const;
		QDate getStart() const;
		QDate getEnd() const;
		void setStart( QDate );
		void setEnd( QDate );
		bool moveStartEnd( int days );
		bool moveEnd( int days );
		QString getLocation() const;
	};
}

#endif
