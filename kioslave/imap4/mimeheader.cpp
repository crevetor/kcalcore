/***************************************************************************
                          mimeheader.cc  -  description
                             -------------------
    begin                : Fri Oct 20 2000
    copyright            : (C) 2000 by Sven Carstens
    email                : s.carstens@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "mimeheader.h"
#include "mimehdrline.h"
#include "mailheader.h"

#include <QRegExp>

// #include <iostream.h>
#include <kglobal.h>
#include <kcomponentdata.h>
#include <kiconloader.h>
#include <kmimetype.h>
#include <kcodecs.h>
#include <kdebug.h>

#include <kimap/rfccodecs.h>
using namespace KIMAP;

mimeHeader::mimeHeader ()
    : typeList (), dispositionList (),
      _contentType("application/octet-stream"),
      _contentDisposition(), _contentDescription()
{
  // Case insensitive hashes are killing us.  Also are they too small?
  nestedMessage = NULL;
  contentLength = 0;
}

mimeHeader::~mimeHeader ()
{
}

/*
QPtrList<mimeHeader> mimeHeader::getAllParts()
{
QPtrList<mimeHeader> retVal;

  // caller is responsible for clearing
  retVal.setAutoDelete( false );
  nestedParts.setAutoDelete( false );

  // shallow copy
  retVal = nestedParts;

  // can't have duplicate pointers
  nestedParts.clear();

  // restore initial state
  nestedParts.setAutoDelete( true );

  return retVal;
} */

