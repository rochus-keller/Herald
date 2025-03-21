#ifndef CALMAINWINDOW_H
#define CALMAINWINDOW_H

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

#include <QMainWindow>
#include <Udb/Obj.h>
#include <Oln2/OutlineUdbCtrl.h>

namespace He
{
    class EmailMainWindow;
    class ScheduleBoard;
    class CalendarView;
    class AttrViewCtrl;
    class TextViewCtrl;
    class SearchView;
    class RefViewCtrl;
    class BrowserCtrl;

    class CalMainWindow : public QMainWindow
    {
        Q_OBJECT
    public:
        explicit CalMainWindow(EmailMainWindow * email );
        void showObj( const Udb::Obj&  );
        void showIcs( const QString& path );
        void restore();
    protected slots:
        void onStatusMessage( const QString& );
        void onSelectSchedule( quint64 );
        void onEdit();
        void onDateChanged(bool);
        void onCalendarMoved(bool);
        void onSelectCalendar( quint64 );
        void onFollowObject( const Udb::Obj& );
        void onShowLocalObject( const Udb::Obj& );
        void onUrlActivated( const QUrl& );
        void onSelectOutline(quint64);
        void onAddDock();
        void onRemoveDock();
        void onGoBack();
        void onGoForward();
        void onFollowEmail(const QByteArray & emailAddr );
    protected:
        void setCaption();
        void setupCalendar();
        void setupTextView();
        void setupAttrView();
        void setupRefView();
        void setupBrowserView();
        void setupMenus();
        void showAsDock( const Udb::Obj& oln, bool visible );
        void addTopCommands( Gui::AutoMenu* );
        void pushBack( const Udb::Obj& );
        void followObject( const Udb::Obj&, bool openDoc = true );
        // overrides
    private:
        EmailMainWindow* d_email;
        ScheduleBoard* d_sb;
        CalendarView* d_cv;
        BrowserCtrl* d_browser;
        AttrViewCtrl* d_attrView;
        TextViewCtrl* d_textView;
        RefViewCtrl* d_refView;
        QList<Oln::OutlineUdbCtrl*> d_docks;
        QList<Udb::OID> d_backHisto; // d_backHisto.last() ist aktuell angezeigtes Objekt
		QList<Udb::OID> d_forwardHisto;
        bool d_pushBackLock;
    };
}

#endif // CALMAINWINDOW_H
