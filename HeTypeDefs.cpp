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

#include "HeTypeDefs.h"
#include <Udb/Obj.h>
#include <Udb/Transaction.h>
#include <Udb/Database.h>
#include <Udb/ContentObject.h>
#include <Txt/TextOutHtml.h>
#include <Oln2/OutlineUdbMdl.h>
#include <Oln2/OutlineUdbCtrl.h>
#include <QtCore/QMimeData>
#include <QCryptographicHash>
#include <QFile>
#include <QtDebug>
#include "HeraldApp.h"
#include "MailObj.h"
using namespace He;
using namespace Udb;

const char* IndexDefs::IdxInternalId = "IdxInternalId";
const char* IndexDefs::IdxCustomId = "IdxCustomId";
const char* IndexDefs::IdxEmailAddress = "IdxEmailAddress";
const char* IndexDefs::IdxPartyAddrDate = "IdxPartyAddrDate";
const char* IndexDefs::IdxPartyPersDate = "IdxPartyPersDate";
const char* IndexDefs::IdxPrincipalFirstName = "IdxPrincipalFirstName";
const char* IndexDefs::IdxMessageId = "IdxMessageId";
const char* IndexDefs::IdxInReplyTo = "IdxInReplyTo";
const char* IndexDefs::IdxDocumentRef = "IdxDocumentRef";
const char* IndexDefs::IdxFileHash = "IdxFileHash";
const char* IndexDefs::IdxSentOn = "IdxSentOn";
const char* IndexDefs::IdxIdentAddr = "IdxIdentAddr";
const char* IndexDefs::IdxForwardOf = "IdxForwardOf";
const char* IndexDefs::IdxStartDate = "IdxStartDate";
const char* IndexDefs::IdxCausing = "AttrCause";
const char* IndexDefs::IdxSchedOwner = "IdxSchedOwner";

const char* HeTypeDefs::s_mimeAttachRefs = "application/herald/attachment-refs";

void HeTypeDefs::init(Udb::Database &db)
{
    Udb::Database::Lock lock( &db);

	db.presetAtom( "HeStart + 1000", HeEnd );

    if( db.findIndex( IndexDefs::IdxInternalId ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrInternalId ) );
		db.createIndex( IndexDefs::IdxInternalId, def );
	}
	if( db.findIndex( IndexDefs::IdxCustomId ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrCustomId ) );
		db.createIndex( IndexDefs::IdxCustomId, def );
	}
    if( db.findIndex( IndexDefs::IdxEmailAddress ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrEmailAddress ) );
		db.createIndex( IndexDefs::IdxEmailAddress, def );
	}
    if( db.findIndex( IndexDefs::IdxPartyAddrDate ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrPartyAddr ) );
        def.d_items.append( IndexMeta::Item( AttrPartyDate, IndexMeta::None, true, true ) );
		db.createIndex( IndexDefs::IdxPartyAddrDate, def );
	}
    if( db.findIndex( IndexDefs::IdxPartyPersDate ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
        def.d_items.append( IndexMeta::Item( AttrPartyPers ) );
		def.d_items.append( IndexMeta::Item( AttrPartyDate, IndexMeta::None, true, true ) );
		db.createIndex( IndexDefs::IdxPartyPersDate, def );
	}
    if( db.findIndex( IndexDefs::IdxPrincipalFirstName ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrPrincipalName, IndexMeta::NFKD_CanonicalBase ) );
		def.d_items.append( IndexMeta::Item( AttrFirstName, IndexMeta::NFKD_CanonicalBase ) );
		db.createIndex( IndexDefs::IdxPrincipalFirstName, def );
	}
    if( db.findIndex( IndexDefs::IdxMessageId ) == 0 )
	{
		IndexMeta def( IndexMeta::Unique );
		def.d_items.append( IndexMeta::Item( AttrMessageId ) );
		db.createIndex( IndexDefs::IdxMessageId, def );
	}
    if( db.findIndex( IndexDefs::IdxInReplyTo ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrInReplyTo ) );
		db.createIndex( IndexDefs::IdxInReplyTo, def );
	}
    if( db.findIndex( IndexDefs::IdxDocumentRef ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrDocumentRef ) );
		db.createIndex( IndexDefs::IdxDocumentRef, def );
	}
    if( db.findIndex( IndexDefs::IdxFileHash ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrFileHash ) );
		db.createIndex( IndexDefs::IdxFileHash, def );
	}
    if( db.findIndex( IndexDefs::IdxSentOn ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrSentOn, IndexMeta::None, true, true ) );
		db.createIndex( IndexDefs::IdxSentOn, def );
	}
    if( db.findIndex( IndexDefs::IdxIdentAddr ) == 0 )
	{
		IndexMeta def( IndexMeta::Unique );
		def.d_items.append( IndexMeta::Item( AttrIdentAddr, IndexMeta::None ) );
		db.createIndex( IndexDefs::IdxIdentAddr, def );
	}
    if( db.findIndex( IndexDefs::IdxForwardOf ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrForwardOf ) );
		db.createIndex( IndexDefs::IdxForwardOf, def );
	}
    if( db.findIndex( IndexDefs::IdxStartDate ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrStartDate ) );
		db.createIndex( IndexDefs::IdxStartDate, def );
	}
    if( db.findIndex( IndexDefs::IdxCausing ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrCausing ) );
		db.createIndex( IndexDefs::IdxCausing, def );
	}
    if( db.findIndex( IndexDefs::IdxSchedOwner ) == 0 )
	{
		IndexMeta def( IndexMeta::Value );
		def.d_items.append( IndexMeta::Item( AttrSchedOwner ) );
		db.createIndex( IndexDefs::IdxSchedOwner, def );
	}
}

