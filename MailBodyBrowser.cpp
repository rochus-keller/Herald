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

#include "MailBodyBrowser.h"
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
//#include <Txt/TextHtmlImporter.h>
#include <private/qtextdocumentfragment_p.h>
#include <QBuffer>
#include <QApplication>
#include <QtDebug>
#include "HeTypeDefs.h"
#include "MailObj.h"
using namespace He;

static const int s_timeout = 10000;
static const bool s_renderInSeparateThread = false;

MailBodyRenderer::MailBodyRenderer(MailBodyBrowser *p):QThread(0),d_width(0),d_abort(false)
{
	connect(this,SIGNAL(sigRendered( QTextDocument* )),p,SLOT(onDocReady(QTextDocument*)) );
	connect( p, SIGNAL(sigKill()), this, SLOT(onKill()) );
	connect(this,SIGNAL(finished()),this,SLOT(deleteLater()));
}

MailBodyRenderer::~MailBodyRenderer()
{
	if( isRunning() )
	{
		terminate();
	}
}

void MailBodyRenderer::render(const QString &html, const QFont &font, int width)
{
	if( isRunning() )
		return;
	d_html = html;
	d_font = font;
	d_width = width;
	startTimer( s_timeout );
	start();
}

void MailBodyRenderer::run()
{
    QTextDocument* doc = new QTextDocument();
    //doc->setUseDesignMetrics(true);
    doc->setDefaultFont( d_font );
    doc->setPageSize(QSize(d_width, -1));

    // unnecessary with LeanQt: Txt::TextHtmlImporter imp( &doc, d_html);
    QTextHtmlImporter imp( doc, d_html, QTextHtmlImporter::ImportToDocument);
	imp.import();
	if( d_abort )
	{
        delete doc;
        return;
	}
    //QTextDocument* clone = doc.clone();
    //clone->setPageSize(QSize(d_width, -1));

    // We did that to calculate layout in separate thread, and this worked in Qt 4.3.
    // Aparently this was the cause of the crash in LeanQt:
    //QAbstractTextDocumentLayout *layout = doc->documentLayout(); // das scheint am meisten Rechenarbeit zu verursachen

    if( d_abort )
	{
        delete doc;
		return;
	}
    doc->moveToThread( QApplication::instance()->thread() );
    emit sigRendered( doc );
}

void MailBodyRenderer::timerEvent(QTimerEvent * e)
{
	killTimer( e->timerId() );
	if( d_abort )
		return;
	d_abort = true;
	emit sigRendered(0);
}

void MailBodyRenderer::onKill()
{
	if( d_abort )
		return;
	d_abort = true;
	emit sigRendered(0);
}


MailBodyBrowser::MailBodyBrowser(QWidget *parent) :
    QTextBrowser(parent),d_msgA(0)
{
    setFont( QFont( "Arial", 10 ) );
	setOpenLinks( false );
	setOpenExternalLinks(false);
}

void MailBodyBrowser::showMail(MailMessage * msg)
{
    if( d_msgA == msg )
        return;
    clear();
    d_msgA = msg;
    if( d_msgA != 0 )
    {
		if( !d_msgA->htmlBody().isEmpty() )
            setHtml(d_msgA->htmlBody());
        else
        {
            Q_ASSERT( !d_msgA->contentType().startsWith( "text/html" ) );
            // Wenn ich das nicht via doc, bleiben bei PlainText Formate vom vorherigen HTML übrig
			QTextDocument* doc = new QTextDocument(this);
			doc->setDefaultFont( font() );
			doc->setPlainText( d_msgA->plainTextBody() );
			replaceDoc( doc );
		}
    }
}

