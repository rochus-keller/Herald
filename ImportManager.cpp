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

#include "ImportManager.h"
#include <QFile>
#include <QBuffer>
#include <QDir>
#include <QtDebug>
#include <QApplication>
#include <Mail/MailMessage.h>
#include <Udb/Idx.h>
#include <Udb/Extent.h>
#include "MailObj.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include "ObjectHelper.h"
using namespace He;

#define IMPORT_SOURCE_PATH "<set to path>"

static inline bool isMailSeparator( const QByteArray& line )
{
    // Folgender Code findet den Separator, den Poco vor jede Mail setzt
    return qstrncmp( line, "From ???@??? Sun Apr 18 12:34:56 1999", 37 ) == 0;
}

ImportManager::ImportManager(Udb::Transaction * txn, QObject *parent) :
    QObject(parent),d_txn(txn)
{
    Q_ASSERT( txn != 0 );
}

bool ImportManager::importMbx(const QString &path, bool inbound)
{
    QFile in( path );
    if( !in.open( QIODevice::ReadOnly ) )
        return false;

    Udb::Obj inbox = d_txn->getOrCreateObject( HeraldApp::s_inboxUuid, TypeInbox );
    int count = 0;
    int msgNumber = 1;
    while( !in.atEnd() )
    {
        QByteArray buf;
        eatNextMail( in, buf );
        if( !buf.isEmpty() )
        {
            MailMessage msg;
            msg.fromRFC822( LongString( buf ) ); // TODO: Errors?
            QDateTime dt = MailMessage::parseRfC822DateTime( msg.header( "Delivery-Date" ) );
            if( dt.isValid() )
                msg.setReceived( dt );

			MailObj mail = MailObj::acceptInOrOutbound( d_txn, &msg, inbound );
            if( !mail.isNull() )
            {
                MailMessage::StringList l = msg.headers( "X-Poco-Attachment" );
                foreach( QByteArray path, l )
                {
                    QFileInfo info( path );
                    MailObj::createAttachment( mail, path, info.fileName(), inbound, false );
                }

                // inbox.appendSlot( mail ); // TEST
                inbox.commit();
                count++;
//                QFile file( QDir( ObjectHelper::getInboxPath( d_txn ) ).absoluteFilePath(
//                               QString("%1.eml").arg( mail.getString( AttrInternalId ) ) ) );
//                file.open( QIODevice::WriteOnly );
//                file.write( buf.data() );
            }else
            {
                QFile file( QDir( ObjectHelper::getInboxPath( d_txn ) ).absoluteFilePath(
                                QUuid::createUuid().toString() ) );
                file.open( QIODevice::WriteOnly );
                file.write( buf.data() );
                emit sigError( tr("Error importing message '%1'").arg(msgNumber) );
            }
        }
        msgNumber++;
    }
    emit sigStatus( tr("Finished importing %1 messages").arg(count) );
    return true;
}

void ImportManager::eatNextMail(QFile &in, QByteArray &out)
{
    bool utf8 = false;
    bool headerDone = false;
    while( !in.atEnd() )
    {
        QByteArray line = in.readLine();
        if( isMailSeparator( line ) )
            return;
        else
        {
            out.append( line );
            if( !headerDone )
            {
                if( qstrnicmp( line, "Content-Type:", 13 ) == 0 )
                {
                    const int pos = line.indexOf( "charset=" );
                    if( pos != -1 )
                        utf8 = ( qstrnicmp( line.data() + pos + 9, "utf-8", 5 ) == 0 );
                }else if( line == "\r\n" )
                {
                    headerDone = true;
                    if( utf8 )
                        out = QString::fromUtf8( out ).toLatin1();
                }
            }
        }
    }
}

