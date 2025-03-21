#ifndef ICSDOCUMENT_H
#define ICSDOCUMENT_H

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
#include <QPair>
#include <QVariant>
#include <QDateTime>

class QFile;
class QTextStream;

namespace He
{
    class IcsDocument : public QObject
    {
        Q_OBJECT
    public:
        enum TypeCode {
            UnknownType,
            BINARY,             // QByteArray
            BOOLEAN,            // bool
            CAL_ADDRESS,        // QUrl
            DATE,               // QDate
            DATE_TIME,          // QDateTime
            DUR,                // DURATION, qint64, Seconds
            FLOAT,              // double
            INTEGER,            // int
            PERIOD,             // struct Period
            RECUR,              // QByteArray, originalsyntax
            TEXT,               // QString
            TIME,               // QTime
            URI,                // QUrl
            UTC_OFFSET          // int, Seconds
        };
        static const char* s_typeNames[];

        enum ParamCode {
            UnknownParam,
            ALTREP,         // QUrl
            CN,             // QString
            CUTYPE,         // CUTYPE_Code
            DELEGATED_FROM, // QByteArray: RfC822 1#addr-spec
            DELEGATED_TO,   // QByteArray: RfC822 1#addr-spec
            DIR,            // QUrl
            ENCODING,       // ignoriert
            FMTTYPE,        // QByteArray
            FBTYPE,         // FBTYPE_Code
            LANGUAGE,       // QByteArray
            MEMBER,         // QByteArray: RfC822 1#addr-spec
            PARTSTAT,       // PARTSTAT_Code
            RANGE,          // bool
            RELATED,        // bool
            RELTYPE,        // RELTYPE_Code
            ROLE,           // ROLE_Code
            RSVP,           // bool
            SENT_BY,        // QByteArray: RfC822 addr-spec
            TZID,           // QString
            VALUE           // Wird direkt angewendet und nicht gespeichert
        };
        static const char* s_paramNames[];

        enum CUTYPE_Code
        {
            INDIVIDUAL = 0, // Default
            GROUP,
            RESOURCE,
            ROOM
            // UNKNOWN etc. werden weggeworfen
        };

        enum FBTYPE_Code
        {
            BUSY = 0, // Default
            FREE,
            BUSY_UNAVAILABLE,
            BUSY_TENTATIVE
            // Rest weggeworfen
        };

        enum PARTSTAT_Code
        {
            NEEDS_ACTION = 0, // Default
            ACCEPTED,
            DECLINED,
            TENTATIVE,
            DELEGATED,
            COMPL       // COMPLETED
            // Rest weggeworfen
        };

        enum RELTYPE_Code
        {
            PARENT = 0, // Default
            CHILD,
            SIBLING
            // Rest weggeworfen
        };

        enum ROLE_Code
        {
            REQ_PARTICIPANT = 0, // Default
            OPT_PARTICIPANT,
            NON_PARTICIPANT,
            CHAIR
            // Rest weggeworfen
        };

        enum PropCode {
            UnknownProp,
            X_Property,
            ACTION,
            ATTACH,
            ATTENDEE,
            BEGIN,
            CALSCALE, // top, optional
            CATEGORIES,
            CLASS,
            COMMENT,
            COMPLETED,
            CONTACT,
            CREATED,
            DESCRIPTION,
            DTEND,
            DTSTAMP,
            DTSTART,
            DUE,
            DURATION,
            END,
            EXDATE,
            FREEBUSY,
            GEO,
            LAST_MODIFIED,
            LOCATION,
            METHOD, // top, optional
            ORGANIZER,
            PERCENT_COMPLETE,
            PRIORITY,
            PRODID, // top, mandatory
            RDATE,
            RECURRENCE_ID,
            RELATED_TO,
            REPEAT,
            REQUEST_STATUS,
            RESOURCES,
            RRULE,
            SEQUENCE,
            STATUS,
            SUMMARY,
            TRANSP,
            TRIGGER,
            TZID_,
            TZNAME,
            TZOFFSETFROM,
            TZOFFSETTO,
            TZURL,
            UID,
            URL,
            VERSION // top, mandatory
        };
        static const char* s_propNames[];
        static const TypeCode s_propTypes[];

