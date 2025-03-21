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

#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/DatabaseException.h>
#include <Udb/BtreeCursor.h>
#include <Udb/BtreeStore.h>
#include <Oln2/OutlineUdbMdl.h>
#include <QApplication>
#include <QtSingleApplication>
#include <QMessageBox>
#include <QFileDialog>
#include <QIcon>
#include <QSplashScreen>
#include <QtPlugin>
#include <QtDebug>
#include "EmailMainWindow.h"
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include <Mail/MailMessage.h>
using namespace He;

//Q_IMPORT_PLUGIN(qgif)
//Q_IMPORT_PLUGIN(qjpeg)

class MyApp : public QtSingleApplication
{
public:
	MyApp(const QString &id, int &argc, char **argv):QtSingleApplication(id, argc, argv) {}
	bool notify(QObject * o, QEvent * e)
	{
		try
		{
			return QtSingleApplication::notify(o, e);
		}catch( Udb::DatabaseException& ex )
		{
			qWarning() << "MyApp::notify exception:" << ex.getMsg();
			QMessageBox::critical( activeWindow(), HeraldApp::tr("Herald Error"),
								   QString("Database Error: [%1] %2").arg( ex.getCodeString() ).arg( ex.getMsg() ), QMessageBox::Abort );
		}catch( std::exception& ex )
		{
			qWarning() << "MyApp::notify exception:" << ex.what();
			QMessageBox::critical( activeWindow(), HeraldApp::tr("Herald Error"),
				QString("Generic Error: %1").arg( ex.what() ), QMessageBox::Abort );
		}catch( ... )
		{
			qWarning() << "MyApp::notify exception: unknown";
			QMessageBox::critical( activeWindow(), HeraldApp::tr("Herald Error"),
				QString("Generic Error: unexpected internal exception"), QMessageBox::Abort );
		}
		return false;
	}
};

int main(int argc, char *argv[])
{
	MyApp app( HeraldApp::s_appName, argc, argv);

    QIcon icon;
	icon.addFile( ":/images/herald16.png" );
	icon.addFile( ":/images/herald32.png" );
	icon.addFile( ":/images/herald48.png" );
	icon.addFile( ":/images/herald64.png" );
	icon.addFile( ":/images/herald128.png" );
	app.setWindowIcon( icon );

    Oln::OutlineUdbMdl::registerPixmap( TypeDocument, QString( ":/images/document-medium.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeAttachment, QString( ":/images/paper-clip.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeInboundMessage, QString( ":/images/document-mail-open.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeOutboundMessage, QString( ":/images/document-mail.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeInbox, QString( ":/images/inbox-download.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeEmailAddress, QString( ":/images/at-sign-small.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeFromParty, QString( ":/images/from-party.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeToParty, QString( ":/images/to-party.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeCcParty, QString( ":/images/cc-party.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeBccParty, QString( ":/images/bcc-party.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeResentParty, QString( ":/images/resent-to-party.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeOutlineItem, QString( ":/images/outline_item.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeOutline, QString( ":/images/outline.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypePerson, QString( ":/images/person.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeMailDraft, QString( ":/images/mail-draft.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeScheduleSet, QString( ":/images/schedule_set.png" ) );
	Oln::OutlineUdbMdl::registerPixmap( TypeSchedule, QString( ":/images/calendar.png" ) );
	Oln::OutlineUdbMdl::registerPixmap( TypeEvent, QString( ":/images/schedule_event.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeAppointment, QString( ":/images/appointment.png" ) );
    Oln::OutlineUdbMdl::registerPixmap( TypeDeadline, QString( ":/images/deadline.png" ) );

#ifndef _DEBUG
	try
	{
#endif
		HeraldApp ctx;
        QObject::connect( &app, SIGNAL( messageReceived(const QString&)),
                              &ctx, SLOT( onOpen(const QString&) ) );

		QString path;

		if( ctx.getSet()->contains( "App/Font" ) )
		{
			QFont f1 = QApplication::font();
			QFont f2 = ctx.getSet()->value( "App/Font" ).value<QFont>();
			f1.setFamily( f2.family() );
			f1.setPointSize( f2.pointSize() );
			QApplication::setFont( f1 );
            ctx.setAppFont( f1 );
		}else
            ctx.setAppFont( QApplication::font() );

		QStringList args = QCoreApplication::arguments();
        QString oidArg;
		for( int i = 1; i < args.size(); i++ ) // arg 0 enthält Anwendungspfad
		{
			if( !args[ i ].startsWith( '-' ) )
			{
				path = args[ i ];
			}else
            {
                if( args[ i ].startsWith( "-oid:") )
                    oidArg = args[ i ];
            }
		}

		if( path.isEmpty() )
        {
            //oidArg.clear();
			path = QFileDialog::getSaveFileName( 0, HeraldApp::tr("Create/Open Repository - Herald"),
				QString(), QString( "*.%1" ).arg( QLatin1String( HeraldApp::s_extension ) ),
				0, QFileDialog::DontConfirmOverwrite );
        }
        // NOTE: DontUseNativeDialog kann mit Aliassen nicht umgehen
		if( path.isEmpty() )
			return 0;

        // NOTE: path kommt immer ohne Anführungs- und Schlusszeichen, auch wenn er Spaces enthält
        if( path.contains( QChar(' ') ) && path[0] != QChar('"') )
            path = QChar('"') + path + QChar('"');

        if( !oidArg.isEmpty() )
            path += QChar(' ') + oidArg;

//#ifndef _DEBUG
        if( app.sendMessage( path ) )
                 return 0; // Eine andere Instanz ist bereits offen
//#endif
        QSplashScreen splash( QPixmap( ":/images/herald_splash.png" ) );
		QFont f = splash.font();
		f.setBold( true );
		splash.setFont( f );
		splash.show();
		splash.showMessage( HeraldApp::tr("Loading Herald %1...").arg( HeraldApp::s_version ),
			Qt::AlignHCenter | Qt::AlignBottom, Qt::blue );
		app.processEvents();

        if( !ctx.open( path ) )
			return -1;

        if( !ctx.getDocs().isEmpty() )
            splash.finish( ctx.getDocs().first() );
        else
            splash.close();

        const int res = app.exec();
		return res;
#ifndef _DEBUG
	}catch( Udb::DatabaseException& e )
	{
		QMessageBox::critical( 0, HeraldApp::tr("Herald Error"),
			QString("Database Error: [%1] %2").arg( e.getCodeString() ).arg( e.getMsg() ), QMessageBox::Abort );
		return -1;
	}catch( std::exception& e )
	{
		QMessageBox::critical( 0, HeraldApp::tr("Herald Error"),
			QString("Generic Error: %1").arg( e.what() ), QMessageBox::Abort );
		return -1;
	}catch( ... )
	{
		QMessageBox::critical( 0, HeraldApp::tr("Herald Error"),
			QString("Generic Error: unexpected internal exception"), QMessageBox::Abort );
		return -1;
	}
#endif
	return 0;}
