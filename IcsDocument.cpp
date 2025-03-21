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

#include "IcsDocument.h"
#include <QFile>
#include <QtDebug>
#include <QtAlgorithms>
#include <QUrl>
#include <QDateTime>
#include <QTextDocument> // wegen Qt::escape
using namespace He;

// Protokoll nach RFC 5545 2009
const char* IcsDocument::s_typeNames[] =
{
    "Unknown",
    "BINARY",
    "BOOLEAN",
    "CAL-ADDRESS",
    "DATE",
    "DATE-TIME",
    "DURATION",
    "FLOAT",
    "INTEGER",
    "PERIOD",
    "RECUR",
    "TEXT",
    "TIME",
    "URI",
    "UTC-OFFSET"
};

const char* IcsDocument::s_paramNames[] =
{
    "Unknown",
    "ALTREP", // 3.2.1.  Alternate Text Representation
    "CN", // 3.2.2.  Common Name
    "CUTYPE", // 3.2.3.  Calendar User Type, INDIVIDUAL, GROUP, RESOURCE, ROOM, etc.
    "DELEGATED-FROM", // 3.2.4.  Delegators
    "DELEGATED-TO", // 3.2.5.  Delegatees
    "DIR", // 3.2.6.  Directory Entry Reference
    "ENCODING", // 3.2.7.  Inline Encoding, 8BIT, BASE64
    "FMTTYPE", // 3.2.8.  Format Type
    "FBTYPE", // 3.2.9.  Free/Busy Time Type, FREE, BUSY, BUSY-UNAVAILABLE, BUSY-TENTATIVE
    "LANGUAGE", // 3.2.10.  Language
    "MEMBER", // 3.2.11.  Group or List Membership
    "PARTSTAT", // 3.2.12.  Participation Status, NEEDS-ACTION, ACCEPTED, DECLINED, TENTATIVE, DELEGATED, etc.
    "RANGE", // 3.2.13.  Recurrence Identifier Range
    "RELATED", // 3.2.14.  Alarm Trigger Relationship, START, END
    "RELTYPE", //3.2.15.  Relationship Type, PARENT, CHILD, SIBLING, etc.
    "ROLE", // 3.2.16.  Participation Role, CHAIR, REQ-PARTICIPANT, OPT-PARTICIPANT, NON-PARTICIPANT, etc.
    "RSVP", // 3.2.17.  RSVP Expectation, TRUE, FALSE
    "SENT-BY", // 3.2.18.  Sent By
    "TZID", // 3.2.19.  Time Zone Identifier
    "VALUE" // 3.2.20.  Value Data Types, BINARY, BOOLEAN, CAL-ADDRESS, DATE, DATE-TIME, DURATION, etc.
};

const char* IcsDocument::s_propNames[] =
{
    "Unknown",
    "X-",
    "ACTION",
    "ATTACH",
    "ATTENDEE",
    "BEGIN",
    "CALSCALE", // top, optional
    "CATEGORIES",
    "CLASS",
    "COMMENT",
    "COMPLETED",
    "CONTACT",
    "CREATED",
    "DESCRIPTION",
    "DTEND",
    "DTSTAMP",
    "DTSTART",
    "DUE",
    "DURATION",
    "END",
    "EXDATE",
    "FREEBUSY",
    "GEO",
    "LAST-MODIFIED",
    "LOCATION",
    "METHOD", // top, optional
    "ORGANIZER",
    "PERCENT-COMPLETE",
    "PRIORITY",
    "PRODID", // top, mandatory
    "RDATE",
    "RECURRENCE-ID",
    "RELATED-TO",
    "REPEAT",
    "REQUEST-STATUS",
    "RESOURCES",
    "RRULE",
    "SEQUENCE",
    "STATUS",
    "SUMMARY",
    "TRANSP",
    "TRIGGER",
    "TZID",
    "TZNAME",
    "TZOFFSETFROM",
    "TZOFFSETTO",
    "TZURL",
    "UID",
    "URL",
    "VERSION" // top, mandatory
};

