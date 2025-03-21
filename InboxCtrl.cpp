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

#include "InboxCtrl.h"
#include <QTreeView>
#include <QHeaderView>
#include <QTextBrowser>
#include <QMessageBox>
#include <QPainter>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QDir>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/Idx.h>
#include <Mail/MailMessage.h>
#include <Mail/MailMessagePart.h>
#include "HeTypeDefs.h"
#include "ObjectHelper.h"
#include "MailObj.h"
#include "HeraldApp.h"
using namespace He;

InboxCtrl::InboxCtrl(QTreeView * parent, InboxMdl* mdl) :
    QObject(parent), d_mdl(mdl)
{
    Q_ASSERT( mdl != 0 );
}

InboxCtrl *InboxCtrl::create(QWidget *parent, const Udb::Obj &root)
{
    QTreeView* tree = new QTreeView( parent );
	tree->setAlternatingRowColors( true );
	tree->setRootIsDecorated( false );
	//tree->setUniformRowHeights( false );
	tree->setAllColumnsShowFocus(true);
	//tree->setWordWrap( true );
	tree->setSelectionBehavior( QAbstractItemView::SelectRows );
	tree->setSelectionMode( QAbstractItemView::ExtendedSelection );
	tree->header()->hide();
    InboxMdl* mdl = new InboxMdl( tree );
    root.getDb()->addObserver( mdl, SLOT(onDbUpdate( Udb::UpdateInfo )), false );
    mdl->setQueue( root );
    tree->setModel( mdl );

    InboxDeleg* deleg = new InboxDeleg( tree, root.getTxn() );
    tree->setItemDelegate( deleg );

    InboxCtrl* ctrl = new InboxCtrl(tree, mdl);
    connect( tree->selectionModel(), SIGNAL( selectionChanged ( const QItemSelection &, const QItemSelection & ) ),
        ctrl, SLOT( onSelectionChanged() ) );
    connect( tree, SIGNAL(clicked(QModelIndex)), ctrl, SLOT( onSelectionChanged() ) );

    return ctrl;
}

void InboxCtrl::addCommands(Gui::AutoMenu * pop)
{
    pop->addCommand( tr("Accept Mail"), this, SLOT( onAccept() ) );
    pop->addSeparator();
    pop->addCommand( tr("Show Source..."), this, SLOT( onShowSource() ) );
    pop->addCommand( tr("Save as..."), this, SLOT( onSaveAs() ) );
    pop->addCommand( tr("Save Body..."), this, SLOT( onSaveBodyAs() ) );
    pop->addSeparator();
    pop->addCommand( tr("Delete Items"), this, SLOT( onDelete() ), tr("CTRL+D"), true );
}

QTreeView *InboxCtrl::getTree() const
{
    return dynamic_cast<QTreeView*>( parent() );
}

Udb::Obj InboxCtrl::getSelectedObject() const
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    if( sr.size() == 1 )
    {
        Udb::Qit q = d_mdl->getItem( sr.first() );
        if( q.getValue().isOid() )
            return d_mdl->getQueue().getObject( q.getValue().getOid() );
    }// else
    return Udb::Obj();
}

QString InboxCtrl::getSelectedFile() const
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    if( sr.size() == 1 )
    {
        Udb::Qit q = d_mdl->getItem( sr.first() );
        Udb::Obj::ValueList array = Udb::Obj::unpackArray( q.getValue() );
        if( !array.isEmpty() )
            return HeraldApp::adjustPath(array[InboxFilePath].getStr());
    }// else
    return QString();
}

Udb::OID InboxCtrl::getSelectedPopAccount() const
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    if( sr.size() == 1 )
    {
        Udb::Qit q = d_mdl->getItem( sr.first() );
        Udb::Obj::ValueList array = Udb::Obj::unpackArray( q.getValue() );
        if( !array.isEmpty() )
            return array[InboxPopAcc].getOid();
    }// else
    return 0;
}

