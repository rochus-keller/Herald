#ifndef RAWMAILVIEW2_H
#define RAWMAILVIEW2_H

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

#include <QFrame>
#include <Udb/Obj.h>
#include <GuiTools/AutoMenu.h>
#include <Udb/UpdateInfo.h>
#include "MailBodyBrowser.h"

class QLabel;
class QListWidget;
class MailMessage;
class QListWidgetItem;
class QUrl;
class QFile;

namespace He
{
    class TempFile;

    class MailView : public QFrame
    {
        Q_OBJECT
    public:
		explicit MailView(Udb::Transaction* txn = 0, QWidget *parent = 0);
        bool showMail( const QString& path );
        bool showMail( TempFile* );
        bool showMail( MailMessage* );
        bool showMail( const Udb::Obj& );
        void clear();
        MailBodyBrowser* getBody() const { return d_body; }
        void addCommands( Gui::AutoMenu* );
        const Udb::Obj& getMail() const { return d_msgB; }
    signals:
        void sigActivated( const Udb::Obj& );
        void sigActivated( const QByteArray& emailAddr );
        void sigShowIcd( const QString& fileName );
    public slots:
        void onSaveAs();
        void onShowSource();
        void onPlainText();
        void onOpenAttachment();
        void onSaveAttachment();
        void onAcceptIcsAttachment();
        void onAcquireFile();
        void onListInlines();
        void onCopy();
        void onCopyAtt();
        void onDispose();
        void onShowInBrowser();
        void onShowInWindow();
        void onDbUpdate( Udb::UpdateInfo info );
    protected slots:
        void onLinkActivated( const QString & link );
        void onBodyHeight( int h );
        void onDoubleClicked(QListWidgetItem * item);
        void onAttClicked(QListWidgetItem * item);
        void onAnchorClicked(const QUrl & url);
    protected:
        void setLabelAtts( QLabel* );
        void removeTemps();
        void openAttachment(qlonglong i);
        void saveAttachment(qlonglong i);
        void reloadAttachments();
        void gotoAttachment( const Udb::Obj& att );
        void showIcs(QFile & f, const QString &title);
        static QString formatAddress( const QString& addr );
        static void formatAddressList( QStringList& );
    private:
        QLabel* d_from;
        QLabel* d_sent;
        QLabel* d_received;
        QLabel* d_to;
        QLabel* d_cc;
        QLabel* d_bcc;
        QLabel* d_resentTo;
        QLabel* d_subject;
        QLabel* d_id;
		Udb::Transaction* d_txn;
        MailBodyBrowser* d_body;
        QListWidget* d_attachments;
        MailMessage* d_msgA;
        Udb::Obj d_msgB;
        QList<TempFile*> d_temps;
        bool d_listInlines;
        bool d_lock;
    };
}

#endif // RAWMAILVIEW2_H