bool ImportManager::importEmlDir(const QString &path, bool inbound )
{
    QDir dir( path );
    QStringList files = dir.entryList( QStringList() << "*.eml",
                                       QDir::Files | QDir::Readable );
    Udb::Obj inbox = d_txn->getOrCreateObject( HeraldApp::s_inboxUuid, TypeInbox );
    int count = 0;
    foreach( QString fileName, files )
    {
        emit sigStatus(tr("Importing file '%1'").arg(fileName) );
        QApplication::processEvents();
        MailObj mail;
        {
            MailMessage msg;
            msg.fromRFC822( LongString( dir.absoluteFilePath( fileName ), false ) ); // TODO: errors
            // Poco kodiert doppelt UTF-8 im Header. Darum hier rückgängig machen.
            const int pos = msg.contentType().indexOf("charset=");
            if( pos != 0 && msg.contentType().toLower().contains("utf-8") )
            {
                // RISK: bei einem Teil der importierten Mails war es bereits korrekt, bzw. die folgende
                // Massnahme ist kontraproduktiv.
                msg.setSubject( QString::fromUtf8( msg.subject().toLatin1() ) );
            }

            QDateTime dt = MailMessage::parseRfC822DateTime( msg.header( "Delivery-Date" ) );
            if( dt.isValid() )
                msg.setReceived( dt );
			mail = MailObj::acceptInOrOutbound( d_txn, &msg, inbound );
            if( !mail.isNull() )
            {
                MailMessage::StringList l = msg.headers( "X-Poco-Attachment" );
                foreach( QByteArray path, l )
                {
                    QFileInfo info( path );
                    MailObj::createAttachment( mail, path, info.fileName(), inbound, false );
                }
                //inbox.appendSlot( mail ); // TEST
            }
        }
        if( !mail.isNull() )
        {
            dir.rename( fileName, fileName + QChar('_' ) ); // Macht Kopie!
            mail.setString( mail.getAtom( "Source" ), fileName );
            d_txn->commit();
            count++;
        }else
        {
            d_txn->rollback();
            emit sigError( tr("Error importing '%1'").arg(fileName) );
        }
    }
    emit sigStatus( tr("Finished importing %1 messages").arg(count) );
    return true;
}

void ImportManager::fixOutboundAttachments(Udb::Transaction * txn)
{
    // Irrtümlich auch gesendete Dokumente mit Import übernommen. 6834 Dokumente total. 1.46 GB auf HD
    // Alle loswerden ausser Attached Message.eml oder andere die nach IMPORT_SOURCE_PATH zeigen

    QFile moved( "moved.txt");
    moved.open( QIODevice::WriteOnly );
    QSet<QString> done;

    QDir barca( IMPORT_SOURCE_PATH );
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( mail.getType() == TypeOutboundMessage )
        {
            MailObj::Attachments atts = mail.getAttachments();
            QMap<QString,Udb::Obj> fileNames;
            foreach( AttachmentObj a, atts )
            {
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                fileNames[ doc.getString( AttrText ) ] = doc;
            }
            MailMessage m;
            m.fromRFC822( LongString( mail.getValue(AttrRawHeaders).getArr() ) );
            MailMessage::StringList l = m.headers( "X-Poco-Attachment" );
            // Ich habe verifiziert, dass pro Mail die fileName alle unique sind
            // Ich habe verifiziert, dass bei keinem einzigen Document AttrFilePath gesetzt ist
            foreach( QByteArray path, l )
            {
                QFileInfo info( path );
                if( info.absoluteDir() != barca )
                {
                    Udb::Obj doc = fileNames.value( info.fileName() );
                    if( doc.isNull() )
                        qDebug() << "No Document exists for file" << path;
                    else
                    {
                        // Wir haben das zugehörige Dokument gefunden
                        if( !doc.hasValue( AttrFilePath ) )
                        {
                            // Das Dokument wurde noch nicht repariert
                            const QString wrongPath = AttachmentObj::getFilePath( doc );
                            const QString rightPath = info.absoluteFilePath();
                            QFile f(wrongPath);
                            if( f.exists() )
                            {
                                if( f.rename( rightPath ) )
                                {
                                    doc.setString( AttrFilePath, rightPath );
                                    moved.write( rightPath.toLatin1() );
                                    done.insert( rightPath );
                                    doc.commit();
                                }else
                                    qDebug() << "Cannot rename" << wrongPath << "to" << rightPath;
                            }else
                            {
                                // Die Datei existiert nicht mehr; bereits verschoben oder verloren
                                if( !done.contains( rightPath ) )
                                    qDebug() << "Missing source file" << wrongPath << "for" << rightPath;
                            }
                        }
                    }
                }
            }
        }
    }while( idx.next() );
}

