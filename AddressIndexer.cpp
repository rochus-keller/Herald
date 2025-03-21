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

#include "AddressIndexer.h"
#include <Udb/Transaction.h>
#include <Udb/Idx.h>
#include "HeTypeDefs.h"
#include "ObjectHelper.h"
#include <QtDebug>
#include "MailObj.h"
#include "HeraldApp.h"
using namespace He;


AddressIndexer::AddressIndexer(Udb::Transaction * txn, QObject *parent) :
    QObject(parent)
{
    Q_ASSERT( txn != 0 );

    d_index = txn->getOrCreateObject( QUuid( HeraldApp::s_addressIndex ) );
    txn->commit();
	txn->setIndividualNotify(false);
    txn->addObserver( this, SLOT(onDbUpdate( Udb::UpdateInfo ) ), false );
//    Udb::Idx idx( txn, IndexDefs::IdxEmailAddress );
//    if( idx.first() ) do
//    {
//        Udb::Obj o = txn->getObject( idx.getOid() );
//        qDebug() << o.getString(AttrEmailAddress) << idx.getOid();
//    }while( idx.next() );
}

void AddressIndexer::test()
{
    Udb::Idx idx( d_index.getTxn(), IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        Udb::Obj mail = d_index.getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        qDebug() << "mail" << mail.getString(AttrInternalId) << mail.getString(AttrText);
        Udb::Obj party = mail.getFirstObj();
        if( !party.isNull() )do
        {
            if( HeTypeDefs::isParty( party.getType() ) )
            {
                qDebug() << "party" << party.getString(AttrText) << party.getValue(AttrPartyDate).getDateTime();
                Udb::Obj addr = party.getValueAsObj(AttrPartyAddr);
                Q_ASSERT( !addr.isNull(true) );
                qDebug() << "address" << addr.getString(AttrEmailAddress);
                addr.setValue(AttrLastUse,party.getValue(AttrPartyDate));
                addr.incCounter(AttrUseCount);
            }
        }while( party.next() );

    }while( idx.next() );
    d_index.commit();
}
bool _lt(const QByteArray &s1, const QByteArray &s2)
 {
    return memcmp( s1.data(), s2.data(), s1.length() ) < 0;
    // NOTE: strcmp, strncmp oder s1 < s2 funktionieren nicht, da stop bei erstem Nullzeichen
 }

void AddressIndexer::test2()
{
    QList<QByteArray> sort;
    Udb::Idx idx( d_index.getTxn(), IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        Udb::Obj mail = d_index.getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        QDateTime dt = mail.getValue(AttrSentOn).getDateTime();
        //sort.append( Stream::DataCell().setDateTime( dt ).writeCell() );
        qDebug() << "mail" << mail.getString(AttrInternalId) <<
                    HeTypeDefs::prettyDateTime( mail.getValue(AttrSentOn).getDateTime() );

    }while( idx.next() );
//    qSort(sort.begin(), sort.end(), _lt );
//    foreach( QByteArray a, sort )
//    {
//        Stream::DataCell d;
//        d.readCell( a );
//        qDebug() << d.getDateTime().toString(Qt::ISODate ) << a.toHex();
//    }
}

void AddressIndexer::test3()
{
//    Udb::Idx idx( d_index.getTxn(), IndexDefs::IdxPartyAddrDate );
//    idx.rebuildIndex();
    QList<QByteArray> sort;
    Udb::Idx idx( d_index.getTxn(), IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        Udb::Obj mail = d_index.getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( mail.getType() == TypeOutboundMessage && !mail.hasValue( AttrInternalId ) )
            mail.setString( AttrInternalId, ObjectHelper::getNextIdString(
                                d_index.getTxn(), TypeOutboundMessage ) );
    }while( idx.next() );
    d_index.commit();
}

void AddressIndexer::test4()
{
}

static QStringList _print( const Udb::Obj::KeyList& k )
{
    QStringList l;
    for( int i = 0; i < k.size(); i++ )
        l.append( k[i].toPrettyString() );
    return l;
}

void AddressIndexer::indexAll(bool clear)
{
    if( clear )
    {
        Udb::Transaction* txn = d_index.getTxn();
        d_index.erase();
        txn->commit();
        d_index = txn->getOrCreateObject( QUuid( HeraldApp::s_addressIndex ) );
        txn->commit();
    }else
    {
        Udb::Idx idx( d_index.getTxn(), IndexDefs::IdxEmailAddress );
        if( idx.first() ) do
        {
            Udb::Obj addr = d_index.getObject( idx.getOid() );
//            qDebug() << "address:" << splitAddress( addr.getString( AttrEmailAddress ) );
//            qDebug() << "name:" << splitName( addr.getString( AttrText ) );
            addToIndex( splitAddress( addr.getString( AttrEmailAddress ) ), addr, clear );
            addToIndex( splitName( addr.getValue( AttrText ).toString() ), addr, clear );
        }while( idx.next() );
    }
    d_index.commit();

//    Udb::Xit xit = d_index.findCells( QByteArray() );
//    qDebug() << "****************** Index ********************";
//    if( !xit.isNull() )do
//    {
//        qDebug() << "index:" << xit.getKey().toHex() << xit.getKey();
//    }while( xit.nextKey() );
//    qDebug() << "****************** End ********************";
}

