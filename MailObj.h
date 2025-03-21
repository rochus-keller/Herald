#ifndef MAILOBJ_H
#define MAILOBJ_H

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

#include <Udb/Obj.h>

class MailMessage;

namespace He
{
    class AttachmentObj : public Udb::Obj
    {
    public:
        AttachmentObj(const Udb::Obj& o ):Obj( o ) {}

        QString getDocumentPath() const;
        static QString getFilePath( const Udb::Obj& document );
        bool isInline() const;
        QByteArray getContentId(bool withBrackets = true) const;
        void setContentId( QByteArray );
    };

    class MailObj : public Udb::Obj
    {
    public:
        struct MailAddr
        {
            QString prettyNameEmail(bool rfc822 = false) const;
            QString htmlLink(bool oid = false) const;
            QString href(bool oid = false) const;
            QByteArray d_addr;
            QString d_name;
            Udb::Obj d_party;
        };
        typedef QList<MailAddr> MailAddrList;
        typedef QList<AttachmentObj> Attachments;

        MailObj(const Udb::Obj& o ):Obj( o ) {}
        MailObj() {}

        MailAddr getFrom( bool nameNotEmpty = true ) const;
        MailAddrList getTo( bool nameNotEmpty = true ) const;
        MailAddrList getCc( bool nameNotEmpty = true ) const;
        MailAddrList getBcc( bool nameNotEmpty = true ) const;
        MailAddrList getResentTo( bool nameNotEmpty = true ) const;
        MailAddr getReplyTo( bool nameNotEmpty = true ) const;
        Attachments getAttachments() const;
        QDateTime getLocalSentOn() const;
        Udb::Obj findIdentity() const;

        Udb::Obj createParty( const QByteArray& addr, const QString& name, quint32 type );
        Udb::Obj createAttachment( const QString& filePath, const QString& name, bool acquire, bool toDispose );
        Udb::Obj findAttachment( const QByteArray& contentId );

		bool accept( MailMessage* msg );

        static Udb::Obj acceptInbound(Udb::Transaction* txn, const QString& path );
        static Udb::Obj acceptInbound(Udb::Transaction* txn, const QByteArray& stream );
		static Udb::Obj acceptInOrOutbound(Udb::Transaction* txn, MailMessage* msg, bool inbound );
        static Udb::Obj acceptOutbound(const Udb::Obj& draft, MailMessage* msg );
        static bool acceptDraftParty( const Udb::Obj& draftParty, MailMessage* msg );
        static Udb::Obj getOrCreateEmailAddress( Udb::Transaction*,const QByteArray& addr, const QString& name );
        static Udb::Obj getEmailAddress( Udb::Transaction*, const QByteArray& addr );
        static MailAddr getPartyAddr( const Udb::Obj& party, bool nameNotEmpty );
        static Udb::Obj createParty( Udb::Obj& mail, const QByteArray& addr, const QString& name, quint32 type );
        static Udb::Obj getOrCreateDocument( Udb::Transaction*, const QString& filePath,
                                             const QString& name, bool acquire, bool toDispose );
        static Udb::Obj createAttachment( Udb::Obj& mail, const QString& filePath,
                                             const QString& name, bool acquire, bool toDispose );
		static Udb::Obj getOrCreateIdentity( const Udb::Obj& addr, const QString& name, bool create = true );
        static QString formatAddress( const Udb::Obj& addrOrParty, bool rfc822 ); // Address oder Party
        static void adjustInReplyTo(Udb::Transaction* txn);
        static QString correctWordMailHtml( const QString& );
        static QString removeAllMetaTags( QString );
        static QString plainText2Html( QString );
        static Udb::Obj simpleAddressEntry( QWidget*, Udb::Transaction* txn, const Udb::Obj& = Udb::Obj() );
		static QString findCertificate( Udb::Transaction*, const QByteArray& addr );
		static QString findPrivateKey( Udb::Transaction*, MailMessage* msg );
    };

}

#endif // MAILOBJ_H