void ImportManager::checkFilesExist()
{
    QFile out("missing.txt");
    out.open(QIODevice::WriteOnly );
    QFile in("files.txt");
    in.open(QIODevice::ReadOnly);
    while( !in.atEnd() )
    {
        QFileInfo info( QString::fromLatin1( in.readLine().trimmed() ) );
        if( !info.exists() )
        {
            out.write( info.filePath().toLatin1() );
            out.write( "\n");
        }else
            qDebug() << "found" << info.filePath();
    }
}

void ImportManager::checkLocalOutboundFiles(Udb::Transaction* txn)
{
    QDir barca( IMPORT_SOURCE_PATH );
    QFile found( "found.txt");
    found.open(QIODevice::WriteOnly);
    QFile notfound( "notfound.txt");
    notfound.open( QIODevice::WriteOnly );
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( mail.getType() == TypeOutboundMessage )
        {
            MailMessage m;
            m.fromRFC822( LongString( mail.getValue(AttrRawHeaders).getArr() ) );
            MailMessage::StringList l = m.headers( "X-Poco-Attachment" );
            QMap<QString,QString> paths;
            foreach( QByteArray path, l )
            {
                QFileInfo info( path );
                paths[info.fileName()]=info.filePath();
            }
            MailObj::Attachments atts = mail.getAttachments();
            foreach( AttachmentObj a, atts )
            {
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                if( !doc.hasValue( AttrFilePath ) )
                {
                    QString path = paths.value( doc.getString( AttrText ).trimmed() );
                    QFileInfo wrongPathInfo( AttachmentObj::getFilePath(doc) );
                    if( !path.isEmpty() )
                    {
                        if( path[0] == QChar('"') )
                            path = path.mid( 1, path.size() - 2 );
                        QFileInfo rightPathInfo( path );
                        if( rightPathInfo.absoluteDir() != barca )
                        {
                            found.write( wrongPathInfo.fileName().toLatin1() );
                            found.write( "\t");
                            if( wrongPathInfo.exists() )
                                found.write( "true" );
                            else
                                found.write( "false" );
                            found.write( "\t");
                            if( rightPathInfo.exists() )
                                found.write( "true" );
                            else
                            {
                                found.write( "false" );
                                qDebug() << "Missing :" << path << ":";
                            }
                            found.write( "\t");
                            found.write( path.toLatin1() );
                            found.write("\n");
                        }
                    }else
                    {
                        notfound.write( wrongPathInfo.fileName().toLatin1() );
                        notfound.write( "\t");
                        foreach( QString s, paths.keys() )
                        {
                            notfound.write( s.toLatin1() );
                            notfound.write( "\t");
                        }
                        notfound.write("\n");
                    }
                }
            }
        }
    }while( idx.next() );
}