QByteArray HeTypeDefs::calcHash( const QString& filePath )
{
	QFile f( filePath );
	if( !f.open( QIODevice::ReadOnly ) )
		return QByteArray();
	QCryptographicHash h( QCryptographicHash::Sha1 );
    QByteArray arr;
	const int len = 65535; // Gute Grösse, 4096 bei grossen Dateien 500MB deutlich langsamer.
    arr.resize(len);
	qint64 n;
	while( ( n = f.read( arr.data(), len ) ) > 0 )
	{
		h.addData( arr.data(), n );
	}
	return h.result();
}

QString HeTypeDefs::prettyDate( const QDate& d, bool includeWeek, bool includeDay )
{
	const char* s_dateFormat = "yyyy-MM-dd";
	QString res;
    if( includeWeek )
		res += QString( "W%1, " ).arg( d.weekNumber(), 2, 10, QChar( '0' ) );
	if( includeDay )
		res += d.toString( "ddd, " );
    res += d.toString( s_dateFormat );
	return res;
}

QString HeTypeDefs::prettyDateTime( const QDateTime& t, bool includeWeek, bool includeDay )
{
    QString utc;
    if( t.timeSpec() == Qt::UTC )
        utc = " UTC";
	return prettyDate( t.date(), includeWeek, includeDay ) + t.time().toString( " hh:mm" ) + utc;
}

