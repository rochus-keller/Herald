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

#include "HeraldApp.h"
#include <QApplication>
#include <QMessageBox>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QFileInfo>
#include <QPainter>
#include <QLibrary>
#include <Udb/Database.h>
#include <Udb/Transaction.h>
#include <Udb/DatabaseException.h>
#include <Oln2/OutlineItem.h>
#include <Oln2/OutlineCtrl.h>
//#include <Txt/TextHtmlImporter.h>
#include <private/qtextdocumentfragment_p.h>
#include "EmailMainWindow.h"
#include "HeTypeDefs.h"
#include "MailView.h"
#include "IcsDocument.h"
#include <QDir>
#include <stdlib.h>
using namespace He;

const char* HeraldApp::s_company = "Dr. Rochus Keller";
const char* HeraldApp::s_domain = "www.rochus-keller.ch";
const char* HeraldApp::s_appName = "Herald";
const QUuid HeraldApp::s_rootUuid( "{88A92E74-ECF5-4780-B4E3-8397745A6409}" );
const char* HeraldApp::s_version = "0.9.2";
const char* HeraldApp::s_date = "2025-03-18";
const char* HeraldApp::s_extension = "hedb";
const QUuid HeraldApp::s_inboxUuid( "{C51269EA-A779-4878-BDF3-96E526B4E0EC}" );
const char* HeraldApp::s_addressIndex = "{F827F895-C6A8-4d63-986F-589900CC012A}";
const QUuid HeraldApp::s_accounts( "{E1E99EEB-938B-4797-BA59-CA623F846925}" );
const QUuid HeraldApp::s_drafts( "{1423CDA6-8A90-4fbb-8F11-F0605F9AC03B}" );
const QUuid HeraldApp::s_outlines( "{034D4107-50FE-45c4-8E27-DFC87B23423D}" );

static HeraldApp* s_inst = 0;
static QLibrary* s_ssl = 0;
static QLibrary* s_crypto = 0;

HeraldApp::HeraldApp(QObject *parent) :
    QObject(parent), d_lastWindowX(0)
{
    s_inst = this;
    qApp->setOrganizationName( s_company );
	qApp->setOrganizationDomain( s_domain );
	qApp->setApplicationName( s_appName );

	Oln::OutlineItem::AttrAlias = AttrItemLink;
	Oln::OutlineItem::AttrText = AttrText;
	Oln::OutlineItem::AttrIsTitle = AttrItemIsTitle;
	Oln::OutlineItem::AttrIsReadOnly = AttrItemIsReadOnly;
	Oln::OutlineItem::AttrIsExpanded = AttrItemIsExpanded;
	Oln::OutlineItem::AttrHome = AttrItemHome;
	Oln::OutlineItem::AttrCreatedOn = AttrCreatedOn;
	Oln::OutlineItem::AttrModifiedOn = AttrModifiedOn;
	Oln::OutlineItem::TID = TypeOutlineItem;
	Oln::OutlineItem::AttrIdent = AttrCustomId;
	Oln::OutlineItem::AttrAltIdent = AttrInternalId;

    qApp->setStyle("Fusion");

    d_set = new QSettings( s_appName, s_appName, this );

	d_styles = new Txt::Styles( this );
	Txt::Styles::setInst( d_styles );

    QDesktopServices::setUrlHandler( tr("xoid"), this, "onHandleXoid" ); // hier ohne SLOT und ohne Args nötig!

#ifdef Q_OS_UNIX
    if( s_crypto == 0 )
    {
        // preload from known location to avoid load by QtSsl from any random location
        QFileInfo crypto(QDir(QApplication::applicationDirPath()).absoluteFilePath("libcrypto.so.1.0.0"));
        if( crypto.exists() )
        {
            setenv("OPENSSL_CONF","/dev/null",1); // without there might be still missing functions from config dependencies
            s_crypto = new QLibrary(crypto.absoluteFilePath());
            if( s_crypto->load() )
            {
                qDebug() << "successfully loaded" << crypto.absoluteFilePath();
                QFileInfo ssl(QDir(QApplication::applicationDirPath()).absoluteFilePath("libssl.so.1.0.0"));
                s_ssl = new QLibrary(ssl.absoluteFilePath());
                if( s_ssl->load() )
                    qDebug() << "successfully loaded" << ssl.absoluteFilePath();
                else
                {
                    qCritical() << "cannot load" << ssl.absoluteFilePath();
                    delete s_crypto;
                    delete s_ssl;
                    s_crypto = 0;
                    s_ssl = 0;
                }
            }else
                qCritical() << "cannot load" << crypto.absoluteFilePath();
        }else
            qDebug() << "libcrypto and libssl not preloaded";
    }
#endif
}

HeraldApp::~HeraldApp()
{
    foreach( EmailMainWindow* w, d_docs )
        w->close(); // da selbst qApp->quit() zuerst alle Fenster schliesst, dürfte das nie vorkommen.
    s_inst = 0;
}