void ImportManager::fixOutboundAttachments2(Udb::Transaction *txn)
{
    // Nach Reparatur noch 5343 Dateien in docstore mit 1 GB; also 1491 Dateien bzw. 460 MB repariert.
    QDir barca( IMPORT_SOURCE_PATH );
    QFile found( "found.txt");
    found.open(QIODevice::WriteOnly);
    QFile notfound( "notfound.txt");
    notfound.open( QIODevice::WriteOnly );
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( mail.getType() == TypeOutboundMessage )
        {
            MailMessage m;
            m.fromRFC822( LongString( mail.getValue(AttrRawHeaders).getArr() ) );
            MailMessage::StringList l = m.headers( "X-Poco-Attachment" );
            QMap<QString,QString> paths;
            foreach( QByteArray path, l )
            {
                QFileInfo info( path );
                paths[info.fileName()]=info.filePath();
            }
            MailObj::Attachments atts = mail.getAttachments();
            foreach( AttachmentObj a, atts )
            {
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                if( !doc.hasValue( AttrFilePath ) )
                {
                    QString path = paths.value( doc.getString( AttrText ).trimmed() );
                    QFileInfo locinfo( AttachmentObj::getFilePath(doc) );
                    if( !path.isEmpty() )
                    {
                        if( path[0] == QChar('"') )
                            path = path.mid( 1, path.size() - 2 );
                        QFileInfo pinfo( path );
                        if( pinfo.absoluteDir() != barca )
                        {
                            const bool lExists = locinfo.exists();
                            const bool pExists = pinfo.exists();
                            const QString wrongPath = locinfo.filePath();
                            const QString rightPath = pinfo.filePath();
                            if( lExists && !pExists )
                            {
                                QFile f(wrongPath);
                                if( f.rename( rightPath ) )
                                {
                                    doc.setString( AttrFilePath, rightPath );
                                    doc.commit();
                                }else
                                    qDebug() << "Cannot rename" << wrongPath << "to" << rightPath;
                            }else if( lExists && pExists )
                            {
                                QFile f(wrongPath);
                                if( f.remove() )
                                {
                                    doc.setString( AttrFilePath, rightPath );
                                    doc.commit();
                                }else
                                    qDebug() << "Cannot remove" << wrongPath;
                            }else if( !lExists && pExists )
                            {
                                doc.setString( AttrFilePath, rightPath );
                                doc.commit();
                            }else
                            {
                                // Ich habe verifiziert, dass diese Dateien entweder nicht mehr existieren
                                // oder nach dem senden verschoben wurden. Diese waren also bereits beim
                                // fehlerhaften Import nicht mehr zugänglich.
                                // Wir setzen also trotzdem den Pfad
                                doc.setString( AttrFilePath, rightPath );
                                doc.commit();
                                notfound.write( locinfo.fileName().toLatin1() );
                                notfound.write( "\t");
                                notfound.write( path.toLatin1() );
                                notfound.write("\n");
                            }
                        }
                    }else
                    {
                        notfound.write( locinfo.fileName().toLatin1() );
                        notfound.write( "\t");
                        foreach( QString s, paths.keys() )
                        {
                            notfound.write( s.toLatin1() );
                            notfound.write( "\t");
                        }
                        notfound.write("\n");
                    }
                }
            }
        }
    }while( idx.next() );
}

void ImportManager::checkDocumentAvailability(Udb::Transaction *txn)
{
    // 2014-05-14 filesystem 4008 docstore 640 found 3712 missing 936 für Outbound
    // 2014-05-14 filesystem 4091 docstore 7028 found 9175 missing 1944 für In und Outbound
    QFile found( "found.txt");
    found.open(QIODevice::WriteOnly);
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    int fs = 0; int ds = 0; int ok = 0; int nok = 0;
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( HeTypeDefs::isEmail( mail.getType() ) )
        {
            MailObj::Attachments atts = mail.getAttachments();
            foreach( AttachmentObj a, atts )
            {
                QFileInfo info( a.getDocumentPath() );
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                if( !doc.hasValue( AttrFilePath ) )
                {

                }
                found.write( doc.getString(AttrInternalId).toLatin1() );
                found.write( "\t");
                const bool inFs = doc.hasValue( AttrFilePath );
                if( inFs )
                {
                    fs++;
                    found.write( "filesystem" );
                }else
                {
                    ds++;
                    found.write( "docstore" );
                }
                found.write( "\t");
                if( info.exists() )
                {
                    ok++;
                    found.write( "\t");
                    if( inFs )
                        found.write( info.filePath().toLatin1() );
                    else
                        found.write( doc.getString( AttrText ).toLatin1() );
                }else
                {
                    nok++;
                    found.write( "missing" );
                    found.write( "\t");
                    found.write( info.filePath().toLatin1() );
                }
                found.write( "\t");
                found.write( doc.getValue(AttrFileHash).getArr().toBase64() );
                found.write("\n");
            }
        }
    }while( idx.next() );
    qDebug() << "filesystem" << fs << "docstore" << ds << "found" << ok << "missing" << nok;
}

