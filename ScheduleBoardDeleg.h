#ifndef __He_ScheduleBoardDeleg__
#define __He_ScheduleBoardDeleg__

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

#include <QAbstractItemDelegate>
#include <QDate>
#include <Udb/Obj.h>
#include <QMap>
#include <QSet>

class QTreeView;

namespace He
{
	class ScheduleBoardDeleg : public QAbstractItemDelegate
	{
		Q_OBJECT
	public:
		ScheduleBoardDeleg( QTreeView*, Udb::Transaction* );

		void setDate( const QDate& d, bool refill = true );
		void setDayWidth( int w ) { d_dw = w; }
		QTreeView* getView() const;
		bool hasRange() const { return d_selRow >= 0; }
		QPair<QDate,QDate> getRange() const;
        void setRange( const QDate& start, const QDate& end );
		int getRangeRow() const { return d_selRow; }
		Udb::OID getSelCal() const;
		Udb::OID getSelItem() const { return d_picked; }
		void setSelItem( Udb::OID, bool notify = false );
		void refill() const;
		void refillRow( int row ) const;
		void clearRow( int row ) const;
		void setWeekendColor( const QColor& c ) { d_weekend = c; }
		void clearSelection();
		void setFilter( const QList<Udb::Obj>& );
		const QList<Udb::Obj>& getFilter() const { return d_filter; }


		//* Overrides von QAbstractItemDelegate
		QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &) const;
		void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
		bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);
	signals:
		void showStatusMessage( QString );
		void itemSelected( quint64 );
		void doubleClicked();
	protected:
		void drawGrid(QPainter&, const QStyleOptionViewItem &option, const QModelIndex &) const;
		void drawBars(QPainter&, const QStyleOptionViewItem &option, const QModelIndex &) const;
		quint32 pickBar( const QPoint&, const QRect&, const QModelIndex &index );
		bool handleMouseMove( QMouseEvent*,const QStyleOptionViewItem &option, int th );
		bool eventFilter(QObject *watched, QEvent *event);
	protected slots:
		void onDatabaseUpdate( Udb::UpdateInfo );
		void onModelReset ();
		void onRowsAboutToBeRemoved ( const QModelIndex & parent, int start, int end );
		void clearCache() const;
	private:
		QDate d_left;
        Udb::Transaction* d_txn;
		int d_dw; // day width
		mutable QPersistentModelIndex d_edit;
		mutable int d_selStart, d_selEnd, d_selRow;
		typedef QMap<quint32,int> CalCache; // calendar -> row
		mutable CalCache d_calCache;
		typedef QMap<quint32,QPair<int,quint8> > ItemCache; // event -> row|thread
		mutable ItemCache d_itemCache;
		quint32 d_picked;
        struct Slot
        {
            Slot( qint16 s = 0, qint16 e = 0 ):start(s),end(e){}
            qint16 start;
            qint16 end;
        };
		typedef QMap<QPair<QPair<int,quint8>,quint32>,Slot> RowCache;
		mutable RowCache d_rowCache; // (row|thread)|event -> Slot
		QPoint d_pos;
		QColor d_weekend;
		QList<Udb::Obj> d_filter; // Liste von Objekten um ScheduleItems zu filtern
		mutable QSet<quint32> d_passed; // TODO: ev in den obigen Cache integrieren
		bool d_dragging;
        bool d_showCalendarItems;
	};
}

#endif