void HeraldApp::onOpen(const QString &path)
{
    if( !open( path ) )
    {
        if( d_docs.isEmpty() )
            qApp->quit();
    }
}

void HeraldApp::onClose()
{
    EmailMainWindow* w = static_cast<EmailMainWindow*>( sender() );
    if( d_docs.removeAll( w ) > 0 )
    {
       // w->deleteLater(); // Wird mit Qt::WA_DeleteOnClose von MainWindow selber gelöscht
    }
}

void HeraldApp::onHandleXoid(const QUrl & url)
{
    const QString oid = url.userInfo();
    const QString uid = url.host();
    const QString path = Udb::Database::findDatabase( uid );
    // openUrl funktioniert mit path alleine, aber nicht mit Argumenten, weder mit " noch ohne
    //bool res = QDesktopServices::openUrl( QUrl::fromLocalFile(tr("\"%1\" -oid:%2").arg(path).arg(oid) ) );
    bool res = Udb::Database::runDatabaseApp( uid, QStringList() << tr("-oid:%1").arg(oid) );
    qDebug() << res;
}

QString HeraldApp::parseFilePath(const QString & cmdLine )
{
    if( cmdLine.isEmpty() )
        return QString();
    if( cmdLine[0] == QChar('"') )
    {
        // "path"
        const int pos = cmdLine.indexOf( QChar('"'), 1 );
        if( pos != -1 )
            return cmdLine.mid( 1, pos - 1 );
    }else
    {
        // c:\abc.hedb -oid:
        const int pos = cmdLine.indexOf( QChar(' '), 1 );
        if( pos != -1 )
            return cmdLine.left( pos );
    }
    return cmdLine;
}

quint64 HeraldApp::parseOid(const QString & cmdLine)
{
    if( cmdLine.isEmpty() )
        return 0;
    int pos = -1;
    if( cmdLine[0] == QChar('"') )
        // "path"
       pos = cmdLine.indexOf( QChar('"'), 1 );
    else
        pos = cmdLine.indexOf( QChar(' '), 1 );
    if( pos != -1 )
    {
        // -oid:12345 -xx
        const int pos2 = cmdLine.indexOf( QString("-oid:"), pos + 1 );
        if( pos2 != -1 )
        {
            const int pos3 = cmdLine.indexOf( QChar(' '), pos2 + 5 );
            if( pos3 != -1 )
                return cmdLine.mid( pos2 + 5, pos3 - ( pos2 + 5 ) ).toULongLong();
            else
                return cmdLine.mid( pos2 + 5 ).toULongLong();
        }
    }
    return 0;
}

class _SplashDialog : public QDialog
{
public:
	_SplashDialog( QWidget* p ):QDialog(p, Qt::Dialog | Qt::FramelessWindowHint )
	{
		p->activateWindow();
		p->raise();
		setFixedSize(128,128);
#ifndef _WIN32
		exec();
#endif
	}
	void paintEvent ( QPaintEvent * )
	{
		QPainter p(this);
		QPixmap pix( ":/images/herald128.png" );
		p.drawPixmap( 0, 0, pix );
		QMetaObject::invokeMethod(this,"accept", Qt::QueuedConnection );
	}
};

