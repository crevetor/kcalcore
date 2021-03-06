/*
 * This file is part of the syndication library
 *
 * Copyright (C) 2006 Frank Osterfeld <osterfeld@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef SYNDICATION_MAPPER_IMAGERSS2IMPL_H
#define SYNDICATION_MAPPER_IMAGERSS2IMPL_H

#include <image.h>
#include <rss2/image.h>

namespace Syndication {
    
class ImageRSS2Impl;
typedef boost::shared_ptr<ImageRSS2Impl> ImageRSS2ImplPtr;

/**
 *
 * @internal
 * @author Frank Osterfeld
 */
class ImageRSS2Impl : public Syndication::Image
{
    public:

        explicit ImageRSS2Impl(const Syndication::RSS2::Image& image);
        
        bool isNull() const;
        
        QString url() const;
        
        QString title() const;
        
        QString link() const;
        
        QString description() const;
        
        uint width() const;
        
        uint height() const;
        
    private:
        Syndication::RSS2::Image m_image;
};
    
} // namespace Syndication

#endif // SYNDICATION_MAPPER_IMAGERSS2IMPL_H
