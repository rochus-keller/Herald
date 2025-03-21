#ifndef HETYPEDEFS_H
#define HETYPEDEFS_H

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

#include <QObject>
#include <Udb/Obj.h>
#include <QtDebug>

class QMimeData;

namespace He
{
    enum HeNumbers
	{
		HeStart = 0x30000,
		HeMax = HeStart + 109,
		HeEnd = HeStart + 1000 // Ab hier werden dynamische Atome angelegt
	};

    struct IndexDefs
	{
		static const char* IdxInternalId; // AttrInternalId
		static const char* IdxCustomId; // AttrCustomId
        static const char* IdxEmailAddress; // AttrEmailAddress (welche EmailAddress gibt es)
        static const char* IdxPartyAddrDate; // AttrPartyAddr, AttrPartyDate
        static const char* IdxPartyPersDate; // AttrPartyPers, AttrPartyDate
        static const char* IdxPrincipalFirstName; // AttrPrincipalName, AttrFirstName
        static const char* IdxMessageId; // AttrMessageId
        static const char* IdxInReplyTo; // AttrInReplyTo
        static const char* IdxDocumentRef; // AttrDocumentRef
        static const char* IdxFileHash; // AttrFileHash
        static const char* IdxSentOn; // AttrSentOn
        static const char* IdxIdentAddr; // AttrIdentAddr
        static const char* IdxForwardOf; // AttrForwardOf
        static const char* IdxStartDate; // AttrStartDate
        static const char* IdxCausing;    // AttrCausing
        static const char* IdxSchedOwner; // AttrSchedOwner
    };

    enum TypeDef_Object // abstract
	{
		AttrCreatedOn = HeStart + 1, // DateTime
		AttrModifiedOn = HeStart + 2, // DateTime
		AttrText = HeStart + 3, // String, RTXT oder HTML, optional; anzeigbarer Titel/Text des Objekts
		AttrInternalId = HeStart + 4, // String, wird bei einigen Klassen mit automatischer ID gesetzt
		AttrCustomId = HeStart + 5, // String, der User kann seine eigene ID verwenden, welche die interne übersteuert
        AttrCausing = HeStart + 101, // OID. Referenz auf Objekt, das die Folge ist für dieses Objekt.
        // Beispiel: iCal-Attachment, das einen Termin verursacht hat. TODO: braucht es mehrere Referenzen?
		AttrMessageId = HeStart + 48 // Für Messages und iCal: latin-1, indiziert; wird ohne <> gespeichert!
	};

    enum TypeDef_OutlineItem // inherits Object
	{
		TypeOutlineItem = HeStart + 6,
		TypeOutline = HeStart + 7,
		AttrItemIsExpanded = HeStart + 8, // bool: offen oder geschlossen
		AttrItemIsTitle = HeStart + 9, // bool: stellt das Item einen Title dar oder einen Text
		AttrItemIsReadOnly = HeStart + 10, // bool: ist Item ein Fixtext oder kann es bearbeitet werden
		AttrItemHome = HeStart + 11, // OID: Referenz auf Root des Outlines vom Typ TypeOutline
		AttrItemLink = HeStart + 12 // OID: optionale Referenz auf Irgend ein Object, dessen Body statt des eigenen angezeigt wird.
	};

    enum TypeDef_Person // inherits Object
	{
		TypePerson = HeStart + 28,
		// AttrText wird für Pretty-Darstellung der Form "Dr. Rochus Keller" verwendet
		AttrFirstName = HeStart + 29, // String
        AttrPrincipalName = HeStart + 30, // String
		AttrQualiTitle = HeStart + 31, // String: Dr., Hptm., Dipl. Ing. etc.
		AttrOrganization = HeStart + 32, // String
		AttrDepartment = HeStart + 33, // String
		AttrOffice = HeStart + 34, // String: Bürokoordinaten
		AttrJobTitle = HeStart + 35, // String
		AttrManager = HeStart + 36,	// String: Name des Vorgesetzten
		AttrAssistant = HeStart + 37,	// String: Name des Assistenten, Sekretärin etc.
		AttrPhoneDesk = HeStart + 38, // String
		AttrPhoneMobile = HeStart + 39, // String
		AttrPostalAddr = HeStart + 41, // String, multiline, Kasernenstrasse 19, CH-3001 Bern
		AttrPhonePriv = HeStart + 42, // String
		AttrMobilePriv = HeStart + 43, // String
		AttrPostalAddrPriv = HeStart + 44 // String
	};

