#ifndef __He_ScheduleItemPropsDlg__
#define __He_ScheduleItemPropsDlg__

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

#include <QDialog>
#include <Udb/Obj.h>

class QLineEdit;
class QLabel;
class QDateTimeEdit;
class QSpinBox;
class QTimeEdit;
class QComboBox;

namespace He
{
	class ScheduleItemPropsDlg : public QDialog
	{
	public:
		ScheduleItemPropsDlg( QWidget* );
		bool edit( Udb::Obj& item );
		bool createEvent( QDate from, QDate to );
		bool createAppointment( QDate from, const Stream::TimeSlot& = Stream::TimeSlot() );
		bool createDeadline( QDate from, const Stream::TimeSlot& = Stream::TimeSlot() );
		bool saveTo( Udb::Obj& item );
		bool validate( quint32 type );
		void enable( quint32 type );
	protected:
		bool loadFrom( const Udb::Obj& item );
	private:
		QLineEdit* d_title;
		QLabel* d_pix;
		QLabel* d_class;
		QDateTimeEdit* d_start;
		QDateTimeEdit* d_end;
		QLineEdit* d_from;
		QLineEdit* d_to;
		QLineEdit* d_ident;
		QLineEdit* d_altIdent;
		QLineEdit* d_loc;
        QComboBox* d_status;
	};
}

#endif
