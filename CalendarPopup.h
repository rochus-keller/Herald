#ifndef __He_CalendarPopup__
#define __He_CalendarPopup__

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

#include <QCalendarWidget>

namespace He
{
	class CalendarPopup : public QWidget
	{
		Q_OBJECT
	public:
		CalendarPopup(QWidget *parent = 0, QCalendarWidget *cw = 0);
		QDate selectedDate() { return calendar->selectedDate(); }
		void setDate(const QDate &date);
		void setDateRange(const QDate &min, const QDate &max);
		void setFirstDayOfWeek(Qt::DayOfWeek dow) { calendar->setFirstDayOfWeek(dow); }
		QCalendarWidget *calendarWidget() const { return calendar; }
		void setCalendarWidget(QCalendarWidget *cw);
	Q_SIGNALS:
		void activated(const QDate &date);
		void newDateSelected(const QDate &newDate);
		void hidingCalendar(const QDate &oldDate);
		void resetButton();

	private Q_SLOTS:
		void dateSelected(const QDate &date);
		void dateSelectionChanged();

	protected:
		void hideEvent(QHideEvent *);
		void mousePressEvent(QMouseEvent *e);
		void mouseReleaseEvent(QMouseEvent *);
		bool event(QEvent *e);

	private:
		QCalendarWidget *calendar;
		QDate oldDate;
		bool dateChanged;
	};
}

#endif
