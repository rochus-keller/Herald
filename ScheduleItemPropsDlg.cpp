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

#include "ScheduleItemPropsDlg.h"
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QCalendarWidget>
#include <QMessageBox>
#include <QTimeEdit>
#include <Oln2/OutlineUdbMdl.h>
#include <Udb/Transaction.h>
#include "HeTypeDefs.h"
using namespace He;
using namespace Stream;

// 1:1 aus MasterPlan

ScheduleItemPropsDlg::ScheduleItemPropsDlg( QWidget* p ):QDialog( p )
{
	setWindowTitle( tr("Schedule Item Properties") );

	QVBoxLayout* vbox = new QVBoxLayout( this );

	QHBoxLayout* header = new QHBoxLayout();
	header->setMargin(0);
	header->setSpacing(2);
	d_pix = new QLabel( this );
	header->addWidget( d_pix );
	d_class = new QLabel( this );
	header->addWidget( d_class );
	header->addStretch();
	vbox->addLayout( header );

	QGridLayout* grid = new QGridLayout();
	vbox->addLayout( grid );

    int row = 0;
	// AttrText
	d_title = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "Name:" ), this ), row, 0 );
	grid->addWidget( d_title, row, 1, 1, 3 );
    row++;
	// AttrInternalId, AttrCustomId
	d_ident = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "Ident.:" ), this ), row, 0 );
	grid->addWidget( d_ident, row, 1 );

	d_altIdent = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "Alt. Ident.:" ), this ), row, 2 );
	grid->addWidget( d_altIdent, row, 3 );
    row++;
	// AttrStartDate
	d_start = new QDateTimeEdit( this );
	d_start->setDisplayFormat( tr("dd.MM.yyyy") );;
	d_start->setCalendarPopup( true );
    d_start->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
	d_start->calendarWidget()->setFirstDayOfWeek( Qt::Monday );
	d_start->calendarWidget()->setGridVisible( true );
	grid->addWidget( new QLabel( tr( "Start Date:" ), this ), row, 0 );
	grid->addWidget( d_start, row, 1 );

	// AttrEndDate
	d_end = new QDateTimeEdit( this );
	d_end->setDisplayFormat( tr("dd.MM.yyyy") );;
	d_end->setCalendarPopup( true );
    d_end->calendarWidget()->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
	d_end->calendarWidget()->setFirstDayOfWeek( Qt::Monday );
	d_end->calendarWidget()->setGridVisible( true );
	d_end->setEnabled( false );
	grid->addWidget( new QLabel( tr( "End Date:" ), this ), row, 2 );
	grid->addWidget( d_end, row, 3 );
    row++;
	// AttrTimeSlot
	d_from = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "Start Time:" ), this ), row, 0 );
	grid->addWidget( d_from, row, 1 );
	d_from->setEnabled( false );

	d_to = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "End Time:" ), this ), row, 2 );
	grid->addWidget( d_to, row, 3 );
	d_to->setEnabled( false );
    row++;
	// AttrLocation
	d_loc = new QLineEdit( this );
	grid->addWidget( new QLabel( tr( "Location:" ), this ), row, 0 );
	grid->addWidget( d_loc, row, 1, 1, 3 );
	d_loc->setEnabled( false );
    row++;
    d_status = new QComboBox( this );
	grid->addWidget( new QLabel( tr( "Status:" ), this ), row, 0 );
	grid->addWidget( d_status, row, 1, 1, 3 );
	d_status->setEnabled( true );
    d_status->addItem( HeTypeDefs::prettyStatus( Stat_Planned ), Stat_Planned );
    d_status->addItem( HeTypeDefs::prettyStatus( Stat_Tentative ), Stat_Tentative );
    d_status->addItem( HeTypeDefs::prettyStatus( Stat_Canceled ), Stat_Canceled );

	vbox->addStretch();

	QDialogButtonBox* bb = new QDialogButtonBox(QDialogButtonBox::Ok
		| QDialogButtonBox::Cancel, Qt::Horizontal, this );
	vbox->addWidget( bb );
    connect(bb, SIGNAL(accepted()), this, SLOT(accept()));
    connect(bb, SIGNAL(rejected()), this, SLOT(reject()));
}

