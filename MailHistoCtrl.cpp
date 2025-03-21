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

#include "MailHistoCtrl.h"
#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>
#include <QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>
#include <QMessageBox>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/UpdateInfo.h>
#include "MailListDeleg.h"
#include "HeTypeDefs.h"
using namespace He;

MailHistoCtrl::MailHistoCtrl(QWidget *parent) :
    QObject(parent)
{
}

QWidget *MailHistoCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

class MailHistoMdl : public MailListMdl
{
public:
    MailHistoMdl(QTreeView* p):MailListMdl(p) {}

    QTreeView* getTree() const { return static_cast<QTreeView*>( QObject::parent() ); }
    void onDbUpdate( const Udb::UpdateInfo &info )
    {
        if( info.d_kind == Udb::UpdateInfo::ValueChanged && info.d_name == AttrSentOn )
        {
            refill();
            getTree()->scrollTo( index(0, 0 ) );
        }else
            MailListMdl::onDbUpdate( info );
    }

};

MailHistoCtrl *MailHistoCtrl::create(QWidget *parent, Udb::Transaction *txn)
{
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 0 );

    MailHistoCtrl* ctrl = new MailHistoCtrl( pane );

    ctrl->d_tree = new QTreeView( parent );
	ctrl->d_tree->setAlternatingRowColors( true );
	ctrl->d_tree->setRootIsDecorated( false );
	ctrl->d_tree->setAllColumnsShowFocus(true);
	ctrl->d_tree->setSelectionBehavior( QAbstractItemView::SelectRows );
	ctrl->d_tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	ctrl->d_tree->header()->hide();
    ctrl->d_tree->setItemDelegate( new MailListDeleg( ctrl->d_tree ) );
    vbox->addWidget( ctrl->d_tree );

    ctrl->d_mdl = new MailHistoMdl( ctrl->d_tree );
    ctrl->d_mdl->setIdx( Udb::Idx( txn, IndexDefs::IdxSentOn ) ); // RISK
    txn->getDb()->addObserver( ctrl->d_mdl, SLOT(onDbUpdate( Udb::UpdateInfo )), false );
    ctrl->d_tree->setModel( ctrl->d_mdl );

    connect( ctrl->d_tree->selectionModel(),
             SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             ctrl, SLOT(onSelectionChanged()) );
    connect( ctrl->d_tree, SIGNAL(clicked(QModelIndex)), ctrl, SLOT( onSelectionChanged() ) );

    return ctrl;
}

void MailHistoCtrl::addCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Copy"), this,SLOT(onCopy()), tr("CTRL+C"), true );
    pop->addCommand( tr("Delete..."), this,SLOT(onDelete()), tr("CTRL+D"), true );
    pop->addCommand( tr("Rebuild Index..."), this,SLOT(onRebuildIndex()) );
}

Udb::Obj MailHistoCtrl::getSelectedMail() const
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
    if( sr.size() == 1 )
        return d_mdl->getObject( sr.first() );
    // else
    return Udb::Obj();
}

void MailHistoCtrl::onSelectionChanged()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
    if( sr.size() == 1 )
        emit sigSelectionChanged();
}

void MailHistoCtrl::onSelect(const QList<Udb::Obj> & l)
{
    d_tree->selectionModel()->clearSelection();
    QItemSelection sel;
    foreach( Udb::Obj o, l )
    {
        QModelIndex i = d_mdl->getIndex( o.getOid() );
        if( i.isValid() )
            sel.select( i, i );
    }
    if( !sel.isEmpty() )
    {
        d_tree->selectionModel()->select( sel, QItemSelectionModel::Select );
        d_tree->setFocus();
        d_tree->scrollTo( sel.last().topLeft() );
    }
}

void MailHistoCtrl::onCopy()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
	ENABLED_IF( sr.size() > 0 );

    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    foreach( QModelIndex i, sr )
	{
		Udb::Obj o = d_mdl->getObject( i );
		Q_ASSERT( !o.isNull() );
        objs.append( o );
	}
    HeTypeDefs::writeObjectRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );

}

void MailHistoCtrl::onDelete()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
	ENABLED_IF( sr.size() == 1 );

	Udb::Obj o = d_mdl->getObject( sr.first() );

    const int res = QMessageBox::warning( getWidget(), tr("Delete Message - Herald"),
		tr("Do you really want to permanently delete the selected message? This cannot be undone." ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
	if( res == QMessageBox::No )
		return;

    o.erase();
    d_mdl->getTxn()->commit();
    // TODO nicht mehr benötigte Documents löschen?
}

void MailHistoCtrl::onRebuildIndex()
{
    ENABLED_IF( true );
    const int res = QMessageBox::warning( getWidget(), tr("Rebuild Index - Herald"),
		tr("Do you really want to rebuild the Sent On index? This can take a while." ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
	if( res == QMessageBox::No )
		return;
    Udb::Idx idx( d_mdl->getTxn(), IndexDefs::IdxSentOn );
    QApplication::setOverrideCursor( Qt::WaitCursor );
    idx.rebuildIndex();
    QApplication::restoreOverrideCursor();
    d_mdl->refill();
}
