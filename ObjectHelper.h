#ifndef OBJECTHELPER_H
#define OBJECTHELPER_H

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

#include <Udb/Obj.h>
#include <QObject>

namespace He
{
    class ObjectHelper : public QObject // wegen tr
    {
        ObjectHelper() {}
    public:
        static QString getInboxPath( Udb::Transaction * );
        static QString getOutboxPath( Udb::Transaction * );
        static QString getDocStorePath( Udb::Transaction * );
		static QString getCertDbPath( Udb::Transaction * );
		static Udb::Obj getRoot( Udb::Transaction * );
        static Udb::Obj createObject( quint32 type, Udb::Obj parent, const Udb::Obj &before = Udb::Obj() );
        static Udb::Obj createObject( quint32 type, Udb::Transaction* );
        static void initObject( Udb::Obj& );
        static quint32 getNextId( Udb::Transaction *, quint32 type );
        static QString getNextIdString( Udb::Transaction *, quint32 type );

    };
}

#endif // OBJECTHELPER_H
