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

#include "DownloadManager.h"
#include <Mail/PopClient.h>
#include <Mail/MailMessage.h>
#include <Mail/LongString.h>
#include <QFileInfo>
#include <QDir>
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <QtDebug>
#include "ObjectHelper.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
using namespace He;

DownloadManager::DownloadManager(const Udb::Obj &inbox, QObject *parent) :
    QObject(parent), d_inbox( inbox ),d_quit(false)
{
    Q_ASSERT( !inbox.isNull() );

    d_pop = new PopClient( this );
    d_pop->setInboxPath( ObjectHelper::getInboxPath( inbox.getTxn() ) );
    d_pop->setDelete( true );
    connect( d_pop, SIGNAL( messageAvailable() ), this, SLOT( onMessageAvailable() ) );
    connect( d_pop, SIGNAL(updateStatus(int,int)), this, SLOT( onPopStatus(int, int) ) );
    connect( d_pop, SIGNAL(notifyError(int,QString)), this, SLOT( onPopError(int, QString) ) );
}

bool DownloadManager::fetchMail()
{
    if( !d_curAccount.isNull() )
        return false; // wir sind gerade an einer Runde

    startNextAccount(true);
    return true;
}

void DownloadManager::quit()
{
    if( !d_curAccount.isNull() )
    {
        d_quit = true;
    }
}

void DownloadManager::onMessageAvailable()
{
    while( d_pop->hasMails() )
    {
        QString path = d_pop->fetchMail();
        MailMessage msg;
        LongString ls( path, false );
        msg.fromRFC822( ls );
        Udb::Obj::ValueList array(InboxArrayLen, Stream::DataCell().setNull() );
        // setNull, Ansonsten schreibt packarray die Zellen nicht raus
        array[InboxFilePath].setString( path );
        array[InboxFrom].setString( msg.from() );
        array[InboxSubject].setString( msg.subject() );
        array[InboxDateTime].setDateTime( msg.dateTime().toLocalTime() );
        array[InboxAttCount].setUInt16( msg.messagePartCount() );
        array[InboxPopAcc] = d_curAccount;
        d_inbox.appendSlot( Udb::Obj::packArray( array ) );
        d_inbox.commit();
    }
}

void DownloadManager::onPopStatus(int code, int msg)
{
    Q_ASSERT( code >= 0 );
    Q_ASSERT( !d_curAccount.isNull() );
    switch( code )
    {
    case PopClient::DnsLookup:
        emit sigStatus( tr("Start fetching from '%1'").arg( d_curAccount.getString( AttrText ) ) );
        break;
    case PopClient::StartToRetreive:
        emit sigStatus( tr("Found %2 messages on '%1'").
                        arg( d_curAccount.getString( AttrText ) ).arg( msg ) );
        break;
    case PopClient::DownloadComplete:
        emit sigStatus( tr("Finished fetching from '%1'").arg( d_curAccount.getString( AttrText ) ) );
        startNextAccount(false);
        break;
//    default: // Das führt zu unötigen Log-Einträgen:
//        emit sigStatus( tr("Unknown Status %1 %2").arg( code ).arg( msg ) );
    }
}

void DownloadManager::onPopError(int code, const QString & msg)
{
    Q_ASSERT( code < 0 );
    Q_ASSERT( !d_curAccount.isNull() );
    switch( code )
    {
    case PopClient::UnknownResponse:
        emit sigError( tr("POP3 protocol error while fetching from '%1': %2").arg(
                           d_curAccount.getString( AttrText ) ).arg(msg) );
        break;
    case PopClient::LoginFailed:
        emit sigError( tr("Login failed with '%1': %2").arg( d_curAccount.getString( AttrText ) ).arg(msg) );
        break;
    case PopClient::FileSystemFull:
    case PopClient::CannotOpenBuffer:
        emit sigError( tr("Local file system error fetching from '%1'").arg( d_curAccount.getString( AttrText ) ) );
        break;
    case PopClient::SocketError:
        emit sigError( tr("Network error %2 while fetching from '%1'").
                         arg( d_curAccount.getString( AttrText ) ).arg( msg ) );
        break;
    case PopClient::UnknownError:
        emit sigError( tr("Unknown error while fetching from '%1': %2").
                       arg( d_curAccount.getString( AttrText ) ).arg(msg) );
        break;
    default:
        qWarning() << "DownloadManager::onPopError" << tr("Unknown Error %1 %2").arg( code ).arg( msg );
    }
    startNextAccount( false ); // RISK: Vollstop oder nächstes?
}

void DownloadManager::startNextAccount( bool startOver )
{
    if( d_curAccount.isNull() && !startOver )
        return;
    if( d_curAccount.isNull() )
    {
        Udb::Obj accounts = d_inbox.getTxn()->getObject( HeraldApp::s_accounts );
        Udb::Obj acc = accounts.getFirstObj();
        if( !acc.isNull() ) do
        {
            if( acc.getType() == TypePopAccount && acc.getValue(AttrSrvActive).getBool() )
            {
                d_curAccount = acc;
                break;
            }
        }while( acc.next() );
    }else
    {
        HeraldApp::inst()->removeLock( d_curAccount );
        Udb::Obj acc = d_curAccount;
        d_curAccount = Udb::Obj();
        if( acc.next() ) do
        {
            if( acc.getType() == TypePopAccount && acc.getValue(AttrSrvActive).getBool() )
            {
                d_curAccount = acc;
                break;
            }
        }while( acc.next() );
        // Wir sind auf natürliche Weise fertig
        d_quit = false;
    }
    if( !d_curAccount.isNull() )
    {
        HeraldApp::inst()->addLock( d_curAccount );
        int port = -1;
        if( d_curAccount.getValue(AttrSrvPort).getType() == Stream::DataCell::TypeUInt16 )
            port = d_curAccount.getValue(AttrSrvPort).getUInt16();
        d_pop->setAccount( d_curAccount.getValue(AttrSrvDomain).getArr(),
                           d_curAccount.getValue(AttrSrvUser).getArr(),
                           d_curAccount.getValue(AttrSrvPwd).getArr(),
                           d_curAccount.getValue(AttrSrvSsl).getBool(),
                           d_curAccount.getValue(AttrSrvTls).getBool(),
                           port );
        Q_ASSERT( !d_pop->isBusy() );
        d_pop->transmitAllMessages();
    }
}

void DownloadManager::checkForQuit()
{
    if( !d_quit )
        return;
    if( !d_curAccount.isNull() )
    {
        HeraldApp::inst()->removeLock( d_curAccount );
        d_curAccount = Udb::Obj();
        d_pop->quit();
        d_quit = false;
    }
}
