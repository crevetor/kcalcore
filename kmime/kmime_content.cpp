/*
    kmime_content.cpp

    KMime, the KDE internet mail/usenet news message library.
    Copyright (c) 2001 the KMime authors.
    See file AUTHORS for details
    Copyright (c) 2006 Volker Krause <vkrause@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/
/**
  @file
  This file is part of the API for handling @ref MIME data and
  defines the Content class.

  @brief
  Defines the Content class.

  @authors the KMime authors (see AUTHORS file),
  Volker Krause \<vkrause@kde.org\>
*/

#include "kmime_content.h"
#include "kmime_content_p.h"
#include "kmime_parsers.h"

#include <kcharsets.h>
#include <kcodecs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kdebug.h>

#include <QtCore/QTextCodec>
#include <QtCore/QTextStream>
#include <QtCore/QByteArray>

using namespace KMime;

namespace KMime {

Content::Content()
  : d_ptr( new ContentPrivate( this ) )
{
}

Content::Content( const QByteArray &h, const QByteArray &b )
  : d_ptr( new ContentPrivate( this ) )
{
  d_ptr->head = h;
  d_ptr->body = b;
}

Content::Content( ContentPrivate *d ) :
    d_ptr( d )
{
}

Content::~Content()
{
  qDeleteAll( h_eaders );
  h_eaders.clear();
  delete d_ptr;
}

bool Content::hasContent() const
{
  return !d_ptr->head.isEmpty() || !d_ptr->body.isEmpty() || !d_ptr->contents.isEmpty();
}

void Content::setContent( const QList<QByteArray> &l )
{
  Q_D(Content);
  //qDebug("Content::setContent( const QList<QByteArray> &l ) : start");
  d->head.clear();
  d->body.clear();

  //usage of textstreams is much faster than simply appending the strings
  QTextStream hts( &( d->head ), QIODevice::WriteOnly );
  QTextStream bts( &( d->body ), QIODevice::WriteOnly );
  hts.setCodec( "ISO 8859-1" );
  bts.setCodec( "ISO 8859-1" );

  bool isHead = true;
  foreach ( QByteArray line, l ) {
    if ( isHead && line.isEmpty() ) {
      isHead = false;
      continue;
    }
    if ( isHead ) {
      hts << line << "\n";
    } else {
      bts << line << "\n";
    }
  }

  //qDebug("Content::setContent( const QList<QByteArray> & l ) : finished");
}

void Content::setContent( const QByteArray &s )
{
  Q_D(Content);
  d->head.clear();
  d->body.clear();

  // empty header
  if ( s.startsWith( '\n' ) ) {
    d->body = s.right( s.length() - 1 );
    return;
  }

  int pos = s.indexOf( "\n\n", 0 );
  if ( pos > -1 ) {
    d->head = s.left( ++pos );  //header *must* end with "\n" !!
    d->body = s.mid( pos + 1, s.length() - pos - 1 );
  } else {
    d->head = s;
  }
}

QByteArray Content::head() const
{
  return d_ptr->head;
}

void Content::setHead( const QByteArray &head )
{
  d_ptr->head = head;
}

QByteArray Content::body() const
{
  return d_ptr->body;
}

void Content::setBody( const QByteArray &body )
{
  d_ptr->body = body;
}

//parse the message, split multiple parts
void Content::parse()
{
  Q_D(Content);
  //qDebug("void Content::parse() : start");
  qDeleteAll( h_eaders );
  h_eaders.clear();

  // check this part has already been partioned into subparts.
  // if this is the case, we will not try to reparse the body
  // of this part.
  if ( d->body.size() == 0 && !d->contents.isEmpty() ) {
    // reparse all sub parts
    foreach ( Content *c, d->contents ) {
      c->parse();
    }
    return;
  }

  qDeleteAll( d->contents );
  d->contents.clear();

  Headers::ContentType *ct = contentType();
  QByteArray tmp;
  Content *c;
  Headers::contentCategory cat;

  // just "text" as mimetype is suspicious, perhaps this article was
  // generated by broken software, better check for uuencoded binaries
  if ( ct->mimeType() == "text" ) {
    ct->setMimeType( "invalid/invalid" );
  }

  if ( ct->isText() || ct->isEmpty() ) { // default is text/plain
    return; //nothing to do
  }

  if ( ct->isMultipart() ) {   //this is a multipart message
    tmp = ct->boundary(); //get boundary-parameter

    if ( !tmp.isEmpty() ) {
      Parser::MultiPart mpp( d->body, tmp );
      if ( mpp.parse() ) { //at least one part found

        if ( ct->isSubtype( "alternative") ) { //examine category for the sub-parts
          cat = Headers::CCalternativePart;
        } else {
          cat = Headers::CCmixedPart;  //default to "mixed"
        }

        QList<QByteArray> parts = mpp.parts();
        QList<QByteArray>::Iterator it;
        for ( it=parts.begin(); it != parts.end(); ++it ) {
          //create a new Content for every part
          c = new Content();
          c->setContent( *it );
          c->parse();
          c->contentType()->setCategory( cat ); //set category of the sub-part
          d->contents.append( c );
          //qDebug("part:\n%s\n\n%s", c->h_ead.data(), c->b_ody.left(100).data());
        }

        //the whole content is now split into single parts, so it's safe delete the message-body
        d->body.clear();
      } else { //sh*t, the parsing failed so we have to treat the message as "text/plain" instead
        ct->setMimeType( "text/plain" );
        ct->setCharset( "US-ASCII" );
      }
    }
  }
  else if ( ct->mimeType() == "invalid/invalid" ) {
    //non-mime body => check for uuencoded content
    Parser::UUEncoded uup( d->body, rawHeader( "Subject" ) );

    if ( uup.parse() ) { // yep, it is uuencoded

      if ( uup.isPartial() ) {
        //this seems to be only a part of the message so we treat
        //it as "message/partial"
        ct->setMimeType( "message/partial" );
        //ct->setId( uniqueString() ); not needed yet
        ct->setPartialParams( uup.partialCount(), uup.partialNumber() );
        contentTransferEncoding()->setEncoding( Headers::CE7Bit );
      } else { //it's a complete message => treat as "multipart/mixed"
        //the whole content is now split into single parts, so it's safe
        //to delete the message-body
        d->body.clear();

        //binary parts
        for ( int i = 0; i < uup.binaryParts().count(); ++i ) {
          c = new Content();
          //generate content with mime-compliant headers
          tmp = "Content-Type: ";
          tmp += uup.mimeTypes().at( i );
          tmp += "; name=\"";
          tmp += uup.filenames().at( i );
          tmp += "\"\nContent-Transfer-Encoding: x-uuencode\nContent-Disposition: attachment; filename=\"";
          tmp += uup.filenames().at( i );
          tmp += "\"\n\n";
          tmp += uup.binaryParts().at( i );
          c->setContent( tmp );
          addContent( c );
        }

        if ( !d->contents.isEmpty() && d->contents.first() ) {
          //readd the plain text before the uuencoded part
          d->contents.first()->setContent(
            "Content-Type: text/plain\nContent-Transfer-Encoding: 7Bit\n\n" +
            uup.textPart() );
          d->contents.first()->contentType()->setMimeType( "text/plain" );
        }
      }
    } else {
      Parser::YENCEncoded yenc( d->body );

      if ( yenc.parse() ) {
        /* If it is partial, just assume there is exactly one decoded part,
         * and make this that part */
        if ( yenc.isPartial() ) {
          ct->setMimeType( "message/partial" );
          //ct->setId( uniqueString() ); not needed yet
          ct->setPartialParams( yenc.partialCount(), yenc.partialNumber() );
          contentTransferEncoding()->setEncoding( Headers::CEbinary );
        } else { //it's a complete message => treat as "multipart/mixed"
          //the whole content is now split into single parts, so it's safe
          //to delete the message-body
          d->body.clear();

          //binary parts
          for ( int i=0; i<yenc.binaryParts().count(); i++ ) {
            c = new Content();
            //generate content with mime-compliant headers
            tmp = "Content-Type: ";
            tmp += yenc.mimeTypes().at( i );
            tmp += "; name=\"";
            tmp += yenc.filenames().at( i );
            tmp += "\"\nContent-Transfer-Encoding: binary\nContent-Disposition: attachment; filename=\"";
            tmp += yenc.filenames().at( i );
            tmp += "\"\n\n";
            c->setContent( tmp );

            // the bodies of yenc message parts are binary data, not null-terminated strings:
            c->setBody( yenc.binaryParts()[i] );

            addContent( c );
          }

          if ( !d->contents.isEmpty() && d->contents.first() ) {
            //readd the plain text before the uuencoded part
            d->contents.first()->setContent(
              "Content-Type: text/plain\nContent-Transfer-Encoding: 7Bit\n\n" +
              yenc.textPart() );
            d->contents.first()->contentType()->setMimeType( "text/plain" );
          }
        }
      } else { //no, this doesn't look like uuencoded stuff => we treat it as "text/plain"
        ct->setMimeType( "text/plain" );
      }
    }
  }

  //qDebug("void Content::parse() : finished");
}

void Content::assemble()
{
  d_ptr->head = assembleHeaders();
  foreach ( Content *c, contents() ) {
    c->assemble();
  }
}

QByteArray Content::assembleHeaders()
{
  QByteArray newHead = "";

  //Content-Type
  Headers::Base *h = contentType( false );
  if ( h && !h->isEmpty() ) {
    newHead += contentType()->as7BitString() + '\n';
  }

  //Content-Transfer-Encoding
  h = contentTransferEncoding( false );
  if ( h && !h->isEmpty() ) {
    newHead += contentTransferEncoding()->as7BitString() + '\n';
  }

  //Content-Description
  h = contentDescription( false );
  if ( h ) {
    newHead += h->as7BitString() + '\n';
  }

  //Content-Disposition
  h = contentDisposition( false );
  if ( h ) {
    newHead += h->as7BitString() + '\n';
  }

  return newHead;
}

void Content::clear()
{
  Q_D(Content);
  qDeleteAll( h_eaders );
  h_eaders.clear();
  qDeleteAll( d->contents );
  d->contents.clear();
  d->head.clear();
  d->body.clear();
}

QByteArray Content::encodedContent( bool useCrLf )
{
  Q_D(Content);
  QByteArray e;

  // hack to convert articles with uuencoded or yencoded binaries into
  // proper mime-compliant articles
  if ( !d->contents.isEmpty() ) {
    bool convertNonMimeBinaries=false;

    // reencode non-mime binaries...
    foreach ( Content *c, d->contents ) {
      if ( ( c->contentTransferEncoding( true )->encoding() == Headers::CEuuenc ) ||
           ( c->contentTransferEncoding( true )->encoding() == Headers::CEbinary ) ) {
        convertNonMimeBinaries = true;
        c->setBody( KCodecs::base64Encode( c->decodedContent(), true ) + '\n' );
        c->contentTransferEncoding( true )->setEncoding(Headers::CEbase64);
        c->contentTransferEncoding( true )->setDecoded( false );
        c->removeHeader("Content-Description");
        c->assemble();
      }
    }

    // add proper mime headers...
    if ( convertNonMimeBinaries ) {
      int beg = 0, end = 0;
      beg = d->head.indexOf( "MIME-Version: " );
      if ( beg >= 0 ) {
        end = d->head.indexOf( '\n', beg );
      }
      if ( beg >= 0 && end > beg ) {
        d->head.remove( beg, end - beg );
      }
      beg = d->head.indexOf( "Content-Type: " );
      if ( beg >= 0 ) {
        end = d->head.indexOf( '\n', beg );
      }
      if ( beg >= 0 && end > beg ) {
        d->head.remove( beg, end - beg );
      }
      beg = d->head.indexOf( "Content-Transfer-Encoding: " );
      if ( beg >= 0 ) {
        end = d->head.indexOf( '\n', beg );
      }
      if ( beg >= 0 && end > beg ) {
        d->head.remove( beg, end - beg );
      }

      d->head += "MIME-Version: 1.0\n";
      d->head += contentType( true )->as7BitString() + '\n';
      d->head += contentTransferEncoding( true )->as7BitString() + '\n';
    }
  }

  //head
  e = d->head;
  e += '\n';

  //body
  if ( !d->body.isEmpty() ) { //this message contains only one part
    Headers::ContentTransferEncoding *enc=contentTransferEncoding();

    if (enc->needToEncode()) {
      if ( enc->encoding() == Headers::CEquPr ) {
        e += KCodecs::quotedPrintableEncode( d->body, false );
      } else {
        e += KCodecs::base64Encode( d->body, true );
        e += '\n';
      }
    } else {
      e += d->body;
    }
  }
  else if ( !d->contents.isEmpty() ) { //this is a multipart message
    Headers::ContentType *ct=contentType();
    QByteArray boundary = "\n--" + ct->boundary();

    //add all (encoded) contents separated by boundaries
    foreach ( Content *c, d->contents ) {
      e+=boundary + '\n';
      e += c->encodedContent( false );  // don't convert LFs here, we do that later!!!!!
    }
    //finally append the closing boundary
    e += boundary+"--\n";
  };

  if ( useCrLf ) {
    return LFtoCRLF( e );
  } else {
    return e;
  }
}

QByteArray Content::decodedContent()
{
  QByteArray temp, ret;
  Headers::ContentTransferEncoding *ec=contentTransferEncoding();
  bool removeTrailingNewline=false;
  int size = d_ptr->body.length();

  if ( size == 0 ) {
    return ret;
  }

  temp.resize( size );
  memcpy( temp.data(), d_ptr->body.data(), size );

  if ( ec->decoded() ) {
    ret = temp;
    removeTrailingNewline = true;
  } else {
    switch( ec->encoding() ) {
    case Headers::CEbase64 :
      KCodecs::base64Decode( temp, ret );
      break;
    case Headers::CEquPr :
      ret = KCodecs::quotedPrintableDecode( d_ptr->body );
      ret.resize( ret.size() - 1 );  // remove null-char
      removeTrailingNewline = true;
      break;
    case Headers::CEuuenc :
      KCodecs::uudecode( temp, ret );
      break;
    case Headers::CEbinary :
      ret = temp;
      removeTrailingNewline = false;
      break;
    default :
      ret = temp;
      removeTrailingNewline = true;
    }
  }

  if ( removeTrailingNewline && (ret.size() > 0 ) && ( ret[ret.size()-1] == '\n') ) {
    ret.resize( ret.size() - 1 );
  }

  return ret;
}

QString Content::decodedText( bool trimText, bool removeTrailingNewlines )
{
  if ( !decodeText() ) { //this is not a text content !!
    return QString();
  }

  bool ok = true;
  QTextCodec *codec =
    KGlobal::charsets()->codecForName( contentType()->charset(), ok );

  QString s = codec->toUnicode( d_ptr->body.data(), d_ptr->body.length() );

  if ( trimText && removeTrailingNewlines ) {
    int i;
    for ( i = s.length() - 1; i >= 0; --i ) {
      if ( !s[i].isSpace() ) {
        break;
      }
    }
    s.truncate( i + 1 );
  } else {
    if ( s.right( 1 ) == "\n" ) {
      s.truncate( s.length() - 1 ); // remove trailing new-line
    }
  }

  return s;
}

void Content::fromUnicodeString( const QString &s )
{
  bool ok = true;
  QTextCodec *codec =
    KGlobal::charsets()->codecForName( contentType()->charset(), ok );

  if ( !ok ) { // no suitable codec found => try local settings and hope the best ;-)
    codec = KGlobal::locale()->codecForEncoding();
    QByteArray chset = KGlobal::locale()->encoding();
    contentType()->setCharset( chset );
  }

  d_ptr->body = codec->fromUnicode( s );
  contentTransferEncoding()->setDecoded( true ); //text is always decoded
}

Content *Content::textContent()
{
  Content *ret=0;

  //return the first content with mimetype=text/*
  if ( contentType()->isText() ) {
    ret = this;
  } else {
    foreach ( Content *c, d_ptr->contents ) {
      if ( ( ret = c->textContent() ) != 0 ) {
        break;
      }
    }
  }
  return ret;
}

Content::List Content::attachments( bool incAlternatives )
{
  List attachments;
  if ( d_ptr->contents.isEmpty() ) {
    attachments.append( this );
  } else {
    foreach ( Content *c, d_ptr->contents ) {
      if ( !incAlternatives &&
           c->contentType()->category() == Headers::CCalternativePart ) {
        continue;
      } else {
        attachments += c->attachments( incAlternatives );
      }
    }
  }

  if ( isTopLevel() ) {
    Content *text = textContent();
    if ( text ) {
      attachments.removeAll( text );
    }
  }
  return attachments;
}

Content::List Content::contents() const
{
  return d_ptr->contents;
}

void Content::addContent( Content *c, bool prepend )
{
  Q_D(Content);
  if ( d->contents.isEmpty() && !contentType()->isMultipart() ) {
    // this message is not multipart yet

    // first we convert the body to a content
    Content *main = new Content();

    //the Mime-Headers are needed, so we move them to the new content
    for ( Headers::Base::List::iterator it = h_eaders.begin();
          it != h_eaders.end(); ) {
      if ( (*it)->isMimeHeader() ) {
        // append to new content
        main->h_eaders.append( *it );
        // and remove from this content
        it = h_eaders.erase( it );
      } else {
        ++it;
      }
    }

    //"main" is now part of a multipart/mixed message
    main->contentType()->setCategory(Headers::CCmixedPart);

    //the head of "main" is empty, so we assemble it
    main->assemble();

    //now we can copy the body and append the new content;
    main->setBody( d->body );
    d->contents.append( main );
    d->body.clear(); //not longer needed

    //finally we have to convert this article to "multipart/mixed"
    Headers::ContentType *ct=contentType();
    ct->setMimeType( "multipart/mixed" );
    ct->setBoundary( multiPartBoundary() );
    ct->setCategory( Headers::CCcontainer );
    contentTransferEncoding()->clear();  // 7Bit, decoded

  }
  //here we actually add the content
  if ( prepend ) {
    d->contents.insert( 0, c );
  } else {
    d->contents.append( c );
  }
}

void Content::removeContent( Content *c, bool del )
{
  Q_D(Content);
  if ( d->contents.isEmpty() ) { // what the ..
    return;
  }

  d->contents.removeAll( c );
  if ( del ) {
    delete c;
  }

  //only one content left => turn this message in a single-part
  if ( d->contents.count() == 1 ) {
    Content *main = d->contents.first();

    //first we have to move the mime-headers
    for ( Headers::Base::List::iterator it = main->h_eaders.begin();
          it != main->h_eaders.end(); ) {
      if ( (*it)->isMimeHeader() ) {
        kDebug(5320) << "Content::removeContent(Content *c, bool del) : mime-header moved: "
                     << (*it)->as7BitString() << endl;
        // first remove the old header
        removeHeader( (*it)->type() );
        // then append to new content
        h_eaders.append( *it );
        // and finally remove from this content
        it = main->h_eaders.erase( it );
      } else {
        ++it;
      }
    }

    //now we can copy the body
    d->body = main->body();

    //finally we can delete the content list
    qDeleteAll( d->contents );
    d->contents.clear();
  }
}

void Content::changeEncoding( Headers::contentEncoding e )
{
  Headers::ContentTransferEncoding *enc = contentTransferEncoding();
  if ( enc->encoding() == e ) { //nothing to do
    return;
  }

  if ( decodeText() ) {
    enc->setEncoding( e ); // text is not encoded until it's sent or saved
                           // so we just set the new encoding
  } else {
    // this content contains non textual data, that has to be re-encoded
    if ( e != Headers::CEbase64 ) {
      //kWarning(5003) << "Content::changeEncoding() : non textual data "
      //               << "and encoding != base64 - this should not happen => "
      //               << "forcing base64" << endl;
      e = Headers::CEbase64;
    }

    if ( enc->encoding() != e ) { // ok, we reencode the content using base64
      d_ptr->body = KCodecs::base64Encode( decodedContent(), true );
      d_ptr->body.append( "\n" );
      enc->setEncoding( e ); //set encoding
      enc->setDecoded( false );
    }
  }
}

void Content::toStream( QTextStream &ts, bool scrambleFromLines )
{
  QByteArray ret = encodedContent( false );

  if ( scrambleFromLines ) {
    // FIXME Why are only From lines with a preceding empty line considered?
    //       And, of course, all lines starting with >*From have to be escaped
    //       because otherwise the transformation is not revertable.
    ret.replace( "\n\nFrom ", "\n\n>From ");
  }
  ts << ret;
}

Headers::Generic *Content::getNextHeader( QByteArray &head )
{
  int pos1=-1, pos2=0, len=head.length()-1;
  bool folded( false );
  Headers::Generic *header=0;

  pos1 = head.indexOf( ": " );

  if ( pos1 > -1 ) {    //there is another header
    pos2 = pos1 += 2; //skip the name

    if ( head[pos2] != '\n' ) {  // check if the header is not empty
      while ( 1 ) {
        pos2 = head.indexOf( '\n', pos2 + 1 );
        if ( pos2 == -1 || pos2 == len ||
             ( head[pos2+1] != ' ' && head[pos2+1] != '\t' ) ) {
          //break if we reach the end of the string, honor folded lines
          break;
        } else {
          folded = true;
        }
      }
    }

    if ( pos2 < 0 ) {
      pos2 = len + 1; //take the rest of the string
    }

    if ( !folded ) {
      header = new Headers::Generic(head.left(pos1-2), this, head.mid(pos1, pos2-pos1));
    } else {
      QByteArray hdrValue = head.mid( pos1, pos2 - pos1 );
      header = new Headers::Generic( head.left( pos1 - 2 ), this, unfoldHeader( hdrValue ) );
    }

    head.remove( 0, pos2 + 1 );
  } else {
    head = "";
  }

  return header;
}

Headers::Base *Content::getHeaderByType( const char *type )
{
  if ( !type ) {
    return 0;
  }

  //first we check if the requested header is already cached
  foreach ( Headers::Base *h, h_eaders ) {
    if ( h->is( type ) ) {
      return h; //found
    }
  }

  //now we look for it in the article head
  Headers::Base *h = 0;
  QByteArray raw=rawHeader( type );
  if ( !raw.isEmpty() ) { //ok, we found it
    //choose a suitable header class
    if ( strcasecmp( "Message-Id", type ) == 0 ) {
      h = new Headers::MessageID( this, raw );
    } else if ( strcasecmp( "Subject", type ) == 0 ) {
      h = new Headers::Subject( this, raw );
    } else if ( strcasecmp( "Date", type ) == 0 ) {
      h = new Headers::Date( this, raw );
    } else if ( strcasecmp( "From", type ) == 0 ) {
      h = new Headers::From( this, raw );
    } else if ( strcasecmp( "Organization", type ) == 0 ) {
      h = new Headers::Organization( this, raw );
    } else if ( strcasecmp( "Reply-To", type ) == 0 ) {
      h = new Headers::ReplyTo( this, raw );
    } else if ( strcasecmp( "Mail-Copies-To", type ) == 0 ) {
      h = new Headers::MailCopiesTo( this, raw );
    } else if ( strcasecmp( "To", type ) == 0 ) {
      h = new Headers::To( this, raw );
    } else if ( strcasecmp( "CC", type ) == 0 ) {
      h = new Headers::Cc( this, raw );
    } else if ( strcasecmp( "BCC", type ) == 0 ) {
      h = new Headers::Bcc( this, raw );
    } else if ( strcasecmp( "Newsgroups", type ) == 0 ) {
      h = new Headers::Newsgroups( this, raw );
    } else if ( strcasecmp( "Followup-To", type ) == 0 ) {
      h = new Headers::FollowUpTo( this, raw );
    } else if ( strcasecmp( "References", type ) == 0 ) {
      h = new Headers::References( this, raw );
    } else if ( strcasecmp( "Lines", type ) == 0 ) {
      h = new Headers::Lines( this, raw );
    } else if ( strcasecmp( "Content-Type", type ) == 0 ) {
      h = new Headers::ContentType( this, raw );
    } else if ( strcasecmp( "Content-Transfer-Encoding", type ) == 0 ) {
      h = new Headers::ContentTransferEncoding( this, raw );
    } else if ( strcasecmp( "Content-Disposition", type ) == 0 ) {
      h = new Headers::ContentDisposition( this, raw );
    } else if ( strcasecmp( "Content-Description", type ) == 0 ) {
      h = new Headers::ContentDescription( this, raw );
    } else {
      h = new Headers::Generic( type, this, raw );
    }
    h_eaders.append( h );  //add to cache
    return h;
  } else {
    return 0; //header not found
  }
}

void Content::setHeader( Headers::Base *h )
{
  if ( !h ) {
    return;
  }
  removeHeader( h->type() );
  h_eaders.append( h );
}

bool Content::removeHeader( const char *type )
{
  for ( Headers::Base::List::iterator it = h_eaders.begin();
        it != h_eaders.end(); ++it )
    if ( (*it)->is(type) ) {
      delete (*it);
      h_eaders.erase( it );
      return true;
    }

  return false;
}

bool Content::hasHeader( const char *type )
{
  return getHeaderByType( type ) != 0;
}

Headers::ContentType *Content::contentType( bool create )
{
  Headers::ContentType *p=0;
  return getHeaderInstance( p, create );
}

Headers::ContentTransferEncoding *Content::contentTransferEncoding( bool create )
{
  Headers::ContentTransferEncoding *p=0;
  return getHeaderInstance( p, create );
}

Headers::ContentDisposition *Content::contentDisposition( bool create )
{
  Headers::ContentDisposition *p=0;
  return getHeaderInstance( p, create );
}

Headers::ContentDescription *Content::contentDescription( bool create )
{
  Headers::ContentDescription *p=0;
  return getHeaderInstance( p, create );
}

int Content::size()
{
  int ret = d_ptr->body.length();

  if ( contentTransferEncoding()->encoding() == Headers::CEbase64 ) {
    return ret * 3 / 4; //base64 => 6 bit per byte
  }

  return ret;
}

int Content::storageSize() const
{
  const Q_D(Content);
  int s = d->head.size();

  if ( d->contents.isEmpty() ) {
    s += d->body.size();
  } else {
    foreach ( Content *c, d->contents ) {
      s += c->storageSize();
    }
  }

  return s;
}

int Content::lineCount() const
{
  const Q_D(Content);
  int ret = 0;
  if ( !isTopLevel() ) {
    ret += d->head.count( '\n' );
  }
  ret += d->body.count( '\n' );

  foreach ( Content *c, d->contents ) {
    ret += c->lineCount();
  }

  return ret;
}

QByteArray Content::rawHeader( const char *name ) const
{
  return KMime::extractHeader( d_ptr->head, name );
}

bool Content::decodeText()
{
  Q_D(Content);
  Headers::ContentTransferEncoding *enc = contentTransferEncoding();

  if ( !contentType()->isText() ) {
    return false; //non textual data cannot be decoded here => use decodedContent() instead
  }
  if ( enc->decoded() ) {
    return true; //nothing to do
  }

  switch( enc->encoding() )
  {
  case Headers::CEbase64 :
    d->body = KCodecs::base64Decode( d->body );
    d->body.append( "\n" );
    break;
  case Headers::CEquPr :
    d->body = KCodecs::quotedPrintableDecode( d->body );
    break;
  case Headers::CEuuenc :
    d->body = KCodecs::uudecode( d->body );
    d->body.append( "\n" );
    break;
  case Headers::CEbinary :
    // nothing to decode
    d->body.append( "\n" );
  default :
    break;
  }

  enc->setDecoded( true );
  return true;
}

QByteArray Content::defaultCharset() const
{
  return d_ptr->defaultCS;
}

void Content::setDefaultCharset( const QByteArray &cs )
{
  d_ptr->defaultCS = KMime::cachedCharset( cs );

  foreach ( Content *c, d_ptr->contents ) {
    c->setDefaultCharset( cs );
  }

  // reparse the part and its sub-parts in order
  // to clear cached header values
  parse();
}

bool Content::forceDefaultCharset() const
{
  return d_ptr->forceDefaultCS;
}

void Content::setForceDefaultCharset( bool b )
{
  d_ptr->forceDefaultCS = b;

  foreach ( Content *c, d_ptr->contents ) {
    c->setForceDefaultCharset( b );
  }

  // reparse the part and its sub-parts in order
  // to clear cached header values
  parse();
}

Content * KMime::Content::content( const ContentIndex &index ) const
{
  if ( !index.isValid() ) {
    return const_cast<KMime::Content*>( this );
  }
  ContentIndex idx = index;
  unsigned int i = idx.pop() - 1; // one-based -> zero-based index
  if ( i < (unsigned int)d_ptr->contents.size() ) {
    return d_ptr->contents[i]->content( idx );
  } else {
    return 0;
  }
}

ContentIndex KMime::Content::indexForContent( Content * content ) const
{
  int i = d_ptr->contents.indexOf( content );
  if ( i > 0 ) {
    ContentIndex ci;
    ci.push( i + 1 ); // zero-based -> one-based index
    return ci;
  }
  // not found, we need to search recursively
  for ( int i = 0; i < d_ptr->contents.size(); ++i ) {
    ContentIndex ci = d_ptr->contents[i]->indexForContent( content );
    if ( ci.isValid() ) {
      // found it
      ci.push( i + 1 ); // zero-based -> one-based index
      return ci;
    }
  }
  return ContentIndex(); // not found
}

bool Content::isTopLevel() const
{
  return false;
}

} // namespace KMime
