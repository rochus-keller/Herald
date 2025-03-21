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

#include "ConfigurationDlg.h"
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QtDebug>
#include <Mail/MailMessage.h>
#include <Udb/Idx.h>
#include "HeTypeDefs.h"
#include "HeraldApp.h"
#include "ObjectHelper.h"
#include "MailObj.h"
using namespace He;

ConfigurationDlg::ConfigurationDlg(Udb::Transaction *txn, QWidget *parent) :
    QDialog(parent),d_txn(txn)
{
    Q_ASSERT( txn != 0 );
    QVBoxLayout* vbox = new QVBoxLayout( this );
    QTabWidget* tab = new QTabWidget( this );
    vbox->addWidget( tab );

    setWindowTitle( tr("Configuration - Herald" ) );

    IdentityTab* ident = new IdentityTab( txn, this );
    tab->addTab( ident, tr("Identities") );

    AccountTab* pops = new AccountTab( txn, TypePopAccount, this );
    tab->addTab( pops, tr("POP3 Accounts") );

    AccountTab* smtps = new AccountTab( txn, TypeSmtpAccount, this );
    tab->addTab( smtps, tr("SMTP Accounts") );

    QDialogButtonBox* bb = new QDialogButtonBox( QDialogButtonBox::Close, Qt::Horizontal, this );
	vbox->addWidget( bb );
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    d_txn->getOrCreateObject( HeraldApp::s_accounts );
    d_txn->commit();
}

AccountTab::AccountTab(Udb::Transaction *txn, quint32 type, QWidget * parent):
    QWidget( parent ),d_txn( txn ),d_type(type)
{
    Q_ASSERT( txn != 0 );
    QHBoxLayout* hbox = new QHBoxLayout( this );

    d_accounts = new QListWidget( this );
    d_accounts->setAlternatingRowColors(true);
    hbox->addWidget( d_accounts );

    QVBoxLayout* vbox = new QVBoxLayout();
    hbox->addLayout( vbox );

    QPushButton* add = new QPushButton( tr("New..."), this );
    vbox->addWidget( add );

    d_edit = new QPushButton( tr("Edit..."), this );
    d_edit->setEnabled( false );
    vbox->addWidget( d_edit );

    d_delete = new QPushButton( tr("Delete..."), this );
    d_delete->setEnabled( false );
    vbox->addWidget( d_delete );

    vbox->addStretch();

    connect( add, SIGNAL(clicked()), this, SLOT(onNew()) );
    connect( d_edit, SIGNAL(clicked()), this, SLOT(onEdit()) );
    connect( d_delete, SIGNAL(clicked()), this, SLOT(onDelete()) );
    connect( d_accounts,SIGNAL(itemSelectionChanged()), this, SLOT(onSelect()) );

    fillList();
}

static void _setText( QListWidgetItem* item, const Udb::Obj& acc )
{
    const bool active = acc.getValue(AttrSrvActive).getBool();
    item->setText( AccountTab::tr("%1 (%2)").arg( acc.getString( AttrText ) ).
                   arg( (active)?"active":"inactive") );
}

void AccountTab::onNew()
{
    Editor dlg( this );
    dlg.setWindowTitle( tr("Create %1 - Herald").arg( HeTypeDefs::prettyName( d_type ) ) );
    if( dlg.edit() )
    {
        Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
        Udb::Obj acc = ObjectHelper::createObject( d_type, accounts );
        dlg.writeTo( acc );
        acc.commit();
        QListWidgetItem* item = new QListWidgetItem( d_accounts );
        _setText( item, acc );
        item->setData( Qt::UserRole, acc.getOid() );
    }
}

void AccountTab::onEdit()
{
    Q_ASSERT( d_accounts->currentItem() != 0 );
    Udb::Obj acc = d_txn->getObject( d_accounts->currentItem()->data(Qt::UserRole).toULongLong() );
    Editor dlg( this );
    dlg.setWindowTitle( tr("Edit %1 - Herald").arg( HeTypeDefs::prettyName( d_type ) ) );
    dlg.readFrom( acc );
    if( dlg.edit() )
    {
        dlg.writeTo( acc );
        acc.commit();
        _setText( d_accounts->currentItem(), acc );
    }
}