QString HeTypeDefs::prettyName(quint32 type)
{
    switch( type )
    {
    case AttrCreatedOn:
		return tr("Created On");
	case AttrModifiedOn:
		return tr("Modified On");
	case AttrText:
		return tr("Header/Text");
	case AttrInternalId:
		return tr("Internal ID");
	case AttrCustomId:
		return tr("Custom ID");
    case TypePerson:
        return tr("Person");
    case AttrFirstName:
		return tr("First Name");
	case AttrPrincipalName:
		return tr("Principal Name");
	case AttrQualiTitle:
		return tr("Title");
	case AttrOrganization:
		return tr("Organization");
	case AttrDepartment:
		return tr("Department");
	case AttrOffice:
		return tr("Office");
	case AttrJobTitle:
		return tr("Job Title");
	case AttrManager:
		return tr("Manager");
	case AttrAssistant:
		return tr("Assistant");
	case AttrPhoneDesk:
		return tr("Business Phone");
	case AttrPhoneMobile:
		return tr("Business Mobile");
	case AttrPostalAddr:
		return tr("Business Address");
	case AttrPhonePriv:
		return tr("Home Phone");
	case AttrMobilePriv:
		return tr("Private Mobile");
	case AttrPostalAddrPriv:
		return tr("Home Address");
    case TypeDocument:
        return tr("Document");
    case TypeAttachment:
        return tr("Attachment");
    case AttrDocumentRef:
        return tr("Document");
    case AttrInlineDispo:
        return tr("Disposition");
    case TypeInboundMessage:
        return tr("Inbound Mail");
    case TypeOutboundMessage:
        return tr("Outbound Mail");
    case AttrSentOn:
        return tr("Sent On");
    case AttrReceivedOn:
        return tr("Received On");
    case AttrInReplyTo:
        return tr("In Reply To");
    case TypeInbox:
        return tr("Inbox");
    case TypeFromParty:
        return tr("From Party");
    case TypeToParty:
        return tr("To Party");
    case TypeCcParty:
        return tr("CC Party");
    case TypeBccParty:
        return tr("BCC Party");
    case AttrReplyTo:
        return tr("Reply To");
    case AttrNotifyTo:
        return tr("Dispo. Notification");
    case AttrPartyAddr:
        return tr("Email Address");
    case AttrPartyPers:
        return tr("Person");
    case TypeEmailAddress:
        return tr("Email Address");
    case AttrEmailAddress:
        return tr("Address");
    case AttrEmailObsolete:
        return tr("Obsolete");
    case AttrAttCount:
        return tr("Attachments");
    case AttrUseCount:
        return tr("Use Count");
    case AttrLastUse:
        return tr("Last Use");
    case TypePopAccount:
        return tr("POP3 Account");
    case TypeSmtpAccount:
        return tr("SMTP Account");
    case TypeIdentity:
        return tr("Identity");
    case AttrIdentAddr:
        return tr("Email Address");
    case AttrIdentPop:
        return tr("POP3 Account");
    case AttrIdentSmtp:
        return tr("SMTP Account");
    case AttrSrvDomain:
        return tr("Domain Name");
    case AttrSrvUser:
        return tr("User Name");
    case AttrSrvSsl:
        return tr("Use SSL");
    case AttrSrvTls:
        return tr("Start TLS");
    case AttrSrvActive:
        return tr("Active");
    case AttrForwardOf:
        return tr("Forward of");
    case AttrFromDraft:
        return tr("From Draft");
    case AttrDraftCommitted:
        return tr( "Draft Commited");
    case AttrDraftSubmitted:
        return tr("Draft Submitted");
    case AttrDraftScheduled:
        return tr("Draft Scheduled");
    case TypeMailDraft:
        return tr("Draft Mail");
    case AttrFilePath:
    	return tr("File Path");
    case AttrPartyDate:
        return tr("Sent on");
    case TypeScheduleSet:
        return tr("Schedule Set");
    case TypeSchedule:
        return tr("Schedule");
    case TypeEvent:
        return tr("Event");
    case TypeAppointment:
        return tr("Appointment");
    case TypeDeadline:
        return tr("Deadline");
    case AttrStartDate:
        return tr("Start");
    case AttrEndDate:
        return tr("Finish");
    case AttrTimeSlot:
        return tr("Time");
    case AttrLocation:
        return tr("Location");
    case AttrCausing:
        return tr("Causing");
    case AttrDefaultSchedule:
        return tr("Default Schedule");
    case AttrSchedOwner:
        return tr("Owner");
    case AttrStatus:
        return tr("Status");
	case AttrIsEncrypted:
		return tr("Encrypted");
	case AttrIsSigned:
		return tr("Signed");
	case AttrUseKeyId:
		return tr("Uses Outlook 2010");

        // TEST
//    case AttrMessageId:
//        return tr("Message-ID");
//    case AttrFileHash:
//        return tr("Hash");
    }
    return QString();
}

QString HeTypeDefs::prettyName( quint32 atom, Udb::Transaction* txn )
{
	if( atom < HeEnd )
		return prettyName( atom );
	else
	{
        Q_ASSERT( txn != 0 );
		return QString::fromLatin1( txn->getDb()->getAtomString( atom ) );
    }
}

QString HeTypeDefs::prettyStatus(quint8 s)
{
    switch( s )
    {
    case Stat_Planned:
        return tr("Planned");
    case Stat_Tentative:
        return tr("Tentative");
    case Stat_Canceled:
        return tr("Canceled");
    default:
        return tr("<unknown>");
    }
}

