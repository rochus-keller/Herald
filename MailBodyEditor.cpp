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


#include "MailBodyEditor.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QFontComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QMenu>
#include <QMenuBar>
#include <QMimeData>
//#include <QPrintDialog>
//#include <QPrinter>
//#include <QPrintPreviewDialog>
#include <QTextCodec>
#include <QTextEdit>
#include <QToolBar>
#include <QTextCursor>
#include <QTextList>
#include <QtDebug>
#include <QLabel>
#include <QCloseEvent>
#include <QMessageBox>
#include <QImageReader>
#include <QToolButton>
#include <QLineEdit>
#include <QUrl>
#include <QShortcut>
#include <GuiTools/AutoMenu.h>
#include <GuiTools/AutoShortcut.h>
#include <Stream/DataCell.h>
#include <Oln2/OutlineCtrl.h>
using namespace He;

// Abgeleitet aus Qt Demo TextEdit, aber ziemlich geändert

const QString rsrcPath = ":/images";

MailBodyEditor::MailBodyEditor(QWidget *parent)
    : QTextEdit(parent),d_rmgr(0)
{
    setupEditActions();
    setupTextActions();

    connect(this, SIGNAL(currentCharFormatChanged(const QTextCharFormat &)),
            this, SLOT(currentCharFormatChanged(const QTextCharFormat &)));
    connect(this, SIGNAL(cursorPositionChanged()),
            this, SLOT(cursorPositionChanged()));

    fontChanged(font());
    colorChanged(QTextEdit::textColor());
    alignmentChanged(alignment());

//    connect(document(), SIGNAL(modificationChanged(bool)),
//            this, SLOT(setWindowModified(bool)));
    connect(document(), SIGNAL(undoAvailable(bool)),
            actionUndo, SLOT(setEnabled(bool)));
    connect(document(), SIGNAL(redoAvailable(bool)),
            actionRedo, SLOT(setEnabled(bool)));

    //setWindowModified(document()->isModified());
    actionUndo->setEnabled(document()->isUndoAvailable());
    actionRedo->setEnabled(document()->isRedoAvailable());

    connect(actionUndo, SIGNAL(triggered()), this, SLOT(undo()));
    connect(actionRedo, SIGNAL(triggered()), this, SLOT(redo()));

    actionCut->setEnabled(false);
    actionCopy->setEnabled(false);

    connect(actionCut, SIGNAL(triggered()), this, SLOT(cut()));
    connect(actionCopy, SIGNAL(triggered()), this, SLOT(copy()));
	//new Gui2::AutoShortcut( tr("CTRL+V"), this, this, SLOT(onPaste()) );
    new Gui::AutoShortcut( tr("CTRL+SHIFT+V"), this, this, SLOT(onPastePlainText()) );
	createContextMenu();

    connect(this, SIGNAL(copyAvailable(bool)), actionCut, SLOT(setEnabled(bool)));
    connect(this, SIGNAL(copyAvailable(bool)), actionCopy, SLOT(setEnabled(bool)));
}

void MailBodyEditor::fillToolBar(QToolBar * tb)
{
    tb->addWidget(comboFont);
    tb->addWidget(comboSize);
    tb->addWidget( new QLabel( " ", this ) );
    tb->addAction(actionTextBold);
    tb->addAction(actionTextItalic);
    tb->addAction(actionTextUnderline);
    tb->addAction(actionTextColor);
    tb->addActions(alignActionGroup->actions());
    QToolButton* b = new QToolButton( tb );
    b->setPopupMode( QToolButton::DelayedPopup );
    b->setDefaultAction( actionBulletList );
    tb->addWidget( b );
    b = new QToolButton( tb );
    b->setPopupMode( QToolButton::DelayedPopup );
    b->setDefaultAction( actionOrderedList );
	tb->addWidget( b );
}

