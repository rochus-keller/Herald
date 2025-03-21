#ifndef MAINWINDOW_H
#define MAINWINDOW_H

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
#include <Udb/Transaction.h>
#include <GuiTools/AutoMenu.h>
#include <QList>

class QTextBrowser;

namespace Mp
{
	class DocTabWidget;
}
namespace He
{
    class DownloadManager;
    class UploadManager;
    class InboxCtrl;
    class MailView;
    class MailEdit;
    class AttrViewCtrl;
    class AddressIndexer;
    class AddressListCtrl;
    class MailListCtrl;
    class MailHistoCtrl;
    class TextViewCtrl;
    class SearchView;
    class RefViewCtrl;
    class CalMainWindow;

    class EmailMainWindow : public QMainWindow
    {
        Q_OBJECT

    public:
        EmailMainWindow(Udb::Transaction*);
        ~EmailMainWindow();
        Udb::Transaction* getTxn() const { return d_txn; }
        void showOid( quint64 oid );
        void addTopCommands( Gui::AutoMenu* );
        static QDockWidget* createDock( QMainWindow* parent, const QString& title, quint32 id, bool visi );
    signals:
        void closing();
    public slots:
        void onFullScreen();
        void onAbout();
        void onGoBack();
        void onGoForward();
        void onSearch();
		void onTest();
    protected slots:
        void onFetchMail();
        void onSetFont();
        void onInboxSelected();
        void onMailListSelected();
        void onMailHistoSelected();
        void onFollowObject( const Udb::Obj& );
        void onFollowEmail( const QByteArray& );
        void onImportMail();
        void onConfig();
        void onDownloadStatus( const QString& msg );
        void onDownloadError( const QString& msg );
        void onNewMessage();
        void onReply();
        void onReplyAll();
        void onForward();
        void onResend();
        void onShowDraft();
        void onEditorDestroyed( QObject * );
        void onUploadStatus(const QString& msg );
        void onUploadError( const QString& msg );
        void onSelectById();
        void onImportStatus(const QString& msg );
        void onImportError( const QString& msg );
        void onResendInbox();
        void onUnselect();
        void onGoUpward();
        void onShowIcd( const QString& fileName );
		void onOpenFile();
		void onSendFromFile();
		void onSendCert1();
        void onShowCalendar();
	protected:
        void setCaption();
        void setupInbox();
        void setupAttrView();
        void setupAddressList();
        void setupMailList();
        void setupMailHisto();
        void setupMailView();
        void setupTextView();
        void setupRefView();
        void setupSearchView();
        void setupLog();
        void showCalendar();
        MailEdit* createMailEdit();
        void pushBack( const Udb::Obj& );
        static void toFullScreen( QMainWindow* );
        // overrides
        void closeEvent(QCloseEvent *event);
    private:
        //Mp::DocTabWidget* d_tab;
        Udb::Transaction* d_txn;
        DownloadManager* d_dmgr;
        UploadManager* d_umgr;
        QTextBrowser* d_log;
        InboxCtrl* d_inbox;
        MailView* d_mailView;
        AttrViewCtrl* d_attrView;
        AddressIndexer* d_aidx;
        AddressListCtrl* d_addressList;
        MailListCtrl* d_mailList;
        MailHistoCtrl* d_mailHisto;
        TextViewCtrl* d_textView;
        RefViewCtrl* d_refView;
        SearchView* d_sv;
        QList<MailEdit*> d_editors;
        QList<Udb::OID> d_backHisto; // d_backHisto.last() ist aktuell angezeigtes Objekt
		QList<Udb::OID> d_forwardHisto;
        CalMainWindow* d_calendar;
        bool d_pushBackLock;
        bool d_fullScreen;
    };
}

#endif // MAINWINDOW_H