const IcsDocument::TypeCode IcsDocument::s_propTypes[] =
{
    UnknownType,    // UnknownProp,
    TEXT,           // X-Property
    TEXT,           // ACTION,
    URI,            // oder BINARY, ATTACH,
    CAL_ADDRESS,    // ATTENDEE,
    UnknownType,    // BEGIN
    TEXT,           // CALSCALE, // top, optional
    TEXT,           // CATEGORIES,
    TEXT,           // CLASS,
    TEXT,           // COMMENT,
    DATE_TIME,      // COMPLETED,
    TEXT,           // CONTACT,
    DATE_TIME,      // CREATED,
    TEXT,           // DESCRIPTION,
    DATE_TIME,      // oder DATE, DTEND,
    DATE_TIME,      // DTSTAMP,
    DATE_TIME,      // oder DATE, DTSTART,
    DATE_TIME,      // oder DATE, DUE,
    DUR,            // DURATION,
    UnknownType,    // END
    DATE_TIME,      // oder DATE, EXDATE,
    PERIOD,         // FREEBUSY,
    FLOAT,          // GEO, two SEMICOLON-separated
    DATE_TIME,      // LAST_MODIFIED,
    TEXT,           // LOCATION,
    TEXT,           // METHOD, // top, optional
    CAL_ADDRESS,    // ORGANIZER,
    INTEGER,        // PERCENT_COMPLETE,
    INTEGER,        // PRIORITY,
    TEXT,           // PRODID, // top, mandatory
    DATE_TIME,      // oder DATE, RDATE,
    DATE_TIME,      // oder DATE, RECURRENCE_ID,
    TEXT,           // RELATED_TO,
    INTEGER,        // REPEAT,
    TEXT,           // REQUEST_STATUS,
    TEXT,           // RESOURCES,
    RECUR,          // RRULE,
    INTEGER,        // SEQUENCE,
    TEXT,           // STATUS,
    TEXT,           // SUMMARY,
    TEXT,           // TRANSP,
    DUR,            // oder DATE-TIME, TRIGGER,
    TEXT,           // TZID,
    TEXT,           // TZNAME,
    UTC_OFFSET,     // TZOFFSETFROM,
    UTC_OFFSET,     // TZOFFSETTO,
    URI,            // TZURL,
    TEXT,           // UID,
    URI,            // URL,
    TEXT            // VERSION // top, mandatory
};

const char* IcsDocument::s_methodNames[] =
{
    "Unknown",
    "ADD",
    "CANCEL",
    "COUNTER",
    "DECLINECOUNTER",
    "PUBLISH",
    "REFRESH",
    "REPLY",
    "REQUEST"
};

IcsDocument::IcsDocument(QObject *parent) :
    QObject(parent)
{
}

bool IcsDocument::loadFrom(const QString &path)
{
    d_comps.clear();
    d_error.clear();
    d_line = 0;

    QFile f( path );
    if( !f.open(QIODevice::ReadOnly ) )
    {
        d_error = tr("Cannot open file '%1' for reading").arg(path);
        return false;
    }
    return loadFrom( f );
}

bool IcsDocument::loadFrom(QFile &in)
{
    d_comps.clear();
    d_error.clear();
    d_method = UnknownMethod;
    d_line = 0;

    // Parser für http://tools.ietf.org/html/rfc5545
    QByteArray curLine = readUnfoldedLine( in );
    const ParsedLine beginVcal = parseLine( curLine );
    if( beginVcal.d_name != "BEGIN" || beginVcal.d_value != "VCALENDAR" )
    {
        d_error = tr("File does not start with BEGIN:VCALENDAR on line %1").arg(d_line);
        return false;
    }
    while( !in.atEnd() )
    {
        curLine = readUnfoldedLine( in );
        ParsedLine pl = parseLine( curLine );
        if( pl.d_name.isEmpty() )
            return false; // d_error bereits gesetzt
        else if( pl.d_name == "BEGIN" )
        {
            Component c = readComponent( in, pl.d_value.toUpper() );
            if( c.d_type == UnknownType )
                return false; // d_error bereits gesetzt
            else
                d_comps.append( c );
        }else if( pl.d_name == "END" )
        {
            if( pl.d_value == "VCALENDAR" )
            {
                if( in.atEnd() )
                    return true;
                else
                {
                    d_error = tr("END:VCALENDAR is not the end, line %1").arg(d_line);
                    return false;
                }
            }
        }else if( pl.d_name == "METHOD" )
        {
            d_method = findMethod(pl.d_value);
        }else
        {
            // Ignore Line
            qWarning() << "IcsDocument::loadFrom ignoring line:" << curLine;
        }
//        qDebug() << "******* Parsed:";
//        qDebug() << pl.d_name << "=" << pl.d_value;
//        for( int i = 0; i < pl.d_params.size(); i++ )
//            qDebug() << pl.d_params[i].first << "=" << pl.d_params[i].second;
    }
    return false;
}

QString IcsDocument::parseText(const QByteArray &data)
{
    QString res = QString::fromUtf8( data );
    res.replace( "\\n", "\n" );
    res.replace( "\\N", "\n" );
    res.replace( "\\,", "," );
    res.replace( "\\;", ";" );
    res.replace( "\\\\", "\\" );
    return res;
}