    enum TypeDef_EmailAddress // inherits Object
    {
        TypeEmailAddress = HeStart + 13,
        AttrEmailAddress = HeStart + 14, // latin1, indexed
        AttrEmailObsolete = HeStart + 15, // bool: falls true soll Adresse nicht mehr gebraucht werden soll
        // AttrText für Pretty-Name zur Adresse, falls vorhanden

        AttrUseCount = HeStart + 16, // uint32: wird bei jedem Receive (ob to, cc oder bcc) erhöht
		AttrLastUse = HeStart + 17, // dateTime: Timestamp der letzten Verwendung in einer Party
		AttrUseKeyId = HeStart + 109 // bool, optional, openssl cms -keyid

        // Folgende Felder sind unnötig, da redundant zur Logik (man kann alles berechnen)
        //AttrSendCount = HeStart + 18, // uint32: wird bei jedem Send (ob to, cc oder bcc) erhöht
        //AttrLastSend = HeStart + 19, // dateTime: Timestamp des letzten Increment von AttrSendCount

        // NOTE: EmailAddress ist alleinstehend oder Aggregat der Person, welche diese Besitzt
        // NOTE: der Domain Part unterscheidet nicht Gross/Kleinschreibung, der Local Part schon.
        // Hier wird jedoch auch beim Local Part nicht unterschieden; stattdessen alles in Kleinbuchstaben umwandeln.
    };

    enum TypeDef_Party
    {
        // Parent ist EmailMessage

        // Wir nehmen in Kauf, dass pro Message viele OID wegen Party verbraucht werden.
        TypeFromParty = HeStart + 20,
        TypeToParty = HeStart + 21,
        TypeCcParty = HeStart + 22,
        TypeBccParty = HeStart + 23,
        TypeResentParty = HeStart + 87,

        // AttrText als optionaler Party Name, String
        AttrPartyAddr = HeStart + 26, // OID|null: Referenz auf TypeEmailAddress; indiziert
        AttrPartyPers = HeStart + 27, // OID|null: Referenz auf TypePerson; indiziert
        AttrPartyDate = HeStart + 61, // DateTime: Kopie von AttrSentOn für Indizierung

        // NOTE: IdxPartyPersAddr ist ein chronologisch aufsteigender Index aller Messages pro Person

		TypeResentPartyDraft = HeStart + 88
        // Enthält folgende Felder: AttrDraftFrom, AttrDraftTo, AttrDraftScheduled,
        // AttrDraftSubmitted, AttrDraftCommitted
    };

    enum Inbox_Attrs
    {
        InboxFilePath = 0,  // String: Path zur heruntergeladenen Datei
        InboxFrom = 1,      // String
        InboxSubject = 2,   // String
        InboxDateTime = 3,  // dateTime (Local Time!)
        InboxAttCount = 4,  // uint16: number of attachments
        InboxPopAcc = 5,    // OID: pop account
        InboxSeen = 8,      // bool: seen
        InboxDispAtt = 9,   // array(uint16): disposed attachments
        InboxArrayLen = 10
    };

    enum TypeDef_Inbox
    {
        TypeInbox = HeStart + 50
        // static Object s_inboxUuid
        // Als Queue verwendet mit Slots als Array Inbox_Attrs
        // oder OID auf EmailMessage
    };

    enum TypeDef_EmailMessage // inherits Object
    {
        TypeInboundMessage = HeStart + 45,
        TypeOutboundMessage = HeStart + 52,
        // AttrText geerbt als Subject
        AttrBody = HeStart + 46, // String|HTML
        AttrSentOn = HeStart + 47,	// DateTime, indiziert: die in der Mail genannte Sendezeit in UTC
        AttrReceivedOn = HeStart + 59, // DateTime: local Time
        AttrInReplyTo = HeStart + 49, // OID|latin-1, indiziert: Referenz auf vorangehende EmailMessage
        AttrForwardOf = HeStart + 82, // OID, indiziert: Referenz auf weitergeleitete EmailMessage
        AttrRawHeaders = HeStart + 51, // latin-1, compressed; der Originalheader, non-parsed; eigentlich ascii; ich will aber keine Exceptions
        AttrAttCount = HeStart + 62, // uint32, counter: number of Attachments
        AttrReplyTo = HeStart + 24, // OID, optional
        AttrNotifyTo = HeStart + 25, // OID|bool, optional, Disposition Notification
        AttrFromDraft = HeStart + 85, // String, optional
		AttrIsEncrypted = HeStart + 107, // bool, optional
		AttrIsSigned = HeStart + 108, // bool, optional