void
mimeHeader::addHdrLine (mimeHdrLine * aHdrLine)
{
  mimeHdrLine *addLine = new mimeHdrLine( aHdrLine );
  if ( addLine ) {
    originalHdrLines.append( addLine );
    if ( qstrnicmp( addLine->getLabel(), "Content-", 8 ) ) {
      additionalHdrLines.append( addLine );
    } else {
      int skip;
      const char *aCStr = addLine->getValue().data();
      QHash < QString, QString > *aList = 0;

      skip = mimeHdrLine::parseSeparator( ';', aCStr );
      if ( skip > 0 ) {
        int cut = 0;
        if ( skip >= 2 ) {
          if ( aCStr[skip - 1] == '\r' ) {
            cut++;
          }
          if ( aCStr[skip - 1] == '\n' ) {
            cut++;
          }
          if ( aCStr[skip - 2] == '\r' ) {
            cut++;
          }
          if ( aCStr[skip - 1] == ';' ) {
            cut++;
          }
        }
        QByteArray mimeValue( aCStr, skip - cut );

        if ( !qstricmp( addLine->getLabel(), "Content-Disposition" ) ) {
          aList = &dispositionList;
          setDisposition( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-Type" ) ) {
          aList = &typeList;
          setType( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-Transfer-Encoding" ) ) {
          setEncoding( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-ID" ) ) {
          setID( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-Description" ) ) {
          setDescription( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-MD5" ) ) {
          setMD5( mimeValue );
        } else if ( !qstricmp( addLine->getLabel(), "Content-Length" ) ) {
          contentLength = mimeValue.toUInt();
        } else {
          additionalHdrLines.append( addLine );
        }
//        cout << addLine->getLabel().data() << ": '" << mimeValue.data() << "'" << endl;

        aCStr += skip;
        while ( ( skip = mimeHdrLine::parseSeparator( ';', aCStr ) ) ) {
          if ( skip > 0 ) {
            if ( aList ) {
              addParameter( QByteArray( aCStr, skip ).simplified(), *aList );
            }
            mimeValue = QByteArray( addLine->getValue().data(), skip );
            aCStr += skip;
          } else {
            break;
          }
        }
      }
    }
  }
}

void
mimeHeader::addParameter (const QByteArray& aParameter, QHash < QString, QString > &aList)
{
  QString aValue;
  QByteArray aLabel;
  int pos = aParameter.indexOf( '=' );
//  cout << aParameter.left( pos ).data();
  aValue = QString::fromLatin1( aParameter.right( aParameter.length() - pos - 1 ) );
  aLabel = aParameter.left( pos );
  if ( aValue[0] == '"' ) {
    aValue = aValue.mid( 1, aValue.length() - 2 );
  }

  aList.insert( aLabel.toLower(), aValue );
//  cout << "=" << aValue->data() << endl;
}

QString
mimeHeader::getDispositionParm (const QByteArray& aStr)
{
  return getParameter( aStr, dispositionList );
}

QString
mimeHeader::getTypeParm (const QByteArray& aStr)
{
  return getParameter( aStr, typeList );
}

void
mimeHeader::setDispositionParm (const QByteArray& aLabel, const QString& aValue)
{
  setParameter( aLabel, aValue, dispositionList );
  return;
}

void
mimeHeader::setTypeParm (const QByteArray& aLabel, const QString& aValue)
{
  setParameter( aLabel, aValue, typeList );
}

QHashIterator < QString, QString > mimeHeader::getDispositionIterator ()
{
  return QHashIterator < QString, QString > ( dispositionList );
}

QHashIterator < QString, QString > mimeHeader::getTypeIterator ()
{
  return QHashIterator < QString, QString > ( typeList );
}

QListIterator < mimeHdrLine *> mimeHeader::getOriginalIterator ()
{
  return QListIterator < mimeHdrLine *> ( originalHdrLines );
}

QListIterator < mimeHdrLine *> mimeHeader::getAdditionalIterator ()
{
  return QListIterator < mimeHdrLine *> ( additionalHdrLines );
}

void
mimeHeader::outputHeader (mimeIO & useIO)
{
  if ( !getDisposition().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-Disposition: " )
                          + getDisposition()
                          + outputParameter( dispositionList ) );
  }

  if ( !getType().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-Type: " )
                          + getType() + outputParameter( typeList ) );
  }
  if ( !getDescription().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-Description: " ) +
                          getDescription() );
  }
  if ( !getID().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-ID: " ) + getID() );
  }
  if ( !getMD5().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-MD5: " ) + getMD5() );
  }
  if ( !getEncoding().isEmpty() ) {
    useIO.outputMimeLine( QByteArray( "Content-Transfer-Encoding: " ) +
                          getEncoding() );
  }

  QListIterator < mimeHdrLine *> ait = getAdditionalIterator();
  mimeHdrLine *hdrline;
  while ( ait.hasNext() ) {
    hdrline = ait.next();
    useIO.outputMimeLine( hdrline->getLabel() + ": " +
                          hdrline->getValue() );
  }
  useIO.outputMimeLine( QByteArray( "" ) );
}

QString
mimeHeader::getParameter (const QByteArray& aStr, QHash < QString, QString > &aDict)
{
  QString retVal, found;
  //see if it is a normal parameter
  found = aDict.value( aStr );
  if ( found.isEmpty() ) {
    //might be a continuated or encoded parameter
    found = aDict.value( aStr + '*' );
    if ( found.isEmpty() ) {
      //continuated parameter
      QString decoded, encoded;
      int part = 0;

      do {
        QByteArray search;
        search.setNum( part );
        search = aStr + '*' + search;
        found = aDict.value( search );
        if ( found.isEmpty() ) {
          found = aDict.value( search + '*' );
          if ( !found.isEmpty() ) {
            encoded += KIMAP::encodeRFC2231String( found );
          }
        } else {
          encoded += found;
        }
        part++;
      } while ( !found.isEmpty() );
      if ( encoded.contains( '\'' ) ) {
        retVal = KIMAP::decodeRFC2231String( encoded.toLocal8Bit() );
      } else {
        retVal = KIMAP::decodeRFC2231String( QByteArray( "''" ) + encoded.toLocal8Bit() );
      }
    } else {
      //simple encoded parameter
      retVal = KIMAP::decodeRFC2231String( found.toLocal8Bit() );
    }
  } else {
    retVal = found;
  }
  return retVal;
}

void
mimeHeader::setParameter (const QByteArray& aLabel, const QString& aValue,
                          QHash < QString, QString > &aDict)
{
  bool encoded = true;
  uint vlen, llen;
  QString val = aValue;

  //see if it needs to get encoded
  if ( encoded && !aLabel.contains( '*' ) ) {
    val = KIMAP::encodeRFC2231String( aValue );
  }
  //kDebug( 7116 ) << "mimeHeader::setParameter() - val = '" << val << "'";
  //see if it needs to be truncated
  vlen = val.length();
  llen = aLabel.length();
  if ( vlen + llen + 4 > 80 && llen < 80 - 8 - 2 ) {
    const int limit = 80 - 8 - 2 - (int)llen;
    // the -2 is there to allow extending the length of a part of val
    // by 1 or 2 in order to prevent an encoded character from being
    // split in half
    int i = 0;
    QString shortValue;
    QByteArray shortLabel;

    while ( !val.isEmpty() ) {
      int partLen; // the length of the next part of the value
      if ( limit >= int(vlen) ) {
        // the rest of the value fits completely into one continued header
        partLen = vlen;
      } else {
        partLen = limit;
        // make sure that we don't split an encoded char in half
        if ( val[partLen-1] == '%' ) {
          partLen += 2;
        } else if ( partLen > 1 && val[partLen-2] == '%' ) {
          partLen += 1;
        }
        // make sure partLen does not exceed vlen (could happen in case of
        // an incomplete encoded char)
        if ( partLen > int(vlen) ) {
          partLen = vlen;
        }
      }
      shortValue = val.left( partLen );
      shortLabel.setNum( i );
      shortLabel = aLabel + '*' + shortLabel;
      val = val.right( vlen - partLen );
      vlen = vlen - partLen;
      if ( encoded ) {
        if ( i == 0 ) {
          shortValue = "''" + shortValue;
        }
        shortLabel += '*';
      }
      //kDebug( 7116 ) << "mimeHeader::setParameter() - shortLabel = '" << shortLabel << "'";
      //kDebug( 7116 ) << "mimeHeader::setParameter() - shortValue = '" << shortValue << "'";
      //kDebug( 7116 ) << "mimeHeader::setParameter() - val        = '" << val << "'";
      aDict.insert( shortLabel.toLower(), shortValue );
      i++;
    }
  } else {
    aDict.insert( aLabel.toLower(), val );
  }
}

QByteArray mimeHeader::outputParameter (QHash < QString, QString > &aDict)
{
  QByteArray retVal;
  QHashIterator < QString, QString > it( aDict );
  while ( it.hasNext() ) {
    it.next();
    retVal += ( ";\n\t" + it.key() + '=' ).toLatin1();
    if ( it.value().indexOf( ' ' ) > 0 || it.value().indexOf( ';' ) > 0 ) {
      retVal += '"' + it.value().toUtf8() + '"';
    } else {
      retVal += it.value().toUtf8();
    }
  }
  retVal += '\n';

  return retVal;
}

void
mimeHeader::outputPart (mimeIO & useIO)
{
  QListIterator < mimeHeader *> nestedPartsIterator = getNestedIterator();
  QByteArray boundary;
  if ( !getTypeParm( "boundary" ).isEmpty() ) {
    boundary = getTypeParm( "boundary" ).toLatin1();
  }

  outputHeader( useIO );
  if ( !getPreBody().isEmpty() ) {
    useIO.outputMimeLine( getPreBody() );
  }
  if ( getNestedMessage() ) {
    getNestedMessage()->outputPart( useIO );
  }

  mimeHeader *mimeline;
  while ( nestedPartsIterator.hasNext() ) {
    mimeline = nestedPartsIterator.next();
    if ( !boundary.isEmpty() ) {
      useIO.outputMimeLine( "--" + boundary );
    }
    mimeline->outputPart( useIO );
  }
  if ( !boundary.isEmpty() ) {
    useIO.outputMimeLine( "--" + boundary + "--" );
  }
  if ( !getPostBody().isEmpty() ) {
    useIO.outputMimeLine( getPostBody() );
  }
}

#if 0
int
mimeHeader::parsePart (mimeIO & useIO, const QString& boundary)
{
  int retVal = 0;
  bool mbox = false;
  QByteArray preNested, postNested;
  mbox = parseHeader( useIO );

  kDebug( 7116 ) << "mimeHeader::parsePart - parsing part '" << getType() << "'";
  if ( !qstrnicmp( getType(), "Multipart", 9 ) ) {
    retVal = parseBody( useIO, preNested, getTypeParm( "boundary" ) );  //this is a message in mime format stuff
    setPreBody( preNested );
    int localRetVal;
    do {
      mimeHeader *aHeader = new mimeHeader;

      // set default type for multipart/digest
      if ( !qstrnicmp( getType(), "Multipart/Digest", 16 ) ) {
        aHeader->setType( "Message/RFC822" );
      }

      localRetVal = aHeader->parsePart( useIO, getTypeParm( "boundary" ) );
      addNestedPart( aHeader );
    } while ( localRetVal );        //get nested stuff
  }
  if ( !qstrnicmp( getType(), "Message/RFC822", 14 ) ) {
    mailHeader *msgHeader = new mailHeader;
    retVal = msgHeader->parsePart( useIO, boundary );
    setNestedMessage( msgHeader );
  } else {
    retVal = parseBody( useIO, postNested, boundary, mbox ); //just a simple part remaining
    setPostBody( postNested );
  }
  return retVal;
}

int
mimeHeader::parseBody (mimeIO & useIO, QByteArray & messageBody,
                       const QString& boundary, bool mbox)
{
  QByteArray inputStr;
  QByteArray buffer;
  QString partBoundary;
  QString partEnd;
  int retVal = 0;               //default is last part

  if ( !boundary.isEmpty() ) {
    partBoundary = QString( "--" ) + boundary;
    partEnd = QString( "--" ) + boundary + "--";
  }

  while ( useIO.inputLine( inputStr ) ) {
    //check for the end of all parts
    if ( !partEnd.isEmpty() &&
         !qstrnicmp( inputStr, partEnd.toLatin1(), partEnd.length() - 1 ) ) {
      retVal = 0;               //end of these parts
      break;
    } else if ( !partBoundary.isEmpty() &&
                !qstrnicmp( inputStr, partBoundary.toLatin1(),
                            partBoundary.length() - 1 ) ) {
      retVal = 1;               //continue with next part
      break;
    } else if ( mbox && inputStr.startsWith( "From " ) ) {
      retVal = 0;               // end of mbox
      break;
    }
    buffer += inputStr;
    if ( buffer.length() > 16384 ) {
      messageBody += buffer;
      buffer = "";
    }
  }

  messageBody += buffer;
  return retVal;
}
#endif

bool mimeHeader::parseHeader (mimeIO & useIO)
{
  bool mbox = false;
  bool first = true;
  mimeHdrLine my_line;
  QByteArray inputStr;

  kDebug( 7116 ) << "mimeHeader::parseHeader - starting parsing";
  while ( useIO.inputLine( inputStr ) ) {
    int appended;
    if ( !inputStr.startsWith( "From " ) || !first ) { //krazy:exclude=strings
      first = false;
      appended = my_line.appendStr( inputStr );
      if ( !appended ) {
        addHdrLine( &my_line );
        appended = my_line.setStr( inputStr );
      }
      if ( appended <= 0 ) {
        break;
      }
    } else {
      mbox = true;
      first = false;
    }
    inputStr = QByteArray();
  }

  kDebug( 7116 ) << "mimeHeader::parseHeader - finished parsing";
  return mbox;
}

mimeHeader *
mimeHeader::bodyPart (const QString & _str)
{
  // see if it is nested a little deeper
  int pt = _str.indexOf( '.' );
  if ( pt != -1 ) {
    QString tempStr = _str;
    mimeHeader *tempPart;

    tempStr = _str.right( _str.length() - pt - 1 );
    if ( nestedMessage ) {
      kDebug( 7116 ) << "mimeHeader::bodyPart - recursing message";
      tempPart = nestedMessage->nestedParts.at( _str.left( pt ).toULong() - 1 );
    } else {
      kDebug( 7116 ) << "mimeHeader::bodyPart - recursing mixed";
      tempPart = nestedParts.at( _str.left( pt ).toULong() - 1 );
    }
    if ( tempPart ) {
      tempPart = tempPart->bodyPart( tempStr );
    }
    return tempPart;
  }

  kDebug( 7116 ) << "mimeHeader::bodyPart - returning part" << _str;
  // or pick just the plain part
  if ( nestedMessage ) {
    kDebug( 7116 ) << "mimeHeader::bodyPart - message";
    return nestedMessage->nestedParts.at( _str.toULong() - 1 );
  }
  kDebug( 7116 ) << "mimeHeader::bodyPart - mixed";
  return nestedParts.at( _str.toULong() - 1 );
}

void mimeHeader::serialize(QDataStream& stream)
{
  int nestedcount = nestedParts.count();
  if ( nestedParts.isEmpty() && nestedMessage ) {
    nestedcount = 1;
  }
  stream << nestedcount;
  stream << _contentType;
  stream << QString( getTypeParm( "name" ) );
  stream << _contentDescription;
  stream << _contentDisposition;
  stream << _contentEncoding;
  stream << contentLength;
  stream << partSpecifier;
  // serialize nested message
  if ( nestedMessage ) {
    nestedMessage->serialize( stream );
  }

  // serialize nested parts
  if ( !nestedParts.isEmpty() ) {
    QListIterator < mimeHeader *> it( nestedParts );
    mimeHeader* part;
    while ( it.hasNext() ) {
      part = it.next();
      part->serialize( stream );
    }
  }
}

#ifdef KMAIL_COMPATIBLE
// compatibility subroutines
QString
mimeHeader::bodyDecoded ()
{
  kDebug( 7116 ) << "mimeHeader::bodyDecoded";
  QByteArray temp = bodyDecodedBinary();
  return QString::fromLatin1( temp.data(), temp.count() );
}

QByteArray
mimeHeader::bodyDecodedBinary ()
{
  QByteArray retVal;

  if ( contentEncoding.startsWith( QLatin1String( "quoted-printable" ), Qt::CaseInsensitive ) ) {
    retVal = KCodecs::quotedPrintableDecode( postMultipartBody );
  } else if ( contentEncoding.startsWith( QLatin1String( "base64" ), Qt::CaseInsensitive ) ) {
    KCodecs::base64Decode( postMultipartBody, retVal );
  } else {
    retVal = postMultipartBody;
  }

  kDebug( 7116 ) << "mimeHeader::bodyDecodedBinary - size is" << retVal.size();
  return retVal;
}

void
mimeHeader::setBodyEncodedBinary (const QByteArray & _arr)
{
  setBodyEncoded( _arr );
}

void
mimeHeader::setBodyEncoded (const QByteArray & _arr)
{
  QByteArray setVal;

  kDebug( 7116 ) << "mimeHeader::setBodyEncoded - in size" << _arr.size();
  if ( contentEncoding.startsWith( QLatin1String( "quoted-printable" ), Qt::CaseInsensitive ) ) {
    setVal = KCodecs::quotedPrintableEncode( _arr );
  } else if ( contentEncoding.startsWith( QLatin1String( "base64" ), Qt::CaseInsensitive ) ) {
    KCodecs::base64Encode( _arr, setVal );
  } else {
    setVal.duplicate( _arr );
  }
  kDebug( 7116 ) << "mimeHeader::setBodyEncoded - out size" << setVal.size();

  postMultipartBody.duplicate( setVal );
  kDebug( 7116 ) << "mimeHeader::setBodyEncoded - out size" << postMultipartBody.size();
}

QString
mimeHeader::iconName ()
{
  QString fileName =
    KMimeType::mimeType( contentType.toLower() )->icon( QString(), false );
  QString iconFileName =
    KGlobal::mainComponent().iconLoader()->iconPath( fileName, KIconLoader::Desktop );
//  if ( iconFileName.isEmpty() )
//    iconFileName = KGlobal::mainComponent().iconLoader()->iconPath( "unknown", KIconLoader::Desktop );
  return iconFileName;
}

void
mimeHeader::setNestedMessage (mailHeader * inPart, bool destroy)
{
//  if( nestedMessage && destroy ) delete nestedMessage;
  nestedMessage = inPart;
}

QString
mimeHeader::headerAsString ()
{
  mimeIOQString myIO;

  outputHeader( myIO );
  return myIO.getString();
}

QString
mimeHeader::magicSetType (bool aAutoDecode)
{
  QByteArray body;

  if ( aAutoDecode ) {
    body = bodyDecodedBinary();
  } else {
    body = postMultipartBody;
  }

  KMimeType::Ptr mime = KMimeType::findByContent( body );
  QString mimetype = mime->name();
  contentType = mimetype;
  return mimetype;
}
#endif
