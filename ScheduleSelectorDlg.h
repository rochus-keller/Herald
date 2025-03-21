#ifndef __He_ScheduleSelectorDlg__
#define __He_ScheduleSelectorDlg__

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

class QListWidget;
class QDialogButtonBox;

namespace He
{
	class ScheduleSelectorDlg : public QDialog
	{
		Q_OBJECT
	public:
		ScheduleSelectorDlg( QWidget*, Udb::Transaction* );

		typedef QList<Udb::Obj> Objs;
		Objs select(const Objs& = Objs(), bool many = true );
	protected slots:
		void onSelectionChanged();
	private:
		QListWidget* d_list;
		QDialogButtonBox* d_bb;
        Udb::Transaction* d_txn;
	};

	class ScheduleSetSelectorDlg : public QDialog
	{
		Q_OBJECT
	public:
		ScheduleSetSelectorDlg( QWidget*, Udb::Transaction* );
		typedef QList<Udb::Obj> Objs;
		Objs select();
	protected slots:
		void onSelectionChanged();
	private:
		QListWidget* d_list;
		QDialogButtonBox* d_bb;
        Udb::Transaction* d_txn;
	};
}

#endif