QString HeTypeDefs::formatObjectTitle(const Udb::Obj & o)
{
    if( o.isNull() )
        return tr("<null>");
    Udb::Obj resolvedObj = o.getValueAsObj( AttrItemLink );
    if( resolvedObj.isNull() )
        resolvedObj = o;
    QString id = resolvedObj.getString( AttrCustomId, true );
    if( id.isEmpty() )
        id = resolvedObj.getString( AttrInternalId, true );
    QString name = resolvedObj.getString( AttrText, true );
    if( name.isEmpty() )
        name = prettyName( resolvedObj.getType() );

    if( id.isEmpty() )
        return name;
    else
        return QString("%1 %3").arg( id ).arg( name );
}

QString HeTypeDefs::formatObjectId(const Obj & o, bool useOid)
{
    if( o.isNull() )
        return tr("<null>");
    QString id = o.getString( AttrCustomId );
    if( id.isEmpty() )
        id = o.getString( AttrInternalId );
    if( id.isEmpty() && useOid )
        id = QString("#%1").arg( o.getOid() );
    return id;
}

QString HeTypeDefs::formatValue(Udb::Atom name, const Udb::Obj & obj, bool useIcons)
{
    if( obj.isNull() )
        return tr("<null>");
    const Stream::DataCell v = obj.getValue( name );

    switch( name )
    {
    case AttrInlineDispo:
        return (v.getBool())?tr("Inline"):tr("Attachment");
    case AttrSentOn:
    case AttrPartyDate:
        return prettyDateTime( v.getDateTime().toLocalTime(), true, true );
    case AttrFileHash:
        return v.getArr().toBase64();
    case AttrStatus:
        return prettyStatus( v.getUInt8() );
    }

    // TODO: Auflösung von weiteren EnumDefs

	switch( v.getType() )
	{
	case Stream::DataCell::TypeDateTime:
		return prettyDateTime( v.getDateTime() );
	case Stream::DataCell::TypeDate:
		return prettyDate( v.getDate(), false );
	case Stream::DataCell::TypeTime:
		return v.getTime().toString( "hh:mm" );
	case Stream::DataCell::TypeBml:
		{
			Stream::DataReader r( v.getBml() );
			return r.extractString(true);
		}
	case Stream::DataCell::TypeUrl:
		{
			QString host = v.getUrl().host();
			if( host.isEmpty() )
				host = v.getUrl().toString();
            return QString( "<a href=\"%1\">%2</a>" ).arg( QString::fromLatin1( v.getArr() ) ).arg( host );
		}
	case Stream::DataCell::TypeHtml:
		return Stream::DataCell::stripMarkup( v.getStr(), true ).simplified();
	case Stream::DataCell::TypeOid:
		{
			Udb::Obj o = obj.getObject( v.getOid() );
			if( !o.isNull() )
			{
				QString img;
				if( useIcons )
					img = Oln::OutlineUdbMdl::getPixmapPath( o.getType() );
				if( !img.isEmpty() )
					img = QString( "<img src=\"%1\" align=\"middle\"/>&nbsp;" ).arg( img );
                return Txt::TextOutHtml::linkToHtml( Oln::Link( o ).writeTo(),
                                                         img + formatObjectTitle( o ) );
			}
		}
	case Stream::DataCell::TypeTimeSlot:
		{
			Stream::TimeSlot t = v.getTimeSlot();
			if( t.d_duration == 0 )
				return t.getStartTime().toString( "hh:mm" );
			else
				return t.getStartTime().toString( "hh:mm - " ) + t.getEndTime().toString( "hh:mm" );
		}
	case Stream::DataCell::TypeNull:
	case Stream::DataCell::TypeInvalid:
		return QString();
	case Stream::DataCell::TypeAtom:
		return prettyName( v.getAtom() );
	default:
		return v.toPrettyString();
	}
    return QString();
}

bool HeTypeDefs::isParty(quint32 type)
{
    switch( type )
    {
    case TypeFromParty:
    case TypeToParty:
    case TypeCcParty:
    case TypeBccParty:
    case TypeResentParty:
        return true;
    }
    return false;
}

bool HeTypeDefs::isEmail(quint32 type)
{
    switch( type )
    {
    case TypeInboundMessage:
    case TypeOutboundMessage:
        return true;
    }
    return false;
}

bool HeTypeDefs::isPrintableAscii(const QByteArray & str, bool spaceAllowed, bool emptyAllowed)
{
    if( !emptyAllowed && str.isEmpty() )
        return false;
    for( int i = 0; i < str.size(); i++ )
        if( ( !spaceAllowed && str[i] == ' ' ) || str[i] < ' ' || str[i] >= 127 )
            return false;
    return true;
}