bool _lessThan(const QPair<QByteArray,Udb::Obj> &s1, const QPair<QByteArray,Udb::Obj> &s2)
{
    int res = ::memcmp( s1.first.data(), s2.first.data(), qMin( s1.first.size(), s2.first.size() ) );
    if( res == 0 )
        return s1.first.size() < s2.first.size();
    else
        return res < 0;
}

void ImportManager::checkDocumentHash(Udb::Transaction *txn)
{
#ifdef _v1
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    QSet<Udb::OID> test;
    typedef QPair<QByteArray,Udb::Obj> Pair;
    QList<Pair> list; // hash -> obj
    qDebug() << "****** Search";
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( HeTypeDefs::isEmail( mail.getType() ) )
        {
            MailObj::Attachments atts = mail.getAttachments();
            foreach( AttachmentObj a, atts )
            {
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                if( !test.contains( doc.getOid() ) )
                {
                    list.append( qMakePair( doc.getValue(AttrFileHash).getArr(), doc ) );
                    test.insert( doc.getOid() );
                }
            }
        }
    }while( idx.next() );
    qDebug() << "****** Sort";
    qSort( list.begin(), list.end(), _lessThan );
    QPair<QByteArray,Udb::Obj> p0;
    QStringList ids;
    qDebug() << "****** Group";
    foreach( Pair p, list )
    {
        if( _lessThan( p, p0 ) || _lessThan( p0, p ) )
        {
            if( ids.size() > 1 )
                qDebug() << ids.join( ", " );
            ids.clear();
            p0 = p;
        }
        ids.append( p.second.getString( AttrInternalId ) );
    }
    qDebug() << "****** End";
#else
    int count = 0;
    Udb::Idx idx( txn, IndexDefs::IdxFileHash );
    if( idx.first() ) do
    {
        Udb::Obj doc = txn->getObject( idx.getOid() );
        Q_ASSERT( !doc.isNull() );
        QStringList twins;
        if( idx.seek( doc.getValue( AttrFileHash ) ) ) do
        {
            twins.append( txn->getObject( idx.getOid() ).getString( AttrInternalId ) );
            count++;
        }while( idx.nextKey() );
        if( twins.size() > 1 )
        {
            qDebug() << twins.join( ", " );
        }
    }while( idx.isOnIndex() );
    // 8524 indexed docs
    qDebug() << "indexed docs" << count;
#endif
}

bool _equalHash(const QByteArray&s1, const QByteArray&s2)
{
    if( s1.size() == 0 && s2.size() == 0 )
        return true;
    int res = ::memcmp( s1.data(), s2.data(), qMin( s1.size(), s2.size() ) );
    if( res == 0 )
        return s1.size() == s2.size();
    else
        return false;
}

