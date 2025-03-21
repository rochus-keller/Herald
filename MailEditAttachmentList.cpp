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

#include "MailEditAttachmentList.h"
#include <QtDebug>
#include <QMimeData>
#include <QUrl>
#include <QCryptographicHash>
#include <QApplication>
#include <QClipboard>
#include <QMessageBox>
#include <QFile>
#include <GuiTools/AutoMenu.h>
#include <Udb/Transaction.h>
#include "HeTypeDefs.h"
#include "HeraldApp.h"
using namespace He;

void MailEditAttachmentList::addFile(const QString &path, const QString &name, const QByteArray &cid)
{
    for( int i = 0; i < count(); i++ )
    {
        if( item(i)->data( FilePath ).toString() == path )
            return; // Datei ist bereits ein Attachment
    }
    QLocale loc;
    QFile file( path );
    QListWidgetItem* item = new QListWidgetItem( this );
    item->setText( tr("%1 (%2 k%3)").arg( (name.isEmpty())?path:name )
                   .arg( loc.toString( ( file.size() * 10 / 1024 ) * 0.1 ) ).
                   arg( (!cid.isEmpty())?", inline":"" ) );
    item->setIcon( HeraldApp::inst()->getIconFromPath( path ) );
    item->setData( FilePath, path );
    item->setData( AttrName, name );
    // hier cid ohne <>!
    item->setData( ContentID, cid );
    if( !file.exists() )
    {
        QFont strikeout = item->font();
        strikeout.setStrikeOut( true );
        item->setFont( strikeout );
    }
    d_modified = true;
}

void MailEditAttachmentList::addImage(const QImage &img, const QByteArray &cid)
{
    d_images[cid] = img;
    QListWidgetItem* item = new QListWidgetItem( this );
    item->setText( tr("Inline Image %1" ).arg( d_images.size() ) );
    item->setIcon( QIcon(":/images/edit-image.png") );
    // hier cid ohne <>!
    item->setData( ContentID, cid );
    d_modified = true;
}

QImage MailEditAttachmentList::fetchImage(const QByteArray &cid)
{
    // entweder ist Image bereits im Cache
    QHash<QByteArray,QImage>::const_iterator i = d_images.find( cid );
    if( i != d_images.end() )
        return i.value();
    // oder es gibt eine Datei mit dem cid, die noch geladen werden muss
    for( int i = 0; i < count(); i++ )
    {
        // hier cid ohne <>!
        if( item(i)->data( ContentID ).toByteArray() == cid )
        {
            QImage img;
            if( img.load( item(i)->data( FilePath ).toString() ) )
            {
                // Der Pfad ist ladbar als Image
                d_images[cid] = img;
                return img;
            }else
            {
                // Der Pfad ist nicht ladbar als Image; entferne das cid
                item(i)->setData( ContentID, QVariant() );
            }
            break;
        }
    }
    // oder wir können mit dem cid gar nichts anfangen
    qDebug() << "MailEditAttachmentList::fetchImage: failure" << cid;
    return QImage();
}

bool MailEditAttachmentList::dropMimeData(int, const QMimeData *data, Qt::DropAction action)
{
    if( data->hasFormat(HeTypeDefs::s_mimeAttachRefs) )
    {
        QList<QPair<QString, QUrl> > atts = HeTypeDefs::readAttachRefs( data );
        for( int i = 0; i < atts.size(); i++ )
        {
            addFile( atts[i].second.toLocalFile(), atts[i].first, QByteArray() );
        }
        return true;
    }else if( data->hasFormat( QLatin1String( "text/uri-list" ) ) )
    {
        foreach( QUrl url, data->urls() )
        {
            if( url.scheme() == QLatin1String("file" ) )
                addFile( url.toLocalFile(), QString(), QByteArray() );
        }
        return true;
    }else
        return false;
}

QStringList MailEditAttachmentList::mimeTypes() const
{
    return QStringList() << QLatin1String( QLatin1String( "text/uri-list" ) );
}

QByteArray MailEditAttachmentList::registerImage(const QImage &image)
{
    // NOTE: Content-IDs müssten die Form "<" links@rechts ">" haben
    // Siehe http://tools.ietf.org/html/rfc2111
    QCryptographicHash h( QCryptographicHash::Md5 );
    h.addData( QByteArray::fromRawData( (const char*)image.bits(), image.byteCount() ) );
    QByteArray cid = h.result().toBase64();
    cid.chop( 2 ); // "==" am Schluss
    cid += "@hld";
    if( d_images.contains( cid ) )
        return cid;
    addImage( image, cid );
    return cid;
}

QByteArray MailEditAttachmentList::registerFile(const QString &path)
{
    QListWidgetItem* existingItem = 0;
    for( int i = 0; i < count(); i++ )
    {
        if( item(i)->data( FilePath ).toString() == path )
        {
            QByteArray cid = item(i)->data( ContentID ).toByteArray();
            if( !cid.isEmpty() )
                return cid; // Datei ist bereits registriert
            // Datei ist zwar bekannt, aber es gibt noch keine cid
            existingItem = item(i);
            break;
        }
    }
    // Prüfe, ob man es als Image verwenden kann
    QImage img;
    QByteArray cid;
    if( img.load( path ) )
    {
        // NOTE: Content-IDs müssten die Form "<" links@rechts ">" haben
        // Siehe http://tools.ietf.org/html/rfc2111
        cid = Stream::DataCell().setDateTime( QDateTime::currentDateTime() ).
        writeCell(true).mid( 2 ).toBase64() + "@hld";
        d_images[cid] = img;
    }
    if( existingItem == 0 )
        addFile( path, QString(), cid );
    else
        existingItem->setData( ContentID, cid );     // hier cid ohne <>!
    d_modified = true;
    return cid;
}

void MailEditAttachmentList::onDelete()
{
    QList<QListWidgetItem *> l = selectedItems();
    ENABLED_IF( !l.isEmpty() );
//    if( QMessageBox::warning( this, tr("Delete Attachment - Herald"),
//                          tr("Do you really want to delete '%1'? This cannot be undone.").
//                              arg( currentItem()->text() ), QMessageBox::Ok | QMessageBox::Cancel,
//                          QMessageBox::Ok ) == QMessageBox::Cancel )
//        return;

    foreach( QListWidgetItem* i, l )
    {
        d_images.remove( i->data(ContentID).toByteArray() );
        delete i;
        d_modified = true;
    }
}

void MailEditAttachmentList::onOpen()
{
    ENABLED_IF( selectedItems().size() == 1 && !currentItem()->data(FilePath).isNull() );
    HeraldApp::openUrl( QUrl::fromLocalFile( currentItem()->data(FilePath).toString() ) );
}

void MailEditAttachmentList::onPaste()
{
    ENABLED_IF( QApplication::clipboard()->mimeData()->hasFormat( HeTypeDefs::s_mimeAttachRefs ) ||
                 QApplication::clipboard()->mimeData()->hasFormat( "text/uri-list" ) );
    if( QApplication::clipboard()->mimeData()->hasFormat( HeTypeDefs::s_mimeAttachRefs ) )
    {
        QList<QPair<QString, QUrl> > atts = HeTypeDefs::readAttachRefs( QApplication::clipboard()->mimeData() );
        for( int i = 0; i < atts.size(); i++ )
        {
            addFile( atts[i].second.toLocalFile(), atts[i].first, QByteArray() );
        }
    }else
    {
        foreach( QUrl url, QApplication::clipboard()->mimeData()->urls() )
        {
            if( url.scheme() == QLatin1String("file" ) )
                addFile( url.toLocalFile(), QString(), QByteArray() );
        }
    }
}

