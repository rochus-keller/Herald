#ifndef __Oln_ScheduleBoard__
#define __Oln_ScheduleBoard__

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

#include <QWidget>
#include <Udb/Obj.h>

class QTreeView;
class QScrollBar;
class QComboBox;
class QModelIndex;
class QDate;
class QSplitter;
class QHeaderView;

namespace He
{
	class ScheduleListMdl;
	class ScheduleBoardDeleg;
	class TimelineView;
	class CalendarPopup;

	class ScheduleBoard : public QWidget
	{
		Q_OBJECT
	public:
		ScheduleBoard( QWidget*, Udb::Transaction* );
		QTreeView* getList() const { return d_list; }
		TimelineView* getTimeline() const { return d_timeLine; }
		QTreeView* getBoard() const { return d_board; }
        QPair<QDate,QDate> getCursor() const;
        Udb::Obj getSelectedCalendar() const;
		void setFilter( const QList<Udb::Obj>& );
		bool gotoItem( quint64 );
        void gotoDate( const QDate& start, const QDate& end = QDate() );
		void addSchedules( const QList<quint64>& );
        void restoreSplitter();
	signals:
		void showStatusMessage( QString );
		void objectSelected( quint64 );
		void objectDblClicked( quint64 );
	public slots:
		void onCreateSchedule();
		void onRenameSchedule();
		void onAdjustScrollbar();
		void onCreateEvent();
		void onCreateDeadline();
		void onCreateAppointment();
		void onRemoveItem();
		void onEraseItem();
		void onZoom();
		void onExportAll();
		void onExportSel();
		void onSelectDate();
		void onRenameScheduleSet();
		void onCreateScheduleSet();
		void onDeleteScheduleSet();
		void onSaveScheduleSets();
		void onLoadScheduleSets();
		void onPasteItems();
		void onRemoveSchedules();
		void onSelectSchedules();
		void onSelectPersons();
		void onToDeadline();
		void onToEvent();
		void onToAppointment();
		void onShowFirstItem();
		void onShowLastItem();
		void onSortListByTypeName();
		void onSortListByName();
		void onAdjustSections();
		void onCollapseSections();
		void onLayoutSections();
		void onCopyLink();
        void onImportSched();
        void onDefaultSched();
        void onSetOwner();
	protected slots:
		void onSectionResized( int, int, int );
		void onDateChanged( bool );
		void onDayWidthChanged( bool );
		void onRowMoved(int,int,int);
		void onRowsInserted ( const QModelIndex & parent, int start, int end );
		void onLeftSel( const QDate & );
		void onSplitterMoved(int,int);
		void onSelectScheduleSet(int);
		void onStatusMessage( QString );
		void onSelected( quint64 );
		void onSelectSchedule();
		void onVsel(int);
		void onDoubleClicked();
	protected:
		void fillScheduleListMenu();
        void readItems(Stream::DataReader & in, Udb::Obj &parent );
		// override
		void changeEvent ( QEvent * e );
	private:
		ScheduleListMdl* d_mdl;
        Udb::Transaction* d_txn;
		QTreeView* d_list;
		QScrollBar* d_scrollBar;
		QSplitter* d_splitter;
		QTreeView* d_board;
		ScheduleBoardDeleg* d_boardDeleg;
		TimelineView* d_timeLine;
		CalendarPopup* d_startSelector;
		QComboBox* d_setSelector;
        QHeaderView* d_vh;
		bool d_block;
	};
}

#endif 
