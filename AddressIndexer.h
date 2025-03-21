#ifndef ADDRESSINDEXER_H
#define ADDRESSINDEXER_H

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
#include <Udb/Obj.h>
#include <Udb/UpdateInfo.h>

namespace He
{
    class AddressIndexer : public QObject
    {
        Q_OBJECT
    public:
        explicit AddressIndexer(Udb::Transaction*, QObject *parent = 0);

        void test();
        void test2();
        void test3();
        void test4();
        void indexAll(bool clear = false);
        void clearIndex();
        QList<Udb::Obj> find( QString, bool noObsoletes = false ) const;
        Udb::Transaction* getTxn() const { return d_index.getTxn(); }
    protected slots:
		void onDbUpdate( const Udb::UpdateInfo& );
    protected:
        void addToIndex( const QStringList&, const Udb::Obj&, bool remove = false );
        QStringList splitAddress( const QString& addr );
        QStringList splitName( const QString& name );
    private:
        Udb::Obj d_index;
        // Das ist ein Word-Index der Felder AttrEmailAddress und AttrText von TypeEmailAddress
    };
}

#endif // ADDRESSINDEXER_H