bool ScheduleItemPropsDlg::createEvent( QDate from, QDate to )
{
	enable( TypeEvent );
	d_pix->setPixmap( Oln::OutlineUdbMdl::getPixmap( TypeEvent ) );
	d_class->setText( HeTypeDefs::prettyName( TypeEvent ) );
	d_start->setDate( from );
	d_end->setDate( to );
	while( exec() == QDialog::Accepted )
	{
		if( validate( TypeEvent ) )
			return true;
	}
	return false;
}

bool ScheduleItemPropsDlg::createAppointment( QDate from, const Stream::TimeSlot& s )
{
	enable( TypeAppointment );
	d_pix->setPixmap( Oln::OutlineUdbMdl::getPixmap( TypeAppointment ) );
	d_class->setText( HeTypeDefs::prettyName( TypeAppointment ) );
	d_start->setDate( from );
	if( s.isValid() )
	{
		d_from->setText( s.getStartTime().toString( "h:mm" ) );
		d_to->setText( s.getEndTime().toString( "h:mm" ) );
	}
	while( exec() == QDialog::Accepted )
	{
		if( validate( TypeAppointment ) )
			return true;
	}
	return false;
}

bool ScheduleItemPropsDlg::createDeadline( QDate from, const Stream::TimeSlot& s )
{
	enable( TypeDeadline );
	d_pix->setPixmap( Oln::OutlineUdbMdl::getPixmap( TypeDeadline ) );
	d_class->setText( HeTypeDefs::prettyName( TypeDeadline ) );
	d_start->setDate( from );
	if( s.isValid() )
	{
		d_from->setText( s.getStartTime().toString( "h:mm" ) );
	}
	while( exec() == QDialog::Accepted )
	{
		if( validate( TypeDeadline ) )
			return true;
	}
	return false;
}

void ScheduleItemPropsDlg::enable( quint32 type )
{
	d_from->setEnabled( false );
	d_to->setEnabled( false );
	d_loc->setEnabled( false );
	d_end->setEnabled( false );
	d_from->clear();
	d_to->clear();
	d_loc->clear();
	d_end->clear();
	switch( type )
	{
	case TypeAppointment:
		d_from->setEnabled( true );
		d_to->setEnabled( true );
		d_loc->setEnabled( true );
		break;
	case TypeDeadline:
		d_from->setEnabled( true );
		break;
	case TypeEvent:
		d_end->setEnabled( true );
		d_loc->setEnabled( true );
		break;
	}
}

bool ScheduleItemPropsDlg::loadFrom( const Udb::Obj& item )
{
	if( item.isNull() )
		return false;
	enable( item.getType() );
	d_pix->setPixmap( Oln::OutlineUdbMdl::getPixmap( item.getType() ) );
	d_title->setText( item.getValue( AttrText ).toString() );
	d_ident->setText( item.getValue( AttrInternalId ).toString() );
	d_altIdent->setText( item.getValue( AttrCustomId ).toString() );
	d_start->setDate( item.getValue( AttrStartDate ).getDate() );
    d_status->setCurrentIndex( d_status->findData( item.getValue( AttrStatus ).getUInt8() ) );
	switch( item.getType() )
	{
	case TypeAppointment:
		{
			DataCell v = item.getValue( AttrTimeSlot );
			if( v.isTimeSlot() )
			{
				TimeSlot t = v.getTimeSlot();
				d_from->setText( t.getStartTime().toString( "h:mm" ) );
				d_to->setText( t.getEndTime().toString( "h:mm" ) );
			}
			d_loc->setText( item.getValue( AttrLocation ).toString() );
		}
		break;
	case TypeDeadline:
		if( item.getValue( AttrTimeSlot ).isTimeSlot() )
			d_from->setText( item.getValue( AttrTimeSlot ).getTimeSlot().getStartTime().toString( "h:mm" ) );
		break;
	case TypeEvent:
		d_end->setDate( item.getValue( AttrEndDate ).getDate() );
		d_loc->setText( item.getValue( AttrLocation ).toString() );
		break;
	}
	d_class->setText( HeTypeDefs::prettyName( item.getType() ) );
	return true;
}

