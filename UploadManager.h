#ifndef UPLOADMANAGER_H
#define UPLOADMANAGER_H

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
#include <Udb/Transaction.h>
#include <Udb/Obj.h>

class SmtpClient;
class MailMessage;

namespace He
{
    class UploadManager : public QObject
    {
        Q_OBJECT
    public:
        explicit UploadManager(Udb::Transaction*, QObject *parent = 0);
        bool resendMessage( const QString& path, const Udb::Obj& identity, const QStringList& to );
		bool sendFromFile( const QString& path );
		bool sendMessage( MailMessage* ); // uebernimmt Ownership
		bool resendTo( const Udb::Obj& draftParty );
        bool isIdle() const;
        void sendAll();
        static bool renderMessageTo( const Udb::Obj &mail, MailMessage*, bool checkAttExists );
    signals:
        void sigError( const QString&);
        void sigStatus( const QString& );
    public slots:
        void onSendMessage( quint64 draftOid );
    protected slots:
        void onUpdateStatus(int statusCode, int number, QString internalId );
        void onNotifyError( int statusCode, const QString& msg );
        void onUploadCommitted( MailMessage* );
    protected:
        bool renderDraftTo( const Udb::Obj &draft, MailMessage* );
        bool sendMessage( Udb::Obj );
        bool sendParty( Udb::Obj );
        bool setAccountFromIdentity( const Udb::Obj& id );
		bool renderMessageTo( const Udb::Obj &mail, MailMessage*, const Udb::Obj &draftParty );
    private:
        Udb::Transaction* d_txn;
        SmtpClient* d_smtp;
        QList<Udb::OID> d_queue;
    };
}

#endif // UPLOADMANAGER_H
