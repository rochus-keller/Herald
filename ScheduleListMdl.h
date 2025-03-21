#ifndef __Mp_CalendarListMdl__
#define __Mp_CalendarListMdl__

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

#include <QAbstractItemModel>
#include <Udb/Obj.h>

namespace He
{
	class ScheduleListMdl : public QAbstractItemModel
	{
		Q_OBJECT
	public: 
		enum Role { 
			ThreadCountRole = Qt::UserRole + 1,
			ScheduleOid,
			ElementOid,
            MinRowHeight
		};
		ScheduleListMdl( QObject*, Udb::Transaction* );

		void setList( const Udb::Obj& );
		const Udb::Obj& getList() const { return d_list; }
		void refill();
		Udb::Obj getElem( const QModelIndex & ) const;
		QModelIndex getIndex( const Udb::Obj& elem ) const;
		void sortListByTypeName();
		void sortListByName();
        int getMinRowHeight() const { return d_minRowHeight; }
        void setMinRowHeight( int h );

		// Overrides
		int columnCount( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
        QVariant headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent(const QModelIndex &) const;
		Qt::ItemFlags flags(const QModelIndex &index) const;
		//bool canFetchMore ( const QModelIndex & parent ) const;
		//void fetchMore ( const QModelIndex & parent );
		bool setData ( const QModelIndex & index, const QVariant & value, int role = Qt::EditRole );
		QStringList mimeTypes() const;
		QMimeData* mimeData(const QModelIndexList &indexes) const;
	protected slots:
		void onDatabaseUpdate( Udb::UpdateInfo );
	protected:
		int findElementRow( Udb::OID ) const;
	private:
		Udb::Obj d_list;
		typedef QList<Udb::Obj> Rows; // Elemente, die auf Schedules zeigen.
		Rows d_rows; 
        QList<Udb::OID> d_scheds; // redundant, die Schedules
        int d_minRowHeight;
	};
}

#endif
