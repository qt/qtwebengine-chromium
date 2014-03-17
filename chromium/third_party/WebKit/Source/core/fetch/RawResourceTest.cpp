/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/fetch/RawResource.h"

#include "core/fetch/ImageResourceClient.h"
#include "core/fetch/MemoryCache.h"
#include "core/fetch/MockImageResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/ResourcePtr.h"
#include "core/loader/DocumentLoader.h"
#include "core/testing/DummyPageHolder.h"
#include "core/testing/UnitTestHelpers.h"
#include "platform/SharedBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLResponse.h"
#include "public/platform/WebUnitTestSupport.h"

using namespace WebCore;

namespace {

TEST(RawResourceTest, DontIgnoreAcceptForCacheReuse)
{
    ResourceRequest jpegRequest;
    jpegRequest.setHTTPAccept("image/jpeg");

    RawResource jpegResource(jpegRequest, Resource::Raw);

    ResourceRequest pngRequest;
    pngRequest.setHTTPAccept("image/png");

    ASSERT_FALSE(jpegResource.canReuse(pngRequest));
}

TEST(RawResourceTest, RevalidationSucceeded)
{
    // Create two RawResources and set one to revalidate the other.
    RawResource* oldResourcePointer = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    RawResource* newResourcePointer = new RawResource(ResourceRequest("data:text/html,"), Resource::Raw);
    newResourcePointer->setResourceToRevalidate(oldResourcePointer);
    ResourcePtr<Resource> oldResource = oldResourcePointer;
    ResourcePtr<Resource> newResource = newResourcePointer;
    memoryCache()->add(oldResource.get());
    memoryCache()->remove(oldResource.get());
    memoryCache()->add(newResource.get());

    // Simulate a successful revalidation.
    // The revalidated resource (oldResource) should now be in the cache, newResource
    // should have been sliently switched to point to the revalidated resource, and
    // we shouldn't hit any ASSERTs.
    ResourceResponse response;
    response.setHTTPStatusCode(304);
    newResource->responseReceived(response);
    EXPECT_EQ(memoryCache()->resourceForURL(KURL(ParsedURLString, "data:text/html,")), oldResource.get());
    EXPECT_EQ(oldResource.get(), newResource.get());
    EXPECT_NE(newResource.get(), newResourcePointer);
}

} // namespace