void MailBodyEditor::createContextMenu()
{
    Gui::AutoMenu* pop = new Gui::AutoMenu(this,true);
	pop->addAction(actionUndo);
	pop->addAction(actionRedo);
	pop->addSeparator();
	pop->addAction(actionCut);
	pop->addAction(actionCopy);
	pop->addCommand( tr("Paste"), this, SLOT(onPaste()), QKeySequence::Paste );
	pop->addCommand( tr("Paste Plain Text"), this, SLOT(onPastePlainText()), tr("CTRL+SHIFT+V") );
	pop->addSeparator();
	pop->addCommand( tr("Select All"), this, SLOT(onSelectAll()), QKeySequence::SelectAll );
}

void MailBodyEditor::onCut()
{
    ENABLED_IF( actionCut->isEnabled() );
    actionCut->trigger();
}

void MailBodyEditor::onCopy()
{
    ENABLED_IF( actionCopy->isEnabled() );
    actionCopy->trigger();
}

void MailBodyEditor::onPaste()
{
	ENABLED_IF( canPaste() );
	paste();
}

void MailBodyEditor::onPastePlainText()
{
	ENABLED_IF( !QApplication::clipboard()->text().isEmpty() );
	textCursor().insertText( QApplication::clipboard()->text() );
}

void MailBodyEditor::onSelectAll()
{
	ENABLED_IF(true);
	selectAll();
}

void MailBodyEditor::onUndo()
{
    ENABLED_IF( actionUndo->isEnabled() );
    actionUndo->trigger();
}

void MailBodyEditor::onRedo()
{
    ENABLED_IF( actionRedo->isEnabled() );
    actionRedo->trigger();
}

void MailBodyEditor::setupEditActions()
{
    QAction *a;
    a = actionUndo = new QAction(QIcon(rsrcPath + "/editundo.png"), tr("&Undo"), this);
    a->setShortcut(QKeySequence::Undo);
    a = actionRedo = new QAction(QIcon(rsrcPath + "/editredo.png"), tr("&Redo"), this);
    a->setShortcut(QKeySequence::Redo);
    a = actionCut = new QAction(QIcon(rsrcPath + "/editcut.png"), tr("Cu&t"), this);
    a->setShortcut(QKeySequence::Cut);
    a = actionCopy = new QAction(QIcon(rsrcPath + "/editcopy.png"), tr("&Copy"), this);
    a->setShortcut(QKeySequence::Copy);
}

