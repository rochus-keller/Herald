#ifndef BROWSERCTRL_H
#define BROWSERCTRL_H

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

#include <QObject>

class QTextBrowser;
class QLabel;
class QUrl;

namespace He
{
    class BrowserCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit BrowserCtrl(QWidget *parent = 0);
        static BrowserCtrl* create(QWidget* parent );
        void setTitleText( const QString& title, const QString& text );
        QWidget* getWidget() const;
    signals:
        void sigActivated( const QByteArray& emailAddr );
    protected slots:
        void onAnchorClicked(const QUrl & url);
    private:
        QLabel* d_title;
        QTextBrowser* d_text;
    };
}

#endif // BROWSERCTRL_H