        // Folgendes als Map der Form AttrReference, Lnr -> OID
		AttrReference = HeStart + 86 // OID, Speichert das RFC822-References-Feld, sonst keine Funktion

        // From, To, CC, BCC als aggregierte Party
        // Attachments als Aggregate
    };

    enum TypeDef_Attachment
    {
        TypeAttachment = HeStart + 53,
        // AttrText als lokaler Dateiname
        AttrDocumentRef = HeStart + 54, // oid, indiziert: ref auf TypeDocument
        AttrInlineDispo = HeStart + 55, // bool, optional: true bei "Content-Disposition: inline"
		AttrContentId = HeStart + 60 // ascii, wird mit <> gespeichert!
    };

    enum TypeDef_Document // inherits Object
    {
        TypeDocument = HeStart + 56,
        AttrFileHash = HeStart + 57, // lob, indiziert: sha1 der Datei
        // AttrText als Original-Dateiname, ohne Pfad
		AttrFilePath = HeStart + 58 // String, optional: Pfad auf gespeicherte Datei
        // Falls AttrFilePath fehlt, wird in lokalem Verzeichnis "docstore" nach Dxyz_<filename> gesucht
    };

    enum TypeDef_Identity // inherits Object
    {
        TypeIdentity = HeStart + 63, // Alle aggregiert in s_accounts
        // AttrText als pretty Name
        AttrIdentAddr = HeStart + 64, // OID, unique: Ref auf TypeEmailAddress
        AttrIdentPop = HeStart + 65, // OID: Ref auf TypePopAccount
        AttrIdentSmtp = HeStart + 66, // OID: Ref auf TypeSmtpAccount
		AttrPrivateKey = HeStart + 106, // String, optional

        // Attribut von Accounts Root
        AttrDefaultIdent = HeStart + 103 // OID oder nil
    };

    enum TypeDef_PopSmtpAccount // inherits Object
    {
        TypePopAccount = HeStart + 67, // Alle aggregiert in s_accounts
        TypeSmtpAccount = HeStart + 74,
        // AttrText als pretty Name
        AttrSrvDomain = HeStart + 68, // ascii
        AttrSrvPort = HeStart + 101, // uint16
        AttrSrvUser = HeStart + 69, // ascii
        AttrSrvPwd = HeStart + 70, // ascii
        AttrSrvSsl = HeStart + 71, // bool
        AttrSrvTls = HeStart + 72, // bool
		AttrSrvActive = HeStart + 73  // bool
    };

    enum TypeDef_MailDraft // inherits Object
    {
        TypeMailDraft = HeStart + 75,
        // AttrText geerbt als Subject
        // AttrMessageId, AttrInReplyTo, AttrForwardOf, AttrNotifyTo und AttrBody von EmailMessage verwendet
        AttrDraftFrom = HeStart + 76, // OID auf Identity
        AttrDraftTo = HeStart + 77, // BML
        AttrDraftCc = HeStart + 78, // BML
        AttrDraftBcc = HeStart + 79, // BML
        AttrDraftSubmitted = HeStart + 80, // DateTime: wann gesendet bevor Accepted
        AttrDraftCommitted = HeStart + 84, // DateTime: wann vom Server committed bevor Accepted
        AttrDraftAttachments = HeStart + 81, // BML
		AttrDraftScheduled = HeStart + 83 // bool, true sobald Draft gesendet werden soll
    };

    enum EnumDef_Status
    {
        Stat_Planned = 0,
        Stat_Tentative = 1,
        Stat_Canceled = 2
    };