bool InboxCtrl::markSelectedAsRead()
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    if( sr.size() == 1 )
    {
        Udb::Qit q = d_mdl->getItem( sr.first() );
        Udb::Obj::ValueList array = Udb::Obj::unpackArray( q.getValue() );
        if( array.size() < InboxArrayLen )
            array.resize( InboxArrayLen );
        array[InboxSeen].setBool(true );
        q.setValue( Udb::Obj::packArray( array ) );
        d_mdl->getQueue().getTxn()->commit();
        return true;
    }// else
    return false;
}

void InboxCtrl::onDelete()
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
	ENABLED_IF( sr.size() > 0 );

	// Die Objekte vor dem Dialog extrahieren. Sonst könnte währenddessen weitere Objekte dazukommen und
	// die Selektion verändern
	QList<Udb::Qit> objs;
	for( int i = 0; i < sr.size(); i++ )
	{
		Udb::Qit q = d_mdl->getItem( sr[i] );
		if( q.getValue().isOid() )
		{
			// Kommt nicht mehr vor
		}else
		{
			objs.append( q );
		}
	}

    const int res = QMessageBox::warning( getTree(), tr("Delete Items"),
		tr("Do you really want to permanently delete the %1 selected items? This cannot be undone." ).
		arg( objs.size() ),
		QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes );
	if( res == QMessageBox::No )
		return;

    emit sigUnselect();

	for( int i = 0; i < objs.size(); i++ )
    {
		Udb::Obj::ValueList array = Udb::Obj::unpackArray( objs[i].getValue() );
		objs[i].erase();
		if( !array.isEmpty() )
			QFile( HeraldApp::adjustPath(array[InboxFilePath].getStr() ) ).remove();
	}
    d_mdl->getQueue().getTxn()->commit();
}

void InboxCtrl::onAccept()
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    ENABLED_IF( !sr.isEmpty() );

    emit sigUnselect();

    QList<Udb::Qit> qits;
    foreach( QModelIndex i, sr )
    {
        qits.append( d_mdl->getItem( i ) );
    }
    QList<Udb::Obj> objs;
    foreach( Udb::Qit q, qits )
    {
        Udb::Obj o = acceptMail( q );
        if( !o.isNull() )
        {
            objs.append( o );
            // Wir müssen bei jedem accept Commit machen, da sonst EmailAdressen mehrfach erstellt werden
            d_mdl->getQueue().getTxn()->commit();
        }else
        {
            d_mdl->getQueue().getTxn()->rollback();
            return;
        }
    }
    emit sigAccepted( objs );
}

void InboxCtrl::onShowSource()
{
    QString path = getSelectedFile();
    ENABLED_IF( !path.isEmpty() );

    QTextBrowser* b = new QTextBrowser();
    QFile f( path );
    if( !f.open(QIODevice::ReadOnly) )
    {
        QMessageBox::critical( getTree(), tr("Show Source - Herald"),
                              tr("Cannot find source file '%1'").arg( path ) );
        return;
    }
    b->setLineWrapMode( QTextEdit::NoWrap );
    b->setPlainText( QString::fromLatin1( f.readAll() ) );
    b->setAttribute( Qt::WA_DeleteOnClose );
    b->setWindowTitle( tr("Source '%1' - Herald").arg( path ) );
    b->show();
    HeraldApp::inst()->setWindowGeometry( b );
}

void InboxCtrl::onSaveAs()
{
    QString path = getSelectedFile();
    ENABLED_IF( !path.isEmpty() );

    QString to = QFileDialog::getSaveFileName( getTree(), tr("Save Message - Herald"),
        QString(), tr("Email RfC822 (*.eml)") );
    if( to.isEmpty() )
        return;
    QFile f( path );
    f.copy( to );
}

void InboxCtrl::onSaveBodyAs()
{
    QString path = getSelectedFile();
    ENABLED_IF( !path.isEmpty() );

    QString to = QFileDialog::getSaveFileName( getTree(), tr("Save Body - Herald"),
        QString(), tr("HTML (*.html)") );
    if( to.isEmpty() )
        return;

    MailMessage msg;
    msg.fromRFC822( LongString( path, false ) ); // TODO: Errors?
    QFile f( to );
    if( !f.open( QIODevice::WriteOnly ) )
    {
        QMessageBox::critical( getTree(), tr("Save Body - Herald"),
                               tr( "Cannot open file for writing: %1" ).arg( to ) );
        return;
    }
    if( !msg.htmlBody().isEmpty() )
        f.write( msg.htmlBody().toUtf8() );
    else
    {
        f.write( "<html>\n"
            "<META http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\">\n"
            "<body>\n" );
        f.write( MailObj::plainText2Html( msg.plainTextBody() ).toUtf8() );
        f.write( "</body></html>" );
    }
}

