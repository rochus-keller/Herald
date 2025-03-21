#ifndef MAILLISTDELEG_H
#define MAILLISTDELEG_H

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

#include <QAbstractItemDelegate>
#include <Udb/Obj.h>
#include <Udb/ObjIndexMdl.h>

namespace He
{
    class MailListDeleg : public QAbstractItemDelegate
    {
        Q_OBJECT
    public:
        explicit MailListDeleg(QObject *parent = 0);
    protected:
        void paintItem ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
        // Overrides
        void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    };

    class MailListMdl : public Udb::ObjIndexMdl
    {
    public:
        typedef QSet<Stream::DataCell::Atom> TypeFilter;
        enum Role { Name = Qt::UserRole, Sent, Subject, AttCount, Pixmap1, Pixmap2 };

        MailListMdl(QObject*p):ObjIndexMdl(p) {}
        const TypeFilter& getTypeFilter() const { return d_typeFilter; }
        void setTypeFilter( const TypeFilter& );

        Udb::Obj getObject( const QModelIndex & index ) const;
        // Overrides
        QVariant data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
        virtual bool filtered( Udb::OID );
    private:
        TypeFilter d_typeFilter;
    };
}

#endif // MAILLISTDELEG_H
