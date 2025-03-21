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

#include "SearchView.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QApplication>
#include <QShortcut>
#include <QLabel>
#include <QSettings>
#include <QResizeEvent>
#include <QMimeData>
#include <QHeaderView>
#include <GuiTools/UiFunction.h>
#include <Oln2/OutlineUdbMdl.h>
#include "FullTextIndexer.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
using namespace He;

static const int s_objectCol = 0;
static const int s_dateCol = 1;
static const int s_scoreCol = 2;

struct _SearchViewItem : public QTreeWidgetItem
{
	_SearchViewItem( QTreeWidget* w ):QTreeWidgetItem( w ) {}

	bool operator<(const QTreeWidgetItem &other) const
	{
		const int col = treeWidget()->sortColumn();
		switch( col )
		{
		case s_scoreCol:
			return text(s_scoreCol) < other.text(s_scoreCol);
		case s_objectCol:
			return text(s_objectCol) < other.text(s_objectCol);
		case s_dateCol:
			return text(s_dateCol) + text(s_scoreCol) < other.text(s_dateCol) + other.text(s_scoreCol );
		}
		return text(col) < other.text(col);
	}
	QVariant data(int column, int role) const
	{
		if( role == Qt::ToolTipRole )
			role = Qt::DisplayRole;
		return QTreeWidgetItem::data( column, role );
	}
};

SearchView::SearchView(Udb::Transaction* txn, QWidget * p):QWidget( p )
{
	setWindowTitle( tr("Herald Search") );

	d_idx = new FullTextIndexer( txn, this );

	QVBoxLayout* vbox = new QVBoxLayout( this );
	vbox->setMargin( 0 );
	vbox->setSpacing( 1 );
	QHBoxLayout* hbox = new QHBoxLayout();
	hbox->setMargin( 1 );
	hbox->setSpacing( 1 );
	vbox->addLayout( hbox );

	hbox->addWidget( new QLabel( tr("Enter query:"), this ) );

	d_query = new QLineEdit( this );
	connect( d_query, SIGNAL( returnPressed() ), this, SLOT( onSearch() ) );
	hbox->addWidget( d_query );

	QPushButton* doit = new QPushButton( tr("&Search"), this );
	doit->setDefault(true);
	connect( doit, SIGNAL( clicked() ), this, SLOT( onSearch() ) );
	hbox->addWidget( doit );

	d_result = new QTreeWidget( this );
	d_result->setHeaderLabels( QStringList() << tr("Object") << tr("Sent") << tr("Score") ); // s_item, s_doc, s_score
	d_result->header()->setStretchLastSection( false );
	d_result->setAllColumnsShowFocus( true );
	d_result->setRootIsDecorated( false );
	d_result->setSortingEnabled(true);
	QPalette pal = d_result->palette();
	pal.setColor( QPalette::AlternateBase, QColor::fromRgb( 245, 245, 245 ) );
	d_result->setPalette( pal );
	d_result->setAlternatingRowColors( true );
	connect( d_result, SIGNAL( itemActivated ( QTreeWidgetItem *, int ) ), this, SLOT( onGotoImp() ) );
	connect( d_result, SIGNAL( itemDoubleClicked ( QTreeWidgetItem *, int ) ), this, SLOT( onGotoImp() ) );
	vbox->addWidget( d_result );
}

static QString _valuta( const Udb::Obj& o )
{
	Stream::DataCell v = o.getValue( AttrSentOn );
    if( !v.isDateTime() && !o.getParent().isNull() )
		v = o.getParent().getValue( AttrSentOn );
	if( v.isDateTime() )
		return v.getDateTime().toLocalTime().toString( "yyyyMMdd-hhmm " );
	else
		return QString();
}

