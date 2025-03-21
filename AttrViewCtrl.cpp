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

#include "AttrViewCtrl.h"
#include "ObjectTitleFrame.h"
#include <QVBoxLayout>
#include <QInputDialog>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <Oln2/OutlineUdbCtrl.h>
#include <Udb/Database.h>
#include "HeTypeDefs.h"
#include "HeraldApp.h"
using namespace He;

// 1:1 aus WorkTree

void ObjectDetailPropsMdl::setObject(const Udb::Obj& o)
{
    if( d_obj.equals( o ) )
        return;
    beginResetModel();
    clear();
    d_obj = o;
    Slot* root = new Slot();
    add( root, 0 );
    if( d_obj.isNull() )
        return;
    Udb::Obj::Names n = o.getNames();
    QMap<QString,quint32> dir;
    for( int i = 0; i < n.size(); i++ )
    {
        QString txt = HeTypeDefs::prettyName( n[i], o.getTxn() );
        if( !txt.isEmpty() )
            dir[ txt ] = n[i];
    }
    QMap<QString,quint32>::const_iterator j;
    for( j = dir.begin(); j != dir.end(); ++j )
    {
        ObjSlot* s = new ObjSlot();
        s->d_name = j.value();
        add( s, root );
    }
    endResetModel();
}

AttrViewCtrl::AttrViewCtrl(QWidget *parent) :
    QObject(parent)
{
}

He::AttrViewCtrl *He::AttrViewCtrl::create(QWidget *parent, Udb::Transaction *txn)
{
    Q_ASSERT( txn != 0 );
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 1 );

    AttrViewCtrl* ctrl = new AttrViewCtrl( pane );

    ctrl->d_title = new ObjectTitleFrame( pane );
    vbox->addWidget( ctrl->d_title );


    ctrl->d_props = new ObjectDetailPropsMdl( pane );

	ctrl->d_tree = new Oln::OutlineTree( pane );
	ctrl->d_tree->setAlternatingRowColors( true );
	ctrl->d_tree->setIndentation( 10 ); // RISK
	ctrl->d_tree->setHandleWidth( 7 );
	ctrl->d_tree->setShowNumbers( false );
	ctrl->d_tree->setModel( ctrl->d_props );
	Oln::OutlineCtrl* ctrl2 = new Oln::OutlineCtrl( ctrl->d_tree );
    Oln::OutlineDeleg* deleg = ctrl2->getDeleg();
    deleg->setReadOnly(true);
    connect( deleg->getEditCtrl(), SIGNAL(anchorActivated(QByteArray,bool) ),
		ctrl, SLOT( onAnchorActivated(QByteArray, bool) ) );
	ctrl->d_tree->setAcceptDrops( false );
    vbox->addWidget( ctrl->d_tree );

    ctrl->d_title->adjustFont();
	pane->updateGeometry();
    connect( ctrl->d_title, SIGNAL(signalClicked()), ctrl, SLOT(onTitleClick()) );

    txn->getDb()->addObserver( ctrl, SLOT( onDbUpdate( Udb::UpdateInfo ) ) );

    return ctrl;
}

void AttrViewCtrl::setObj(const Udb::Obj & o)
{
    d_props->setObject( o );
    d_title->setObj( o );
}

void AttrViewCtrl::onTitleClick()
{
    if( !d_title->getObj().isNull() )
        emit signalFollowObject( d_title->getObj() );
}

const Udb::Obj &AttrViewCtrl::getObj() const
{
    return d_title->getObj();
}

QWidget *AttrViewCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

void AttrViewCtrl::addCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Edit Text..."), this,SLOT(onEditText()), tr("CTRL+E"), true );
    pop->addCommand( tr("Copy Ref."), this,SLOT(onCopy()), tr("CTRL+SHIFT+C"), true );
}

void AttrViewCtrl::onAnchorActivated(QByteArray data, bool url)
{
    if( url )
        HeraldApp::openUrl( QUrl::fromEncoded( data ) );
    else
    {
        Oln::Link link;
        if( link.readFrom( data ) && d_title->getObj().getDb()->getDbUuid() == link.d_db )
            emit signalFollowObject( d_title->getObj().getObject( link.d_oid ) );
    }
}

void AttrViewCtrl::onDbUpdate(Udb::UpdateInfo info)
{
    switch( info.d_kind )
	{
    case Udb::UpdateInfo::ObjectErased:
        if( info.d_id == d_title->getObj().getOid() )
            setObj( Udb::Obj() );
        break;
    }
}

void AttrViewCtrl::onEditText()
{
    ENABLED_IF( !d_title->getObj().isNull() );
    bool ok;
    QString str = QInputDialog::getText( getWidget(), tr("Edit Text - Herald"),
                           tr("Text:"), QLineEdit::Normal,
                                         d_title->getObj().getString( AttrText ), &ok );
    if( !ok )
        return;
    Udb::Obj o = d_title->getObj();
    o.setString( AttrText, str );
    o.setTimeStamp(AttrModifiedOn);
    o.commit();
}

void AttrViewCtrl::onCopy()
{
    ENABLED_IF( !d_title->getObj().isNull() );
    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    objs.append( d_title->getObj() );
    HeTypeDefs::writeObjectRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );

}

QVariant ObjectDetailPropsMdl::ObjSlot::getData(const Oln::OutlineMdl * m, int role) const
{
    const ObjectDetailPropsMdl* mdl = static_cast<const ObjectDetailPropsMdl*>(m);
	if( role == Qt::DisplayRole )
	{
		QString html = "<html>";
		html += "<b>" + HeTypeDefs::prettyName( d_name, mdl->getObj().getTxn() ) + ":</b> ";
		html += HeTypeDefs::formatValue( d_name, mdl->getObj() );
		html += "</html>";
		return QVariant::fromValue( OutlineMdl::Html( html ) );
	}else
		return QVariant();
}
