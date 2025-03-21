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

#include "AddressListCtrl.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QtDebug>
#include <QKeyEvent>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>
#include <QApplication>
#include <QMessageBox>
#include <Udb/Transaction.h>
#include <Mail/MailMessage.h>
#include <Udb/Idx.h>
#include "HeTypeDefs.h"
#include "AddressIndexer.h"
#include "HeraldApp.h"
#include "PersonPropsDlg.h"
#include "ObjectHelper.h"
#include "PersonListView.h"
using namespace He;

AddressListCtrl::AddressListCtrl(QWidget *parent) :
    QObject(parent),d_showObsolete(false)
{
    d_doSearch.setSingleShot(true);
    d_doSearch.setInterval( 300 );
    connect( &d_doSearch, SIGNAL(timeout()), this, SLOT(onSearch()) );
    d_showObsolete = HeraldApp::inst()->getSet()->value( "AddressList/ShowObsolete", true ).toBool();
}

QWidget *AddressListCtrl::getWidget() const
{
    return static_cast<QWidget*>( parent() );
}

AddressListCtrl *AddressListCtrl::create(QWidget *parent, AddressIndexer * idx)
{
    Q_ASSERT( idx != 0 );
    QWidget* pane = new QWidget( parent );
    QVBoxLayout* vbox = new QVBoxLayout( pane );
	vbox->setMargin( 0 );
	vbox->setSpacing( 1 );

    AddressListCtrl* ctrl = new AddressListCtrl(pane);
    ctrl->d_idx = idx;

    ctrl->d_edit = new QLineEdit( pane );
    vbox->addWidget( ctrl->d_edit );
    connect( ctrl->d_edit,SIGNAL(textEdited(QString)),ctrl,SLOT(onEdited(QString)) );

    ctrl->d_list = new QListWidget( pane );
    ctrl->d_list->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
    ctrl->d_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    vbox->addWidget( ctrl->d_list );
    connect( ctrl->d_list, SIGNAL(itemPressed(QListWidgetItem*)), ctrl,SLOT(onClicked(QListWidgetItem*)) );
    return ctrl;
}

void AddressListCtrl::addCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Edit Text..."), this,SLOT(onEditText()), tr("CTRL+E"), true );
    pop->addCommand( tr("Create Person..."), this,SLOT(onCreatePerson()), tr("CTRL+N"), true );
    pop->addCommand( tr("Assign to Person..."), this,SLOT(onAssignPerson()) );
    pop->addCommand( tr("Mark Obsolete"), this,SLOT(onObsolete()) );
	pop->addCommand( tr("Uses Outlook 2010"), this,SLOT(onOutlook2010()) );
	pop->addCommand( tr("Copy"), this,SLOT(onCopy()), tr("CTRL+C"), true );
    pop->addSeparator();
    pop->addCommand( tr("Show Obsolete"), this,SLOT(onShowObsolete()) );
    pop->addCommand( tr("Rebuild Index"), this,SLOT(onRebuildIndex()) );
	pop->addCommand( tr("Remove Doublettes"), this,SLOT(onRemoveDoubles()) );
}

Udb::Obj AddressListCtrl::getSelected() const
{
	QList<QListWidgetItem *> l = d_list->selectedItems();
	if( l.size() != 1 )
		return Udb::Obj();

   return d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
}

void AddressListCtrl::onEdited(const QString & str)
{
    d_doSearch.start();
}

void AddressListCtrl::onClicked(QListWidgetItem* item)
{
    if( item == 0 )
        return;
    emit sigSelected( d_idx->getTxn()->getObject( item->data(Qt::UserRole).toULongLong() ) );
}

void AddressListCtrl::setName(QListWidgetItem * item, const Udb::Obj & addr)
{
    QString name = addr.getString( AttrText );
    if( name.isEmpty() )
        item->setText( addr.getString( AttrEmailAddress ) );
    else
        item->setText( tr("%1 [%2]").arg(name).arg(addr.getString( AttrEmailAddress )) );
    Udb::Obj pers = addr.getParent();
    QString persString;
    if( !pers.isNull() )
        persString = QChar('\n') + pers.getString( AttrText );
    item->setToolTip( item->text() + persString );
}

void AddressListCtrl::onEditText()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    ENABLED_IF( l.size() == 1 );

    Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
    bool ok;
    QString str = QInputDialog::getText( getWidget(), tr("Edit Text"),
                           tr("Enter a brief description:"), QLineEdit::Normal,
                                         addr.getString( AttrText ), &ok );
    if( !ok )
        return;
    addr.setString( AttrText, str );
    addr.setTimeStamp(AttrModifiedOn);
    setName( l.first(), addr );
    addr.commit();
}