    enum TypeDef_ScheduleItem // inherits Object
	{
		AttrStartDate = HeStart + 89, // QDate
		AttrEndDate = HeStart + 90,	// QDate, Null im Falle von Meilensteinen, Deadlines oder Appointments
		AttrTimeSlot = HeStart + 91, // TimeSlot, wenn EndDate leer, sonst Null
		AttrLocation = HeStart + 92, // QString, optional
        AttrStatus = HeStart + 105, // UInt8, EnumDef_Status, optional

		// Varianten:
		// Event: StartDate und EndDate nicht null, TimeSlot null, Location null
		// Appointment: StartDate nicht null, TimeSlot nicht null (mit start und duration), Location nicht null, EndDate null
		// Deadline: StartDate nicht null, TimeSlot nicht null (start ohne duration), Location null, EndDate null


        // Ein Kalendereintrag mit Auflösung Minuten. Geht nicht über Tagesdatum hinaus.
        TypeAppointment = HeStart + 93,

        // Ein Kalendereintrag ohne Dauer mit Auflösung Minuten. Geht nicht über Tagesdatum hinaus.
        TypeDeadline = HeStart + 94,

        // Generisches ScheduleItem. Kann verwendet werden für Feiertage, Ferien, etc.
		TypeEvent = HeStart + 95
	};

    // Ein Schedule stellt die ScheduleItems dar.
	// Ein Schedule enthält ein Map der Form ScheduleItem -> Thread, anhand dessen die Darstellungszuordnung von
	// ScheduleItems zu Schedules gegeben ist sowie die Zeile für dessen Darstellung
	enum TypeDef_Schedule // inherits Object
	{
		TypeSchedule = HeStart + 96,
        AttrRowCount = HeStart + 97,	// UInt8, Anzahl Zeilen im Schedule, minimal 1
        AttrSchedOwner = HeStart + 104, // OID|nil: Referenz auf EmailAddress; indiziert

        // Für Schedules Root
		AttrDefaultSchedule = HeStart + 102 // OID oder nil
	};

	// Eine geordnete Liste (mittels ListElements) von Schedules.
	// Diese Liste enthält die Zeilen, welche in ScheduleBoardDeleg dargestellt werden
	enum TypeDef_ScheduleSet // inherits Object
	{
		TypeScheduleSet = HeStart + 98
	};

    enum TypeDef_ScheduleSetElement
	{
		TypeSchedSetElem = HeStart + 99,
		AttrElementValue = HeStart + 100 // Wert des Listenelements
	};

    struct HeTypeDefs : public QObject // wegen tr
    {
    public:
        static const char* s_mimeAttachRefs;

        static void init(Udb::Database& db);
        static QByteArray calcHash( const QString& filePath );
        static QString prettyDate( const QDate& d, bool includeWeek = true, bool includeDay = false );
        static QString prettyDateTime( const QDateTime& t, bool includeWeek = false, bool includeDay = false );
        static QString prettyName( quint32 type );
        static QString prettyName( quint32 atom, Udb::Transaction* txn );
        static QString prettyStatus( quint8 );
        static QString formatObjectTitle( const Udb::Obj& );
        static QString formatObjectId( const Udb::Obj&, bool useOid = false );
        static QString formatValue( Udb::Atom, const Udb::Obj&, bool useIcons = true );
        static bool isParty( quint32 type );
        static bool isEmail( quint32 type );
        static bool isPrintableAscii( const QByteArray&, bool spaceAllowed = true, bool emptyAllowed = true );
        static QList<Udb::Obj> readObjectRefs(const QMimeData *data, Udb::Transaction* txn );
        static void writeObjectRefs(QMimeData *data, const QList<Udb::Obj>& );
        static QList<QPair<QString, QUrl> > readAttachRefs(const QMimeData *data );
        static void writeAttachRefs(QMimeData *data, const QList<Udb::Obj>& );
        static bool isValidAggregate( quint32 child, quint32 parent );
        static bool isScheduleItem( quint32 type );
        static bool isCalendarItem( quint32 type );
        static QString timeSlotToolTip( QDate d, const Stream::TimeSlot& );
		static QString dateRangeToolTip( const QDate& a, const QDate& b );
        static QString toolTip(const Udb::Obj&);
    };
}

#endif // HETYPEDEFS_H
