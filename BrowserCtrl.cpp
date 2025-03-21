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

#include "BrowserCtrl.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextBrowser>
#include <QtDebug>
#include "HeraldApp.h"
using namespace He;

BrowserCtrl::BrowserCtrl(QWidget *parent) :
    QObject(parent)
{
}

BrowserCtrl *BrowserCtrl::create(QWidget *parent)
{
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 1 );

    BrowserCtrl* ctrl = new BrowserCtrl( pane );

    ctrl->d_title = new QLabel( pane );
    ctrl->d_title->setMinimumWidth( 100 );
    ctrl->d_title->setTextInteractionFlags( Qt::TextSelectableByMouse );
    ctrl->d_title->setWordWrap(true);
    ctrl->d_title->setFrameStyle( QFrame::Panel | QFrame::StyledPanel );
	ctrl->d_title->setLineWidth( 1 );
    ctrl->d_title->setAutoFillBackground( true );
    ctrl->d_title->setBackgroundRole( QPalette::Light );
    QFont f = ctrl->d_title->font();
	f.setBold( true );
	ctrl->d_title->setFont( f );
	ctrl->d_title->setSizePolicy( QSizePolicy::Expanding, QSizePolicy::Minimum );
    ctrl->d_title->updateGeometry();
    vbox->addWidget( ctrl->d_title );

    ctrl->d_text = new QTextBrowser( pane );
    ctrl->d_text->setOpenLinks( false );
    ctrl->d_text->setOpenExternalLinks(false);
    vbox->addWidget( ctrl->d_text );

    connect( ctrl->d_text, SIGNAL(anchorClicked(QUrl)), ctrl, SLOT(onAnchorClicked(QUrl)) );

    return ctrl;
}

void BrowserCtrl::setTitleText(const QString &title, const QString &text)
{
    d_title->setText( title );
    d_text->setText( text );
}

QWidget *BrowserCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

void BrowserCtrl::onAnchorClicked(const QUrl & url)
{
    if( url.scheme().compare( QLatin1String("mailto"), Qt::CaseInsensitive ) == 0 )
    {
        const QByteArray fixedAddr = url.path().toLower().trimmed().toLatin1();
        emit sigActivated( fixedAddr );
    }else if( url.isRelative() )
    {
        QString anch = url.toString();
        if( anch.startsWith( QChar('#') ) )
            anch = anch.mid(1);
        d_text->scrollToAnchor( anch );
    }else
        HeraldApp::openUrl( url );
}