void AddressListCtrl::onObsolete()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    if( l.size() != 1 )
        return;
    Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
    CHECKED_IF( !addr.isNull(), !addr.isNull() && addr.getValue( AttrEmailObsolete ).getBool() );

    addr.setValue( AttrEmailObsolete, Stream::DataCell().setBool(
                       !addr.getValue( AttrEmailObsolete ).getBool() ) );
    addr.commit();

    QFont f = l.first()->font();
    f.setStrikeOut( addr.getValue( AttrEmailObsolete ).getBool() );
	l.first()->setFont( f );

}

void AddressListCtrl::onOutlook2010()
{
	QList<QListWidgetItem *> l = d_list->selectedItems();
	if( l.size() != 1 )
		return;
	Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
	CHECKED_IF( !addr.isNull(), !addr.isNull() && addr.getValue( AttrUseKeyId ).getBool() );

	addr.setValue( AttrUseKeyId, Stream::DataCell().setBool(
					   !addr.getValue( AttrUseKeyId ).getBool() ) );
	addr.commit();
}

void AddressListCtrl::onShowObsolete()
{
    CHECKED_IF( true, d_showObsolete );
    d_showObsolete = !d_showObsolete;
    HeraldApp::inst()->getSet()->setValue( "AddressList/ShowObsolete", d_showObsolete );
    onSearch();
}

void AddressListCtrl::onRebuildIndex()
{
    ENABLED_IF(true);

    d_idx->indexAll(true);
    d_idx->indexAll();
}

void AddressListCtrl::onCopy()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    ENABLED_IF( !l.isEmpty() );

    QList<Udb::Obj> objs;
    QMimeData* mime = new QMimeData();
    foreach( QListWidgetItem* i, l )
	{
        Udb::Obj o = d_idx->getTxn()->getObject( i->data(Qt::UserRole).toULongLong() );
		Q_ASSERT( !o.isNull() );
        objs.append( o );
	}
    HeTypeDefs::writeObjectRefs( mime, objs );
    QApplication::clipboard()->setMimeData( mime );
}

void AddressListCtrl::onCreatePerson()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    if( l.size() != 1 )
        return;
    Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
    ENABLED_IF( !addr.isNull() && addr.getParent().isNull() );

    PersonPropsDlg dlg( getWidget() );
    dlg.setText( addr.getString( AttrText ) );
    if( dlg.exec() != QDialog::Accepted )
		return;

	Udb::Obj pers = ObjectHelper::createObject( TypePerson, d_idx->getTxn() );
	dlg.saveTo( pers );
    addr.aggregateTo( pers );
    pers.commit();

    QApplication::setOverrideCursor( Qt::WaitCursor );
    Udb::Idx idx( d_idx->getTxn(), IndexDefs::IdxPartyAddrDate );
    if( idx.seek( addr ) ) do
    {
        Udb::Obj party = pers.getObject( idx.getOid() );
        if( HeTypeDefs::isParty( party.getType() ) )
        {
            party.setValueAsObj(AttrPartyPers, pers );
        }
    }while( idx.nextKey() );
    pers.commit();
    QApplication::restoreOverrideCursor();
}

void AddressListCtrl::onAssignPerson()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    if( l.size() != 1 )
        return;
    Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
    ENABLED_IF( !addr.isNull() );

    PersonSelectorDlg dlg( getWidget(), d_idx->getTxn() );
    quint64 oid = dlg.selectOne();
    if( oid == 0 )
        return;
    Udb::Obj pers = d_idx->getTxn()->getObject( oid );
    addr.aggregateTo( pers );
    QApplication::setOverrideCursor( Qt::WaitCursor );
    Udb::Idx idx( d_idx->getTxn(), IndexDefs::IdxPartyAddrDate );
    if( idx.seek( addr ) ) do
    {
        Udb::Obj party = pers.getObject( idx.getOid() );
        if( HeTypeDefs::isParty( party.getType() ) )
        {
            party.setValueAsObj(AttrPartyPers, pers );
        }
    }while( idx.nextKey() );
    pers.commit();
    QApplication::restoreOverrideCursor();
}

void AddressListCtrl::onRemoveDoubles()
{
    QList<QListWidgetItem *> l = d_list->selectedItems();
    if( l.size() != 1 )
        return;
    Udb::Obj addr = d_idx->getTxn()->getObject( l.first()->data(Qt::UserRole).toULongLong() );
    ENABLED_IF( !addr.isNull() );

    Udb::Idx addrIdx( d_idx->getTxn(), IndexDefs::IdxEmailAddress );
    int dblCount = 0, updCount = 0;
    if( addrIdx.seek( addr.getValue(AttrEmailAddress) ) ) do
    {
        if( addrIdx.getOid() != addr.getOid() )
        {
            Udb::Obj dbl = d_idx->getTxn()->getObject( addrIdx.getOid() );
            // qDebug() << "Found doublette" << dbl.getOid();
            Udb::Idx partyIdx( d_idx->getTxn(), IndexDefs::IdxPartyAddrDate );
            if( partyIdx.seek( dbl ) ) do
            {
                Udb::Obj party = d_idx->getTxn()->getObject( partyIdx.getOid() );
//                qDebug() << "Used in" << HeTypeDefs::prettyName( party.getType() ) <<
//                    party.getValue(AttrPartyDate).getDateTime() << party.getString( AttrText ) <<
//                    party.getParent().getString( AttrText );
                party.setValueAsObj( AttrPartyAddr, addr );
                updCount++;
            }while( partyIdx.nextKey() );
            dbl.erase();
            dblCount++;
        }
    }while( addrIdx.nextKey() );
    d_idx->getTxn()->commit();
    QMessageBox::information( getWidget(), tr("Remove Doublettes"),
                              tr("%1 Roles updated, %2 double Addresses joined").arg( updCount).arg(dblCount ) );
}

