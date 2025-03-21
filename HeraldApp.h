#ifndef HERALDAPP_H
#define HERALDAPP_H

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
#include <QSettings>
#include <QUuid>
#include <Txt/Styles.h>
#include <Udb/Obj.h>
#include <QSet>

class QTextBrowser;

namespace He
{
    class EmailMainWindow;

    class HeraldApp : public QObject
    {
        Q_OBJECT
    public:
        static const char* s_company;
        static const char* s_domain;
        static const char* s_appName;
        static const QUuid s_rootUuid;
        static const char* s_version;
        static const char* s_date;
        static const char* s_extension;
        static const QUuid s_inboxUuid;
        static const char* s_addressIndex;
        static const QUuid s_accounts;
        static const QUuid s_drafts;
        static const QUuid s_outlines;

        explicit HeraldApp(QObject *parent = 0);
        ~HeraldApp();

        bool open(const QString&);
        static HeraldApp* inst();
        QSettings* getSet() const { return d_set; }
        const QList<EmailMainWindow*>& getDocs() const { return d_docs; }
        void setAppFont( const QFont& f );
        void addLock( const Udb::Obj& o ){ d_locks.insert(o); }
        void removeLock( const Udb::Obj& o ){ d_locks.remove(o); }
        bool isLocked( const Udb::Obj& o ) const { return d_locks.contains(o); }
        void setWindowGeometry( QWidget* );
        QTextBrowser* openHtmlBrowser( const QString& html, const QString& title = QString() );
        static void openUrl( const QUrl& );
        static QString adjustPath( QString path );
        QIcon getIconFromPath( const QString& path );
    public slots:
        void onOpen(const QString & path);
        void onClose();
    protected slots:
        void onHandleXoid( const QUrl& );
    protected:
        static QString parseFilePath( const QString& );
        static quint64 parseOid( const QString& );
    private:
        QList<EmailMainWindow*> d_docs;
        QSettings* d_set;
        QString d_dbPath;
		Txt::Styles* d_styles;
        QSet<Udb::Obj> d_locks;
        int d_lastWindowX;
   };
}

#endif // HERALDAPP_H
