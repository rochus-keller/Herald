#ifndef REFVIEWCTRL_H
#define REFVIEWCTRL_H

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

#include <QWidget>
#include <GuiTools/AutoMenu.h>
#include <Udb/UpdateInfo.h>
#include <Udb/Obj.h>

class QListWidget;
class QListWidgetItem;

namespace He
{
    class ObjectTitleFrame;

    class RefViewCtrl : public QWidget
    {
        Q_OBJECT
    public:
        explicit RefViewCtrl(QWidget *parent = 0);
        QWidget* getWidget() const;
        static RefViewCtrl* create(QWidget* parent, Udb::Transaction* );
        void addCommands( Gui::AutoMenu* );
        void setObj( const Udb::Obj& );
        void clear();
    signals:
        void sigSelected(const Udb::Obj& );
        void sigDblClicked(const Udb::Obj& );
    protected slots:
        void onClicked(QListWidgetItem* item);
        void onDblClicked(QListWidgetItem* item);
        void onDbUpdate( Udb::UpdateInfo info );
        void onTitleClick();
    protected:
        void fillMailRefs();
        void fillDocRefs();
        void fillCausingRefs();
		void addItemRefs(const Udb::Obj &o);
        QListWidgetItem *addItem( const QString& title, const Udb::Obj& o );
        static QString generateToolTip( const Udb::Obj& mail );
    private:
        ObjectTitleFrame* d_title;
        QListWidget* d_list;
        bool d_lock;
    };
}

#endif // REFVIEWCTRL_H
