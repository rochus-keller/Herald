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

#include "ResendToDlg.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QLabel>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <Udb/Transaction.h>
#include "HeraldApp.h"
#include "HeTypeDefs.h"
#include "AddressListCtrl.h"
#include "AddressIndexer.h"
#include "MailTextEdit.h"
#include "MailObj.h"
using namespace He;

static QLabel* _label( const QString& text, QWidget* parent )
{
    QLabel* l = new QLabel( text, parent );
    QFont f = l->font();
    f.setBold( true );
    l->setFont( f );
    return l;
}
ResendToDlg::ResendToDlg(AddressIndexer *idx, QWidget *parent) :
    QDialog(parent),d_idx(idx)
{
    Q_ASSERT( idx != 0 );

    setWindowTitle( tr("Resend To - Herald" ));
    QVBoxLayout* vbox = new QVBoxLayout( this );
	// vbox->setMargin( 0 );
    vbox->setSpacing( 2 );

    QFormLayout* form = new QFormLayout();
	d_form = form;
    form->setMargin( 0 );
    form->setSpacing( 2 );
    vbox->addLayout( form );

    d_from = new QComboBox( this );
    d_from->setInsertPolicy( QComboBox::InsertAlphabetically );
    d_from->setSizeAdjustPolicy( QComboBox::AdjustToContents );
    //d_from->setEditable(true);
    form->addRow( _label( tr("From:"), this ), d_from );

    d_to = new MailTextEdit( d_idx->getTxn(), this );
    d_to->setAutoFillBackground(true);
    d_to->setBackgroundRole( QPalette::Light );
    d_to->setMinimumWidth( 300 );
    connect( d_to, SIGNAL(sigKeyPressed(QString)), this, SLOT(onKeyPress(QString)) );
    form->addRow( _label( tr("To:"), this ), d_to );

    vbox->addStretch();

    QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
    vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));

    fillFrom();
    d_to->setFocus();
}

void ResendToDlg::setFrom(const Udb::Obj & o)
{
    d_from->setCurrentIndex( d_from->findData( o.getOid() ));
}

QString ResendToDlg::getFrom() const
{
    Udb::Obj id = d_idx->getTxn()->getObject( d_from->itemData( d_from->currentIndex() ).toULongLong() );
    if( id.isNull() )
        return QString();
    else
        return MailObj::formatAddress( id.getValueAsObj(AttrIdentAddr), true );
}

Udb::Obj ResendToDlg::getIdentity() const
{
	return d_idx->getTxn()->getObject( d_from->itemData( d_from->currentIndex() ).toULongLong() );
}

void ResendToDlg::setTo(const Udb::Obj & addr)
{
	if( !addr.isNull() )
		d_to->addAddress( MailObj::formatAddress( addr, true ), QByteArray() );
}

QStringList ResendToDlg::getTo() const
{
    QStringList addrs;
    QList<MailTextEdit::Party> tos = d_to->getParties();
    foreach( MailTextEdit::Party p, tos )
    {
        addrs.append( p.render() );
    }
	return addrs;
}

QByteArray ResendToDlg::getToBml() const
{
    return d_to->getBml();
}

bool ResendToDlg::select()
{
    while( exec() == QDialog::Accepted )
    {
        Udb::Obj id = d_idx->getTxn()->getObject( d_from->itemData( d_from->currentIndex() ).toULongLong() );
        if( id.isNull(true) )
        {
            QMessageBox::critical( this, windowTitle(),  tr("No identity selected!") );
            continue;
        }
        Udb::Obj addrObj = id.getValueAsObj(AttrIdentAddr);
        if( addrObj.isNull(true) )
        {
            QMessageBox::critical( this, windowTitle(),
                                   tr("Selected identity has no associated Email address!") );
            continue;
        }
        QList<MailTextEdit::Party> tos = d_to->getParties();
        foreach( MailTextEdit::Party p, tos )
        {
            if( !p.d_error.isEmpty() )
            {
                QMessageBox::critical( this, windowTitle(),
                    tr("Invalid Email address in To: %1 (%2)")
                    .arg( p.d_name ).arg( p.d_error ) );
                continue;
            }
        }
        if( tos.isEmpty() )
        {
            QMessageBox::critical( this, windowTitle(), tr("To Email address is missing" ) );
            return false;
        }
        return true;
    }
    return false;
}

void ResendToDlg::onKeyPress(const QString & str)
{
    MailTextEdit* te = dynamic_cast<MailTextEdit*>( sender() );
    Q_ASSERT( te != 0 );

    AddressSelectorPopup* asp = new AddressSelectorPopup( d_idx, this );
    connect( asp, SIGNAL(sigSelected(QString,QString)), te, SLOT(onSelected(QString,QString)) );
    asp->setText( str );
    QRect r = te->geometry();
    r.moveTo( mapToGlobal( r.bottomLeft() ) );
    r.setHeight( 200 );
    asp->setGeometry( r );
    asp->show();
}

void ResendToDlg::fillFrom()
{
    d_from->addItem( tr("<undefined>"), 0 );

    Udb::Obj accounts = d_idx->getTxn()->getObject( HeraldApp::s_accounts );
    Udb::Obj def = accounts.getValueAsObj(AttrDefaultIdent);
    Udb::Obj obj = accounts.getFirstObj();
    if( !obj.isNull() ) do
    {
        if( obj.getType() == TypeIdentity )
        {
            d_from->addItem( obj.getString( AttrText ), obj.getOid() );
        }
    }while( obj.next() );
    d_from->setCurrentIndex( d_from->findData( def.getOid() ));
	d_from->setMaxVisibleItems( d_from->count() );
}

SendCertificateDlg::SendCertificateDlg(AddressIndexer *idx, QWidget *parent):
	ResendToDlg( idx, parent )
{
	setWindowTitle( tr("Send Certificate To - Herald" ));

	d_subject = new QLineEdit( this );
	d_subject->setText( tr("Mein Email-Zertifikat") );
	d_form->addRow( _label( tr("Subject:"), this ), d_subject );
	d_msg = new QPlainTextEdit( this );
	d_msg->setPlainText( tr("Dies ist eine signierte Nachricht, die mein Email-Zertifikat enthält.\n"
							"Mit freundlichen Grüssen") );
	d_form->addRow( _label( tr("Message:"), this ), d_msg );

	QWidget::setTabOrder( d_to, d_subject );
	QWidget::setTabOrder( d_subject, d_msg );
}

QString SendCertificateDlg::getMsg() const
{
	return d_msg->toPlainText();
}

QString SendCertificateDlg::getSubject() const
{
	return d_subject->text();
}