void ImportManager::fixDocumentHash(Udb::Transaction *txn)
{
    Udb::Idx idx( txn, IndexDefs::IdxSentOn );
    QSet<Udb::OID> test;
    if( idx.first() ) do
    {
        MailObj mail = txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( HeTypeDefs::isEmail( mail.getType() ) )
        {
            MailObj::Attachments atts = mail.getAttachments();
            foreach( AttachmentObj a, atts )
            {
                Udb::Obj doc = a.getValueAsObj( AttrDocumentRef );
                Q_ASSERT( !doc.isNull() );
                if( !test.contains( doc.getOid() ) )
                {
                    QFileInfo info( a.getDocumentPath() );
                    test.insert( doc.getOid() );
                    if( info.exists() )
                    {
                        QByteArray hashNew = HeTypeDefs::calcHash( info.filePath() );
                        doc.setValue( AttrFileHash, Stream::DataCell().setLob( hashNew ) );
//                        QByteArray hashOld = doc.getValue(AttrFileHash).getArr();
//                        if( !_equalHash( hashNew, hashOld ) )
//                        {
//                            qDebug() << "wrong hash" << doc.getString(AttrInternalId) << hashNew.toBase64() << hashOld.toBase64();
//                        }
                    }
                }
            }
        }
    }while( idx.next() );
    txn->commit();
}

void ImportManager::fixDocRedundancy(Udb::Transaction *txn)
{
    int count = 0;
//    int noHashCount = 0;
//    Udb::Extent ex(txn);
//    if( ex.first() ) do
//    {
//        Udb::Obj doc = ex.getObj();
//        if( doc.getType() == TypeDocument )
//        {
//            count++;
//            if( !doc.hasValue(AttrFileHash))
//                noHashCount++;
//        }
//    }while(ex.next());
//    qDebug() << "Docs" << count << "No Hash" << noHashCount;
    // Docs 10466 minus No Hash 1942 = 8524, QED
    count = 0;
    Udb::Idx idx( txn, IndexDefs::IdxFileHash );
    if( idx.first() ) do
    {
        Udb::Obj doc = txn->getObject( idx.getOid() );
        Q_ASSERT( !doc.isNull() );
        QList<Udb::Obj> twins;
        if( idx.seek( doc.getValue( AttrFileHash ) ) ) do
        {
            twins.append( txn->getObject( idx.getOid() ) );
            count++;
        }while( idx.nextKey() );
        if( twins.size() > 1 )
        {
            qDebug() << "**** Start twin";
            Udb::Obj winner;
            foreach( Udb::Obj o, twins )
            {
                if( o.hasValue(AttrFilePath) )
                {
                    winner = o;
                    break;
                }
            }
            if( winner.isNull() )
                winner = twins.first();
            if( !winner.hasValue(AttrFilePath) )
                qDebug() << "kept" << AttachmentObj::getFilePath( winner );
            for( int i = 1; i < twins.size(); i++ )
            {
                // alle ausser dem winner (first)
                if( !twins[i].hasValue(AttrFilePath) )
                {
                    // Entferne die lokale Datei
                    QFile f( AttachmentObj::getFilePath( twins[i] ) );
                    if( f.remove() )
                        qDebug() << "removed file" << f.fileName();
                    else
                        qDebug() << "cannot remove" << f.fileName();
                }
                Udb::Idx refs( txn, IndexDefs::IdxDocumentRef );
                if( refs.seek( twins[i] ) ) do
                {
                    Udb::Obj att = txn->getObject( refs.getOid() );
                    att.setValueAsObj( AttrDocumentRef, winner );
                }while( refs.nextKey() );
                qDebug() << "removed doc" << twins[i].getString( AttrInternalId );
                twins[i].erase();
            }
            txn->commit();
            qDebug() << "**** End twin";
        }else
        {
            Q_ASSERT( !twins.isEmpty() );
            if( !twins.first().hasValue(AttrFilePath) )
                qDebug() << "existing" << AttachmentObj::getFilePath( twins.first() );
        }
    }while( idx.isOnIndex() );
    // vorher 8524 indexed docs
    qDebug() << "indexed docs" << count;
    // nach Bereinigung Twins indexed docs 6874
    // ich habe verifiziert, dass die existing und kept in der Summe mit dem tatsächlichen Inhalt von docstore
    // übereinstimmen (ca. 5326, mit dir /B und UltraEdit Diff); Abweichung nur in ca. 3 unwichtigen Dateien
    // ich habe verifiziert, dass auch nach dem Twin-Fix alle Attachments auf eine Datei zeigen
}



