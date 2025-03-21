#ifndef MAILLISTCTRL_H
#define MAILLISTCTRL_H

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

#include <QLabel>
#include <Udb/Idx.h>
#include <Udb/Obj.h>
#include <GuiTools/AutoMenu.h>

class QTreeView;

namespace He
{
    class ObjectTitleFrame;
    class MailListMdl;

    class MailListCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit MailListCtrl(QWidget *parent = 0);
        QWidget* getWidget() const;
        static MailListCtrl* create(QWidget* parent, Udb::Transaction *txn );
        void addCommands( Gui::AutoMenu* );
        void setIdx( const Udb::Idx& );
        void setObj( const Udb::Obj& );
        Udb::Obj getSelectedMail() const;
    signals:
        void sigSelectionChanged();
    protected slots:
        void onSelectionChanged();
        void onCopy();
        void onDelete();
        void onShowFrom();
        void onShowTo();
        void onShowCc();
        void onShowBcc();
        void onShowResent();
		void onResolvePerson();
        void onAllOnOff();
        void onShowInbound();
        void onShowOutbound();
    protected:
        void handleShow(quint32 type);
    private:
        ObjectTitleFrame* d_title;
        QTreeView* d_tree;
        MailListMdl* d_mdl;
		bool d_resolvePerson;
    };

    class SmallToggler : public QLabel
    {
        Q_OBJECT
    public:
        SmallToggler( const QString&, const QPixmap&, quint32 type, bool on, QWidget* w);
        bool isChecked() const { return d_checked; }
        void setChecked( bool );
    signals:
        void sigToggle( bool on, quint32 type );
    protected:
        void mousePressEvent ( QMouseEvent * event );
    private:
        quint32 d_type;
        bool d_checked;
    };

}

#endif // MAILLISTCTRL_H
