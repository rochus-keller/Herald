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

#include "MailListCtrl.h"
#include <QHeaderView>
#include <QTreeView>
#include <QVBoxLayout>
#include <QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QMimeData>
#include <QMessageBox>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <GuiTools/AutoToolBar.h>
#include "ObjectTitleFrame.h"
#include "MailListDeleg.h"
#include "HeTypeDefs.h"
using namespace He;

MailListCtrl::MailListCtrl(QWidget *parent) :
	QObject(parent),d_resolvePerson(true)
{
}

QWidget *MailListCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

MailListCtrl *MailListCtrl::create(QWidget *parent, Udb::Transaction* txn)
{
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 0 );

    MailListCtrl* ctrl = new MailListCtrl( pane );

    ctrl->d_title = new ObjectTitleFrame( pane );
    vbox->addWidget( ctrl->d_title );

    Gui::AutoToolBar* tb = new Gui::AutoToolBar( pane );
    vbox->addWidget( tb );
    QPixmap fromIcon(":/images/from-party.png");
    tb->addCommand( tr("Show From"), ctrl, SLOT(onShowFrom()) )->setIcon(fromIcon);
    tb->addCommand( tr("Show To"), ctrl, SLOT(onShowTo()) )->setIcon(QIcon(":/images/to-party.png"));
    tb->addCommand( tr("Show Cc"), ctrl, SLOT(onShowCc()) )->setIcon(QIcon(":/images/cc-party.png"));
    tb->addCommand( tr("Show Bcc"), ctrl, SLOT(onShowBcc()) )->setIcon(QIcon(":/images/bcc-party.png"));
    tb->addCommand( tr("Show Resent"), ctrl, SLOT( onShowResent()) )->setIcon(QIcon(":/images/resent-to-party.png"));
    tb->addCommand( tr("Show Inbound"), ctrl, SLOT( onShowInbound()) )->setIcon(
        QIcon(":/images/document-mail-open.png"));
    tb->addCommand( tr("Show Outbound"), ctrl, SLOT( onShowOutbound()) )->setIcon(
        QIcon(":/images/document-mail.png"));
	tb->addCommand( tr("Resolve Person"), ctrl, SLOT(onResolvePerson()) )->setIcon(QIcon(":/images/person.png") );
	tb->setIconSize( QSize( 16, 16 ) ); // fromIcon.size() );
    tb->setToolButtonStyle( Qt::ToolButtonIconOnly );

    ctrl->d_tree = new QTreeView( parent );
	ctrl->d_tree->setAlternatingRowColors( true );
	ctrl->d_tree->setRootIsDecorated( false );
	ctrl->d_tree->setAllColumnsShowFocus(true);
	ctrl->d_tree->setSelectionBehavior( QAbstractItemView::SelectRows );
	ctrl->d_tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	ctrl->d_tree->header()->hide();
    ctrl->d_tree->setItemDelegate( new MailListDeleg( ctrl->d_tree ) );
    vbox->addWidget( ctrl->d_tree );

    ctrl->d_mdl = new MailListMdl( ctrl->d_tree );
    ctrl->d_mdl->setInverted( true );
    txn->getDb()->addObserver( ctrl->d_mdl, SLOT(onDbUpdate( Udb::UpdateInfo )), false );
    ctrl->d_tree->setModel( ctrl->d_mdl );

    connect( ctrl->d_tree->selectionModel(),
             SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             ctrl, SLOT(onSelectionChanged()) );
    connect( ctrl->d_tree, SIGNAL(clicked(QModelIndex)), ctrl, SLOT( onSelectionChanged() ) );

    return ctrl;
}

void MailListCtrl::addCommands(Gui::AutoMenu * pop)
{
	pop->addCommand( tr("Show From"), this, SLOT(onShowFrom()) )->setIcon(QIcon(":/images/from-party.png"));
	pop->addCommand( tr("Show To"), this, SLOT(onShowTo()) )->setIcon(QIcon(":/images/to-party.png"));
	pop->addCommand( tr("Show Cc"), this, SLOT(onShowCc()) )->setIcon(QIcon(":/images/cc-party.png"));
	pop->addCommand( tr("Show Bcc"), this, SLOT(onShowBcc()) )->setIcon(QIcon(":/images/bcc-party.png"));
	pop->addCommand( tr("Show Resent"), this, SLOT( onShowResent()) )->setIcon(QIcon(":/images/resent-to-party.png"));
	pop->addCommand( tr("All On/Off"), this, SLOT( onAllOnOff()) )->setIcon(QIcon(":/images/all-party.png") );
	pop->addCommand( tr("Show Inbound"), this, SLOT( onShowInbound()) )->setIcon(
				QIcon(":/images/document-mail-open.png"));
	pop->addCommand( tr("Show Outbound"), this, SLOT( onShowOutbound()) )->setIcon(
				QIcon(":/images/document-mail.png"));
	pop->addCommand( tr("Resolve Person"), this, SLOT(onResolvePerson()) )->setIcon(
				QIcon(":/images/person.png"));
	pop->addSeparator();
	pop->addCommand( tr("Copy"), this,SLOT(onCopy()), tr("CTRL+C"), true );
	pop->addCommand( tr("Delete..."), this,SLOT(onDelete()), tr("CTRL+D"), true );

}

void MailListCtrl::setIdx(const Udb::Idx & idx)
{
    d_mdl->setIdx( idx );
    d_title->setObj( Udb::Obj() );
}

