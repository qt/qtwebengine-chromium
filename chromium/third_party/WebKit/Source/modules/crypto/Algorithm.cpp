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
#include "modules/crypto/Algorithm.h"

#include "modules/crypto/AesCbcParams.h"
#include "modules/crypto/AesCtrParams.h"
#include "modules/crypto/AesKeyGenParams.h"
#include "modules/crypto/HmacKeyParams.h"
#include "modules/crypto/HmacParams.h"
#include "modules/crypto/NormalizeAlgorithm.h"
#include "modules/crypto/RsaKeyGenParams.h"
#include "modules/crypto/RsaSsaParams.h"
#include "platform/NotImplemented.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

PassRefPtr<Algorithm> Algorithm::create(const blink::WebCryptoAlgorithm& algorithm)
{
    switch (algorithm.paramsType()) {
    case blink::WebCryptoAlgorithmParamsTypeNone:
        return adoptRef(new Algorithm(algorithm));
    case blink::WebCryptoAlgorithmParamsTypeAesCbcParams:
        return AesCbcParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeAesKeyGenParams:
        return AesKeyGenParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeHmacParams:
        return HmacParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeHmacKeyParams:
        return HmacKeyParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeRsaSsaParams:
        return RsaSsaParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeRsaKeyGenParams:
        return RsaKeyGenParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeAesCtrParams:
        return AesCtrParams::create(algorithm);
    case blink::WebCryptoAlgorithmParamsTypeAesGcmParams:
    case blink::WebCryptoAlgorithmParamsTypeRsaOaepParams:
        // TODO
        notImplemented();
        break;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

Algorithm::Algorithm(const blink::WebCryptoAlgorithm& algorithm)
    : m_algorithm(algorithm)
{
    ScriptWrappable::init(this);
}

String Algorithm::name()
{
    return algorithmIdToName(m_algorithm.id());
}

} // namespace WebCore