void AddressIndexer::clearIndex()
{
    indexAll( true );
}

bool _compareLastUse(const Udb::Obj &o1, const Udb::Obj &o2)
{
    return o1.getValue( AttrLastUse ).getDateTime() >= o2.getValue( AttrLastUse ).getDateTime();
}

QList<Udb::Obj> AddressIndexer::find( QString what, bool noObsoletes ) const
{
    bool sort = true;
    if( what == QChar('*') )
    {
        sort = false;
        what.clear();
    }

    QList<Udb::Obj> res;
    QByteArray key;
    Udb::Idx::collate( key, 0, what.toLower() ); // Udb::IndexMeta::NFKD_CanonicalBase, what.toLower() );
    Udb::Xit xit = d_index.findCells( key );
    if( !xit.isNull() ) do
    {
        if( xit.getValue().isOid() )
        {
            Udb::Obj o = d_index.getObject( xit.getValue().getOid() );
            if( !o.isNull( true, true ) )
            {
                if( !noObsoletes || !o.getValue( AttrEmailObsolete ).getBool() )
                {
                    if( !res.contains(o) )
                        res.append( o );
                }
            }
        }
    }while( xit.nextKey() );
    if( sort )
        qSort(res.begin(), res.end(), _compareLastUse ); // Sortiere absteigend nach LastUse
    return res;
}

void AddressIndexer::onDbUpdate( const Udb::UpdateInfo& info )
{
    if( info.d_kind != Udb::UpdateInfo::PreCommit )
        return;
    // mache hier eine richtige Kopie da durch die vorliegende Funktion die Notification List
    // ergänzt wird.
    QList<Udb::UpdateInfo> updates = d_index.getTxn()->getPendingNotifications();
    // TODO: mehrere Changes im selben Batch am selben Objekt/Attribut abfangen
    for( int i = 0; i < updates.size(); i++ )
    {
        if( updates[i].d_kind == Udb::UpdateInfo::ValueChanged )
        {
            if( updates[i].d_name == AttrEmailAddress )
            {
                Udb::Obj o = d_index.getObject(updates[i].d_id);

                addToIndex( splitAddress( o.getValue( AttrEmailAddress, true ). // old value
                                               toString() ), o, true );
                addToIndex( splitAddress( o.getValue( AttrEmailAddress ).toString() ), o );
            }else if( updates[i].d_name == AttrText )
            {
                Udb::Obj o = d_index.getObject(updates[i].d_id);
                if( o.getType() == TypeEmailAddress )
                {
                    addToIndex( splitName( o.getValue( AttrText, true ). // old value
                                                   toString() ), o, true );
                    addToIndex( splitName( o.getValue( AttrText ).toString() ), o );
                }
            }
        }else if( updates[i].d_kind == Udb::UpdateInfo::ObjectErased &&
                  updates[i].d_name == TypeEmailAddress )
        {
            Udb::Obj o = d_index.getObject(updates[i].d_id);
            addToIndex( splitAddress( o.getValue( AttrEmailAddress, true ). // old value
                                           toString() ), o, true );
            addToIndex( splitName( o.getValue( AttrText, true ). // old value
                                           toString() ), o, true );
        }
    }

}

void AddressIndexer::addToIndex(const QStringList & l, const Udb::Obj & o, bool remove )
{
    Stream::DataCell v;
    if( remove )
        v.setNull();
    else
        v.setOid( o.getOid() );
    const QByteArray oid = Stream::DataCell().setOid( o.getOid() ).writeCell();
    QByteArray key;
    foreach(QString s, l )
    {
        Udb::Idx::collate( key, 0, s.toLower() ); //Udb::IndexMeta::NFKD_CanonicalBase, s.toLower() );
        key += oid;
        d_index.setCell( key, v );
    }
    // qDebug() << ( (remove)?"remove":"add" ) << "index:" << v.toPrettyString() << l;
}

QStringList AddressIndexer::splitAddress(const QString & str)
{
    QStringList addr = str.split( QChar('@') );
    if( addr.size() == 2 )
    {
        QStringList res = addr.first().split( QRegExp("[!#$%&'*+-/=? ^_`.{|}~]"),
                                  QString::SkipEmptyParts );
        res.append( addr.first() );
		QStringList domain = addr.last().split( QChar('.') );
		for( int i = 0; i < domain.size() - 1; i++ ) // ignoriere letztes segment
			res.append( domain[i] );
		res.append( addr.last() );
        return res;
    }else
        return QStringList();
}

QStringList AddressIndexer::splitName(const QString &name)
{
    QStringList res = name.split( QChar(' '), QString::SkipEmptyParts );
    res.append( name ); // auch noch im Klartext, RISK; ev. stattdessen Suche nach mehreren Begriffen AND
    return res;
}