        // Bedeutung in RFC 5546
        enum MethodCode
        {
            UnknownMethod,
            ADD,    // 3.2.4 add one or more new instances to an existing "VEVENT"
            CANCEL, // 3.2.5 cancellation notice of an existing event request to the affected "Attendees"
            COUNTER,// 3.2.7 submit to the "Organizer" a counter proposal to the event
            DECLINECOUNTER, // 3.2.8 reject a counter proposal submitted by an "Attendee"
            PUBLISH, // 3.2.1 unsolicited posting of an iCalendar object
            REFRESH, // 3.2.6 request an updated description from the event "Organizer"
            REPLY,  // 3.2.3 accept or decline a "REQUEST", may give "COMMENT"
            REQUEST // 3.2.2 Invite, Reschedule, Update, Reconfirm, "Attendees" use "REPLY"
        };
        static const char* s_methodNames[];

        enum ComponentType {
            UnknownComp,
            DAYLIGHT,       // Teil von VTIMEZONE
            STANDARD,       // Teil von VTIMEZONE
            VALARM,         // Teil von VEVENT
            VEVENT,
            VFREEBUSY,
            VJOURNAL,
            VTIMEZONE,
            VTODO
        };

        struct Period
        {
            QDateTime d_start;
            QDateTime d_end;
        };

        struct Property
        {
            PropCode d_type;
            QByteArray d_xType;
            QVariant d_value;
            QList< QPair<ParamCode,QVariant> > d_params;
            Property():d_type(UnknownProp){}
            bool isValid() const { return d_type != UnknownProp; }
            QVariant findParam( ParamCode ) const;
        };

        struct Component
        {
            ComponentType d_type;
            QList<Property> d_props;
            QList<Component> d_subs;
            Component():d_type(UnknownComp){}
            const Property& findProp(PropCode) const;
        };

        explicit IcsDocument(QObject *parent = 0);
        bool loadFrom( const QString& path );
        bool loadFrom( QFile& );
        const QString& getError() const { return d_error; }
        const QList<Component>& getComps() const { return d_comps; }
        MethodCode getMethod() const { return d_method; }
        void writeHtml( QTextStream& out ) const;
        void writeHtml( QTextStream& out, const Component&, bool withTitle = true ) const;
    protected:
        QList<Component> d_comps;
        struct ParsedLine
        {
            QByteArray d_name; // Property Name, Error falls null
            QList< QPair<QByteArray,QList<QByteArray> > > d_params; // Name->ValueList
            QByteArray d_value; // Property Value
        };
        QString d_error;
        int d_line;
        MethodCode d_method;

        static QString parseText( const QByteArray& data );
        static int parseUtcOffset( const QByteArray& data, bool &ok ); // gibt Sekunden zurück
        static Period parsePeriod( const QByteArray& data, bool &ok );
        static qint64 parseDuration( const QByteArray& data, bool& ); // gibt Sekunden zurück
        static QDateTime parseDateTime( const QByteArray& data );
        QVariant readDataValue( TypeCode, const QByteArray& data );
        static ParamCode findParam( const QByteArray& );
        static PropCode findProp( const QByteArray& );
        static TypeCode findType( const QByteArray& );
        static MethodCode findMethod( const QByteArray& );
        ParsedLine parseLine( const QByteArray& );
        QByteArray readUnfoldedLine( QFile & );
        Component readComponent( QFile& in, const QByteArray& type );
        Property readProperty( const ParsedLine& );
        void writeHtmlEvent( const Component&, QTextStream& out, bool withTitle ) const;
        void writeHtmlCalAddr( const Property&, QTextStream& out ) const;
        static QString toString( const QVariant&, int secsOff = 0 );
    };
}
Q_DECLARE_METATYPE( He::IcsDocument::Period )

#endif // ICSDOCUMENT_H
