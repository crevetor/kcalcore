/*
  This file is part of the kblog library.

  Copyright (c) 2004 Reinhold Kainhofer <reinhold@kainhofer.com>
  Copyright (c) 2006 Christian Weilbach <christian@whiletaker.homeip.net>
  Copyright (c) 2007 Mike Arthur <mike@mikearthur.co.uk>

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

#include "blogger.h"
#include "blogger_p.h"
#include "blogposting.h"

#include <KDebug>

#include <QStringList>

using namespace KBlog;

Blogger1::Blogger1( const KUrl &server, QObject *parent )
  : Blog( server, parent ), d( new Blogger1Private )
{
  d->parent = this;
  setUrl( server );
}

Blogger1::~Blogger1()
{
  delete d;
}

QString Blogger1::interfaceName() const
{
  return QLatin1String( "Blogger1  1.0" );
}

void Blogger1::setUrl( const KUrl &server )
{
  Blog::setUrl( server );
  delete d->mXmlRpcClient;
  d->mXmlRpcClient = new KXmlRpc::Client( server );
  d->mXmlRpcClient->setUserAgent( userAgent() );
}

void Blogger1::fetchUserInfo()
{
  return; //TODO
}

void Blogger1::listBlogs()
{

    kDebug(5323) << "Fetch List of Blogs..." << endl;
    QList<QVariant> args( d->defaultArgs() );
    d->mXmlRpcClient->call(
      "blogger.getUsersBlogs", args,
      d, SLOT( slotListBlogs( const QList<QVariant>&, const QVariant& ) ),
      d, SLOT( slotError( int, const QString&, const QVariant& ) ) );
}

void Blogger1::listRecentPostings( int number )
{
    kDebug(5323) << "Fetching List of Posts..." << endl;
    QList<QVariant> args( d->defaultArgs( blogId() ) );
    args << QVariant( number );
    d->mXmlRpcClient->call(
      "blogger.getRecentPosts", args,
      d, SLOT( slotListRecentPostings( const QList<QVariant>&, const QVariant& ) ),
      d, SLOT( slotError( int, const QString&, const QVariant& ) ),
               QVariant( number ) );
}

void Blogger1::fetchPosting( KBlog::BlogPosting *posting )
{
     kDebug(5323) << "Fetching Posting with url " << posting->postingId()
         << endl;
     QList<QVariant> args( d->defaultArgs( posting->postingId() ) );
     d->callMap[ d->callCounter++ ] = posting;
     d->mXmlRpcClient->call(
       "blogger.getPost", args,
       d, SLOT( slotFetchPosting( const QList<QVariant>&, const QVariant& ) ),
       d, SLOT( slotError( int, const QString&, const QVariant& ) ),
                QVariant( d->callCounter ) );
}

void Blogger1::modifyPosting( KBlog::BlogPosting *posting )
{
  if ( !posting ) {
    kDebug(5323) << "Blogger1::modifyPosting: posting is null pointer"
        << endl;
  }
    kDebug(5323) << "Uploading Posting with postingId "
            << posting->postingId() << endl;

    QList<QVariant> args( d->defaultArgs( posting->postingId() ) );
    args << QVariant( posting->content() );
    args << QVariant( posting->isPublished() );
    d->mXmlRpcClient->call(
      "blogger.editPost", args,
      d, SLOT( slotModifyPosting( const QList<QVariant>&, const QVariant& ) ),
      d, SLOT( slotError( int, const QString&, const QVariant& ) ) );
}

void Blogger1::createPosting( KBlog::BlogPosting *posting )
{
  if ( !posting ) {
    kDebug(5323) << "Blogger1::createPosting: posting is null pointer"
        << endl;
  }
    kDebug(5323) << "Creating new Posting with blogid " << blogId() << endl;
    QList<QVariant> args( d->defaultArgs( blogId() ) );
    QStringList categories = posting->categories();
    QString content = "<title>" + posting->title() + "</title>";
    QStringList::const_iterator it;
    for ( it = categories.constBegin(); it != categories.constEnd(); ++it ) {
      content += "<category>" + *it + "</category>";
    }
    content += posting->content();
    args << QVariant( content );
    args << QVariant( posting->isPublished() );
    d->mXmlRpcClient->call(
      "blogger.newPost", args,
      d, SLOT( slotCreatePosting( const QList<QVariant>&, const QVariant& ) ),
      d, SLOT( slotError( int, const QString&, const QVariant& ) ) );
}

void Blogger1::removePosting( KBlog::BlogPosting *posting )
{
//   if ( d->mLock.tryLock() ) {
//     kDebug(5323) << "Blogger1::removePosting: postingId=" << postingId
//          << endl;
//     QList<QVariant> args( d->defaultArgs( postingId ) );
//     args << QVariant( /*publish=*/true );
//     d->mXmlRpcClient->call(
//       "blogger.deletePost", args,
//       d, SLOT( slotModifyPosting( QList<QVariant> &result, QVariant &id ) ),
//       d, SLOT( slotError( int, const QString&, const QVariant& ) ) );
//     return true;
//   }
//   return false;
}

#include "blogger.moc"