void MailBodyEditor::setupTextActions()
{
    actionTextBold = new QAction(QIcon(rsrcPath + "/edit-bold.png"), tr("&Bold"), this);
    actionTextBold->setShortcut(Qt::CTRL + Qt::Key_B);
    QFont bold;
    bold.setBold(true);
    actionTextBold->setFont(bold);
    connect(actionTextBold, SIGNAL(triggered()), this, SLOT(setTextBold()));
    actionTextBold->setCheckable(true);

    actionTextItalic = new QAction(QIcon(rsrcPath + "/edit-italic.png"), tr("&Italic"), this);
    actionTextItalic->setShortcut(Qt::CTRL + Qt::Key_I);
    QFont italic;
    italic.setItalic(true);
    actionTextItalic->setFont(italic);
    connect(actionTextItalic, SIGNAL(triggered()), this, SLOT(setTextItalic()));
    actionTextItalic->setCheckable(true);

    actionTextUnderline = new QAction(QIcon(rsrcPath + "/edit-underline.png"), tr("&Underline"), this);
    actionTextUnderline->setShortcut(Qt::CTRL + Qt::Key_U);
    QFont underline;
    underline.setUnderline(true);
    actionTextUnderline->setFont(underline);
    connect(actionTextUnderline, SIGNAL(triggered()), this, SLOT(setTextUnderline()));
    actionTextUnderline->setCheckable(true);

    actionBulletList = new QAction( QIcon(rsrcPath + "/edit-list.png"), tr("&Bullet List"), this);
    connect(actionBulletList, SIGNAL(triggered()), this, SLOT(setList()));
    actionBulletList->setCheckable(true);

    actionBulletDisc = new QAction( tr("Bullet List (Disc)"), this );
    actionBulletDisc->setCheckable(true);
    actionBulletCircle = new QAction( tr("Bullet List (Circle)"), this );
    actionBulletCircle->setCheckable(true);
    actionBulletSquare = new QAction( tr("Bullet List (Square)"), this );
    actionBulletSquare->setCheckable(true);
    QMenu* bulletMenu = new QMenu( this );
    bulletMenu->addAction( actionBulletDisc );
    bulletMenu->addAction( actionBulletCircle );
    bulletMenu->addAction( actionBulletSquare );
    connect( bulletMenu, SIGNAL(triggered(QAction*)), this,SLOT(setList(QAction*)) );
    actionBulletList->setMenu( bulletMenu );

    actionOrderedList = new QAction( QIcon(rsrcPath + "/edit-list-order.png"), tr("&Ordered List"), this);
    connect(actionOrderedList, SIGNAL(triggered()), this, SLOT(setList()));
    actionOrderedList->setCheckable(true);

    actionOrderedDecimal = new QAction( tr("Ordered List (Decimal)"), this );
    actionOrderedDecimal->setCheckable(true);
    actionOrderedAlphaLower = new QAction( tr("Ordered List (Alpha lower)"), this );
    actionOrderedAlphaLower->setCheckable(true);
    actionOrderedAlphaUpper = new QAction( tr("Ordered List (Alpha upper)"), this );
    actionOrderedAlphaUpper->setCheckable(true);
    QMenu* orderedMenu = new QMenu( this );
    orderedMenu->addAction( actionOrderedDecimal );
    orderedMenu->addAction( actionOrderedAlphaLower );
    orderedMenu->addAction( actionOrderedAlphaUpper );
    connect( orderedMenu, SIGNAL(triggered(QAction*)), this,SLOT(setList(QAction*)) );
    actionOrderedList->setMenu( orderedMenu );

    alignActionGroup = new QActionGroup(this);
    connect(alignActionGroup, SIGNAL(triggered(QAction *)), this, SLOT(setTextAlignment(QAction *)));

    actionAlignLeft = new QAction(QIcon(rsrcPath + "/edit-alignment.png"), tr("&Left"), alignActionGroup);
    actionAlignLeft->setShortcut(Qt::CTRL + Qt::Key_L);
    actionAlignLeft->setCheckable(true);
    actionAlignCenter = new QAction(QIcon(rsrcPath + "/edit-alignment-center.png"), tr("C&enter"), alignActionGroup);
    actionAlignCenter->setShortcut(Qt::CTRL + Qt::Key_E);
    actionAlignCenter->setCheckable(true);
    actionAlignRight = new QAction(QIcon(rsrcPath + "/edit-alignment-right.png"), tr("&Right"), alignActionGroup);
    actionAlignRight->setShortcut(Qt::CTRL + Qt::Key_R);
    actionAlignRight->setCheckable(true);
    actionAlignJustify = new QAction(QIcon(rsrcPath + "/edit-alignment-justify.png"), tr("&Justify"), alignActionGroup);
    actionAlignJustify->setShortcut(Qt::CTRL + Qt::Key_J);
    actionAlignJustify->setCheckable(true);

    QPixmap pix(16, 16);
    pix.fill(Qt::black);
    actionTextColor = new QAction(pix, tr("&Color..."), this);
    connect(actionTextColor, SIGNAL(triggered()), this, SLOT(setTextColor()));

    comboFont = new QFontComboBox(this);
    connect(comboFont, SIGNAL(activated(const QString &)),
            this, SLOT(setTextFamily(const QString &)));

    comboSize = new QComboBox(this);
    comboSize->setObjectName("comboSize");
    comboSize->setEditable(true);

    QFontDatabase db;
    foreach(int size, db.standardSizes())
        comboSize->addItem(QString::number(size));

    connect(comboSize, SIGNAL(activated(const QString &)), this, SLOT(setTextSize(const QString &)));
    // Funktioniert nicht, da bei jedem Buchstaben ausgelöst:
    // connect(comboSize,SIGNAL(editTextChanged(QString)), this,SLOT(setTextSize(QString)) );
    comboSize->setCurrentIndex(comboSize->findText(QString::number(QApplication::font()
                                                                   .pointSize())));
}