int IcsDocument::parseUtcOffset(const QByteArray &data, bool& ok)
{
    ok = true;
    if( ( data.size() != 5 && data.size() != 7 ) || ( data[0] != '+' && data[0] != '-' ) )
    {
        ok = false;
        return 0;
    }

    int hours = 0, mins = 0, secs = 0;
    const bool negative = data[0] == '-';
// TODO: was ist mit negative?
    hours = data.mid(1, 2).toUInt();
    mins = data.mid(3, 2 ).toUInt();
    if( data.size() == 7 )
        secs = data.mid( 5, 2 ).toUInt();
    return secs + 60 * mins + 60 * 60 * hours;
}

IcsDocument::Period IcsDocument::parsePeriod(const QByteArray &data, bool& ok)
{
    ok = true;
    const int slashPos = data.indexOf( '/' );
    if( slashPos == -1 )
    {
        ok = false;
        return Period();
    }
    Period res;
    res.d_start = parseDateTime( data.left( slashPos ) );
    if( !res.d_start.isValid() )
    {
        ok = false;
        return Period();
    }
    QByteArray right = data.mid( slashPos + 1 );
    if( right.isEmpty() || right[0] == '-' )
    {
        ok = false;
        return Period();
    }
    if( QChar( right[0] ).isDigit() )
    {
        res.d_end = parseDateTime( right );
        if( !res.d_end.isValid() )
        {
            ok = false;
            return Period();
        }
    }else
        res.d_end = res.d_start.addSecs( parseDuration( right, ok ) );
    return res;
}

qint64 IcsDocument::parseDuration(const QByteArray &data, bool& ok )
{
    bool negative = false;
    enum State { Sign, Number };
    State s = Sign;
    int days = 0, weeks = 0, hours = 0, mins = 0, secs = 0;
    QByteArray number;
    for( int i = 0; i < data.size(); i++ )
    {
        char c = data[i];
        switch( s )
        {
        case Sign:
            if( c == '-' )
                negative = true;
            else if( c == '+' )
                negative = false;
            else if( c == 'P' )
            {
                number.clear();
                s = Number;
            }else
            {
                ok = false;
                return 0;
            }
            break;
        case Number:
            if( QChar( c ).isDigit() )
                number += c;
            else if( c == 'W' || c == 'H' || c == 'M' || c == 'S' || c == 'D' )
            {
                switch( c )
                {
                case 'W':
                    weeks = number.toUInt();
                    break;
                case 'H':
                    hours = number.toUInt();
                    break;
                case 'M':
                    mins = number.toUInt();
                    break;
                case 'S':
                    secs = number.toUInt();
                    break;
                case 'D':
                    days = number.toUInt();
                    break;
                default:
                    ok = false;
                    return 0;
                }
                number.clear();
            }else if( c == 'T' )
                ; // ignore
            else if( QChar(c).isSpace() )
                ; // ignore
            else
            {
                ok = false;
                return 0;
            }
            break;
        }
    }
    qint64 res = secs + 60 * mins + 60 * 60 * hours + 60 * 60 * 24 * days + 60 * 60 * 24 * 7 * weeks;
    if( negative )
        res *= -1;
    ok = true;
    return res;
}

QDateTime IcsDocument::parseDateTime(const QByteArray &data)
{
    QDateTime dt = QDateTime::fromString( data.left( 15 ), "yyyyMMddThhmmss" );
    if( !dt.isValid() )
        return dt;
    if( data.size() == 15 && data.endsWith( 'Z' ) )
        dt = QDateTime( dt.date(), dt.time(), Qt::UTC );
    return dt;
}