void MailBodyBrowser::showMail(const Udb::Obj & o)
{
    if( d_msgB.equals( o ) )
        return;
    clear();
    d_msgB = o;
    if( !d_msgB.isNull() )
    {
        Stream::DataCell v = d_msgB.getValue( AttrBody );
        if( v.isHtml() )
            setHtml(v.getStr());
        else
		{
            // Wenn ich das nicht via doc, bleiben bei PlainText Formate vom vorherigen HTML übrig
			QTextDocument* doc = new QTextDocument(this);
			doc->setDefaultFont( font() );
			doc->setPlainText( v.getStr() );
			replaceDoc( doc );
		}
    }
}

void MailBodyBrowser::showPlainText(const QString & str)
{
	emit sigKill();
	QTextDocument* doc = new QTextDocument(this);
    doc->setDefaultFont( font() );
    doc->setPlainText( str );
	replaceDoc( doc );
}

void MailBodyBrowser::showHtml(const QString & html)
{
    setHtml(html);
}

void MailBodyBrowser::setHtml(const QString & html)
{
    if( s_renderInSeparateThread )
    {
        setPlainText( tr("Rendering HTML ..."));
        MailBodyRenderer* r = new MailBodyRenderer( this );
        r->render( MailObj::removeAllMetaTags( html ), font(), viewport()->width() );
    }else
    {
        QApplication::processEvents(); // so there is an immediate reaction on the screen before rendering starts

        QTextDocument* doc = new QTextDocument(this);
        doc->setLayoutTimeout(5);
        doc->setDefaultFont( font() );

        QTextHtmlImporter imp( doc, MailObj::removeAllMetaTags( html ), QTextHtmlImporter::ImportToDocument);
        imp.import();
        doc->setPageSize(QSize(viewport()->width(), -1));
        doc->documentLayout(); // does the rendering

        replaceDoc( doc );
    }
}

void MailBodyBrowser::clear()
{
	emit sigKill();
	if( d_msgA != 0 && d_msgA->parent() == this )
        delete d_msgA;
    d_msgA = 0;
    d_msgB = Udb::Obj();
    QTextBrowser::clear();
    d_cache.clear();
}

QVariant MailBodyBrowser::loadResource(int type, const QUrl &name)
{
	// Damit das funktioniert muss this Parent von TextDocument sein!
    QVariant v = d_cache.value( name );
    if( !v.isNull() )
        return v;
    if( name.scheme() == "cid" )
    {
        QByteArray contentId = QString( "<%1>" ).arg( name.path() ).toLatin1();
        if( d_msgA )
        {
            for( quint32 i = 0; i < d_msgA->messagePartCount(); i++ )
            {
                const MailMessagePart& part = d_msgA->messagePartAt(i);
                if( part.contentID() == contentId )
                {
                    QBuffer buf;
                    buf.open( QIODevice::WriteOnly );
                    part.decodedBody( &buf );
                    QByteArray data = buf.data();
                    d_cache[name] = data;
                    return data;
                }
            }
        }else if( !d_msgB.isNull() )
        {
            MailObj mailObj = d_msgB;
            AttachmentObj att = mailObj.findAttachment( contentId );
            if( !att.isNull() )
            {
                QFile file( att.getDocumentPath() );
                if( file.open( QIODevice::ReadOnly ) )
                {
                    QByteArray data = file.readAll();
                    d_cache[name] = data;
                    return data;
                }
            }
        }
    }
	return QVariant();
}

void MailBodyBrowser::onDocReady(QTextDocument * doc)
{
	if( doc )
	{
		doc->setParent(this);
		replaceDoc( doc );
	}else
	{
		setPlainText(tr("Rendering timed out or aborted!") );
	}
}

void MailBodyBrowser::replaceDoc(QTextDocument * doc)
{
	QTextDocument* old = document();
	if( old )
		old->deleteLater();
	// setDocument scheint die docs nicht zu löschen, auch wenn deren parent = this ist
	// QT-BUG: es ist erst QTextControl::setDocument, das auf parent = this prüft, nicht QTextEditor::setDocument.
	// Wenn also QTextEditor der parent ist, wird nicht gelöscht!
    setDocument( doc );
}