void InboxCtrl::onSelectionChanged()
{
    QModelIndexList sr = getTree()->selectionModel()->selectedRows();
    if( sr.size() == 1 )
        emit sigSelectionChanged();
}

QVariant InboxMdl::data( const Udb::Obj& o, int role ) const
{
    if( role != Qt::DisplayRole && role != Qt::ToolTipRole && role < From )
        return QVariant();
    if( o.isNull() )
        return "null";
    MailObj mail = o;
    switch( role )
    {
    case Qt::DisplayRole:
        break;
    case Qt::ToolTipRole:
        return tr("<html><b>ID:</b> %4 <br>"
                  "<b>From:</b> %1 <br>"
                  "<b>Sent:</b> %2 <br>"
                  "<b>Subject:</b> %3" ).arg( mail.getFrom().prettyNameEmail() ).
                arg( HeTypeDefs::prettyDateTime(
                         o.getValue( AttrSentOn ).getDateTime().toLocalTime(), true, true ) ).
                arg( o.getString( AttrText ) ).
                arg( o.getString( AttrInternalId ) );
        break;
    case From:
        return mail.getFrom().d_name;
    case DateTime:
        return o.getValue( AttrSentOn ).getDateTime().toLocalTime();
    case Subject:
        return o.getString( AttrText );
    case Accepted:
        return true;
    case AttCount:
        return o.getValue( AttrAttCount ).getUInt32();
    }
	return QVariant();
}

QVariant InboxMdl::data(const Stream::DataCell & v, int role) const
{
    if( role != Qt::DisplayRole && role != Qt::ToolTipRole && role < From )
        return QVariant();
    Udb::Obj::ValueList array = Udb::Obj::unpackArray( v );
    if( array.size() > 4 )
    {
        QString name;
        QByteArray address;
        MailMessage::parseEmailAddress( array[InboxFrom].getStr(), name, address );
        if( name.isEmpty() )
            name = address;
        switch( role )
        {
        case Qt::DisplayRole:
            return tr("%1: %2").arg( name ).arg( array[InboxSubject].getStr() );
        case Qt::ToolTipRole:
            return tr("<html><b>From:</b> %1 <br>"
                      "<b>Sent:</b> %2 <br>"
                      "<b>Subject:</b> %3" ).arg( array[InboxFrom].getStr().toHtmlEscaped() ).
                    arg( HeTypeDefs::prettyDateTime( array[InboxDateTime].getDateTime(), true, true ) ).
                         arg( array[InboxSubject].getStr() );
        case From:
            return name;
        case DateTime:
            return array[InboxDateTime].getDateTime();
        case Subject:
            return array[InboxSubject].getStr();
        case AttCount:
            return array[InboxAttCount].getUInt16();
        case Opened:
            if( array.size() > InboxSeen )
                return array[InboxSeen].getBool();
        case Accepted:
            return false;
        }
    }
    return QVariant();
}

void InboxDeleg::paint ( QPainter * painter, const QStyleOptionViewItem & option,
							const QModelIndex & index ) const
{
	painter->save();
    if (option.showDecorationSelected && (option.state & QStyle::State_Selected))
	{
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        if (cg == QPalette::Normal && !(option.state & QStyle::State_Active))
            cg = QPalette::Inactive;

        painter->fillRect(option.rect, option.palette.brush(cg, QPalette::Highlight));
		//painter->setOpacity( 0.8 );
	}
    paintItem( painter, option, index );
    if( option.state & QStyle::State_HasFocus )
	{
		QStyleOptionFocusRect o;
		o.QStyleOption::operator=(option);
		o.state |= QStyle::State_KeyboardFocusChange;
		QPalette::ColorGroup cg = (option.state & QStyle::State_Enabled)
								  ? QPalette::Normal : QPalette::Disabled;
		o.backgroundColor = option.palette.color(cg, (option.state & QStyle::State_Selected)
												 ? QPalette::Highlight : QPalette::Window);
		QApplication::style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter);
	}
    painter->restore();
}

