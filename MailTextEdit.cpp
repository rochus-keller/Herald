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

#include "MailTextEdit.h"
#include <QApplication>
#include <QClipboard>
#include <QToolTip>
#include <QtDebug>
#include <QTextDocument>
#include <QTextFrame>
#include <QMimeData>
#include <Mail/MailMessage.h>
#include <GuiTools/AutoShortcut.h>
#include <Udb/Transaction.h>
#include "MailObj.h"
#include "HeTypeDefs.h"
using namespace He;

void MailTextEdit::onSelected( QString name, QString addr )
{
    getTxt()->getView()->getCursor().beginEditBlock();
    if( name.isEmpty() )
        name = addr;
    if( addr.isEmpty() )
    {
        QByteArray addr2;
        QString name2;
        MailMessage::parseEmailAddress( name, name2, addr2 );
        if( name2.isEmpty() )
            name2 = addr2;
        if( addr2.isEmpty() )
            getTxt()->getView()->getCursor().insertText( name2 );
        else
            getTxt()->getView()->getCursor().insertUrl( QString("mailto:%1").arg( addr2.data() ),
                                                        false, name2 );
    }else
        getTxt()->getView()->getCursor().insertUrl( QString("mailto:%1").arg( addr ), false, name );
    getTxt()->getView()->getCursor().insertText( " " );
    getTxt()->getView()->getCursor().endEditBlock();
}

void MailTextEdit::onPaste()
{
    ENABLED_IF( QApplication::clipboard()->mimeData()->hasText() ||
                QApplication::clipboard()->mimeData()->hasHtml() ||
                QApplication::clipboard()->mimeData()->hasUrls() );

    if( QApplication::clipboard()->mimeData()->hasHtml() )
    {
        QTextDocumentFragment frag = QTextDocumentFragment::fromHtml(
                                         QApplication::clipboard()->mimeData()->html() );
        getTxt()->getView()->getCursor().insertFragment( frag );

    }else if( QApplication::clipboard()->mimeData()->hasUrls() )
    {
        QList<QUrl> list = QApplication::clipboard()->mimeData()->urls();
        getTxt()->getView()->getCursor().beginEditBlock();
        foreach( QUrl u, list )
        {
            getTxt()->getView()->getCursor().insertUrl( u );
            getTxt()->getView()->getCursor().insertText( " " );
        }
        getTxt()->getView()->getCursor().endEditBlock();
    }else
    {
        const QString text = QApplication::clipboard()->mimeData()->text().simplified();
        QStringList addrs = findEmailAddresses(text);
        getTxt()->getView()->getCursor().beginEditBlock();
        foreach( QString s, addrs )
        {
            QByteArray addr;
            QString name;
            MailMessage::parseEmailAddress( s, name, addr );
            onSelected( name, addr.data() );
        }
        getTxt()->getView()->getCursor().endEditBlock();
	}
}

void MailTextEdit::onPastePlainText()
{
	ENABLED_IF( QApplication::clipboard()->mimeData()->hasText() ||
				QApplication::clipboard()->mimeData()->hasHtml() ||
				QApplication::clipboard()->mimeData()->hasUrls() );
	getTxt()->getView()->getCursor().insertText( QApplication::clipboard()->mimeData()->text() );
}

QStringList MailTextEdit::findEmailAddresses(const QString & str)
{
    // Zuerst der einfache Fall: Listen mit klaren Trennzeichen
    QStringList splitted = MailMessage::splitQuoted( str, ',' );
    if( splitted.size() > 1 ) // bei size < 1 hätte also gar kein Split stattgefunden
        return splitted;
    splitted = MailMessage::splitQuoted( str, ';' );
    if( splitted.size() > 1 )
        return splitted;
    // Dann der Fall von Adressen ohne Trennzeichnen
    bool foundSplitter;
    splitted = MailMessage::splitQuoted( str, '>', &foundSplitter );
    if( foundSplitter ) // Es gibt mindestens eine Adresse der Form "name <addr>"
        return splitted; // Es ist kein Problem, dass das '>' fehlt.
    MailMessage::splitQuoted( str, '@', &foundSplitter ); // @ darf in Quote enthalten sein
    if( foundSplitter )
    {
        // es sind eine oder mehrere Adressen der Minimalform ohne <> enthalten,
        // aber keine ',' oder ';'. Als Splitter kommt nur noch Space in Frage.
        // Space könnte aber auch Teil von Quote sein.
        splitted = MailMessage::splitQuoted( str, ' ', &foundSplitter );
        if( !splitted.isEmpty() )
            return splitted;
    }// else
    // offensichtlich keine Email-Adresse enthalten
    return QStringList();
}

