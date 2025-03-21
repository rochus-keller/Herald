#ifndef ADDRESSLISTCTRL_H
#define ADDRESSLISTCTRL_H

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
#include <QTimer>
#include <Udb/Obj.h>
#include <GuiTools/AutoMenu.h>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QLabel;

namespace He
{
    class AddressIndexer;

    class AddressListCtrl : public QObject
    {
        Q_OBJECT
    public:
        explicit AddressListCtrl(QWidget *parent);
        QWidget* getWidget() const;
        static AddressListCtrl* create(QWidget* parent, AddressIndexer* );
        void addCommands( Gui::AutoMenu* );
		Udb::Obj getSelected() const;
    signals:
        void sigSelected(const Udb::Obj& );
    public slots:
        void onEditText();
        void onObsolete();
		void onOutlook2010();
        void onShowObsolete();
        void onRebuildIndex();
        void onCopy();
        void onCreatePerson();
        void onAssignPerson();
        void onRemoveDoubles();
        void onSearch();
    protected slots:
        void onEdited( const QString& );
        void onClicked(QListWidgetItem* item);
    protected:
        static void setName( QListWidgetItem*, const Udb::Obj& );
    private:
        QLineEdit* d_edit;
        QListWidget* d_list;
        AddressIndexer* d_idx;
        QTimer d_doSearch;
        bool d_showObsolete;
    };

    class AddressSelectorPopup : public QWidget
    {
        Q_OBJECT
    public:
        AddressSelectorPopup(AddressIndexer*, QWidget*);
        void setText( const QString& );
    signals:
        void sigSelected( const QString& name, const QString& addr );
    protected slots:
        void onAccept();
    protected:
        void fillList( const QString& );
        // overrides
        bool eventFilter(QObject *obj, QEvent *event);
    private:
        AddressIndexer* d_idx;
        QLabel* d_text;
        QListWidget* d_list;
    };
}

#endif // ADDRESSLISTCTRL_H
