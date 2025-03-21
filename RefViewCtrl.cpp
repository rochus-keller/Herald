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

#include "RefViewCtrl.h"
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QInputDialog>
#include <QApplication>
#include <QClipboard>
#include <QtCore/QMimeData>
#include <QListWidget>
#include <Udb/Database.h>
#include <Udb/Idx.h>
#include <Udb/Transaction.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Oln2/OutlineItem.h>
#include "HeTypeDefs.h"
#include "ObjectTitleFrame.h"
#include "MailObj.h"
using namespace He;

RefViewCtrl::RefViewCtrl(QWidget *parent) :
QWidget(parent),d_lock(false)
{
}

QWidget *RefViewCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

RefViewCtrl *RefViewCtrl::create(QWidget *parent, Udb::Transaction* txn )
{
    Q_ASSERT( txn != 0 );
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 1 );

    RefViewCtrl* ctrl = new RefViewCtrl(pane);

    ctrl->d_title = new ObjectTitleFrame( pane );
    vbox->addWidget( ctrl->d_title );

    ctrl->d_list = new QListWidget( pane );
    ctrl->d_list->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    //ctrl->d_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ctrl->d_list->setAlternatingRowColors(true);
    vbox->addWidget( ctrl->d_list );
    connect( ctrl->d_list, SIGNAL(itemPressed(QListWidgetItem*)), ctrl,SLOT(onClicked(QListWidgetItem*)) );
    connect( ctrl->d_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)),
            ctrl,SLOT(onDblClicked(QListWidgetItem*)) );
    connect( ctrl->d_title, SIGNAL(signalClicked()), ctrl, SLOT(onTitleClick()) );

    txn->getDb()->addObserver( ctrl, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );

    return ctrl;
}

void RefViewCtrl::addCommands(Gui::AutoMenu *)
{
}

void RefViewCtrl::setObj(const Udb::Obj & o)
{
    if( d_title->getObj().equals( o ) || d_lock )
        return;

    if( HeTypeDefs::isScheduleItem( o.getType() ) )
    {
        d_title->setObj( o );
        fillCausingRefs();
        return;
    }

    switch( o.getType() )
    {
    case TypeOutboundMessage:
    case TypeInboundMessage:
        d_title->setObj( o );
        fillMailRefs();
        break;
    case TypeDocument:
        d_title->setObj( o );
        fillDocRefs();
        break;
    case TypeAttachment:
        {
            Udb::Obj doc = o.getValueAsObj(AttrDocumentRef);
            d_title->setObj( doc );
            fillDocRefs();
        }
        break;
    }
}

void RefViewCtrl::clear()
{
    d_title->setObj( Udb::Obj() );
    d_list->clear();
}

void RefViewCtrl::onClicked(QListWidgetItem *item)
{
    if( item == 0 )
        return;
    d_lock = true;
    emit sigSelected( d_title->getObj().getObject( item->data(Qt::UserRole).toULongLong() ) );
    d_lock = false;
}

void RefViewCtrl::onDblClicked(QListWidgetItem *item)
{
    if( item == 0 )
        return;
    d_lock = true;
    emit sigDblClicked( d_title->getObj().getObject( item->data(Qt::UserRole).toULongLong() ) );
    d_lock = false;
}

void RefViewCtrl::onDbUpdate(Udb::UpdateInfo info)
{
    switch( info.d_kind )
	{
    case Udb::UpdateInfo::ObjectErased:
        if( info.d_id == d_title->getObj().getOid() )
            clear();
        break;
    }
}


void RefViewCtrl::fillMailRefs()
{
    d_list->clear();

    Udb::Obj mail = d_title->getObj();

    Udb::Obj o = mail.getValueAsObj(AttrInReplyTo);
    // ReplyOf
    if( !o.isNull() )
        addItem( tr("Reply to"), o );
    o = mail.getValueAsObj(AttrForwardOf);
    if( !o.isNull() )
        addItem( tr("Forward to"), o );

    Udb::Idx i1( mail.getTxn(), IndexDefs::IdxForwardOf );
    if( i1.seek( mail ) ) do
    {
        Udb::Obj other = mail.getObject( i1.getOid() );
        Q_ASSERT( !other.isNull(true,true) );
        addItem( tr("Forwarded by"), other );
    }while( i1.nextKey() );
    Udb::Idx i2( mail.getTxn(), IndexDefs::IdxInReplyTo );
    if( i2.seek( mail ) ) do
    {
        Udb::Obj other = mail.getObject( i2.getOid() );
        Q_ASSERT( !other.isNull(true,true) );
        addItem( tr("Replied by"), other );
    }while( i2.nextKey() );

    Udb::Mit i3 = mail.findCells( Udb::Obj::KeyList() << Stream::DataCell().setAtom( AttrReference ) );
    if( !i3.isNull() ) do
    {
        Udb::Obj other = mail.getObject( i3.getValue().getOid() );
		if( !other.isNull(true,true) )
			addItem( tr("Reference"), other );
    }while( i3.nextKey() );
	addItemRefs(mail);
}