void AddressListCtrl::onSearch()
{
    //qDebug() << "onSearch" << QTime::currentTime().toString("hh:mm:ss:zzz"); // TEST
    d_list->clear();
    if( d_edit->text().isEmpty() )
        return;
    QList<Udb::Obj> res = d_idx->find( d_edit->text() );
    QFont strikeOut = d_list->font();
    strikeOut.setStrikeOut( true );
    foreach( Udb::Obj o, res )
    {
        const bool isObsolete = o.getValue( AttrEmailObsolete ).getBool();
        if( !isObsolete || d_showObsolete )
        {
            QListWidgetItem* item = new QListWidgetItem( d_list );
            QString addr = o.getString( AttrEmailAddress );
            setName( item, o );
            item->setData( Qt::UserRole, o.getOid() );
            item->setIcon( QIcon( ":/images/at-sign-small.png") );
            if( !MailMessage::isValidEmailAddress( addr.toLatin1() ) )
                item->setBackground( Qt::red );
            if( isObsolete )
                item->setFont( strikeOut );
        }
    }
}

AddressSelectorPopup::AddressSelectorPopup(AddressIndexer *idx, QWidget * p):
    QWidget( p, Qt::Popup ),d_idx(idx)
{
    Q_ASSERT( idx != 0 );

    setAttribute( Qt::WA_DeleteOnClose );

    QVBoxLayout* vbox = new QVBoxLayout( this );
    vbox->setMargin(0);
    vbox->setSpacing(0);

    d_text = new QLabel( this );
    //d_text->setTextInteractionFlags( Qt::TextEditorInteraction );
    d_text->setAutoFillBackground(true);
    d_text->setBackgroundRole( QPalette::Light );
    d_text->setFrameShape( QFrame::StyledPanel );
    vbox->addWidget( d_text );

    d_list = new QListWidget( this );
    d_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d_list->installEventFilter( this );
    connect( d_list, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(onAccept()) );
    vbox->addWidget( d_list );
    d_list->setFocus();
}

void AddressSelectorPopup::setText(const QString & str)
{
    d_text->setText( str );
}

void AddressSelectorPopup::fillList(const QString & str)
{
    d_list->clear();
    if( str.size() < 2 )
        return;
    QList<Udb::Obj> res = d_idx->find( str );
    foreach( Udb::Obj o, res )
    {
        const bool isObsolete = o.getValue( AttrEmailObsolete ).getBool();
        if( !isObsolete )
        {
            QListWidgetItem* item = new QListWidgetItem( d_list );
            QString name = o.getString( AttrText );
            QString addr = o.getString( AttrEmailAddress );
            if( name.isEmpty() )
                item->setText( addr );
            else
                item->setText( tr("%1 [%2]").arg(name).arg(addr) );
            item->setData( Qt::UserRole, o.getOid() );
            item->setIcon( QIcon( ":/images/at-sign-small.png") );
        }
    }
}

bool AddressSelectorPopup::eventFilter(QObject *obj, QEvent *event)
{
    if( event->type() == QEvent::KeyPress )
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        switch( keyEvent->key() )
        {
        case Qt::Key_Backspace:
            d_text->setText( d_text->text().left( d_text->text().size() - 1 ) );
            fillList( d_text->text() );
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            onAccept();
            return true;
        default:
            if( !keyEvent->text().isEmpty() && keyEvent->text()[0].isPrint() )
            {
                d_text->setText( d_text->text() + keyEvent->text() );
                fillList( d_text->text() );
                return true;
            }else
                return QObject::eventFilter(obj, event);
        }
        return true;
    } else
    {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}

void AddressSelectorPopup::onAccept()
{
    QList<QListWidgetItem *> i = d_list->selectedItems();
    if( i.size() == 1 )
    {
        Udb::Obj o = d_idx->getTxn()->getObject( i.first()->data( Qt::UserRole ).toULongLong() );
        emit sigSelected( o.getString( AttrText ), o.getString( AttrEmailAddress ) );
    }else
        emit sigSelected( d_text->text(), QString() );
    close();
}

