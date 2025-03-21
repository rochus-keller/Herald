#ifndef CONFIGURATIONDLG_H
#define CONFIGURATIONDLG_H

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

#include <QDialog>
#include <Udb/Transaction.h>
#include <QValidator>

class QListWidget;
class QPushButton;
class QLineEdit;
class QCheckBox;
class QLabel;

namespace He
{
    class ConfigurationDlg : public QDialog
    {
        Q_OBJECT
    public:
        explicit ConfigurationDlg(Udb::Transaction* txn, QWidget *parent = 0);
    private:
        Udb::Transaction* d_txn;
    };

    class AccountTab : public QWidget
    {
        Q_OBJECT
    public:
        AccountTab( Udb::Transaction* txn, quint32 type, QWidget* );
    protected slots:
        void onNew();
        void onEdit();
        void onDelete();
        void onSelect();
    protected:
        void fillList();
    private:
        struct Editor : public QDialog
        {
            Editor( QWidget* p );
            QLineEdit* d_name;
            QLineEdit* d_server;
            QLineEdit* d_port;
            QLineEdit* d_user;
            QLineEdit* d_pwd;
            QCheckBox* d_ssl;
            QCheckBox* d_tls;
            QCheckBox* d_active;
            bool edit();
            void writeTo( Udb::Obj& );
            void readFrom( const Udb::Obj& );
        };

        Udb::Transaction* d_txn;
        quint32 d_type;
        QListWidget* d_accounts;
        QPushButton* d_edit;
        QPushButton* d_delete;
    };

    class AccountDlg : public QDialog
    {
    public:
        AccountDlg( Udb::Transaction* txn, quint32 type, QWidget* );
        Udb::Obj select();
    private:
        Udb::Transaction* d_txn;
        quint32 d_type;
        QListWidget* d_accounts;
    };

    class IdentityDlg : public QDialog
    {
        Q_OBJECT
    public:
        IdentityDlg( Udb::Transaction*, QWidget* p );
        bool edit();
        void writeTo( Udb::Obj& );
        void readFrom( const Udb::Obj& );
        bool addrUnique();
    protected:
        void setLabels();
    protected slots:
        void clearAddr();
        void clearPop();
        void clearSmtp();
        void setAddr();
        void setPop();
        void setSmtp();
		void clearPriv();
		void setPriv();
    private:
        Udb::Transaction* d_txn;
        QLineEdit* d_name;
        QLabel* d_addrLbl;
        QLabel* d_popLbl;
        QLabel* d_smtpLbl;
		QLabel* d_privKey;
        Udb::Obj d_addr;
        Udb::Obj d_pop;
        Udb::Obj d_smtp;
        Udb::Obj d_id;
    };
    class IdentityTab : public QWidget
    {
        Q_OBJECT
    public:
        IdentityTab( Udb::Transaction* txn, QWidget* );
    protected slots:
        void onNew();
        void onEdit();
        void onDelete();
        void onSelect();
        void onDeduce();
        void onDeleteAll();
        void onSetDefault();
    protected:
        void fillList();
    private:
        Udb::Transaction* d_txn;
        QListWidget* d_identities;
        QPushButton* d_edit;
        QPushButton* d_delete;
        QPushButton* d_default;
    };
}

#endif // CONFIGURATIONDLG_H
