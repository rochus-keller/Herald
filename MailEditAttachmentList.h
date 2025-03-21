#ifndef MAILEDITATTACHMENTLIST_H
#define MAILEDITATTACHMENTLIST_H

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

#include <QListWidget>
#include "MailBodyEditor.h"

namespace He
{
    class MailEditAttachmentList : public QListWidget, public MailBodyEditor::ResourceManager
    {
        Q_OBJECT
    public:
        enum { FilePath = Qt::UserRole, ContentID, AttrName };
        MailEditAttachmentList( QWidget* parent ):QListWidget(parent),d_modified(false) {}
        void addFile( const QString& path, const QString& name, const QByteArray& cid );
        void addImage( const QImage& img, const QByteArray& cid );
        QImage getImage( const QByteArray& cid ) const { return d_images.value( cid ); }
        bool isModified() const { return d_modified; }
        void setModified( bool y ) { d_modified = y; }

        // Implementing Interface
        virtual QImage fetchImage( const QByteArray& cid );
        virtual QByteArray registerImage( const QImage & image );
        virtual QByteArray registerFile( const QString& path );
    public slots:
        void onDelete();
        void onOpen();
        void onPaste();
    protected:
        // overview
        bool dropMimeData ( int index, const QMimeData * data, Qt::DropAction action );
        QStringList mimeTypes () const;
    private:
        QHash<QByteArray,QImage> d_images; // cid -> image (hier ohne <>!)
        bool d_modified;
    };
}

#endif // MAILEDITATTACHMENTLIST_H