void RefViewCtrl::fillDocRefs()
{
    d_list->clear();
    Udb::Obj doc = d_title->getObj();
    Udb::Idx i1( doc.getTxn(), IndexDefs::IdxDocumentRef );
    if( i1.seek( doc ) ) do
    {
        Udb::Obj other = doc.getObject( i1.getOid() );
        Q_ASSERT( !other.isNull(true,true) );
        addItem( tr("Used by"), other );
    }while( i1.nextKey() );
	addItemRefs(doc);
}

void RefViewCtrl::fillCausingRefs()
{
    d_list->clear();
    Udb::Obj obj = d_title->getObj();
    Udb::Idx i1( obj.getTxn(), IndexDefs::IdxCausing );
    if( i1.seek( obj ) ) do
    {
        Udb::Obj other = obj.getObject( i1.getOid() );
        Q_ASSERT( !other.isNull(true,true) );
        QListWidgetItem* item = addItem( tr("Caused by"), other.getParent() );
        item->setData( Qt::UserRole, other.getOid() );
	}while( i1.nextKey() );
	addItemRefs(obj);
}

void RefViewCtrl::addItemRefs(const Udb::Obj& o)
{
	QList<Oln::OutlineItem> l = Oln::OutlineItem::getReferences(o);
	for( int i = 0; i < l.size(); i++ )
		addItem( tr("Item Ref."), l[i] );
}

QListWidgetItem* RefViewCtrl::addItem(const QString &title, const Udb::Obj &o)
{
    QListWidgetItem* item = new QListWidgetItem( d_list );
    item->setIcon( QIcon( Oln::OutlineUdbMdl::getPixmapPath( o.getType() ) ) );
    item->setText( tr("%1: %2").arg(title).arg( HeTypeDefs::formatObjectTitle( o ) ) );
    item->setData( Qt::UserRole, o.getOid() );
    item->setToolTip( generateToolTip( o ) );
    return item;
}

static QString _formatAddrs( const MailObj::MailAddrList& addrs, bool full )
{
    QStringList res;
    foreach( MailObj::MailAddr a, addrs )
    {
        if( full )
            res.append( a.prettyNameEmail() );
        else
            res.append( a.d_name );
    }
    if( full )
        return res.join( "<br>" );
    else
        return res.join( ", " );
}

QString RefViewCtrl::generateToolTip(const Udb::Obj &o)
{
    MailObj mail = o;
    if( mail.getType() == TypeInboundMessage )
        return tr("<html><b>ID:</b> %4 <br>"
              "<b>From:</b> %1 <br>"
              "<b>Sent:</b> %2 <br>"
              "<b>Subject:</b> %3 <br>"
              "<b>Attachments:</b> %5").arg( mail.getFrom().prettyNameEmail() ).
            arg( HeTypeDefs::prettyDateTime(
                     mail.getValue( AttrSentOn ).getDateTime().toLocalTime(), true, true ) ).
            arg( mail.getString( AttrText ) ).
            arg( mail.getString( AttrInternalId ) ).
            arg( mail.getValue( AttrAttCount ).getUInt32() );
    else if( mail.getType() == TypeOutboundMessage )
        return tr("<html><b>ID:</b> %4 <br>"
              "<b>To:</b> %1 <br>"
              "<b>Sent:</b> %2 <br>"
              "<b>Subject:</b> %3 <br>"
              "<b>Attachments:</b> %5").arg( _formatAddrs( mail.getTo(true), true ) ).
            arg( HeTypeDefs::prettyDateTime(
                     mail.getValue( AttrSentOn ).getDateTime().toLocalTime(), true, true ) ).
            arg( mail.getString( AttrText ) ).
            arg( mail.getString( AttrInternalId ) ).
            arg( mail.getValue( AttrAttCount ).getUInt32() );
    else
        return QString();
}

void RefViewCtrl::onTitleClick()
{
    if( !d_title->getObj().isNull() )
        emit sigSelected( d_title->getObj() );
}