bool HeraldApp::open(const QString & cmdLine)
{
    const QString path = parseFilePath( cmdLine );
    const quint64 oid = parseOid( cmdLine );

    QFileInfo info(path);
    if( info.suffix().compare( "eml", Qt::CaseInsensitive ) == 0 )
	{
		Udb::Transaction* txn = 0;
		if( d_docs.size() == 1 )
			txn = d_docs.first()->getTxn();
		MailView* view = new MailView( txn );
        view->setAttribute( Qt::WA_DeleteOnClose );
        Gui::AutoMenu* pop = new Gui::AutoMenu( view->getBody(), true );
        view->addCommands( pop );
		setWindowGeometry( view );
		view->show();
		_SplashDialog dlg(view);
		// QMessageBox::information(view,"Test","Test"); // nützt nichts; nur der erste kommt vorne
		view->showMail(path);
		return true;
    }else if( info.suffix().compare( "ics", Qt::CaseInsensitive ) == 0 )
    {
        IcsDocument doc;
        if( !doc.loadFrom( path ) )
        {
            QMessageBox::critical( 0, tr("Show iCalendar File"), doc.getError() );
            return false;
        }
        QString html;
        QTextStream out( &html );
        doc.writeHtml( out );
		openHtmlBrowser( html, info.fileName() );
		return true;
	}else if( info.suffix().compare( "html", Qt::CaseInsensitive ) == 0 )
	{
		QFile in( path );
		if( !in.open(QIODevice::ReadOnly ) )
		{
			QMessageBox::critical( 0, HeraldApp::tr("Show HTML File"), HeraldApp::tr("Cannot open file for reading: %1").arg(path) );
			return false;
		}
		openHtmlBrowser( Oln::OutlineCtrl::fetchHtml( in.readAll() ), path );
		return true;
	}else if( info.suffix().compare( s_extension, Qt::CaseInsensitive ) != 0 )
        return false; // Unbekanntes Dateiformat

    foreach( EmailMainWindow* w, d_docs )
    {
        if( w->getTxn()->getDb()->getFilePath().compare( path, Qt::CaseInsensitive ) == 0 )
        {
            w->showOid( oid );
			_SplashDialog dlg(w);
			return true;
        }
    }
    Udb::Transaction* txn = 0;
	try
	{
        Udb::Database* db = new Udb::Database( this );
        d_dbPath = path;
		db->open( path );
		db->setCacheSize( 10000 ); // RISK
        txn = new Udb::Transaction( db, this );
		HeTypeDefs::init( *db );
		Udb::Obj root = txn->getObject( s_rootUuid );
		if( root.isNull() )
			root = txn->createObject( s_rootUuid );
		txn->commit();
		Oln::OutlineItem::doBackRef();
		txn->addCallback( Oln::OutlineItem::itemErasedCallback );
		db->registerDatabase();
	}catch( Udb::DatabaseException& e )
	{
		QMessageBox::critical( 0, tr("Create/Open Repository"),
			tr("Error <%1>: %2").arg( e.getCodeString() ).arg( e.getMsg() ) );

        // Gib false zurück, wenn ansonsten kein Doc offen ist, damit Anwendung enden kann.
		return d_docs.isEmpty();
	}
    Q_ASSERT( txn != 0 );
    EmailMainWindow* w = new EmailMainWindow( txn );
    connect( w, SIGNAL(closing()), this, SLOT(onClose()) );
    if( d_docs.isEmpty() )
    {
        setAppFont( w->font() );
    }
    w->showOid( oid );
	_SplashDialog dlg(w);
	d_docs.append( w );
    return true;
}

HeraldApp *HeraldApp::inst()
{
    return s_inst;
}

void HeraldApp::setAppFont( const QFont& f )
{
    d_styles->setFontStyle( f.family(), f.pointSize() );
}

void HeraldApp::setWindowGeometry(QWidget * w)
{
    QDesktopWidget dw;
    QRect r = dw.availableGeometry();
    const int fixedWidth = 600;
    const int gap = r.height() / 20;
    if( d_lastWindowX == 0 || ( r.left() + d_lastWindowX + fixedWidth ) > r.right() )
        d_lastWindowX = r.left() + gap;
    w->setGeometry( d_lastWindowX, gap, fixedWidth, r.height() - 2 * gap );
    d_lastWindowX += gap;
}

QTextBrowser* HeraldApp::openHtmlBrowser(const QString &html, const QString &title)
{
    QTextBrowser* b = new QTextBrowser();
    b->setFont( QApplication::font() );
	b->setOpenLinks( false );
    b->setOpenExternalLinks(false);
    b->setAttribute( Qt::WA_DeleteOnClose );
	QTextDocument* doc = new QTextDocument(b);
    //Txt::TextHtmlImporter imp( doc, html );
    QTextHtmlImporter imp( doc, html, QTextHtmlImporter::ImportToDocument);
    imp.import();
	b->setDocument(doc);
    setWindowGeometry( b );
	b->show(); // setzt die Geometrie, darum zuerst aufrufen
	_SplashDialog dlg(b);
	if( !title.isEmpty() )
        b->setWindowTitle( tr("%1 - Herald").arg( title ) );
    else
        b->setWindowTitle( tr("HTML Browser - Herald") );
    return b;
}

void HeraldApp::openUrl(const QUrl & url)
{
    if( url.scheme() == "file" )
    {
        const QString path = adjustPath( url.toLocalFile() );
        QFileInfo info( path );
        if( !info.exists() )
            QMessageBox::critical( QApplication::focusWidget(), tr("Open URL - CrossLine"), tr("File not found: %1").arg(path) );
        QDesktopServices::openUrl( QUrl::fromLocalFile( path ) );
    }else
        QDesktopServices::openUrl( url );
}

QString HeraldApp::adjustPath(QString path)
{
    // TODO: folgende Lösung ist temporär; ev. ein konfigurierbares Mapping einführen.
    path.replace( QString("c:/users/me"), QString("/home/me"), Qt::CaseInsensitive );
    return path;
}

QIcon HeraldApp::getIconFromPath(const QString &path)
{
    QFileInfo info( path );
    QStringList locs = QStandardPaths::standardLocations( QStandardPaths::AppDataLocation );
    // .local/share/Dr. Rochus Keller/Herald

    locs.prepend(QFileInfo(d_dbPath).absolutePath()); // *.hedb directory
    locs.prepend(QApplication::applicationDirPath()); // exe directory

    foreach( const QString& l, locs )
    {
        const QString path = QString("%1/icons/%2.png").arg(l).
                arg(info.suffix().toLower());
        QPixmap pix(path);
        if( !pix.isNull() )
            return pix;
    }
    //else
    return QPixmap(":/images/document.png");
}