static bool _checkAccountInUse( const Udb::Obj& acc )
{
    Udb::Obj accounts = acc.getTxn()->getObject( HeraldApp::s_accounts );
    Udb::Obj sub = accounts.getFirstObj();
    if( !sub.isNull() ) do
    {
        if( sub.getType() == TypeIdentity )
        {
            if( sub.getValueAsObj( AttrIdentPop ).equals( acc ) ||
                    sub.getValueAsObj( AttrIdentSmtp ).equals( acc ) )
                return true;
        }
    }while( sub.next() );
    return false;
}

void AccountTab::onDelete()
{
    Q_ASSERT( d_accounts->currentItem() != 0 );
    Udb::Obj acc = d_txn->getObject( d_accounts->currentItem()->data(Qt::UserRole).toULongLong() );
    if( _checkAccountInUse( acc ) || HeraldApp::inst()->isLocked( acc ) )
    {
        QMessageBox::critical( this, tr("Delete Account - Herald"),
                               tr("This account is in use and cannot be deleted!") );
        return;
    }
    if( QMessageBox::warning( this, tr("Delete Account - Herald"),
                          tr("Do you really want to delete '%1'? This cannot be undone.").
                              arg( acc.getString( AttrText ) ), QMessageBox::Ok | QMessageBox::Cancel,
                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;
    acc.erase();
    d_txn->commit();
    delete d_accounts->currentItem();
}

void AccountTab::onSelect()
{
    const bool selected = !d_accounts->selectedItems().isEmpty();
    d_edit->setEnabled( selected );
    d_delete->setEnabled( selected );
}

void AccountTab::fillList()
{
    Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
    Udb::Obj acc = accounts.getFirstObj();
    if( !acc.isNull() ) do
    {
        if( acc.getType() == d_type )
        {
            QListWidgetItem* item = new QListWidgetItem( d_accounts );
            _setText( item, acc );
            item->setData( Qt::UserRole, acc.getOid() );
        }
    }while( acc.next() );
}

AccountTab::Editor::Editor(QWidget *p)
{
    QVBoxLayout* vbox = new QVBoxLayout( this );

    QFormLayout* form = new QFormLayout();
    vbox->addLayout(form);

    d_name = new QLineEdit( this );
    form->addRow( tr("Account Name:"), d_name );

    d_server = new QLineEdit( this );
    form->addRow( tr("Server Domain:"), d_server );

    d_port = new QLineEdit( this );
    form->addRow( tr("Server Port:"), d_port );

    d_user = new QLineEdit( this );
    form->addRow( tr("User Name:"), d_user );

    d_pwd = new QLineEdit( this );
    form->addRow( tr("Password:"), d_pwd );

    d_ssl = new QCheckBox( this );
    form->addRow( tr("Use SSL:"), d_ssl );

    d_tls = new QCheckBox( this );
    form->addRow( tr("Start TLS:"), d_tls );

    d_active = new QCheckBox( this );
    d_active->setChecked(true);
    form->addRow( tr("Is Active:"), d_active );

    vbox->addStretch();

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
    vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    d_name->setMinimumWidth( 200 ); // ansonsten muss man das Ding immer vergrössern
}

bool AccountTab::Editor::edit()
{
    while( exec() == QDialog::Accepted )
    {
        if( d_name->text().isEmpty() )
        {
            QMessageBox::critical( this, windowTitle(), tr("Name cannot be empty") );
            continue;
        }
        if( !d_port->text().isEmpty() )
        {
            bool ok = false;
            const quint32 port = d_port->text().toUInt( &ok );
            if( !ok || port > 49151 )
            {
                QMessageBox::critical( this, windowTitle(), tr("Invalid Port") );
                continue;
            }
        }
        if( !MailMessage::isValidDomain( d_server->text().toLatin1() ) )
        {
            QMessageBox::critical( this, windowTitle(), tr("Invalid domain name") );
            continue;
        }
        if( !HeTypeDefs::isPrintableAscii( d_user->text().toLatin1(), false, false ) )
        {
            QMessageBox::critical( this, windowTitle(), tr("User name is empty or not ASCII") );
            continue;
        }
        if( !HeTypeDefs::isPrintableAscii( d_pwd->text().toLatin1(), false, false ) )
        {
            QMessageBox::critical( this, windowTitle(), tr("Password is empty or not ASCII") );
            continue;
        }
        return true;
    }
    return false;
}

void AccountTab::Editor::writeTo(Udb::Obj & acc )
{
    acc.setString( AttrText, d_name->text() );
    acc.setAscii( AttrSrvDomain, d_server->text().toLatin1() );
    acc.setAscii( AttrSrvUser, d_user->text().toLatin1() );
    acc.setAscii( AttrSrvPwd, d_pwd->text().toLatin1() );
    acc.setBool( AttrSrvSsl, d_ssl->isChecked() );
    acc.setBool( AttrSrvTls, d_tls->isChecked() );
    acc.setBool( AttrSrvActive, d_active->isChecked() );
    if( d_port->text().isEmpty() )
        acc.clearValue( AttrSrvPort );
    else
        acc.setValue( AttrSrvPort, Stream::DataCell().setUInt16( d_port->text().toUInt() ) );
}

void AccountTab::Editor::readFrom(const Udb::Obj & acc)
{
    d_name->setText( acc.getString( AttrText ) );
    d_server->setText( acc.getString( AttrSrvDomain ) );
    d_user->setText( acc.getString(AttrSrvUser) );
    d_pwd->setText( acc.getString(AttrSrvPwd) );
    d_ssl->setChecked( acc.getValue(AttrSrvSsl).getBool() );
    d_tls->setChecked( acc.getValue(AttrSrvTls).getBool() );
    d_active->setChecked( acc.getValue(AttrSrvActive).getBool() );
    if( acc.hasValue(AttrSrvPort))
        d_port->setText( acc.getString( AttrSrvPort ) );
    else
        d_port->clear();
}

IdentityTab::IdentityTab(Udb::Transaction *txn, QWidget *p):
    QWidget(p), d_txn(txn)
{
    Q_ASSERT( txn != 0 );
    Q_ASSERT( txn != 0 );
    QHBoxLayout* hbox = new QHBoxLayout( this );

    d_identities = new QListWidget( this );
    d_identities->setAlternatingRowColors(true);
    hbox->addWidget( d_identities );

    QVBoxLayout* vbox = new QVBoxLayout();
    hbox->addLayout( vbox );

    QPushButton* add = new QPushButton( tr("New..."), this );
    vbox->addWidget( add );

    d_edit = new QPushButton( tr("Edit..."), this );
    d_edit->setEnabled( false );
    vbox->addWidget( d_edit );

    d_default = new QPushButton( tr("Set Default"), this );
    d_default->setEnabled( false );
    vbox->addWidget( d_default );

    d_delete = new QPushButton( tr("Delete..."), this );
    d_delete->setEnabled( false );
    vbox->addWidget( d_delete );

    vbox->addStretch();

    QPushButton* deduce = new QPushButton( tr("Deduce..."), this );
    vbox->addWidget( deduce );

    connect( add, SIGNAL(clicked()), this, SLOT(onNew()) );
    connect( d_edit, SIGNAL(clicked()), this, SLOT(onEdit()) );
    connect( d_delete, SIGNAL(clicked()), this, SLOT(onDelete()) );
    connect( d_default, SIGNAL(clicked()), this, SLOT(onSetDefault()) );
    connect( d_identities,SIGNAL(itemSelectionChanged()), this, SLOT(onSelect()) );
    connect( deduce, SIGNAL(clicked()), this,SLOT(onDeduce()) );

    fillList();
}

void IdentityTab::onNew()
{
    IdentityDlg dlg( d_txn, this );
    dlg.setWindowTitle( tr("Create Identity - Herald" ) );
    if( dlg.edit() )
    {
        Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
        Udb::Obj id = ObjectHelper::createObject( TypeIdentity, accounts );
        dlg.writeTo( id );
        id.commit();
        QListWidgetItem* item = new QListWidgetItem( d_identities );
        item->setText( id.getString( AttrText ) );
        item->setData( Qt::UserRole, id.getOid() );
    }
}

void IdentityTab::onEdit()
{
    Q_ASSERT( d_identities->currentItem() != 0 );
    Udb::Obj id = d_txn->getObject( d_identities->currentItem()->data(Qt::UserRole).toULongLong() );
    IdentityDlg dlg( d_txn, this );
    dlg.setWindowTitle( tr("Edit Identity - Herald") );
    dlg.readFrom( id );
    if( dlg.edit() )
    {
        dlg.writeTo( id );
        id.commit();
        d_identities->currentItem()->setText( id.getString( AttrText ) );
    }
}

void IdentityTab::onDelete()
{
    Q_ASSERT( d_identities->currentItem() != 0 );
    Udb::Obj id = d_txn->getObject( d_identities->currentItem()->data(Qt::UserRole).toULongLong() );
    if( QMessageBox::warning( this, tr("Delete Identity - Herald"),
                          tr("Do you really want to delete '%1'? This cannot be undone.").
                              arg( id.getString( AttrText ) ), QMessageBox::Ok | QMessageBox::Cancel,
                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;
    id.erase();
    d_txn->commit();
    delete d_identities->currentItem();
}

void IdentityTab::onSelect()
{
    const int count = d_identities->selectedItems().size();
    d_edit->setEnabled( count == 1 );
    d_delete->setEnabled( count > 0 );
    d_default->setEnabled( count == 1 );
}

void IdentityTab::onDeduce()
{
    if( QMessageBox::warning( this, tr("Deduce Identities - Herald"),
                          tr("Do you really want to do this? This cannot be undone."),
                              QMessageBox::Ok | QMessageBox::Cancel,
                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;
    QSet<Udb::Obj> froms;
    Udb::Idx idx( d_txn, IndexDefs::IdxSentOn );
    if( idx.first() ) do
    {
        MailObj mail = d_txn->getObject( idx.getOid() );
        Q_ASSERT(!mail.isNull() );
        if( mail.getType() == TypeOutboundMessage )
        {
            MailObj::MailAddr from = mail.getFrom(false);
            froms.insert( from.d_party.getValueAsObj(AttrPartyAddr) );
        }
    }while( idx.next() );
    foreach( Udb::Obj addr, froms )
    {
        MailObj::getOrCreateIdentity( addr, MailObj::formatAddress( addr, false ) );
    }
    d_txn->commit();
    fillList();
}

void IdentityTab::onDeleteAll()
{
    if( QMessageBox::warning( this, tr("Delete Identity - Herald"),
                          tr("Do you really want to delete all identities? This cannot be undone."),
                              QMessageBox::Ok | QMessageBox::Cancel,
                          QMessageBox::Cancel ) == QMessageBox::Cancel )
        return;
    Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
    Udb::Obj obj = accounts.getFirstObj();
    Udb::Obj last;
    if( !obj.isNull() ) do
    {
        if( !last.isNull() )
        {
            last.erase();
            last = Udb::Obj();
        }
        if( obj.getType() == TypeIdentity )
            last = obj;
    }while( obj.next() );
    d_txn->commit();
    fillList();
}

void IdentityTab::onSetDefault()
{
    Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
    Q_ASSERT( d_identities->currentItem() != 0 );
    Udb::Obj id = d_txn->getObject( d_identities->currentItem()->data(Qt::UserRole).toULongLong() );
    accounts.setValueAsObj(AttrDefaultIdent,id);
    accounts.commit();
    fillList();
}

void IdentityTab::fillList()
{
    d_identities->clear();
    Udb::Obj accounts = d_txn->getOrCreateObject( HeraldApp::s_accounts );
    Udb::Obj def = accounts.getValueAsObj(AttrDefaultIdent);
    Udb::Obj obj = accounts.getFirstObj();
    if( !obj.isNull() ) do
    {
        if( obj.getType() == TypeIdentity )
        {
            QListWidgetItem* item = new QListWidgetItem( d_identities );
            item->setText( obj.getString( AttrText ) );
            item->setData( Qt::UserRole, obj.getOid() );
            if( obj.equals( def ) )
            {
                QFont f = item->font();
                f.setBold(true);
                item->setFont( f );
            }
        }
    }while( obj.next() );
}

IdentityDlg::IdentityDlg(Udb::Transaction * txn, QWidget *p):QDialog(p),d_txn( txn )
{
    Q_ASSERT( txn != 0 );

    QVBoxLayout* vbox = new QVBoxLayout( this );

    QGridLayout* grid = new QGridLayout();
    vbox->addLayout( grid );
    int row = 0;
    QPushButton* b;
    grid->addWidget( new QLabel( tr("Name:"), this ), row, 0 );
    d_name = new QLineEdit( this );
    grid->addWidget( d_name, row, 1, 1, 3 );
    row++;
    grid->addWidget( new QLabel( tr("Email Address:"), this ), row, 0 );
    d_addrLbl = new QLabel( this );
    grid->addWidget( d_addrLbl, row, 1 );
    b = new QPushButton( tr("Clear"), this );
    grid->addWidget( b, row, 2 );
    connect( b, SIGNAL(clicked()), this, SLOT(clearAddr()) );
    b = new QPushButton( tr("Set"), this );
    grid->addWidget( b, row, 3 );
    connect( b, SIGNAL(clicked()), this, SLOT(setAddr()) );
    row++;
    grid->addWidget( new QLabel( tr("POP3 Account:"), this ), row, 0 );
    d_popLbl = new QLabel( this );
    grid->addWidget( d_popLbl, row, 1 );
    b = new QPushButton( tr("Clear"), this );
    grid->addWidget( b, row, 2 );
    connect( b, SIGNAL(clicked()), this, SLOT(clearPop()) );
    b = new QPushButton( tr("Set"), this );
    grid->addWidget( b, row, 3 );
    connect( b, SIGNAL(clicked()), this, SLOT(setPop()) );
    row++;
    grid->addWidget( new QLabel( tr("SMTP Account:"), this ), row, 0 );
    d_smtpLbl = new QLabel( this );
    grid->addWidget( d_smtpLbl, row, 1 );
    b = new QPushButton( tr("Clear"), this );
    grid->addWidget( b, row, 2 );
    connect( b, SIGNAL(clicked()), this, SLOT(clearSmtp()) );
    b = new QPushButton( tr("Set"), this );
    grid->addWidget( b, row, 3 );
    connect( b, SIGNAL(clicked()), this, SLOT(setSmtp()) );
	row++;
	grid->addWidget( new QLabel( tr("Private Key:"), this ), row, 0 );
	d_privKey = new QLabel( this );
	grid->addWidget( d_privKey, row, 1 );
	b = new QPushButton( tr("Clear"), this );
	grid->addWidget( b, row, 2 );
	connect( b, SIGNAL(clicked()), this, SLOT(clearPriv()) );
	b = new QPushButton( tr("Set"), this );
	grid->addWidget( b, row, 3 );
	connect( b, SIGNAL(clicked()), this, SLOT(setPriv()) );

    vbox->addStretch();

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
    vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    d_name->setMinimumWidth( 200 ); // ansonsten muss man das Ding immer vergrössern
}

bool IdentityDlg::edit()
{
    while( exec() == QDialog::Accepted )
    {
        if( d_name->text().isEmpty() )
        {
            QMessageBox::critical( this, windowTitle(), tr("Name cannot be empty") );
            continue;
        }
        if( !addrUnique() )
        {
            QMessageBox::critical( this, windowTitle(), tr("Address is already in use by another identity") );
            continue;
        }
        return true;
    }
    return false;
}

void IdentityDlg::writeTo(Udb::Obj & o)
{
    o.setString( AttrText, d_name->text() );
    o.setValueAsObj( AttrIdentAddr, d_addr );
    o.setValueAsObj( AttrIdentPop, d_pop );
    o.setValueAsObj( AttrIdentSmtp, d_smtp );
	o.setString( AttrPrivateKey, d_privKey->toolTip() );
}

void IdentityDlg::readFrom(const Udb::Obj & o)
{
    d_id = o;
    d_name->setText( o.getString( AttrText ) );
    d_addr = o.getValueAsObj(AttrIdentAddr);
    d_pop = o.getValueAsObj(AttrIdentPop);
    d_smtp = o.getValueAsObj(AttrIdentSmtp);
	d_privKey->setToolTip( o.getString( AttrPrivateKey ) );
	if( !d_privKey->toolTip().isEmpty() )
		d_privKey->setText( QFileInfo(d_privKey->toolTip()).fileName() );
    setLabels();
}

bool IdentityDlg::addrUnique()
{
    if( d_addr.isNull(true) )
        return true;
    Udb::Idx idx( d_txn, IndexDefs::IdxIdentAddr );
    if( idx.seek( d_addr ) )
    {
        Udb::Obj o = d_txn->getObject( idx.getOid() );
        Q_ASSERT( !o.isNull() );
        if( o.getType() == 0 )
        {
            o.erase();
            return true;
        }else
            return idx.getOid() == d_id.getOid();
    }else
        return true;
}

void IdentityDlg::setLabels()
{
    d_addrLbl->clear();
    d_popLbl->clear();
    d_smtpLbl->clear();
    if( !d_addr.isNull() )
    {
        d_addrLbl->setText( d_addr.getString( AttrText ) );
		d_addrLbl->setToolTip( d_addr.getString( AttrEmailAddress ) );
        if( d_addrLbl->text().isEmpty() )
            d_addrLbl->setText( d_addr.getString( AttrEmailAddress ) );
    }
    if( !d_pop.isNull() )
        d_popLbl->setText( d_pop.getString( AttrText ) );
    if( !d_smtp.isNull() )
        d_smtpLbl->setText( d_smtp.getString( AttrText ) );
}

void IdentityDlg::clearAddr()
{
    d_addr = Udb::Obj();
    setLabels();
}

void IdentityDlg::clearPop()
{
    d_pop = Udb::Obj();
    setLabels();
}

void IdentityDlg::clearSmtp()
{
    d_smtp = Udb::Obj();
    setLabels();
}

void IdentityDlg::setAddr()
{
    d_addr = MailObj::simpleAddressEntry( this, d_txn, d_addr );
    setLabels();
}

void IdentityDlg::setPop()
{
    AccountDlg dlg( d_txn, TypePopAccount, this );
    Udb::Obj o = dlg.select();
    if( !o.isNull() )
    {
        d_pop = o;
        setLabels();
    }
}

void IdentityDlg::setSmtp()
{
    AccountDlg dlg( d_txn, TypeSmtpAccount, this );
    Udb::Obj o = dlg.select();
    if( !o.isNull() )
    {
        d_smtp = o;
        setLabels();
	}
}

void IdentityDlg::clearPriv()
{
	d_privKey->clear();
}

AccountDlg::AccountDlg(Udb::Transaction *txn, quint32 type, QWidget * p):
    QDialog(p),d_txn(txn),d_type(type)
{
    Q_ASSERT( txn != 0 );

    setWindowTitle( tr("Select %1 - Herald").arg( HeTypeDefs::prettyName( d_type ) ) );
    QVBoxLayout* vbox = new QVBoxLayout( this );

    d_accounts = new QListWidget( this );
    d_accounts->setAlternatingRowColors(true);
    vbox->addWidget( d_accounts );

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
    vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
    connect( d_accounts, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(accept()) );

}

Udb::Obj AccountDlg::select()
{
    Udb::Obj accounts = d_txn->getObject( HeraldApp::s_accounts );
    Udb::Obj acc = accounts.getFirstObj();
    if( !acc.isNull() ) do
    {
        if( acc.getType() == d_type )
        {
            QListWidgetItem* item = new QListWidgetItem( d_accounts );
            _setText( item, acc );
            item->setData( Qt::UserRole, acc.getOid() );
        }
    }while( acc.next() );
    if( exec() == QDialog::Accepted && d_accounts->currentItem() )
    {
        return d_txn->getObject( d_accounts->currentItem()->data(Qt::UserRole).toULongLong() );
    }
	return Udb::Obj();
}

void IdentityDlg::setPriv()
{
	const QString path = QFileDialog::getOpenFileName( this, tr("Select Private Key File"), QString(),
								  tr("PEM File (*.pem *.key)") );
	if( !path.isNull() )
	{
		d_privKey->setToolTip( path );
		if( !d_privKey->toolTip().isEmpty() )
			d_privKey->setText( QFileInfo(path).fileName() );
	}
}

