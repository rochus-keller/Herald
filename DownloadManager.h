#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

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
#include <Udb/Obj.h>

class PopClient;

namespace He
{
    class DownloadManager : public QObject
    {
        Q_OBJECT
    public:
        explicit DownloadManager(const Udb::Obj& inbox, QObject *parent);
        bool fetchMail();
        void quit();
        bool isIdle() const { return d_curAccount.isNull(); }
    signals:
        void sigError( const QString&);
        void sigStatus( const QString& );
    protected slots:
        void onMessageAvailable();
        void onPopStatus( int, int);
        void onPopError( int, const QString& );
    protected:
        void startNextAccount( bool startOver );
        void checkForQuit();
    private:
        PopClient* d_pop;
        Udb::Obj d_inbox;
        Udb::Obj d_curAccount;
        bool d_quit;
    };
}

#endif // DOWNLOADMANAGER_H