void MailTextEdit::keyPressEvent ( QKeyEvent * event )
{
    if( !event->text().isEmpty() && event->text()[0].isLetterOrNumber() )
    {
        emit sigKeyPressed( event->text() );
        event->accept();
    }else
    {
        if( event == QKeySequence::Paste )
        {
            onPaste();
            return;
        }else
            TextEdit::keyPressEvent( event );
    }
}

void MailTextEdit::mousePressEvent(QMouseEvent *e)
{
    TextEdit::mousePressEvent( e );
    if( e->buttons() == Qt::LeftButton && getTxt()->getView()->getCursor().isUrl() )
    {
        QUrl u = getTxt()->getView()->getCursor().getUrl();

        MailObj::MailAddr addr;
        addr.d_name = getTxt()->getView()->getCursor().getSelectedText();
        if( u.scheme() == "mailto" )
            addr.d_addr = u.path().toLatin1();
        else if( u.scheme() == "oid" )
        {
            Udb::Obj o = d_txn->getObject( u.path().toULongLong() );
            if( HeTypeDefs::isParty( o.getType() ) )
                o = o.getValueAsObj(AttrPartyAddr);
            if( o.getType() == TypeEmailAddress )
                addr.d_addr = o.getValue( AttrEmailAddress ).getArr();
            else
                addr.d_addr = u.toString().toLatin1();
        }else if( u.isValid() )
            addr.d_addr = u.toString().toLatin1();
        else
        {
            // NOTE: Barca liefert HTML mit eingebetteten Mailto-Links mit falschem Format der form
            addr.d_addr = u.errorString().toLatin1();
            qDebug() << u.errorString();
        }

        QToolTip::showText(e->globalPos(), addr.prettyNameEmail(), this );
    }
}

void MailTextEdit::mouseReleaseEvent(QMouseEvent *e)
{
    QToolTip::hideText();
    TextEdit::mouseReleaseEvent( e );
}

void MailTextEdit::addAddress(const QString &name, const QByteArray &addr)
{
    onSelected( name, addr );
}

QList<MailTextEdit::Party> MailTextEdit::getParties() const
{
    return getParties( getTxt()->getView()->getDocument(), d_txn );
}

QList<MailTextEdit::Party> MailTextEdit::getParties(QTextDocument* doc, Udb::Transaction * txn)
{
    Q_ASSERT( doc != 0 && txn != 0 );
    QList<Party> res;
    QTextFrame* root = doc->rootFrame();
    for( QTextFrame::Iterator it = root->begin(); it != root->end(); ++it )
	{
        if( QTextFrame *f = it.currentFrame() )
        {
            // Ignoriere Textstrukturen
        }else if( it.currentBlock().isValid() )
        {
            QTextBlock::Iterator it2 = it.currentBlock().begin();

            for( ; !it2.atEnd(); ++it2 )
            {
                const QTextFragment f = it2.fragment();
                const QTextCharFormat tf = f.charFormat();
                if( tf.isAnchor() )
                {
                    Party p;
                    p.d_name = f.text().trimmed();
                    if( tf.anchorHref().startsWith( QLatin1String("mailto:"), Qt::CaseInsensitive )  )
                        p.d_addr = tf.anchorHref().mid( 7 ).toLatin1();
                    else if( tf.anchorHref().startsWith( QLatin1String("oid:" ) ) )
                        p.d_obj = txn->getObject( tf.anchorHref().mid(4).toULongLong() );
                    else
                        p.d_error = tr("Invalid href=%1").arg( tf.anchorHref() );
                    if( p.d_name.toLatin1() == p.d_addr )
                        p.d_name.clear();
                    res.append(p);
                }else
                {
                    // Normaler Text
                    QStringList addrs = findEmailAddresses( f.text() );
                    foreach( QString s, addrs )
                    {
                        if( !s.trimmed().isEmpty() )
                        {
                            Party p;
                            MailMessage::parseEmailAddress( s, p.d_name, p.d_addr );
                            if( p.d_addr.isEmpty() )
                                p.d_error = tr("Invalid address format");
                            res.append( p );
                        } // else ignore whitespace
                    }
                }

            }
        }
    }
    return res;
}

QString MailTextEdit::Party::render() const
{
    if( !d_obj.isNull() )
        return MailObj::formatAddress( d_obj, true );
    if( !d_name.isEmpty() )
        return QString( "%1 <%2>" ).arg( MailMessage::escapeDisplayName(d_name) ).arg( d_addr.data() );
    else
		return QString( "<%1>" ).arg( d_addr.data() );
}

MailTextEdit::MailTextEdit(Udb::Transaction *txn, QWidget *w):TextEdit(w),d_txn(txn)
{
}
