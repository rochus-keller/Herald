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

#include "ScheduleListMdl.h"
#include "HeTypeDefs.h"
#include <Stream/DataReader.h>
#include <Stream/DataWriter.h>
#include <QSize>
#include <QMimeData>
#include <Oln2/OutlineUdbMdl.h>
#include <Udb/Idx.h>
#include <QtDebug>
#include <QApplication>
#include <QFontMetrics>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include "ScheduleObj.h"
#include "ScheduleItemObj.h"
using namespace He;
using namespace Udb;

// 1:1 aus MasterPlan

static const char* s_mime = "application/masterplan/schedule-list";

ScheduleListMdl::ScheduleListMdl( QObject* p, Transaction * txn ):
    QAbstractItemModel(p)
{
    Q_ASSERT( txn != 0 );
	txn->getDb()->addObserver( this, SLOT( onDatabaseUpdate( Udb::UpdateInfo ) ), false );
    d_minRowHeight = QApplication::fontMetrics().height(); // RISK
}

void ScheduleListMdl::setList( const Udb::Obj& queue )
{
	d_list = queue;
	refill();
}

int ScheduleListMdl::rowCount ( const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		return d_rows.size();
	}else
		return 0;
}

int ScheduleListMdl::findElementRow( Udb::OID oid ) const
{
	for( int row = 0; row < d_rows.size(); row++ )
	{
		if( d_rows[row].getOid() == oid )
			return row;
	}
	return -1;
}

QModelIndex ScheduleListMdl::getIndex( const Udb::Obj& elem ) const
{
	const int row = findElementRow( elem.getOid() );
	if( row != -1 )
		return index( row, 0 );
	else
		return QModelIndex();
}

Udb::Obj ScheduleListMdl::getElem( const QModelIndex & idx ) const
{
	if( !idx.isValid() || idx.row() >= d_rows.size() )
		return Udb::Obj();
	return d_rows[idx.row()];
}

QModelIndex ScheduleListMdl::index ( int row, int column, const QModelIndex & parent ) const
{
	if( !parent.isValid() )
	{
		if( row < d_rows.size() && column == 0 )
		{
			return createIndex( row, column, row );
		}
	}
	return QModelIndex();
}

QModelIndex ScheduleListMdl::parent(const QModelIndex & index) const
{
	return QModelIndex();
}

Qt::ItemFlags ScheduleListMdl::flags(const QModelIndex &index) const
{
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled; // | Qt::ItemIsEditable;
}

QVariant ScheduleListMdl::data ( const QModelIndex & index, int role ) const
{
	if( !index.isValid() || d_list.isNull() )
		return QVariant();

	if( index.row() < d_rows.size() && index.column() == 0 )
	{
		Udb::Obj s = d_rows[index.row()].getValueAsObj( AttrElementValue );
		if( s.isNull() )
            return QVariant();
		switch( role )
		{
		case Qt::DisplayRole:
		// case Qt::ToolTipRole: // Nein
			return s.getValue( AttrText ).getStr();
		case Qt::DecorationRole:
			return Oln::OutlineUdbMdl::getPixmap( s.getType() );
		case ThreadCountRole:
			return qMax( s.getValue( AttrRowCount ).getUInt8(), quint8(1) );
		case ScheduleOid:
			return s.getOid();
		case ElementOid:
			return d_rows[index.row()].getOid();
        case Qt::SizeHintRole:
            return QSize( -1, qMax( s.getValue( AttrRowCount ).getUInt8(), quint8(1) ) * d_minRowHeight );
        case MinRowHeight:
            return d_minRowHeight;
		}
	}
    return QVariant();
}

QVariant ScheduleListMdl::headerData(int, Qt::Orientation, int) const
{
    return QVariant();
}

bool ScheduleListMdl::setData ( const QModelIndex & index, const QVariant & value, int role )
{
	if( !index.isValid() || d_list.isNull() )
		return false;
	if( index.row() < d_rows.size() && index.column() == 0 )
	{
		switch( role )
		{
		case ThreadCountRole:
			{
				Udb::Obj s = d_rows[index.row()].getValueAsObj( AttrElementValue );
				Q_ASSERT( !s.isNull() );
				s.setValue( AttrRowCount, Stream::DataCell().setUInt8( qMax( value.toInt(), 1 ) ) );
				s.setTimeStamp( AttrModifiedOn );
                s.commit();
                //emit dataChanged( index, index ); // wird in onDatabaseUpdate aufgerufen
				return true;
			}
		}
	}
	return false;
}

void ScheduleListMdl::refill()
{
    beginResetModel();
	d_rows.clear();
    d_scheds.clear();

	Udb::Obj e = d_list.getFirstObj();
	if( !e.isNull() ) do
	{
		Q_ASSERT( e.getType() == TypeSchedSetElem );
		d_rows.append( e );
        d_scheds.append( e.getValue( AttrElementValue).getOid() );
	}while( e.next() );

    endResetModel();
}

