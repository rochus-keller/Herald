#ifndef INBOXCTRL_H
#define INBOXCTRL_H

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
#include <Udb/InvQueueMdl.h>
#include <Udb/Obj.h>
#include <GuiTools/AutoMenu.h>
#include <QAbstractItemDelegate>

class QTreeView;

namespace He
{
    class InboxMdl;

    class InboxCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit InboxCtrl(QTreeView*, InboxMdl* );
        static InboxCtrl* create(QWidget* parent, const Udb::Obj& root );
        void addCommands( Gui::AutoMenu* );
        QTreeView* getTree() const;
        Udb::Obj getSelectedObject() const;
        QString getSelectedFile() const;
        Udb::OID getSelectedPopAccount() const;
        bool markSelectedAsRead();
    signals:
        void sigUnselect();
        void sigSelectionChanged();
        void sigAccepted( QList<Udb::Obj> );
    public slots:
        void onDelete();
        void onAccept();
        void onShowSource();
        void onSaveAs();
        void onSaveBodyAs();
    protected slots:
        void onSelectionChanged();
    protected:
        static Udb::Obj acceptMail( Udb::Qit& );
    private:
        InboxMdl* d_mdl;
    };

    class InboxMdl : public Udb::InvQueueMdl
	{
	public:
        enum Role { From = SlotNrRole + 1, DateTime, Subject, Opened, Accepted, AttCount };

		InboxMdl(QObject *parent):InvQueueMdl( parent ) {}

		// Overrides
		QVariant data( const Udb::Obj&, int role ) const;
        QVariant data(const Stream::DataCell &, int role) const;
	};

    class InboxDeleg : public QAbstractItemDelegate
	{
	public:
		InboxDeleg( QObject* o, Udb::Transaction* txn ):
            QAbstractItemDelegate(o),d_txn(txn) {}
		void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
        void paintItem ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
		QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
	private:
		Udb::Transaction* d_txn;
	};

}

#endif // INBOXCTRL_H
