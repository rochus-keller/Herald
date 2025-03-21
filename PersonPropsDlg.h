#ifndef __HePersonPropsDlg__
#define __HePersonPropsDlg__

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
class QTextEdit;

namespace He
{
	class PersonPropsDlg : public QDialog
	{
	public:
		PersonPropsDlg( QWidget* );
		void loadFrom( Udb::Obj& );
		void saveTo( Udb::Obj& ) const;
		bool edit( Udb::Obj& );
        void setText( const QString& text );
	private:
		QLineEdit* d_text;
		QLineEdit* d_lastName;
		QLineEdit* d_firstName;
		QLineEdit* d_acaTitle;		
		QLineEdit* d_shortHand;		

		QLineEdit* d_orgName;		
		QLineEdit* d_department;	
		QLineEdit* d_jobTitle;

		QLineEdit* d_phoneDesk;		
		QLineEdit* d_phoneMobile;
		QLineEdit* d_office;	
		QTextEdit* d_addr;

		QLineEdit* d_phonePriv;		
		QLineEdit* d_mobilePriv;		
		QTextEdit* d_addrPriv;

		QLineEdit* d_manager;		
		QLineEdit* d_assistant;		
	};
}

#endif