void SearchView::onSearch()
{
	FullTextIndexer::ResultList res;
	if( !d_idx->exists() )
	{
		if( QMessageBox::question( this, tr("Herald Search"), 
			tr("The index does not yet exist. Do you want to build it? This will take some minutes." ),
			QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Cancel )
			return;
		if( !d_idx->indexDatabase( this ) )
		{
			if( !d_idx->getError().isEmpty() )
				QMessageBox::critical( this, tr("Herald Indexer"), d_idx->getError() );
			return;
		}
	}else if( d_idx->hasPendingUpdates() )
	{
		// Immer aktualisieren ohne zu fragen
		if( true ) /*QMessageBox::question( this, tr("Herald Search"), 
			tr("The index is not up-do-date. Do you want to update it?" ),
			QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Ok )*/
		{
			if( !d_idx->indexIncrements( this ) )
			{
				if( !d_idx->getError().isEmpty() )
					QMessageBox::critical( this, tr("Herald Indexer"), d_idx->getError() );
				return;
			}
		}
	}
	QApplication::setOverrideCursor( Qt::WaitCursor );
	if( !d_idx->query( d_query->text(), res ) )
	{
		QApplication::restoreOverrideCursor();
		QMessageBox::critical( this, tr("Herald Search"), d_idx->getError() );
		return;
	}
	d_result->clear();
	for( int i = 0; i < res.size(); i++ )
	{
		QTreeWidgetItem* item = new _SearchViewItem( d_result );
		const QString text = HeTypeDefs::formatObjectTitle( res[i].d_object );
		item->setText( s_objectCol, text );
		item->setData( s_objectCol, Qt::UserRole, res[i].d_object.getOid() );
		item->setText( s_dateCol, _valuta( res[i].d_object ) );
		item->setIcon( s_objectCol, Oln::OutlineUdbMdl::getPixmap( res[i].d_object.getType() ) );
		item->setText( s_scoreCol, QString::number( res[i].d_score, 'f', 1 ) );
	}
    d_result->header()->setSectionResizeMode( s_objectCol, QHeaderView::Stretch );
    d_result->header()->setSectionResizeMode( s_dateCol, QHeaderView::ResizeToContents );
    d_result->header()->setSectionResizeMode( s_scoreCol, QHeaderView::ResizeToContents );
	d_result->resizeColumnToContents( s_scoreCol );
    d_result->resizeColumnToContents( s_dateCol );
	d_result->sortByColumn( s_dateCol, Qt::DescendingOrder );
    d_result->scrollToItem( d_result->topLevelItem( 0 ) );
	QApplication::restoreOverrideCursor();
}

void SearchView::onNew()
{
	d_query->selectAll();
	d_query->setFocus();
}

void SearchView::onGotoImp()
{
	QTreeWidgetItem* cur = d_result->currentItem();
	if( cur == 0 )
		return;

	emit signalShowItem( d_idx->getTxn()->getObject( cur->data( s_objectCol,Qt::UserRole).toULongLong() ) );
}

void SearchView::onGoto()
{
	ENABLED_IF( d_result->currentItem() );
	onGotoImp();
}

void SearchView::onCopyRef()
{
	ENABLED_IF( d_result->currentItem() );

    QMimeData* mimeData = new QMimeData();
    QList<Udb::Obj> objs;
    objs.append( d_idx->getTxn()->
                 getObject( d_result->currentItem()->data( s_objectCol,Qt::UserRole).toULongLong() ) );
    HeTypeDefs::writeObjectRefs( mimeData, objs );
}

Udb::Obj SearchView::getItem() const
{
	QTreeWidgetItem* cur = d_result->currentItem();
	if( cur == 0 )
        return Udb::Obj();
	else
		return d_idx->getTxn()->getObject( cur->data( s_objectCol,Qt::UserRole).toULongLong() );
}

void SearchView::onRebuildIndex()
{
	ENABLED_IF( true );

	if( !d_idx->indexDatabase( this ) )
	{
		if( !d_idx->getError().isEmpty() )
			QMessageBox::critical( this, tr("Herald Indexer"), d_idx->getError() );
		return;
	}
}

void SearchView::onUpdateIndex()
{
	ENABLED_IF( d_idx->exists() && d_idx->hasPendingUpdates() );

	if( !d_idx->indexIncrements( this ) )
	{
		if( !d_idx->getError().isEmpty() )
			QMessageBox::critical( this, tr("Herald Indexer"), d_idx->getError() );
	}
}

void SearchView::onClearSearch()
{
	ENABLED_IF( d_result->topLevelItemCount() > 0 );

	d_result->clear();
	d_query->clear();
	d_query->setFocus();
}