QVariant IcsDocument::readDataValue(IcsDocument::TypeCode type, const QByteArray &data)
{
    bool ok;
    switch( type )
    {
    case BINARY:
        return QByteArray::fromBase64( data );
    case BOOLEAN:
        if( data.toUpper() == "TRUE")
            return true;
        else
            return false; // RISK: war wirklich FALSE?
    case CAL_ADDRESS:
        {
            QUrl url = QUrl::fromEncoded( data, QUrl::TolerantMode );
            if( !url.isValid() )
            {
                d_error = tr("Invalid CAL_ADDRESS '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return url;
        }
    case DATE:
        {
            QDate d = QDate::fromString( data, "yyyyMMdd");
            if( !d.isValid() )
            {
                d_error = tr("Invalid DATE '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return d;
        }
    case TIME:
        {
            QTime t = QTime::fromString( data.left(6), "hhmmss"); // ignoriere "Z"
            if( !t.isValid() )
            {
                d_error = tr("Invalid TIME '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return t;
        }
    case DATE_TIME:
        {
            QDateTime dt = parseDateTime( data );
            if( !dt.isValid() )
            {
                d_error = tr("Invalid DATE_TIME '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return dt;
        }
    case DUR:
        {
            qint64 d = parseDuration( data, ok );
            if( !ok )
            {
                d_error = tr("Invalid DURATION '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return d;
        }
    case FLOAT:
        {
            double d = data.toDouble(&ok);
            if( !ok )
            {
                d_error = tr("Invalid FLOAT '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return d;
        }
    case INTEGER:
        {
            int i = data.toInt(&ok);
            if( !ok )
            {
                d_error = tr("Invalid INTEGER '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return i;
        }
    case PERIOD:
        {
            Period p = parsePeriod( data, ok );
            if( !ok )
            {
                d_error = tr("Invalid PERIOD '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return QVariant::fromValue( p );
        }
        // v.canConvert<Oln::OutlineMdl::Html>(), v.value<Oln::OutlineMdl::Html>()
    case URI:
        {
            QUrl url = QUrl::fromEncoded( data, QUrl::TolerantMode );
            if( !url.isValid() )
            {
                d_error = tr("Invalid URL '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return url;
        }
    case UTC_OFFSET:
        {
            int o = parseUtcOffset( data, ok );
            if( !ok )
            {
                d_error = tr("Invalid UTC_OFFSET '%1' on line %2").arg(data.data()).arg(d_line);
                return QVariant();
            }
            return o;
        }
    case RECUR:
        return data; // TODO
    case TEXT:
        return parseText( data );
    }
    return QVariant();
}

static inline bool _lessThan( const char* lhs, const char* rhs )
{
    return qstrcmp( lhs, rhs ) < 0;
}

IcsDocument::ParamCode IcsDocument::findParam(const QByteArray & str)
{
    const char** name = qBinaryFind( &s_paramNames[1], &s_paramNames[1] + VALUE, str, _lessThan );
    const int index = name - &s_paramNames[1] + 1;
    if( index >= ALTREP && index <= VALUE )
        return ParamCode(index);
    else
        return UnknownParam;
}

IcsDocument::PropCode IcsDocument::findProp(const QByteArray & str)
{
    if( str.startsWith( "X-" ) )
        return X_Property;
    const char** name = qBinaryFind( &s_propNames[1], &s_propNames[1] + VERSION, str, _lessThan );
    const int index = name - &s_propNames[1] + 1;
    if( index >= ACTION && index <= VERSION )
        return PropCode(index);
    else
        return UnknownProp;
}

IcsDocument::TypeCode IcsDocument::findType(const QByteArray & str)
{
    const char** name = qBinaryFind( &s_typeNames[1], &s_typeNames[1] + UTC_OFFSET, str, _lessThan );
    const int index = name - &s_typeNames[1] + 1;
    if( index >= BINARY && index <= UTC_OFFSET )
        return TypeCode(index);
    else
        return UnknownType;
}

IcsDocument::MethodCode IcsDocument::findMethod(const QByteArray & str)
{
    const char** name = qBinaryFind( &s_methodNames[1], &s_methodNames[1] + REQUEST, str, _lessThan );
    const int index = name - &s_methodNames[1] + 1;
    if( index >= ADD && index <= REQUEST )
        return MethodCode(index);
    else
        return UnknownMethod;
}

IcsDocument::ParsedLine IcsDocument::parseLine(const QByteArray & line)
{
    ParsedLine res;
    enum State { Name, ParamName, ParamValue, ParamQuoteValue, Value };
    State state = Name;
    QByteArray name;
    QList<QByteArray> values;
    QByteArray value;
    foreach( char ch, line )
    {
        switch( state )
        {
        case Name:
            if( ch == ';' )
            {
                res.d_name = name.toUpper();
                if( name.isEmpty() )
                {
                    d_error = tr("Empty name on line %1").arg(d_line);
                    return ParsedLine();
                }
                name.clear();
                state = ParamName;
            }else if( ch == ':' )
            {
                res.d_name = name.toUpper();
                if( name.isEmpty() )
                {
                    d_error = tr("Empty name on line %1").arg(d_line);
                    return ParsedLine();
                }
                name.clear();
                state = Value;
            }else
                name += ch;
            break;
        case ParamName:
            if( ch == '=' )
            {
                values.clear();
                state = ParamValue;
            }else
                name += ch;
            break;
        case ParamValue:
            if( ch == ',' )
            {
                if( value.isEmpty() )
                {
                    d_error = tr("Empty value on line %1").arg(d_line);
                    return ParsedLine();
                }
                else
                    values.append( value );
                value.clear();
            }else if( ch == ';' || ch == ':' )
            {
                if( value.isEmpty() )
                {
                    d_error = tr("Empty value on line %1").arg(d_line);
                    return ParsedLine();
                }
                else
                    values.append( value );
                value.clear();
                if( values.isEmpty() )
                {
                    d_error = tr("Empty param on line %1").arg(d_line);
                    return ParsedLine();
                }
                if( name.isEmpty() )
                {
                    d_error = tr("Empty name on line %1").arg(d_line);
                    return ParsedLine();
                }
                res.d_params.append( qMakePair( name.toUpper(), values ) );
                values.clear();
                name.clear();
                if( ch == ';' )
                    state = ParamName;
                else
                    state = Value;
            }else if( ch == '"' )
            {
                if( value.isEmpty() )
                    state = ParamQuoteValue;
                else
                {
                    d_error = tr("DQUOTE in paramtext on line %1").arg(d_line);
                    return ParsedLine();
                }
            }else
            {
                value += ch;
            }
            break;
        case ParamQuoteValue:
            if( ch == '"' )
            {
                state = ParamValue;
            }else
                value += ch;
            break;
        case Value:
            value += ch;
        }
    }
    if( state != Value )
    {
        d_error = tr("Value missing on line %1").arg(d_line);
        return ParsedLine();
    }else
    {
        res.d_value = value;
//        Property p = findProp(res.d_name);
//        qDebug() << s_propNames[p] << "=" << digestData( s_propTypes[p], value );
        return res;
    }
}

static void _chopCrLf( QByteArray& ba )
{
    if( ba.endsWith( "\r\n") || ba.endsWith( "\n\r" ) )
        ba.chop( 2 );
    else if( ba.endsWith( "\n") || ba.endsWith( "\r") )
        ba.chop( 1 );
}

QByteArray IcsDocument::readUnfoldedLine(QFile & in)
{
    enum State { Idle, ReadLine };
    State state = Idle;
    QByteArray curLine;
    while( true )
    {
        switch( state )
        {
        case Idle:
            curLine = in.readLine();
            d_line++;
            //
            _chopCrLf( curLine ); // CR/LF entfernen
            state = ReadLine;
            break;
        case ReadLine:
            if( !in.atEnd() )
            {
                char ch;
                if( in.peek( &ch, 1) == 1 )
                {
                    if( QChar( ch ).isSpace() )
                    {
                        // unfolding
                        QByteArray newLine = in.readLine();
                        d_line++;
                        _chopCrLf( newLine ); // CR/LF entfernen;
                        curLine += newLine.mid(1); // das erste Leerzeichen wegwerfen
                    }else
                        return curLine;
                }else
                    Q_ASSERT( false ); // Widerspruch mit !in.atEnd()
            }else
                return curLine;
            break;
        }
    }
    return QByteArray();
}

IcsDocument::Component IcsDocument::readComponent(QFile &in, const QByteArray &type)
{
    Component c;
    if( type == "VEVENT" )
        c.d_type = VEVENT;
    else if( type == "VTODO" )
        c.d_type = VTODO;
    else if( type == "VJOURNAL" )
        c.d_type = VJOURNAL;
    else if( type == "VFREEBUSY" )
        c.d_type = VFREEBUSY;
    else if( type == "VTIMEZONE" )
        c.d_type = VTIMEZONE;
    else if( type == "VALARM" )
        c.d_type = VALARM;
    else if( type == "STANDARD" )
        c.d_type = STANDARD;
    else if( type == "DAYLIGHT" )
        c.d_type = DAYLIGHT;
    else
    {
        d_error = tr("Unknown component '%1' on line %2").arg(type.data()).arg(d_line);
        return Component();
    }
    while( !in.atEnd() )
    {
        QByteArray curLine = readUnfoldedLine( in );
        ParsedLine pl = parseLine( curLine );
        if( pl.d_name.isEmpty() )
            return Component(); // d_error bereits gesetzt
        else if( pl.d_name == "END" )
        {
            if( pl.d_value.toUpper() == type )
                return c;
            else
            {
                d_error = tr("Component not properly closed on line %1").arg(d_line);
                return Component();
            }
        }else if( pl.d_name == "BEGIN" )
        {
            Component subc = readComponent( in, pl.d_value.toUpper() );
            if( subc.d_type == UnknownType )
                return Component(); // d_error bereits gesetzt
            c.d_subs.append( subc );
        }else
        {
            Property p = readProperty( pl );
            if( p.d_type != UnknownProp )
                c.d_props.append( p );
            else
                return Component(); // d_error bereits gesetzt
        }
    }
    return Component();
}

IcsDocument::Property IcsDocument::readProperty(const IcsDocument::ParsedLine & pl)
{
    Property res;
    res.d_type = findProp( pl.d_name );
    if( res.d_type == UnknownProp )
    {

        d_error = tr("Unknown property '%1' on line %2").arg(pl.d_name.data()).arg(d_line);
        return Property();
    }else if( res.d_type == X_Property )
        res.d_xType = pl.d_name;

    TypeCode t = s_propTypes[res.d_type];

    for( int i = 0; i < pl.d_params.size(); i++ )
    {
        const ParamCode p = findParam( pl.d_params[i].first );
        QByteArray data = pl.d_params[i].second.first();
        QVariant v;
        switch( p )
        {
        case ALTREP: // für DESCRIPTION etc.
            {
                QUrl url = QUrl::fromEncoded( data, QUrl::TolerantMode );
                if( !url.isValid() )
                {
                    d_error = tr("Invalid ALTREP '%1' on line %2").arg(data.data()).arg(d_line);
                    return Property();
                }
                v = url;
            }
            break;
        case CN: // für CAL-ADDRESS
            v = QString::fromUtf8( data );
            break;
        case CUTYPE: // für CAL-ADDRESS
            if( data == "INDIVIDUAL" )
                v = INDIVIDUAL;
            else if( data == "GROUP" )
                v = GROUP;
            else if( data == "RESOURCE" )
                v = RESOURCE;
            else if( data == "ROOM" )
                v = ROOM;
            break;
        case DELEGATED_FROM: // für CAL-ADDRESS
        case DELEGATED_TO:
        case MEMBER:
        case SENT_BY:
            data.clear();
            foreach( QByteArray addr, pl.d_params[i].second )
            {
                QUrl url = QUrl::fromEncoded( addr, QUrl::TolerantMode );
                if( !url.isValid() )
                {
                    d_error = tr("Invalid SENT BY '%1' on line %2").arg(addr.data()).arg(d_line);
                    return Property();
                }
                if( url.scheme().toLower() == "mailto" )
                {
                    if( !data.isEmpty() )
                        data += ", ";
                    data += url.path().toLatin1();
                }else
                {
                    d_error = tr("Expecting SENT BY 'mailto' instead of '%1' on line %2").arg(url.scheme()).arg(d_line);
                    return Property();
                }
            }
            if( !data.isEmpty() )
                v = data;
            break;
        case DIR: // für CAL-ADDRESS
            {
                QUrl url = QUrl::fromEncoded( data, QUrl::TolerantMode );
                if( !url.isValid() )
                {
                    d_error = tr("Invalid DIR '%1' on line %2").arg(data.data()).arg(d_line);
                    return Property();
                }
                v = url;
            }
            break;
        case ENCODING: // für BINARY etc.
            // ignorieren; unsinnig, da nur mit BINARY relevant und dort obligatorisch
            break;
        case FMTTYPE: // für ATTACH etc.
            v = data;
            break;
        case FBTYPE: // für FREEBUSY etc.
            if( data == "BUSY" )
                v = BUSY;
            else if( data == "FREE" )
                v = FREE;
            else if( data == "BUSY-UNAVAILABLE" )
                v = BUSY_UNAVAILABLE;
            else if( data == "BUSY-TENTATIVE" )
                v = BUSY_TENTATIVE;
            else
            {
                d_error = tr("Invalid FBTYPE '%1' on line %2").arg(data.data()).arg(d_line);
                return Property();
            }
            break;
        case LANGUAGE: // für LOCATION, SUMMARY, etc.
            v = data;
            break;
        case PARTSTAT: // für CAL-ADDRESS
            if( data == "NEEDS-ACTION" )
                v = NEEDS_ACTION;
            else if( data == "ACCEPTED" )
                v = ACCEPTED;
            else if( data == "DECLINED" )
                v = DECLINED;
            else if( data == "TENTATIVE" )
                v = TENTATIVE;
            else if( data == "DELEGATED" )
                v = DELEGATED;
            else if( data == "COMPLETED" )
                v  = COMPL;
            else
            {
                d_error = tr("Invalid PARTSTAT '%1' on line %2").arg(data.data()).arg(d_line);
                return Property();
            }
            break;
        case RANGE: // für RECURRENCE-ID
            v = true; // THISANDFUTURE=true, single instance=false (default)
            break;
        case RELATED: // für TRIGGER
            if( data == "END" )
                v = true;   // START(Default)=false
            break;
        case RELTYPE: // für RELATED-TO etc.
            if( data == "PARENT" )
                v = PARENT;
            else if( data == "CHILD" )
                v = CHILD;
            else if( data == "SIBLING" )
                v = SIBLING;
            else
            {
                d_error = tr("Invalid RELTYPE '%1' on line %2").arg(data.data()).arg(d_line);
                return Property();
            }
            break;
        case ROLE: // für CAL-ADDRESS
            if( data == "REQ-PARTICIPANT" )
                v = REQ_PARTICIPANT;
            else if( data == "OPT-PARTICIPANT" )
                v = OPT_PARTICIPANT;
            else if( data == "NON-PARTICIPANT" )
                v = NON_PARTICIPANT;
            else if( data == "CHAIR" )
                v = CHAIR;
            else
            {
                d_error = tr("Invalid ROLE '%1' on line %2").arg(data.data()).arg(d_line);
                return Property();
            }
            break;
        case RSVP: // für CAL-ADDRESS
            if( data == "TRUE" )
                v = true;
            break;
        case TZID: // für DTSTART, DTEND, DUE, EXDATE, RDATE
            v = QString::fromUtf8( data );
            break;
        case VALUE:
            {
                TypeCode tt = findType( data );
                if( tt != UnknownType )
                    t = tt;
                else
                {
                    d_error = tr("Invalid value type '%1' on line %2").arg(data.data()).arg(d_line);
                    return Property();
                }
            }
            break;
        default:
            d_error = tr("Unknown parameter '%1' on line %2").arg(pl.d_params[i].first.data()).arg(d_line);
            return Property();
        }
        if( v.isValid() )
            res.d_params.append( qMakePair( p, v ) );
    }

    res.d_value = readDataValue( t, pl.d_value );
    if( res.d_value.isNull() )
        return Property(); // d_error bereits gesetzt
    if( res.d_type == METHOD )
        res.d_value = findMethod( res.d_value.toByteArray() );
    return res;
}

void IcsDocument::writeHtml(QTextStream &out) const
{
    out << "<html>" << endl;
    out << "<body>";
    if( !d_error.isEmpty() )
        out << "<p><b>ERROR: " << d_error << "</b></p>";

    for( int i = 0; i < d_comps.size(); i++ )
    {
        writeHtml( out, d_comps[i] );
    }
    out << "</body></html>";
}

void IcsDocument::writeHtml(QTextStream &out, const IcsDocument::Component & c, bool withTitle) const
{
    switch( c.d_type )
    {
    case VEVENT:
        writeHtmlEvent( c, out, withTitle );
        break;
    case VFREEBUSY:
        break;
    case VTIMEZONE:
        break;
    case VTODO:
        break;
    case VJOURNAL:
        break;
    }
}

void IcsDocument::writeHtmlEvent(const Component & c, QTextStream &out, bool withTitle ) const
{
    const Property& subject = c.findProp( SUMMARY );
    if( withTitle )
        out << "<h2>Event: " << subject.d_value.toString().toHtmlEscaped() << "</h2>" << endl;
    if( d_method != UnknownMethod )
        out << "<p><b>Method:</b>&nbsp;" << s_methodNames[d_method] << "</p>" << endl;
    else
    {
        const Property& meth = c.findProp( METHOD );
        MethodCode code = MethodCode( meth.d_value.toInt() );
        if( code != UnknownMethod )
            out << "<p><b>Method:</b>&nbsp;" << s_methodNames[code] << "</p>" << endl;
    }
    const Property& start = c.findProp( DTSTART );
    QVariant tzid = start.findParam( TZID );
    if( !start.d_value.isNull() )
        out << "<p><b>Start:</b>&nbsp;" << toString( start.d_value );
    if( tzid.isValid() )
        out << "&nbsp;(" << tzid.toString() << ")";
    out << "</p>" << endl;
    const Property& end = c.findProp( DTEND );
    const Property& dur = c.findProp( DURATION );
    if( end.isValid() )
    {
        tzid = end.findParam( TZID );
        if( !end.d_value.isNull() )
            out << "<p><b>Finish:</b>&nbsp;" << toString( end.d_value );
        if( tzid.isValid() )
            out << "&nbsp;(" << tzid.toString() << ")";
        out << "</p>" << endl;
    }else if( dur.isValid() )
    {
        if( !start.d_value.isNull() )
            out << "<p><b>Finish:</b>&nbsp;" << toString( start.d_value, dur.d_value.toInt() ) << "</p>" << endl;
    }
    const Property& loc = c.findProp( LOCATION );
    if( loc.isValid() )
        out << "<p><b>Where:</b>&nbsp;" << loc.d_value.toString().toHtmlEscaped() << "</p>" << endl;
    const Property& org = c.findProp( ORGANIZER );
    if( org.isValid() )
    {
        out << "<p><b>Organizer:&nbsp;</b>";
        writeHtmlCalAddr( org, out );
        out << "</p>" << endl;
    }
    const Property& desc = c.findProp( DESCRIPTION );
    if( desc.isValid() )
    {
        QString str = desc.d_value.toString().trimmed().toHtmlEscaped();
        str.replace( QLatin1String("\r\n"), QLatin1String("<br>") );
        str.replace( QLatin1String("\n"), QLatin1String("<br>") );
        out << "<p>" << str << "</p>" << endl;
    }
    bool titleSent = false;
    for( int i = 0; i < c.d_props.size(); i++ )
    {
        if( c.d_props[i].d_type == ATTENDEE )
        {
            if( !titleSent )
            {
                out << "<h3>Attendees:</h3>" << endl << "<ul>" << endl;
                titleSent = true;
            }
            out << "<li>";
            writeHtmlCalAddr( c.d_props[i], out );
            out << "</li>" << endl;
        }
    }
    if( titleSent )
        out << "</ul>" << endl;
    titleSent = false;
    for( int i = 0; i < c.d_props.size(); i++ )
    {
        if( c.d_props[i].d_type == ATTACH )
        {
            if( !titleSent )
            {
                out << "<h3>Attachments:</h3>" << endl << "<ul>" << endl;
                titleSent = true;
            }
            out << "<li>";
            if( c.d_props[i].d_value.type() == QVariant::Url )
            {
                out << "<a href=\"" << c.d_props[i].d_value.toUrl().toEncoded() << "\">" <<
                    c.d_props[i].d_value.toUrl().toString() << "</a>";
            }else
            {
                out << "Binary Attachment ";
                QVariant type = c.d_props[i].findParam(FMTTYPE);
                if( type.isValid() )
                    out << type.toString();
            }
            out << "</li>" << endl;
        }
    }
    if( titleSent )
        out << "</ul>" << endl;
}

void IcsDocument::writeHtmlCalAddr(const IcsDocument::Property & p, QTextStream &out) const
{
    QUrl url = p.d_value.toUrl();
    QVariant cn = p.findParam( CN );
    QVariant role = p.findParam( ROLE );
    //QVariant rsvp = p.findParam( RSVP );
    out << "<a href=\"" << url.toEncoded() << "\">";
    if( cn.isValid() )
        out << cn.toString().toHtmlEscaped();
    else
        out << url.path().toHtmlEscaped();
    out << "</a>";
    if( role.isValid() )
    {
        switch( role.toInt() )
        {
        case REQ_PARTICIPANT:
            out << "&nbsp;(required)";
            break;
        case OPT_PARTICIPANT:
            out << "&nbsp;(optional)";
            break;
        case NON_PARTICIPANT:
            out << "&nbsp;(not participating)";
            break;
        case CHAIR:
            out << "&nbsp;(chairman)";
            break;
        }
    }
}

QString IcsDocument::toString(const QVariant & v, int secsOff)
{
    if( v.type() == QVariant::DateTime )
    {
        QDateTime dt = v.toDateTime().toLocalTime();
        if( secsOff != 0 )
            dt = dt.addSecs( secsOff );
        return QString( "W%1, " ).arg( dt.date().weekNumber(), 2, 10, QChar( '0' ) ) +
                dt.toString( "ddd, yyyy-MM-dd hh:mm" );
    }else if( v.type() == QVariant::Date )
    {
        QDate d = v.toDate();
        if( secsOff != 0 )
            d = v.toDateTime().addSecs( secsOff ).date();
        return QString( "W%1, " ).arg( d.weekNumber(), 2, 10, QChar( '0' ) ) +
                d.toString( "ddd, yyyy-MM-dd" );
    }else
        return v.toString(); // RISK
}

const IcsDocument::Property &IcsDocument::Component::findProp(IcsDocument::PropCode c) const
{
    static Property s_dummy;
    for( int i = 0; i < d_props.size(); i++ )
    {
        if( d_props[i].d_type == c )
            return d_props[i];
    }
    return s_dummy;
}

QVariant IcsDocument::Property::findParam(IcsDocument::ParamCode c) const
{
    for( int i = 0; i < d_params.size(); i++ )
    {
        if( d_params[i].first == c )
            return d_params[i].second;
    }
    return QVariant();
}
