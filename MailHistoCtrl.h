#ifndef MAILHISTOCTRL_H
#define MAILHISTOCTRL_H

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
#include <GuiTools/AutoMenu.h>

class QTreeView;

namespace He
{
    class MailListMdl;

    class MailHistoCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit MailHistoCtrl(QWidget *parent = 0);
        QWidget* getWidget() const;
        static MailHistoCtrl* create(QWidget* parent, Udb::Transaction *txn );
        void addCommands( Gui::AutoMenu* );
        Udb::Obj getSelectedMail() const;
    signals:
        void sigSelectionChanged();
    public slots:
        void onSelect( const QList<Udb::Obj>& );
        void onCopy();
        void onDelete();
        void onRebuildIndex();
    protected slots:
        void onSelectionChanged();
    private:
        QTreeView* d_tree;
        MailListMdl* d_mdl;
    };
}

#endif // MAILHISTOCTRL_H
