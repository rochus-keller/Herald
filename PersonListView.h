#ifndef __Wt_PersListView__
#define __Wt_PersListView__

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
#include <Oln2/OutlineMdl.h>
#include <Udb/Idx.h>
#include <Udb/Obj.h>

class QLineEdit;
class QTreeView;
class QPushButton;
class QListWidget;
class QLabel;
class QListWidgetItem;

namespace He
{
	class PersonListMdl;

	class PersonListView : public QWidget
        {
		Q_OBJECT
	public:
		PersonListView( QWidget*, Udb::Transaction *txn );
		QTreeView* getList() const { return d_list; }
		void showAll();
		quint64 getCurrent() const;
		QList<quint64> getSelected() const;
		void setMultiSelect( bool );
		QLineEdit* getSearch() const { return d_search; }
        Udb::Transaction* getTxn() const;
	signals:
		void objectSelected( quint64 );
	public slots:
		void onRebuildIndex();
        void onCreatePerson();
        void onEditPerson();
        void onCopy();
	protected slots:
		void onTabChanged(int);
		void onSearch();
		void onItemChanged();
	private:
		QLineEdit* d_search;
		QTreeView* d_list;
		PersonListMdl* d_mdl;
	};

	class PersonListMdl : public QAbstractItemModel
	{
	public:
		enum Role { OidRole = Qt::UserRole + 1, ToolTipRole };
		PersonListMdl(QObject*, Udb::Transaction*);
        Udb::Transaction* getTxn() const { return d_txn; }
		void refill();
		void seek( const QString& );
		Udb::Obj getObject( const QModelIndex& ) const;
		void addType( quint32 );
		void removeType( quint32 );
		const QList<quint32>& getTypes() const { return d_types; }
		// overrides
		int columnCount ( const QModelIndex & parent = QModelIndex() ) const { return 1; }
		QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
		QModelIndex index ( int row, int column, const QModelIndex & parent = QModelIndex() ) const;
		QModelIndex parent ( const QModelIndex & index ) const;
		int rowCount ( const QModelIndex & parent = QModelIndex() ) const;
		Qt::ItemFlags flags ( const QModelIndex & index ) const
			{ return Qt::ItemFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled ); }
	private:
		struct Slot
		{
			Udb::Obj d_obj;
			QList<Slot*> d_children;
			Slot* d_parent;
			Slot(Slot* p = 0):d_parent(p){ if( p ) p->d_children.append(this); }
            void clear() { foreach( Slot* s, d_children ) delete s; d_children.clear(); }
            ~Slot() { clear(); }
		};
		Slot d_root;
		QString d_pattern;
		QList<quint32> d_types;
        Udb::Transaction* d_txn;
	};

	class PersonSelectorDlg : public QDialog
	{
		Q_OBJECT
	public:
		PersonSelectorDlg( QWidget*, Udb::Transaction *txn );
		QList<quint64> select(bool current = false ); 
		quint64 selectOne();

	protected slots:
		void onSelect();
	private:
		PersonListView* d_list;
		QPushButton* d_ok;
	};
}

#endif
