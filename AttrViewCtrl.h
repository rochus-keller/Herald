#ifndef ATTRVIEWCTRL_H
#define ATTRVIEWCTRL_H

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
#include <Oln2/OutlineMdl.h>
#include <Udb/UpdateInfo.h>
#include <GuiTools/AutoMenu.h>

namespace Oln
{
    class OutlineTree;
}
namespace He
{
    class ObjectTitleFrame;
    class ObjectDetailPropsMdl;

    class AttrViewCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit AttrViewCtrl(QWidget *parent);
        static AttrViewCtrl* create(QWidget* parent,Udb::Transaction *txn );
        void setObj( const Udb::Obj& );
        const Udb::Obj& getObj() const;
        QWidget* getWidget() const;
        void addCommands( Gui::AutoMenu* );
    signals:
        void signalFollowObject( const Udb::Obj& );
    protected slots:
        void onAnchorActivated(QByteArray data,bool url);
        void onDbUpdate( Udb::UpdateInfo info );
        void onEditText();
        void onCopy();
        void onTitleClick();
    private:
        ObjectTitleFrame* d_title;
        ObjectDetailPropsMdl* d_props;
        Oln::OutlineTree* d_tree;
    };

    class ObjectDetailPropsMdl : public Oln::OutlineMdl
    {
    public:
        ObjectDetailPropsMdl(QObject*p):OutlineMdl(p){}
        void setObject(const Udb::Obj& o);
        const Udb::Obj& getObj() const { return d_obj; }
    private:
        struct ObjSlot : public Slot
        {
        public:
            quint32 d_name;
            virtual quint64 getId() const { return d_name; }
            virtual QVariant getData(const OutlineMdl*,int role) const;
        };
        Udb::Obj d_obj;
    };
}

#endif // ATTRVIEWCTRL_H