QList<Udb::Obj> HeTypeDefs::readObjectRefs(const QMimeData *data, Udb::Transaction* txn )
{
    Q_ASSERT( txn != 0 );
	return Udb::Obj::readObjectRefs( data, txn );
}

void HeTypeDefs::writeObjectRefs(QMimeData *data, const QList<Udb::Obj> & objs)
{
    if( objs.isEmpty() )
        return;
	Udb::Obj::writeObjectRefs( data, objs );
	Udb::Obj::writeObjectUrls( data, objs, Udb::ContentObject::AttrAltIdent, Udb::ContentObject::AttrText );
}

QList<QPair<QString, QUrl> > HeTypeDefs::readAttachRefs(const QMimeData *data)
{
    QList<QPair<QString, QUrl> > res;

    if( !data->hasFormat( QLatin1String( HeTypeDefs::s_mimeAttachRefs ) ) )
        return res;
    Stream::DataReader r( data->data(QLatin1String( HeTypeDefs::s_mimeAttachRefs )) );
    Stream::DataReader::Token t = r.nextToken();
    while( t == Stream::DataReader::BeginFrame )
    {
        QPair<QString, QUrl> pair;
        t = r.nextToken();
        while( t == Stream::DataReader::Slot )
        {
            if( r.getValue().isString() )
                pair.first = r.getValue().getStr();
            else if( r.getValue().getType() == Stream::DataCell::TypeUrl )
                pair.second = r.getValue().getUrl();
            else
                return QList<QPair<QString, QUrl> >();
            t = r.nextToken();
        }
        res.append( pair );
        t = r.nextToken();
    }
    return res;
}

void HeTypeDefs::writeAttachRefs(QMimeData *data, const QList<Obj> & objs)
{
    if( objs.isEmpty() )
        return;
    bool hasRefs = false;
	Stream::DataWriter out1;
    QList<QUrl> urls;
    foreach( Udb::Obj o, objs )
	{
        if( o.getType() == TypeAttachment )
        {
            AttachmentObj att = o;
            out1.startFrame();
            out1.writeSlot( att.getValue( AttrText ) );
            QUrl url = QUrl::fromLocalFile(att.getDocumentPath());
            urls.append( url );
            out1.writeSlot( Stream::DataCell().setUrl( url ) );
            out1.endFrame();
            hasRefs = true;
        }
	}
    if( hasRefs )
    {
        data->setData( QLatin1String( HeTypeDefs::s_mimeAttachRefs ), out1.getStream() );
        // Nein, sonst wird Attachment in Crossline direkt als Datei referenziert: data->setUrls( urls );
    }
}

bool HeTypeDefs::isValidAggregate( quint32 child, quint32 parent )
{
	switch( parent )
	{
	case TypeEvent:
		switch( child )
		{
		case TypeAppointment:
		case TypeDeadline:
			return true;
		}
		break;
	case TypeSchedule:
		switch( child )
		{
		case TypeAppointment:
		case TypeDeadline:
		case TypeEvent:
			return true;
		}
		break;
    }
	return false;
}

bool HeTypeDefs::isScheduleItem( quint32 type )
{
	switch( type )
	{
	case TypeEvent:
	case TypeAppointment:
	case TypeDeadline:
		return true;
	default:
		return false;
	}
}

bool HeTypeDefs::isCalendarItem( quint32 type )
{
	switch( type )
	{
	case TypeAppointment:
	case TypeDeadline:
		return true;
	default:
		return false;
	}
}

QString HeTypeDefs::timeSlotToolTip( QDate d, const Stream::TimeSlot& ts )
{
	return QString( "<b>Date:</b> %1<br><b>Time:</b> %2 - %3<br><b>Duration:</b> %4 Minutes" ).
		arg( prettyDate( d ) ).
		arg( ts.getStartTime().toString("h:mm") ).arg( ts.getEndTime().toString("h:mm") ).
		arg( ts.d_duration );
}

