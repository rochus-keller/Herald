/*
* Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
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


#ifndef MailBodyEditor_H
#define MailBodyEditor_H

#include <QTextEdit>
#include <QHash>

class QAction;
class QComboBox;
class QFontComboBox;
class QTextCharFormat;
class QToolBar;
class QActionGroup;

namespace He
{
    class MailBodyEditor : public QTextEdit
    {
        Q_OBJECT
    public:
        class ResourceManager
        {
        public:
            virtual QImage fetchImage( const QByteArray& cid ) = 0;
            virtual QByteArray registerImage( const QImage & image ) = 0;
            virtual QByteArray registerFile( const QString& path ) = 0;
        };

        MailBodyEditor(QWidget *parent = 0);
        void fillToolBar( QToolBar* );
		void createContextMenu();
        void setRessourceManager( ResourceManager* rmgr ) { d_rmgr = rmgr; }
    public slots:
        void onCut();
        void onCopy();
        void onPaste();
        void onUndo();
        void onRedo();
		void onPastePlainText();
		void onSelectAll();
	public slots:
        void setTextBold();
        void setTextUnderline();
        void setTextItalic();
        void setTextFamily(const QString &f);
        void setTextSize(const QString &p);
        void setTextColor();
        void setTextAlignment(QAction *a);
        void setDefaultFont( const QFont& );
        void setList();
        void setList(QAction *a);
    protected slots:
        void currentCharFormatChanged(const QTextCharFormat &format);
        void cursorPositionChanged();
    protected:
		void paste();
		void insertUrl( const QUrl& );
        QVariant loadResource ( int type, const QUrl & name );
        bool canInsertFromMimeData( const QMimeData *source ) const;
        void insertFromMimeData( const QMimeData *source );
		void keyPressEvent(QKeyEvent *event);
    private:
        void setupEditActions();
        void setupTextActions();
        void mergeFormatOnWordOrSelection(const QTextCharFormat &format);
        void fontChanged(const QFont &f);
        void colorChanged(const QColor &c);
        void alignmentChanged(Qt::Alignment a);
        void textStyleChanged( QTextListFormat::Style );
        void applyStyle( QTextListFormat::Style );

        QAction
            *actionTextBold,
            *actionTextUnderline,
            *actionTextItalic,
            *actionTextColor,
            *actionAlignLeft,
            *actionAlignCenter,
            *actionAlignRight,
            *actionAlignJustify,
            *actionUndo,
            *actionRedo,
            *actionCut,
            *actionCopy,
            *actionBulletList,
            *actionOrderedList,
            *actionBulletDisc,
            *actionBulletCircle,
            *actionBulletSquare,
            *actionOrderedDecimal,
            *actionOrderedAlphaLower,
            *actionOrderedAlphaUpper;
        QActionGroup *alignActionGroup;
        QFontComboBox *comboFont;
        QComboBox *comboSize;
        ResourceManager* d_rmgr;
    };
}

#endif
