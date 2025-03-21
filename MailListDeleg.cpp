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

#include "MailListDeleg.h"
#include <QPainter>
#include <QApplication>
#include <Udb/Transaction.h>
#include <Oln2/OutlineUdbMdl.h>
#include "HeTypeDefs.h"
#include "MailObj.h"
using namespace He;

MailListDeleg::MailListDeleg(QObject *parent) :
    QAbstractItemDelegate(parent)
{
}

void MailListDeleg::paint ( QPainter * painter, const QStyleOptionViewItem & option,
							const QModelIndex & index ) const
{
	painter->save();
    if (option.showDecorationSelected && (option.state & QStyle::State_Selected))
	{
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;

        painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
		//painter->setOpacity( 0.8 );
	}
    paintItem( painter, option, index );
    if( option.state & QStyle::State_HasFocus )
	{
		QStyleOptionFocusRect o;
		o.QStyleOption::operator=(option);
		o.state |= QStyle::State_KeyboardFocusChange;
		QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
								  ? QPalette::Normal : QPalette::Disabled;
		o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
												 ? QPalette::Highlight : QPalette::Window);
		QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
	}
    painter->restore();
}

void MailListDeleg::paintItem(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Zeilenhöhe
    const int lineHeight = option.fontMetrics.height() + 4;
	// Left Indent für Anzeige Ikonen
	const int leftIndent = 16;
	// Breite des Datumsbereichs
	const int dateWidth = option.fontMetrics.width( "000000-0000 " );

    painter->setPen( Qt::black );
    if( option.state & QStyle::State_Selected )
    {
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        painter->setPen( option.palette.color( cg, QPalette::HighlightedText ) );
    }

    const QRect rDate = option.rect.adjusted( option.rect.width() - dateWidth, 0, 0, -lineHeight );
	painter->drawText( rDate.adjusted( 2, 0, -2, -1 ), Qt::AlignLeft | Qt::AlignVCenter,
		index.data( MailListMdl::Sent ).toDateTime().toString( "yyMMdd-hhmm" ) );

    QVariant pix2 = index.data( MailListMdl::Pixmap2 );

    const QRect rName = option.rect.adjusted( leftIndent + 1, 0, -rDate.width() -
                                              ((pix2.isNull())?0:leftIndent), -lineHeight - 1 );
	const QFont f = painter->font();
	QFont bf = f;
	bf.setBold( true );
	painter->setFont( bf );
	painter->drawText( rName, Qt::AlignLeft | Qt::AlignVCenter,
                       painter->fontMetrics().elidedText( index.data( MailListMdl::Name ).toString(),
                                                          Qt::ElideRight, rName.width() ) );
	painter->setFont( f );

    const QRect rSubj = option.rect.adjusted( leftIndent, lineHeight, 0, 0 );
	painter->drawText( rSubj.adjusted( 1, - 2, 0, -1 ), Qt::AlignLeft | Qt::AlignVCenter,
                       painter->fontMetrics().elidedText( index.data( MailListMdl::Subject ).toString(),
                       Qt::ElideRight, rSubj.width() ) );

	painter->drawPixmap( option.rect.topLeft(),
                         index.data( MailListMdl::Pixmap1 ).value<QPixmap>() );

    QRect rPix( option.rect.topLeft(), QSize( leftIndent, lineHeight ) );
    rPix.translate( 0, lineHeight );

    if( !pix2.isNull() )
        painter->drawPixmap( rPix.topLeft(),
                         index.data( MailListMdl::Pixmap2 ).value<QPixmap>() );

    const int attCount = index.data( MailListMdl::AttCount ).toInt();
    if( attCount > 0 )
    {
        QRect r( option.rect.topLeft(), QSize( leftIndent, lineHeight ) );
        if( pix2.isNull() )
            r.translate( 2, lineHeight );
        else
            r.translate( rName.width() + ( leftIndent * 1.4 ), 0 );
        painter->drawPixmap( r.topLeft(), QPixmap(":/images/paper-clip-small.png" ) );
        painter->setFont( QFont( "Small Fonts", 6 ) );
        painter->drawText( r, Qt::AlignTop | Qt::AlignLeft, QString::number( attCount ) );
    }

    painter->setPen( Qt::gray );
	painter->drawLine( option.rect.bottomLeft(), option.rect.bottomRight() );
}

QSize MailListDeleg::sizeHint ( const QStyleOptionViewItem & option,
								const QModelIndex & index ) const
{
    return QSize( 0, 2 * option.fontMetrics.height() + 8 );
}

Udb::Obj MailListMdl::getObject(const QModelIndex &index) const
{
    Udb::OID oid = getOid( index );
    if( oid == 0 )
        return Udb::Obj();
    else
        return getTxn()->getObject( oid );
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

QVariant MailListMdl::data(const QModelIndex &index, int role) const
{
    if( getTxn() == 0 )
        return QVariant();
    if( role != Qt::DisplayRole && role != Qt::ToolTipRole && role < Name )
        return QVariant();
    Udb::Obj obj = getTxn()->getObject( getOid( index ) );
    MailObj mail;
    Udb::Obj party;
    if( HeTypeDefs::isParty( obj.getType() ) )
    {
        mail = obj.getParent();
        party = obj;
    }else if( HeTypeDefs::isEmail( obj.getType() ) )
        mail = obj;
    if( mail.isNull() )
        return QVariant();
    switch( role )
    {
    case Qt::DisplayRole:
        break;
    case Qt::ToolTipRole:
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
        break;
    case Name:
        if( mail.getType() == TypeInboundMessage )
            return mail.getFrom(true).d_name;
        else
            return _formatAddrs( mail.getTo(true), false );
    case Subject:
        return mail.getString( AttrText );
    case Sent:
        return mail.getValue( AttrSentOn ).getDateTime().toLocalTime();
    case AttCount:
        return mail.getValue( AttrAttCount ).getUInt32();
    case Pixmap1:
        return Oln::OutlineUdbMdl::getPixmap( mail.getType() );
    case Pixmap2:
        if( !party.isNull() )
            return Oln::OutlineUdbMdl::getPixmap( party.getType() );
        break;
    }
    return QVariant();
}

bool MailListMdl::filtered(Udb::OID oid)
{
    Udb::Obj o = getTxn()->getObject( oid );
    if( o.isNull(true,true) )
        return true;
    return d_typeFilter.contains( o.getType() ) || d_typeFilter.contains( o.getParent().getType() );
}

void MailListMdl::setTypeFilter(const MailListMdl::TypeFilter & f)
{
    d_typeFilter = f;
    refill();
}