void MailListCtrl::setObj(const Udb::Obj & o)
{
    Udb::Obj obj = o;
	if( d_resolvePerson && o.getType() == TypeEmailAddress && !o.getParent().isNull() )
        obj = o.getParent();
    if( d_title->getObj().equals( obj ) )
        return;
    if( obj.isNull() )
    {
        d_mdl->setIdx( Udb::Idx() );
        d_title->setObj( Udb::Obj() );
        return;
    }
    switch( obj.getType() )
    {
    case TypeEmailAddress:
        d_title->setObj( obj );
        d_mdl->setIdx( Udb::Idx( obj.getTxn(), IndexDefs::IdxPartyAddrDate ) );
        d_mdl->seek( obj );
        d_tree->scrollTo( d_mdl->index(0, 0 ) );
        break;
    case TypePerson:
        d_title->setObj( obj );
        d_mdl->setIdx( Udb::Idx( obj.getTxn(), IndexDefs::IdxPartyPersDate ) );
        d_mdl->seek( obj );
        d_tree->scrollTo( d_mdl->index(0, 0 ) );
        break;
    case TypeFromParty:
    case TypeToParty:
    case TypeCcParty:
    case TypeBccParty:
    case TypeResentParty:
        if( obj.hasValue( AttrPartyPers ) )
        {
            d_title->setObj( obj.getValueAsObj( AttrPartyPers ) );
            d_mdl->setIdx( Udb::Idx( obj.getTxn(), IndexDefs::IdxPartyPersDate ) );
        }else if( obj.hasValue( AttrPartyAddr ) )
        {
            d_title->setObj( obj.getValueAsObj( AttrPartyAddr ) );
            d_mdl->setIdx( Udb::Idx( obj.getTxn(), IndexDefs::IdxPartyAddrDate ) );
        }else
        {
            d_mdl->setIdx( Udb::Idx() );
            d_title->setObj( Udb::Obj() );
        }
        d_mdl->seek( d_title->getObj() );
        d_tree->scrollTo( d_mdl->index(0, 0 ) );
        break;
    }
}

Udb::Obj MailListCtrl::getSelectedMail() const
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
    if( sr.size() == 1 )
        return d_mdl->getObject( sr.first() );
    // else
    return Udb::Obj();
}

void MailListCtrl::onSelectionChanged()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
    if( sr.size() == 1 )
        emit sigSelectionChanged();
}

void MailListCtrl::onCopy()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
	ENABLED_IF( sr.size() > 0 );

    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    foreach( QModelIndex i, sr )
	{
		Udb::Obj o = d_mdl->getObject( i );
		Q_ASSERT( !o.isNull() );
        if( HeTypeDefs::isParty( o.getType() ) )
            o = o.getParent();
        objs.append( o );
	}
    HeTypeDefs::writeObjectRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );
}

void MailListCtrl::onDelete()
{
    QModelIndexList sr = d_tree->selectionModel()->selectedRows();
	ENABLED_IF( sr.size() == 1 );

    const int res = QMessageBox::warning( getWidget(), tr("Delete Message - Herald"),
		tr("Do you really want to permanently delete the selected message? This cannot be undone." ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::No );
	if( res == QMessageBox::No )
		return;

    Udb::Obj o = d_mdl->getObject( sr.first() );
    o.erase();
    d_mdl->getTxn()->commit();
    // TODO nicht mehr benötigte Documents löschen?
}

void MailListCtrl::onShowFrom()
{
    handleShow(TypeFromParty);
}

void MailListCtrl::onShowTo()
{
    handleShow(TypeToParty);
}

void MailListCtrl::onShowCc()
{
    handleShow(TypeCcParty);
}

void MailListCtrl::onShowBcc()
{
    handleShow(TypeBccParty);
}

void MailListCtrl::onShowResent()
{
	handleShow(TypeResentParty);
}

void MailListCtrl::onResolvePerson()
{
	CHECKED_IF( true, d_resolvePerson );
	d_resolvePerson = !d_resolvePerson;
}

void MailListCtrl::onAllOnOff()
{
    ENABLED_IF( true );
    MailListMdl::TypeFilter f = d_mdl->getTypeFilter();
    if( !f.isEmpty() )
        f.clear();
    else
    {
        f.insert( TypeFromParty );
        f.insert( TypeToParty );
        f.insert( TypeCcParty );
        f.insert( TypeBccParty );
        f.insert( TypeResentParty );
    }
    d_mdl->setTypeFilter( f );

}

void MailListCtrl::onShowInbound()
{
    handleShow(TypeInboundMessage);
}

void MailListCtrl::onShowOutbound()
{
    handleShow(TypeOutboundMessage);
}

void MailListCtrl::handleShow(quint32 type)
{
    CHECKED_IF( true, !d_mdl->getTypeFilter().contains( type ) );
    MailListMdl::TypeFilter f = d_mdl->getTypeFilter();
    if( f.contains( type ) )
        f.remove( type );
    else
        f.insert( type );
    d_mdl->setTypeFilter( f );
}

void SmallToggler::setChecked(bool checked)
{
    if( checked )
    {
        setFrameShape( QFrame::Panel );
        setFrameShadow( QFrame::Sunken );
        d_checked = true;
    }else
    {
        setFrameShape( QFrame::NoFrame );
        setFrameShadow( QFrame::Plain );
        d_checked = false;
    }
    emit sigToggle( d_checked, d_type );
}

void SmallToggler::mousePressEvent(QMouseEvent *event)
{
    setChecked( !d_checked );
    QLabel::mousePressEvent( event );
}

SmallToggler::SmallToggler(const QString & str, const QPixmap & pix, quint32 type, bool on, QWidget *w):
    QLabel(w),d_type(type),d_checked(on)
{
    setPixmap( pix );
    setToolTip( str );
    setChecked( on );
}

