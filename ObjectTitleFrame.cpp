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

#include "ObjectTitleFrame.h"
#include <Txt/Styles.h>
#include <Oln2/OutlineUdbMdl.h>
#include <QtGui/QPainter>
#include <QtGui/QMouseEvent>
#include "HeTypeDefs.h"
using namespace He;

// 1:1 aus WorkTree

ObjectTitleFrame::ObjectTitleFrame(QWidget *parent) :
    QFrame(parent)
{
    setFrameStyle( QFrame::Panel | QFrame::StyledPanel );
	setLineWidth( 1 );
	adjustFont();
	setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );
}

void He::ObjectTitleFrame::adjustFont()
{
    QFont f = font(); //Txt::Styles::inst()->getFont();
	f.setBold( true );
	setFont( f );
    updateGeometry();
}

void ObjectTitleFrame::setObj(const Udb::Obj & o)
{
    d_obj = o;
	if( sizeHint().height() != height() )
		updateGeometry();
    if( d_obj.isNull() )
        setToolTip( QString() );
    else
        setToolTip( QString( "%1: %2" ).
                             arg( HeTypeDefs::prettyName( d_obj.getType(), d_obj.getTxn() ) ).
                             arg( HeTypeDefs::formatObjectTitle( d_obj ) ) );
	update();
}

QSize He::ObjectTitleFrame::sizeHint() const
{
    int h = 0;
	if( !d_obj.isNull() )
	{
		QPixmap pix = Oln::OutlineUdbMdl::getPixmap( d_obj.getType() );
		if( !pix.isNull() )
			h = pix.height() + 2; // je eins oben und unten
	}
	return QSize( 16, qMax( h, fontMetrics().height() ) + 2 * frameWidth() );
}

void He::ObjectTitleFrame::paintEvent(QPaintEvent *event)
{
    QFrame::paintEvent( event );
	QPainter p(this);

	QRect r = contentsRect();
	QString str = tr("nothing selected");
	if( !d_obj.isNull() )
	{
		str = HeTypeDefs::formatObjectTitle( d_obj );
		p.fillRect( r, Qt::white );
		QPixmap pix = Oln::OutlineUdbMdl::getPixmap( d_obj.getType() );
		if( !pix.isNull() )
		{
			p.drawPixmap( r.left() + 1, r.top() + 1, pix );
			r.adjust( pix.width() + 1, 0, 0, 0 );
		}
	}else
		p.setPen( Qt::gray );
	r.adjust( 2, 0, -2, 0 );
	p.drawText( r, Qt::AlignLeft | Qt::AlignVCenter |
                Qt::TextSingleLine, fontMetrics().elidedText( str, Qt::ElideRight, r.width() ) );
}

void ObjectTitleFrame::mouseDoubleClickEvent(QMouseEvent *event)
{
    if( event->button() == Qt::LeftButton )
        emit signalDblClicked();
}

void ObjectTitleFrame::mousePressEvent(QMouseEvent *event)
{
    if( event->button() == Qt::LeftButton )
        emit signalClicked();
}
