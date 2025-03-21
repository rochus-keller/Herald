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

#include "ScheduleSelectorDlg.h"
#include "ScheduleObj.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include <Udb/Transaction.h>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QSet>
using namespace He;

// 1:1 aus MasterPlan

ScheduleSelectorDlg::ScheduleSelectorDlg( QWidget* p, Udb::Transaction * txn ):QDialog(p),d_txn(txn)
{
    Q_ASSERT( txn != 0 );
	setWindowTitle( tr("Select Schedules") );
	QVBoxLayout* vbox = new QVBoxLayout( this );
	vbox->addWidget( new QLabel( tr("Select one or more schedules:"), this ) );
	d_list = new QListWidget( this );
	d_list->setAlternatingRowColors( true );
	connect( d_list, SIGNAL( itemSelectionChanged () ), SLOT( onSelectionChanged() ) );
	vbox->addWidget( d_list );
	d_bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( false );
	vbox->addWidget( d_bb );
    connect(d_bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(d_bb, SIGNAL(rejected()), this, SLOT(reject()));
	connect(d_list, SIGNAL( itemDoubleClicked ( QListWidgetItem * ) ), this, SLOT( accept() ) );
}

QList<Udb::Obj> ScheduleSelectorDlg::select( const Objs& selected, bool many )
{
    if( many )
        d_list->setSelectionMode( QListWidget::ExtendedSelection );
    else
        d_list->setSelectionMode( QListWidget::SingleSelection );
	QSet<quint64> sel;
	for( int i = 0; i < selected.size(); i++ )
		sel.insert( selected[i].getOid() );

	QList<Udb::Obj> res;
	Udb::Obj l = d_txn->getObject( ScheduleObj::s_schedules );
	Udb::Obj s = l.getFirstObj();
	QFont f = d_list->font();
	f.setItalic(true);
	if( !s.isNull() ) do
	{
		if( s.getType() == TypeSchedule )
		{
			QListWidgetItem* lwi = new QListWidgetItem( d_list );
			lwi->setText( s.getValue( AttrText ).toString() );
			lwi->setData( Qt::UserRole, s.getOid() );
			if( sel.contains( s.getOid() ) )
				lwi->setFont( f );
			/*
			if( sel.contains( s.getOid() ) )
				lwi->setCheckState( Qt::Checked );
			else
				lwi->setCheckState( Qt::Unchecked );
			*/
			lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
		}
	}while( s.next() );
	d_list->sortItems();
	if( exec() == QDialog::Accepted )
	{
		QList<QListWidgetItem *> lwi = d_list->selectedItems ();
		for( int i = 0; i < lwi.size(); i++ )
		{
			res.append( d_txn->getObject( lwi[i]->data( Qt::UserRole ).toULongLong() ) );
		}
	}
	return res;
}

void ScheduleSelectorDlg::onSelectionChanged()
{
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( d_list->selectionModel()->hasSelection() );
}

ScheduleSetSelectorDlg::ScheduleSetSelectorDlg( QWidget* p, Udb::Transaction * txn ):QDialog(p),d_txn(txn)
{
    Q_ASSERT( txn != 0 );
	setWindowTitle( tr("Select Schedule Sets") );
	QVBoxLayout* vbox = new QVBoxLayout( this );
	vbox->addWidget( new QLabel( tr("Select one or more schedule sets:"), this ) );
	d_list = new QListWidget( this );
	d_list->setAlternatingRowColors( true );
	d_list->setSelectionMode( QListWidget::ExtendedSelection );
	connect( d_list, SIGNAL( itemSelectionChanged () ), SLOT( onSelectionChanged() ) );
	vbox->addWidget( d_list );
	d_bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( false );
	vbox->addWidget( d_bb );
    connect(d_bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(d_bb, SIGNAL(rejected()), this, SLOT(reject()));
	connect(d_list, SIGNAL( itemDoubleClicked ( QListWidgetItem * ) ), this, SLOT( accept() ) );
}

QList<Udb::Obj> ScheduleSetSelectorDlg::select()
{
	QList<Udb::Obj> res;
	QList<Udb::Obj> l = ScheduleObj::getScheduleSets( d_txn );
	for( int i = 0; i < l.size(); i++ )
	{
		QListWidgetItem* lwi = new QListWidgetItem( d_list );
		lwi->setText( l[i].getValue( AttrText ).toString() );
		lwi->setData( Qt::UserRole, l[i].getOid() );
		lwi->setFlags( Qt::ItemIsSelectable | Qt::ItemIsEnabled );
	}
	d_list->sortItems();
	if( exec() == QDialog::Accepted )
	{
		QList<QListWidgetItem *> lwi = d_list->selectedItems ();
		for( int i = 0; i < lwi.size(); i++ )
		{
			res.append( d_txn->getObject( lwi[i]->data( Qt::UserRole ).toULongLong() ) );
		}
	}
	return res;
}

void ScheduleSetSelectorDlg::onSelectionChanged()
{
	d_bb->button( QDialogButtonBox::Ok )->setEnabled( d_list->selectionModel()->hasSelection() );
}
