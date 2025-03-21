#ifndef RESENDTODLG_H
#define RESENDTODLG_H

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

#include <QDialog>
#include <Udb/Obj.h>

class QComboBox;
class QLineEdit;
class QFormLayout;
class QPlainTextEdit;

namespace He
{
    class MailTextEdit;
    class AddressIndexer;

    class ResendToDlg : public QDialog
    {
        Q_OBJECT
    public:
        explicit ResendToDlg(AddressIndexer* idx, QWidget *parent = 0);
        void setFrom( const Udb::Obj& );
        QString getFrom() const;
        Udb::Obj getIdentity() const;
		void setTo( const Udb::Obj& );
        QStringList getTo() const;
        QByteArray getToBml() const;
        bool select();
    protected:
        void fillFrom();
    protected slots:
        void onKeyPress(const QString&);
	protected:
        AddressIndexer* d_idx;
        QComboBox* d_from;
        MailTextEdit* d_to;
		QFormLayout* d_form;
    };

	class SendCertificateDlg : public ResendToDlg
	{
	public:
		SendCertificateDlg(AddressIndexer* idx, QWidget *parent = 0);
		QString getMsg() const;
		QString getSubject() const;
	private:
		QLineEdit* d_subject;
		QPlainTextEdit* d_msg;
	};
}

#endif // RESENDTODLG_H
