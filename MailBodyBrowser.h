#ifndef MAILBODYBROWSER_H
#define MAILBODYBROWSER_H

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

#include <QTextBrowser>
#include <Udb/Obj.h>
#include <QThread>

class MailMessage;

namespace He
{
	class MailBodyBrowser : public QTextBrowser
    {
        Q_OBJECT
    public:
        explicit MailBodyBrowser(QWidget *parent = 0);
        void showMail( MailMessage* );
        void showMail( const Udb::Obj& );
        void showPlainText( const QString& );
		void showHtml( const QString& );
		void clear();
		void replaceDoc( QTextDocument* );
	signals:
		void sigKill();
    protected:
        void setHtml( const QString& );

        // overrides
        QVariant loadResource ( int type, const QUrl & name );
	protected slots:
		void onDocReady(QTextDocument*);
    private:
        MailMessage* d_msgA;
        Udb::Obj d_msgB;
        QMap<QUrl,QVariant> d_cache;
    };

	class MailBodyRenderer: public QThread
	{
		Q_OBJECT
	public:
		MailBodyRenderer(MailBodyBrowser* p);
		void render( const QString& html, const QFont& font, int width );
	signals:
		void sigRendered( QTextDocument* );
	protected:
		~MailBodyRenderer();
		// Override
		void run();
		void timerEvent(QTimerEvent *);
	protected slots:
		void onKill();
	private:
		QString d_html;
		QFont d_font;
		int d_width;
		bool d_abort;
	};
}

#endif // MAILBODYBROWSER_H
