/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
#include "modules/crypto/CryptoResultImpl.h"

#include "bindings/v8/ScriptPromiseResolver.h"
#include "modules/crypto/Key.h"
#include "modules/crypto/KeyPair.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "public/platform/Platform.h"
#include "public/platform/WebArrayBuffer.h"
#include "public/platform/WebCryptoAlgorithm.h"
#include "wtf/ArrayBufferView.h"

namespace WebCore {

CryptoResultImpl::~CryptoResultImpl()
{
    ASSERT(m_finished);
}

PassRefPtr<CryptoResultImpl> CryptoResultImpl::create(ScriptPromise promise)
{
    return adoptRef(new CryptoResultImpl(promise));
}

void CryptoResultImpl::completeWithError()
{
    m_promiseResolver->reject(ScriptValue::createNull());
    finish();
}

void CryptoResultImpl::completeWithBuffer(const blink::WebArrayBuffer& buffer)
{
    m_promiseResolver->resolve(PassRefPtr<ArrayBuffer>(buffer));
    finish();
}

void CryptoResultImpl::completeWithBoolean(bool b)
{
    m_promiseResolver->resolve(ScriptValue::createBoolean(b));
    finish();
}

void CryptoResultImpl::completeWithKey(const blink::WebCryptoKey& key)
{
    m_promiseResolver->resolve(Key::create(key));
    finish();
}

void CryptoResultImpl::completeWithKeyPair(const blink::WebCryptoKey& publicKey, const blink::WebCryptoKey& privateKey)
{
    m_promiseResolver->resolve(KeyPair::create(publicKey, privateKey));
    finish();
}

CryptoResultImpl::CryptoResultImpl(ScriptPromise promise)
    : m_promiseResolver(ScriptPromiseResolver::create(promise))
    , m_finished(false) { }

void CryptoResultImpl::finish()
{
    ASSERT(!m_finished);
    m_finished = true;
}

} // namespace WebCore