void ScheduleListMdl::onDatabaseUpdate( Udb::UpdateInfo info )
{
	switch( info.d_kind )
	{
    case Udb::UpdateInfo::ValueChanged:
        if( info.d_name == AttrRowCount )
        {
            const int row = d_scheds.indexOf( info.d_id );
            if( row != -1 )
            {
//                QModelIndex i = getIndex( d_rows[row] );
//                emit dataChanged( i, i );
                emit layoutChanged();
            }
        }
        break;
	case Udb::UpdateInfo::ObjectErased:
		if( d_list.getOid() == info.d_id )
		{
			d_list = Udb::Obj();
			refill();
		}
		break;
	case Udb::UpdateInfo::Aggregated:
		if( d_list.getOid() == info.d_parent )
		{
			Udb::Obj el = d_list.getObject( info.d_id );

			const int before = (info.d_before)?findElementRow( info.d_before ):d_rows.size();
			Q_ASSERT( before != -1 );
			beginInsertRows( QModelIndex(), before, before );
			d_rows.insert( before, el );
            d_scheds.insert( before, el.getValue( AttrElementValue).getOid() );
			endInsertRows();
		}
		break;
	case Udb::UpdateInfo::Deaggregated:
		if( d_list.getOid() == info.d_parent )
		{
			const int row = findElementRow( info.d_id );
			if( row != -1 )
			{
				beginRemoveRows( QModelIndex(), row, row );
				d_rows.removeAt( row );
                d_scheds.removeAt( row );
				endRemoveRows();
			}
		}
		break;
	}
}

QStringList ScheduleListMdl::mimeTypes() const
{
	QStringList l;
	l << s_mime;
	return l;
}

QMimeData* ScheduleListMdl::mimeData(const QModelIndexList &indexes) const
{
	QMimeData* mimeData = new QMimeData();
	Stream::DataWriter out;
	for( int i = 0; i < indexes.size(); i++ )
	{
		out.startFrame( Stream::NameTag("schl") );
		out.writeSlot( Stream::DataCell().setOid( indexes[i].data(ElementOid).toULongLong() ), Stream::NameTag( "elem" ) );
		out.writeSlot( Stream::DataCell().setOid( indexes[i].data(ScheduleOid).toULongLong() ), Stream::NameTag( "cal" ) );
		out.endFrame();
	}
	mimeData->setData( s_mime, out.getStream() );
	return mimeData;
}

void ScheduleListMdl::sortListByTypeName()
{
	if( d_list.isNull() )
		return;
	//d_rows.clear();
	//reset();
	typedef QMultiMap< QPair<QString,QString>,Udb::Obj> Sort;
	Sort sort;
	Udb::Obj e = d_list.getFirstObj();
	if( !e.isNull() ) do
	{
		Q_ASSERT( e.getType() == TypeSchedSetElem );
		Udb::Obj s = e.getValueAsObj( AttrElementValue );
		QString str;
		if( s.getType() == TypePerson )
			str = s.getString( AttrPrincipalName ) + s.getString( AttrFirstName );
		else
			str = s.getString( AttrText );
		sort.insert( qMakePair( HeTypeDefs::prettyName( s.getType() ), str ), e );
	}while( e.next() );
	Sort::iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		i.value().deaggregate();
	}
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		// qDebug() << i.key().first << i.key().second;
		i.value().aggregateTo( d_list );
	}
	d_list.commit();
	//reset();
}

void ScheduleListMdl::sortListByName()
{
	if( d_list.isNull() )
		return;
	//d_rows.clear();
	//reset();
	typedef QMultiMap< QString,Udb::Obj> Sort;
	Sort sort;
	Udb::Obj e = d_list.getFirstObj();
	if( !e.isNull() ) do
	{
		Q_ASSERT( e.getType() == TypeSchedSetElem );
		Udb::Obj s = e.getValueAsObj( AttrElementValue );
		QString str;
		if( s.getType() == TypePerson )
			str = s.getString( AttrPrincipalName ) + s.getString( AttrFirstName );
		else
			str = s.getString( AttrText );
		sort.insert( str, e );
	}while( e.next() );
	Sort::iterator i;
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		i.value().deaggregate();
	}
	for( i = sort.begin(); i != sort.end(); ++i )
	{
		// qDebug() << i.key().first << i.key().second;
		i.value().aggregateTo( d_list );
	}
	d_list.commit();
    //reset();
}

void ScheduleListMdl::setMinRowHeight(int h)
{
    beginResetModel();
    d_minRowHeight = h;
    endResetModel();
}