QString HeTypeDefs::dateRangeToolTip( const QDate& a, const QDate& b )
{
	const QDate from = (a < b)?a:b;
	const QDate to = (a < b)?b:a;
	if( from == to )
		return QString( "<b>Date:</b> %1" ).arg( prettyDate( from ) );
	else
		return QString( "<b>From:</b> %1<br><b>To:</b> %2<br><b>Days:</b> %3" ).
			arg( prettyDate( from ) ).
			arg( prettyDate( to ) ).
			arg( from.daysTo( to ) + 1 );
}

static QString _prettyAtts( const quint32* atts, const Udb::Obj& o )
{
	Stream::DataCell v;
	int i = 0;
	bool first = true;
	QString html;
	while( atts[i] )
	{
        if( !first )
            html += "<br>";
        first = false;
        html += QString( "<b>%1:</b> %2" ).
            arg( HeTypeDefs::prettyName( atts[i] ) ).
            arg( HeTypeDefs::formatValue( atts[i], o, false ) );
		i++;
	}
	return html;
}

QString HeTypeDefs::toolTip(const Udb::Obj& o)
{
	if( o.isNull() )
		return QString( "Invalid Object" );

	const quint32 type = o.getType();
	const QString header = QString( "<p><img src=\"%1\"/>&nbsp;<b>%2</b></p>" ).
		arg( Oln::OutlineUdbMdl::getPixmapPath( type ) ).arg( QString( prettyName( type ) ) );
	QString html = "<html>";
	html += header;
	if( type == TypePerson )
	{
		static const quint32 s_attrs[] = { AttrPrincipalName, AttrFirstName, AttrOrganization,
			AttrDepartment, AttrOffice, AttrPhoneDesk, AttrPhoneMobile,
			AttrMobilePriv, AttrInternalId, 0 };
		html += _prettyAtts( s_attrs, o );
	}else if( isScheduleItem( type ) )
	{
		const QDate from = o.getValue(AttrStartDate).getDate();
		QDate to = o.getValue(AttrEndDate).getDate();
		if( !to.isValid() )
			to = from;
		QString loc = o.getString( AttrLocation );
		if( !loc.isEmpty() )
			loc = QString( "<b>Where:</b> %1<br>" ).arg( loc );
		if( from == to )
		{
			// Termine und Meilensteine
			QString time;
			Stream::DataCell v = o.getValue( AttrTimeSlot );
			if( v.isTimeSlot() )
			{
				Stream::TimeSlot s = v.getTimeSlot();
				if( s.d_duration == 0 )
					time = QString( "<br><b>Time:</b> %1" ).arg( s.getStartTime().toString( "h:mm" ) );
				else
					time = QString( "<br><b>Time:</b> %1 - %2" ).arg( s.getStartTime().toString( "h:mm" ) ).
						arg( s.getEndTime().toString( "h:mm" ) );
			}
			html += QString( "<b>What:</b> %1<br>"
				"%3<b>Date:</b> %2%4%5<br>"
				"<b>Context:</b> %6" ).
				arg( o.getString(AttrText) ). // 1
				arg( prettyDate( from ) ). // 2
				arg( loc ). // 3
				arg( time ). // 4
                    arg( QString() ). // 5
				arg( o.getParent().getString( AttrText ) ); // 6
		}else
		{
			html += QString( "<b>What:</b> %1<br>%5"
				"<b>Start:</b> %2<br>"
				"<b>End:</b> %3<br>"
				"<b>Days:</b> %4%6<br>"
				"<b>Context:</b> %7" ).
				arg( o.getString(AttrText) ). // 1
				arg( prettyDate( from ) ). // 2
				arg( prettyDate( to ) ). // 3
				arg( from.daysTo( to ) + 1 ). // 4
				arg( loc ). // 5
				arg( QString() ). // 6
				arg( o.getParent().getString( AttrText ) ); // 7
		}
        if( o.getValue( AttrStatus ).getUInt8() == Stat_Canceled )
            html += QString( "<br><b>Status:</b> Canceled" );
	}else
	{
		if( o.getParent().isNull() || prettyName( o.getParent().getType() ).isEmpty() )
			html += QString( "<b>What:</b> %1" ).arg( o.getString( AttrText ) );
		else
			html += QString( "<b>What:</b> %1<br><b>Context:</b> %2" ).
				arg( o.getString( AttrText ) ).arg( o.getParent().getString( AttrText ) );
	}
	html += "</html>";
	return html;
}
