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

#ifndef WebCrypto_h
#define WebCrypto_h

#include "WebCommon.h"
#include "WebCryptoKey.h"
#include "WebPrivatePtr.h"

namespace WebCore { class CryptoResult; }

#if BLINK_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace blink {

class WebArrayBuffer;

class WebCryptoResult {
public:
    WebCryptoResult(const WebCryptoResult& o)
    {
        assign(o);
    }

    ~WebCryptoResult()
    {
        reset();
    }

    WebCryptoResult& operator=(const WebCryptoResult& o)
    {
        assign(o);
        return *this;
    }

    BLINK_EXPORT void completeWithError();
    BLINK_EXPORT void completeWithBuffer(const WebArrayBuffer&);
    BLINK_EXPORT void completeWithBuffer(const void*, unsigned);
    BLINK_EXPORT void completeWithBoolean(bool);
    BLINK_EXPORT void completeWithKey(const WebCryptoKey&);
    BLINK_EXPORT void completeWithKeyPair(const WebCryptoKey& publicKey, const WebCryptoKey& privateKey);

#if BLINK_IMPLEMENTATION
    explicit WebCryptoResult(const WTF::PassRefPtr<WebCore::CryptoResult>&);
#endif

private:
    BLINK_EXPORT void reset();
    BLINK_EXPORT void assign(const WebCryptoResult&);

    WebPrivatePtr<WebCore::CryptoResult> m_impl;
};

class WebCrypto {
public:
    // Starts a one-shot cryptographic operation which can complete either
    // synchronously, or asynchronously.
    //
    // Let the WebCryptoResult be called "result".
    //
    // The result should be set exactly once, from the same thread which
    // initiated the operation.
    //
    //   * WebCryptoAlgorithms parameters are guaranteed to be !isNull(),
    //     unless noted otherwise.
    //   * WebCryptoKey parameters are guaranteeed to be !isNull().
    //   * const unsigned char* data buffers are not valid after return.
    virtual void encrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void decrypt(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void sign(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void verifySignature(const WebCryptoAlgorithm&, const WebCryptoKey&, const unsigned char* signature, unsigned signatureSize, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void digest(const WebCryptoAlgorithm&, const unsigned char* data, unsigned dataSize, WebCryptoResult result) { result.completeWithError(); }
    virtual void generateKey(const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }
    // The WebCryptoAlgorithm for importKey may be "isNull()" meaning that it
    // was unspecified by the caller.
    virtual void importKey(WebCryptoKeyFormat, const unsigned char* keyData, unsigned keyDataSize, const WebCryptoAlgorithm&, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }
    virtual void exportKey(WebCryptoKeyFormat, const WebCryptoKey&, WebCryptoResult result) { result.completeWithError(); }

    virtual void wrapKey(WebCryptoKeyFormat, const WebCryptoKey& key, const WebCryptoKey& wrappingKey, const WebCryptoAlgorithm&, WebCryptoResult result) { result.completeWithError(); }

    // It is possible for unwrappedKeyAlgorithm.isNull() meaning that it was
    // unspecified by the caller.
    virtual void unwrapKey(WebCryptoKeyFormat, const unsigned char* wrappedKey, unsigned wrappedKeySize, const WebCryptoKey&, const WebCryptoAlgorithm& unwrapAlgorithm, const WebCryptoAlgorithm& unwrappedKeyAlgorithm, bool extractable, WebCryptoKeyUsageMask, WebCryptoResult result) { result.completeWithError(); }

protected:
    virtual ~WebCrypto() { }
};

} // namespace blink

#endif
