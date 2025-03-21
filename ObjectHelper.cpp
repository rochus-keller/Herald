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

#include "ObjectHelper.h"
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/Idx.h>
#include <QFileInfo>
#include <QDir>
#include "HeraldApp.h"
#include "HeTypeDefs.h"
using namespace He;

QString ObjectHelper::getInboxPath(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    QFileInfo info( txn->getDb()->getFilePath() );
    const QString path = info.absoluteDir().absoluteFilePath(
                info.completeBaseName() + QLatin1String( ".maildrop" ) );
    QDir dir;
    dir.mkpath( path );
    return path;
}

QString ObjectHelper::getOutboxPath(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    QFileInfo info( txn->getDb()->getFilePath() );
    const QString path = info.absoluteDir().absoluteFilePath(
                info.completeBaseName() + QLatin1String( ".sendcache" ) );
    QDir dir;
    dir.mkpath( path );
    return path;
}

QString ObjectHelper::getDocStorePath(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    QFileInfo info( txn->getDb()->getFilePath() );
    const QString path = info.absoluteDir().absoluteFilePath(
                info.completeBaseName() + QLatin1String( ".docstore" ) );
    QDir dir;
    dir.mkpath( path );
	return path;
}

QString ObjectHelper::getCertDbPath(Udb::Transaction * txn)
{
	Q_ASSERT( txn != 0 );
	QFileInfo info( txn->getDb()->getFilePath() );
	const QString path = info.absoluteDir().absoluteFilePath(
				info.completeBaseName() + QLatin1String( ".certdb" ) );
	QDir dir;
	dir.mkpath( path );
	return path;
}

Udb::Obj ObjectHelper::getRoot(Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    return txn->getOrCreateObject( HeraldApp::s_rootUuid );
}

Udb::Obj ObjectHelper::createObject(quint32 type, Udb::Obj parent, const Udb::Obj& before )
{
    Q_ASSERT( !parent.isNull() );
    Udb::Obj o = parent.createAggregate( type, before );
    initObject( o );
    return o;
}

Udb::Obj ObjectHelper::createObject(quint32 type, Udb::Transaction * txn)
{
    Q_ASSERT( txn != 0 );
    Udb::Obj o = txn->createObject( type );
    initObject( o );
    return o;
}

void ObjectHelper::initObject(Udb::Obj & o)
{
    o.setTimeStamp( AttrCreatedOn );
    const QString id = getNextIdString( o.getTxn(), o.getType() );
    if( !id.isEmpty() )
        o.setString( AttrInternalId, id );
}

quint32 ObjectHelper::getNextId(Udb::Transaction * txn, quint32 type)
{
    Udb::Obj root = getRoot( txn );
    Q_ASSERT( !root.isNull() );
    return root.incCounter( type );
}

QString ObjectHelper::getNextIdString(Udb::Transaction * txn, quint32 type)
{
    QString prefix;
    switch( type )
    {
    case TypeInboundMessage:
        prefix = QLatin1String("MI");
        break;
    case TypeOutboundMessage:
        prefix = QLatin1String("MO");
        break;
    case TypeDocument:
        prefix = QLatin1String("D");
        break;
    case TypeMailDraft:
        prefix = QLatin1String("MD");
    }
    if( !prefix.isEmpty() )
        return QString("%1%2").arg( prefix ).
                     arg( getNextId( txn, type ), 3, 10, QLatin1Char('0' ) );
    else
        return QString();
}