void MailBodyEditor::setTextBold()
{
    QTextCharFormat fmt;
    fmt.setFontWeight(actionTextBold->isChecked() ? QFont::Bold : QFont::Normal);
    mergeFormatOnWordOrSelection(fmt);
}

void MailBodyEditor::setTextUnderline()
{
    QTextCharFormat fmt;
    fmt.setFontUnderline(actionTextUnderline->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MailBodyEditor::setTextItalic()
{
    QTextCharFormat fmt;
    fmt.setFontItalic(actionTextItalic->isChecked());
    mergeFormatOnWordOrSelection(fmt);
}

void MailBodyEditor::setTextFamily(const QString &f)
{
    QTextCharFormat fmt;
    fmt.setFontFamily(f);
    mergeFormatOnWordOrSelection(fmt);
}

void MailBodyEditor::setTextSize(const QString &p)
{
    QTextCharFormat fmt;
    fmt.setFontPointSize(p.toFloat());
    mergeFormatOnWordOrSelection(fmt);
}

void MailBodyEditor::setTextColor()
{
    QColor col = QColorDialog::getColor(QTextEdit::textColor(), this);
    if (!col.isValid())
        return;
    QTextCharFormat fmt;
    fmt.setForeground(col);
    mergeFormatOnWordOrSelection(fmt);
    colorChanged(col);
}

void MailBodyEditor::setTextAlignment(QAction *a)
{
    if (a == actionAlignLeft)
        setAlignment(Qt::AlignLeft);
    else if (a == actionAlignCenter)
        setAlignment(Qt::AlignHCenter);
    else if (a == actionAlignRight)
        setAlignment(Qt::AlignRight);
    else if (a == actionAlignJustify)
        setAlignment(Qt::AlignJustify);
}

void MailBodyEditor::setDefaultFont(const QFont & f)
{
    setFont( f );
    document()->setDefaultFont( f );
    fontChanged( f );
}

void MailBodyEditor::setList()
{
    if( sender() == actionBulletList )
    {
        if( actionBulletList->isChecked() )
            applyStyle( QTextListFormat::ListDisc );
        else
            applyStyle( QTextListFormat::ListStyleUndefined );
    }else if( sender() == actionOrderedList )
    {
        if( actionBulletList->isChecked() )
            applyStyle( QTextListFormat::ListDecimal );
        else
            applyStyle( QTextListFormat::ListStyleUndefined );
    }
}

void MailBodyEditor::setList(QAction *a)
{
    if( a == actionBulletDisc )
        applyStyle( QTextListFormat::ListDisc );
    else if( a == actionBulletCircle )
        applyStyle( QTextListFormat::ListCircle );
    else if( a == actionBulletSquare )
        applyStyle( QTextListFormat::ListSquare );
    else if( a == actionOrderedDecimal )
        applyStyle( QTextListFormat::ListDecimal );
    else if( a == actionOrderedAlphaLower )
        applyStyle( QTextListFormat::ListLowerAlpha );
    else if( a == actionOrderedAlphaUpper )
        applyStyle( QTextListFormat::ListUpperAlpha );
}

void MailBodyEditor::currentCharFormatChanged(const QTextCharFormat &format)
{
    fontChanged(format.font());
    colorChanged(format.foreground().color());
}

void MailBodyEditor::cursorPositionChanged()
{
    alignmentChanged(alignment());
    QTextCursor cursor = textCursor();
    QTextListFormat::Style style = QTextListFormat::ListStyleUndefined;
    if( cursor.currentList() )
        style = cursor.currentList()->format().style();
	textStyleChanged( style );
}

static inline bool isImgFile( const QString& path )
{
	const QString suff = QFileInfo(path).suffix();
	const QList<QByteArray> fmts = QImageReader::supportedImageFormats();
	foreach( const QByteArray& fmt, fmts )
	{
		if( suff.compare( fmt, Qt::CaseInsensitive ) == 0 )
			return true;
	}
	return false;
}

void MailBodyEditor::paste()
{
	const QMimeData* mime = QApplication::clipboard()->mimeData();
	Q_ASSERT( mime != 0 );
	if( mime->hasText() && mime->text().startsWith( QLatin1String( "http" ) ) )
	{
		QUrl url( mime->text(), QUrl::TolerantMode );
		if( url.isValid() )
			insertUrl(url);
		else
			QTextEdit::paste();
	}else if( mime->hasUrls() )
	{
		QList<QUrl> l = mime->urls();
		for( int i = 0; i < l.size(); i++ )
		{
			if( l[i].scheme() == "file" && isImgFile( l[i].toLocalFile() ) )
			{
				QTextEdit::paste();
				continue;
			}
			insertUrl( l[i] );
		}
	}else
		QTextEdit::paste();
}

void MailBodyEditor::insertUrl(const QUrl & url)
{
	textCursor().insertHtml(QString("<a href='%1'>%2</a>&nbsp").arg(url.toEncoded().data()).arg(url.toString() ) );
}

QVariant MailBodyEditor::loadResource(int type, const QUrl &name)
{
    if( d_rmgr != 0 && type == QTextDocument::ImageResource && name.scheme() == QLatin1String("cid") )
    {
        QByteArray cid = name.path().toLatin1();
        QImage img = d_rmgr->fetchImage( cid );
        if( !img.isNull() )
            return img;
    } // else
    return QVariant();
}

bool MailBodyEditor::canInsertFromMimeData(const QMimeData *source) const
{
    if( source->hasImage() )
        return true;
    else if( source->hasUrls() )
        return true;
    else
        return QTextEdit::canInsertFromMimeData(source);
}


void MailBodyEditor::insertFromMimeData(const QMimeData *source)
{
    if( source->hasImage() )
    {
        if( d_rmgr != 0 )
        {
            QByteArray cid = d_rmgr->registerImage( qvariant_cast<QImage>(source->imageData()) );
            if( !cid.isEmpty() )
                textCursor().insertImage( QString("cid:%1").arg( cid.data() ) );
        }
    }else if( source->hasUrls() )
    {
        foreach( QUrl u, source->urls() )
        {
            if( u.scheme() == QLatin1String( "file" ) )
            {
                if( d_rmgr != 0 )
                {
                    QByteArray cid = d_rmgr->registerFile( u.toLocalFile() );
                    if( !cid.isEmpty() )
                        textCursor().insertImage( QString("cid:%1").arg( cid.data() ) );
                }
            }else
            {
                // Fix Google Chrome: dieser gibt bei "text/uri-list" die URL in der Form <url>%0A<url> zurück
                QList<QUrl> l = source->urls();
                if( !l.isEmpty() )
                    u = QUrl( source->text(), QUrl::TolerantMode );
                textCursor().insertHtml( QString("<a href=\"%1\">%2</a>").
                                         arg( u.toEncoded().data() ).arg( u.toString() ) );
            }
        }
	}else if( source->hasHtml() )
	{
		textCursor().insertHtml( Oln::OutlineCtrl::fetchHtml( source ) );
	}else
		QTextEdit::insertFromMimeData( source );
}

void MailBodyEditor::keyPressEvent(QKeyEvent *event)
{
	if( event == QKeySequence::Paste )
		onPaste();
	else if( event == QKeySequence::Cut )
		onCut();
	else if( event == QKeySequence::Copy )
		onCopy();
	else
		QTextEdit::keyPressEvent( event );
}

void MailBodyEditor::mergeFormatOnWordOrSelection(const QTextCharFormat &format)
{
    QTextCursor cursor = textCursor();
    if (!cursor.hasSelection())
        cursor.select(QTextCursor::WordUnderCursor);
    cursor.mergeCharFormat(format);
    mergeCurrentCharFormat(format);
}

void MailBodyEditor::fontChanged(const QFont &f)
{
    ;
    comboFont->setCurrentIndex(comboFont->findText(QFontInfo(f).family()));
    int pointSize = f.pointSize();
    if( pointSize == -1 )
    {
        // http://en.wikipedia.org/wiki/Point_(typography)
        // 1 Point = 0.013889 Inch
        pointSize = ( f.pixelSize() / double( logicalDpiY() ) ) / 0.013889 + 0.5;
    }
    const QString strSize = QString::number( pointSize );
    const int pos = comboSize->findText(strSize);
    comboSize->setCurrentIndex(pos);
    if( pos == -1 )
        comboSize->lineEdit()->setText( strSize );
    actionTextBold->setChecked(f.bold());
    actionTextItalic->setChecked(f.italic());
    actionTextUnderline->setChecked(f.underline());
}

void MailBodyEditor::colorChanged(const QColor &c)
{
    QPixmap pix(16, 16);
    pix.fill(c);
    actionTextColor->setIcon(pix);
}

void MailBodyEditor::alignmentChanged(Qt::Alignment a)
{
    if (a & Qt::AlignLeft) {
        actionAlignLeft->setChecked(true);
    } else if (a & Qt::AlignHCenter) {
        actionAlignCenter->setChecked(true);
    } else if (a & Qt::AlignRight) {
        actionAlignRight->setChecked(true);
    } else if (a & Qt::AlignJustify) {
        actionAlignJustify->setChecked(true);
    }
}

void MailBodyEditor::textStyleChanged(QTextListFormat::Style style)
{
    actionBulletList->setChecked( false );
    actionOrderedList->setChecked( false );
    actionBulletDisc->setChecked( false );
    actionBulletCircle->setChecked( false );
    actionBulletSquare->setChecked( false );
    actionOrderedDecimal->setChecked( false );
    actionOrderedAlphaLower->setChecked( false );
    actionOrderedAlphaUpper->setChecked( false );
    switch (style)
    {
    case QTextListFormat::ListDisc:
        actionBulletList->setChecked( true );
        actionBulletDisc->setChecked( true );
        break;
    case QTextListFormat::ListCircle:
        actionBulletList->setChecked( true );
        actionBulletCircle->setChecked( true );
        break;
    case QTextListFormat::ListSquare:
        actionBulletList->setChecked( true );
        actionBulletSquare->setChecked( true );
        break;
    case QTextListFormat::ListDecimal:
        actionOrderedList->setChecked( true );
        actionOrderedDecimal->setChecked( true );
        break;
    case QTextListFormat::ListLowerAlpha:
        actionOrderedList->setChecked( true );
        actionOrderedAlphaLower->setChecked( true );
        break;
    case QTextListFormat::ListUpperAlpha:
        actionOrderedList->setChecked( true );
        actionOrderedAlphaUpper->setChecked( true );
        break;
    }
}

void MailBodyEditor::applyStyle(QTextListFormat::Style style)
{
    QTextCursor cursor = textCursor();
    if( style != QTextListFormat::ListStyleUndefined )
    {
        cursor.beginEditBlock();

        QTextBlockFormat blockFmt = cursor.blockFormat();

        QTextListFormat listFmt;

        if( cursor.currentList() )
        {
            listFmt = cursor.currentList()->format();
        } else
        {
            listFmt.setIndent(blockFmt.indent() + 1);
            blockFmt.setIndent(0);
            cursor.setBlockFormat(blockFmt);
        }

        listFmt.setStyle(style);

        cursor.createList(listFmt);

        cursor.endEditBlock();
    } else
    {
        // ####
        QTextBlockFormat bfmt;
        bfmt.setObjectIndex(-1);
        cursor.mergeBlockFormat(bfmt);
    }
    textStyleChanged( style );
}


