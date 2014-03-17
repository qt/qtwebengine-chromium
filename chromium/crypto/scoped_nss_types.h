// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRYPTO_SCOPED_NSS_TYPES_H_
#define CRYPTO_SCOPED_NSS_TYPES_H_

#include <keyhi.h>
#include <nss.h>
#include <pk11pub.h>
#include <plarena.h>

#include "base/memory/scoped_ptr.h"

namespace crypto {

template <typename Type, void (*Destroyer)(Type*)>
struct NSSDestroyer {
  void operator()(Type* ptr) const {
    if (ptr)
      Destroyer(ptr);
  }
};

template <typename Type, void (*Destroyer)(Type*, PRBool), PRBool freeit>
struct NSSDestroyer1 {
  void operator()(Type* ptr) const {
    if (ptr)
      Destroyer(ptr, freeit);
  }
};

// Define some convenient scopers around NSS pointers.
typedef scoped_ptr_malloc<
    PK11Context, NSSDestroyer1<PK11Context,
                               PK11_DestroyContext,
                               PR_TRUE> > ScopedPK11Context;
typedef scoped_ptr_malloc<
    PK11SlotInfo, NSSDestroyer<PK11SlotInfo, PK11_FreeSlot> > ScopedPK11Slot;
typedef scoped_ptr_malloc<
    PK11SlotList, NSSDestroyer<PK11SlotList,
                               PK11_FreeSlotList> > ScopedPK11SlotList;
typedef scoped_ptr_malloc<
    PK11SymKey, NSSDestroyer<PK11SymKey, PK11_FreeSymKey> > ScopedPK11SymKey;
typedef scoped_ptr_malloc<
    SECKEYPublicKey, NSSDestroyer<SECKEYPublicKey, SECKEY_DestroyPublicKey> >
    ScopedSECKEYPublicKey;
typedef scoped_ptr_malloc<
    SECKEYPrivateKey, NSSDestroyer<SECKEYPrivateKey, SECKEY_DestroyPrivateKey> >
    ScopedSECKEYPrivateKey;
typedef scoped_ptr_malloc<
    SECAlgorithmID, NSSDestroyer1<SECAlgorithmID,
                                  SECOID_DestroyAlgorithmID,
                                  PR_TRUE> > ScopedSECAlgorithmID;
typedef scoped_ptr_malloc<
    SECItem, NSSDestroyer1<SECItem,
                           SECITEM_FreeItem,
                           PR_TRUE> > ScopedSECItem;
typedef scoped_ptr_malloc<
    PLArenaPool, NSSDestroyer1<PLArenaPool,
                               PORT_FreeArena,
                               PR_FALSE> > ScopedPLArenaPool;

}  // namespace crypto

#endif  // CRYPTO_SCOPED_NSS_TYPES_H_
