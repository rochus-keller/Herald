#ifndef EMAILEDIT_H
#define EMAILEDIT_H

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
#include <QListWidget>
#include <Udb/Obj.h>
#include <Txt/TextEdit.h>

class QComboBox;
class MailMessage;

namespace He
{
    class TextEdit;
    class MailEditAttachmentList;
    class AddressIndexer;
    class MailTextEdit;
    class MailBodyEditor;

    class MailEdit : public QFrame
    {
        Q_OBJECT
    public:
        explicit MailEdit(AddressIndexer*, QWidget *parent = 0);
        void replyTo( const Udb::Obj& mail, bool all );
        void forward( const Udb::Obj& mail );
        void clear();
        bool isModified() const;
        void setModified( bool );
        void saveTo( Udb::Obj& o );
        void loadFrom( const Udb::Obj& draft );
    signals:
        void sigSendMessage( quint64 draftOid );
    protected slots:
        void onBodyHeight(int);
        void onKeyPress( const QString& );
        void onSend();
        void onSave();
        void onDelete();
        void onDispoNot();
		void onEncrypt();
		void onSign();
	protected:
        void fillFrom();
        void generatePrefix( const Udb::Obj& );
        void generateBody( const Udb::Obj& );
        void setCaption();
        bool check();
        // overrides
        void closeEvent(QCloseEvent *);
    private:
        AddressIndexer* d_idx;
        QComboBox* d_from;
        int d_lastFrom;
        MailTextEdit* d_to;
        MailTextEdit* d_cc;
        MailTextEdit* d_bcc;
        Txt::TextEdit* d_subject;
        MailBodyEditor* d_body;
        MailEditAttachmentList* d_attachments;
        Udb::Obj d_inReplyTo;
        Udb::Obj d_forwardOf;
        Udb::Obj d_draft;
        bool d_notify;
		bool d_encrypt;
		bool d_sign;
    };
}

#endif // EMAILEDIT_H
