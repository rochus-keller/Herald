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

#include "CalendarPopup.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QDateTimeEdit>
#include <QStyleOptionComboBox>
#include <QMouseEvent>
using namespace He;

// 1:1 von MasterPlan

CalendarPopup::CalendarPopup(QWidget * parent, QCalendarWidget *cw)
    : QWidget(parent, Qt::Popup), calendar(0)
{
    setAttribute(Qt::WA_WindowPropagation);

    dateChanged = false;
    if (!cw) {
        cw = new QCalendarWidget(this);
        cw->setVerticalHeaderFormat(QCalendarWidget::ISOWeekNumbers);
		cw->setFirstDayOfWeek( Qt::Monday );
		cw->setGridVisible( true );
    }
    setCalendarWidget(cw);
}

void CalendarPopup::setCalendarWidget(QCalendarWidget *cw)
{
    Q_ASSERT(cw);
    QVBoxLayout *widgetLayout = qobject_cast<QVBoxLayout*>(layout());
    if (!widgetLayout) {
        widgetLayout = new QVBoxLayout(this);
        widgetLayout->setMargin(0);
        widgetLayout->setSpacing(0);
    }
    delete calendar;
    calendar = cw;
    widgetLayout->addWidget(calendar);

    connect(calendar, SIGNAL(activated(QDate)), this, SLOT(dateSelected(QDate)));
    connect(calendar, SIGNAL(clicked(QDate)), this, SLOT(dateSelected(QDate)));
    connect(calendar, SIGNAL(selectionChanged()), this, SLOT(dateSelectionChanged()));

    calendar->setFocus();
}


void CalendarPopup::setDate(const QDate &date)
{
    oldDate = date;
    calendar->setSelectedDate(date);
}

void CalendarPopup::setDateRange(const QDate &min, const QDate &max)
{
    calendar->setMinimumDate(min);
    calendar->setMaximumDate(max);
}

void CalendarPopup::mousePressEvent(QMouseEvent *event)
{
    QDateTimeEdit *dateTime = qobject_cast<QDateTimeEdit *>(parentWidget());
    if (dateTime) {
        QStyleOptionComboBox opt;
        opt.init(dateTime);
        QRect arrowRect = dateTime->style()->subControlRect(QStyle::CC_ComboBox, &opt,
                                                            QStyle::SC_ComboBoxArrow, dateTime);
        arrowRect.moveTo(dateTime->mapToGlobal(arrowRect .topLeft()));
        if (arrowRect.contains(event->globalPos()) || rect().contains(event->pos()))
            setAttribute(Qt::WA_NoMouseReplay);
    }
    QWidget::mousePressEvent(event);
}

void CalendarPopup::mouseReleaseEvent(QMouseEvent*)
{
    emit resetButton();
}

bool CalendarPopup::event(QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key()== Qt::Key_Escape)
            dateChanged = false;
    }
    return QWidget::event(event);
}

void CalendarPopup::dateSelectionChanged()
{
    dateChanged = true;
    emit newDateSelected(calendar->selectedDate());
}
void CalendarPopup::dateSelected(const QDate &date)
{
    dateChanged = true;
    emit activated(date);
    close();
}

void CalendarPopup::hideEvent(QHideEvent *)
{
    emit resetButton();
    if (!dateChanged)
        emit hidingCalendar(oldDate);
}
