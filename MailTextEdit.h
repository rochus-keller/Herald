#ifndef MAILTEXTEDIT_H
#define MAILTEXTEDIT_H

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

#include <Txt/TextEdit.h>
#include <Udb/Obj.h>

namespace He
{
    class MailTextEdit : public Txt::TextEdit
    {
        Q_OBJECT
    public:
		MailTextEdit(Udb::Transaction* txn, QWidget*w);
        void addAddress( const QString& name, const QByteArray& addr );
        struct Party
        {
            QString d_name;
            QByteArray d_addr;
            Udb::Obj d_obj;
            QString d_error;
            QString render() const;
        };
        QList<Party> getParties() const;
        static QList<Party> getParties(QTextDocument*,Udb::Transaction*);
    signals:
        void sigKeyPressed(const QString& text);
    public slots:
        void onSelected( QString name, QString addr );
        void onPaste();
		void onPastePlainText();
    protected:
        static QStringList findEmailAddresses( const QString& );
        // overrides
        void keyPressEvent ( QKeyEvent * event );
        void mousePressEvent ( QMouseEvent * event );
        void mouseReleaseEvent ( QMouseEvent * event );
    private:
        Udb::Transaction* d_txn;
    };
}

#endif // MAILTEXTEDIT_H