void InboxDeleg::paintItem(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Zeilenhöhe
    const int lineHeight = option.fontMetrics.height() + 4;
	// Left Indent für Anzeige Ikonen
	const int leftIndent = 16;
	// Breite des Datumsbereichs
	const int dateWidth = option.fontMetrics.width( " 000000-0000 " );

    painter->setPen( Qt::black );
    if( option.state & QStyle::State_Selected )
    {
        QPalette::ColorGroup cg = option.state & QStyle::State_Enabled
                                  ? QPalette::Normal : QPalette::Disabled;
        painter->setPen( option.palette.color( cg, QPalette::HighlightedText ) );
        // painter->setPen( Qt::white );
    }

	const QRect rDate = option.rect.adjusted( option.rect.width() - dateWidth, 0, 0, -lineHeight );
	painter->drawText( rDate.adjusted( 2, 0, -2, -1 ), Qt::AlignLeft | Qt::AlignVCenter,
		index.data( InboxMdl::DateTime ).toDateTime().toString( "yyMMdd-hhmm" ) );

	const QRect rName = option.rect.adjusted( leftIndent + 1, 0, -rDate.width() - 2, -lineHeight - 1 );
	const QFont f = painter->font();
	QFont bf = f;
	bf.setBold( true );
	painter->setFont( bf );
	painter->drawText( rName, Qt::AlignLeft | Qt::AlignVCenter,
                       painter->fontMetrics().elidedText( index.data( InboxMdl::From ).toString(),
                       Qt::ElideRight, rName.width() ) );
	painter->setFont( f );

	const QRect rSubj = option.rect.adjusted( leftIndent, lineHeight, 0, 0 );
	painter->drawText( rSubj.adjusted( 1, - 2, 0, -1 ), Qt::AlignLeft | Qt::AlignVCenter,
                       painter->fontMetrics().elidedText( index.data( InboxMdl::Subject ).toString(),
                       Qt::ElideRight, rSubj.width() ) );

	QPixmap pix;
    if( index.data( InboxMdl::Accepted ).toBool() )
        pix.load( ":/images/document-mail-open.png" );
    else if( index.data( InboxMdl::Opened ).toBool() )
        pix.load( ":/images/mail-medium-open-document.png" );
    else
        pix.load( ":/images/mail-medium.png" );
	painter->drawPixmap( option.rect.topLeft(), pix );

    const int attCount = index.data( InboxMdl::AttCount ).toInt();
    if( attCount > 0 )
    {
        QRect r( option.rect.topLeft(), QSize( leftIndent, lineHeight ) );
        r.translate( 2, lineHeight );
        pix.load( ":/images/paper-clip-small.png" );
        painter->drawPixmap( r.topLeft(), pix );
        painter->setFont( QFont( "Small Fonts", 6 ) ); // , QFont::DemiBold ) );
        painter->drawText( r, Qt::AlignTop | Qt::AlignLeft, QString::number( attCount ) );
    }

    painter->setPen( Qt::gray );
	painter->drawLine( option.rect.bottomLeft(), option.rect.bottomRight() );
}

QSize InboxDeleg::sizeHint ( const QStyleOptionViewItem & option,
								const QModelIndex & index ) const
{
    return QSize( 0, 2 * option.fontMetrics.height() + 8 );
}

Udb::Obj InboxCtrl::acceptMail(Udb::Qit & q)
{
    Udb::Obj::ValueList array = Udb::Obj::unpackArray( q.getValue() );
    if( array.isEmpty() )
        return Udb::Obj();
    Udb::Obj mailObj = MailObj::acceptInbound( q.getTxn(), HeraldApp::adjustPath(array[InboxFilePath].getStr()) );
    if( mailObj.isNull() )
        return Udb::Obj();

    q.erase();
    // q.setValue( mailObj );
    return mailObj;
}