bool ScheduleItemPropsDlg::edit( Udb::Obj& item )
{
	if( !loadFrom( item ) )
		return false;
	bool run = true;
	while( run )
	{
		if( exec() != QDialog::Accepted )
		{
			item.getTxn()->rollback();
			return false;
		}
		item.setValue( AttrModifiedOn, Stream::DataCell().setDateTime( QDateTime::currentDateTime() ) );
		run = !saveTo( item );
	}
	item.commit();
	return true;
}

static QTime _parseTime( const QString& str )
{
    QTime t = QTime::fromString( str, "h:mm" );
    if( t.isValid() )
        return t;
    t = QTime::fromString( str, "hhmm" );
    if( t.isValid() )
        return t;
    t = QTime::fromString( str, "hmm" );
    if( t.isValid() )
        return t;
    t = QTime::fromString( str, "hh" );
    if( t.isValid() )
        return t;
    return QTime::fromString( str, "h" );
}

bool ScheduleItemPropsDlg::validate( quint32 type )
{
	QString str = d_title->text().simplified();
	if( str.isEmpty() )
	{
		QMessageBox::critical( this, windowTitle(), 
			tr("Name cannot be empty!" ) );
		return false;
	}
	switch( type )
	{
	case TypeAppointment:
		{
			QTime from = _parseTime( d_from->text() );
			QTime to = _parseTime( d_to->text() );
			if( !from.isValid() || !to.isValid() )
			{
				QMessageBox::critical( this, windowTitle(), 
					tr("Invalid Start or End Time!" ) );
				return false;
			}
			if( to < from )
			{
				QMessageBox::critical( this, windowTitle(), 
					tr("End Time cannot be earlier than Start Time!" ) );
				return false;
			}
		}
		break;
	case TypeDeadline:
		{
			QTime from = _parseTime( d_from->text() );
			if( !from.isValid() )
			{
				QMessageBox::critical( this, windowTitle(), 
					tr("Invalid Start Time!" ) );
				return false;
			}
		}
		break;
	case TypeEvent:
		if( d_end->date() < d_start->date() )
		{
			QMessageBox::critical( this, windowTitle(), 
				tr("End Date cannot be earlier than Start Date!" ) );
			return false;
		}
		break;
	}
	return true;
}

bool ScheduleItemPropsDlg::saveTo( Udb::Obj& item )
{
	if( !validate( item.getType() ) )
		return false;
	item.setValue( AttrText, DataCell().setString( d_title->text().simplified() ) );
	item.setValue( AttrInternalId, DataCell().setString( d_ident->text().simplified() ) );
	item.setValue( AttrCustomId, DataCell().setString( d_altIdent->text().simplified() ) );
	item.setValue( AttrStartDate, DataCell().setDate( d_start->date() ) );
    const int status = d_status->itemData( d_status->currentIndex() ).toInt();
    if( status == 0 )
        item.clearValue(AttrStatus);
    else
        item.setValue( AttrStatus, Stream::DataCell().setUInt8(status) );
	switch( item.getType() )
	{
	case TypeAppointment:
		{
			QTime from = _parseTime( d_from->text() );
			QTime to = _parseTime( d_to->text() );
			item.setValue( AttrTimeSlot, DataCell().setTimeSlot( TimeSlot( from, to ) ) );
			item.setValue( AttrLocation, DataCell().setString( d_loc->text().simplified() ) );
		}
		break;
	case TypeDeadline:
		{
			QTime from = _parseTime( d_from->text() );
			item.setValue( AttrTimeSlot, DataCell().setTimeSlot( TimeSlot( from ) ) );
		}
		break;
	case TypeEvent:
		item.setValue( AttrLocation, DataCell().setString( d_loc->text().simplified() ) );
		// fall through
	}
	item.setTimeStamp( AttrModifiedOn );
	return true;
}
