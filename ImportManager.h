#ifndef IMPORTMANAGER_H
#define IMPORTMANAGER_H

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
#include <QStringList>
#include <Udb/Transaction.h>

class QFile;
class QBuffer;

namespace He
{
    class ImportManager : public QObject
    {
        Q_OBJECT
    public:
        explicit ImportManager(Udb::Transaction*,QObject *parent = 0);
        bool importMbx( const QString& path, bool inbound );
        bool importEmlDir( const QString& path, bool inbound );
        static void fixOutboundAttachments( Udb::Transaction* );
        static void checkFilesExist();
        static void checkLocalOutboundFiles(Udb::Transaction *txn);
        static void fixOutboundAttachments2(Udb::Transaction *txn);
        static void checkDocumentAvailability(Udb::Transaction *txn);
        static void checkDocumentHash(Udb::Transaction *txn);
        static void fixDocumentHash(Udb::Transaction *txn);
        static void fixDocRedundancy(Udb::Transaction *txn);
    signals:
        void sigError( const QString&);
        void sigStatus( const QString& );
    protected:
        static void eatNextMail( QFile &in, QByteArray &out );
    private:
        Udb::Transaction* d_txn;
    };
}

#endif // IMPORTMANAGER_H
