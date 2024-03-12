// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#[cfg(feature = "alloc")]
extern crate alloc;
#[cfg(feature = "alloc")]
use aead::Payload;
#[cfg(feature = "alloc")]
use alloc::vec::Vec;

// RustCrypto defined traits and types
use aes::cipher::typenum::consts::U16;
use aes::cipher::BlockCipher;
use aes::cipher::BlockEncrypt;
#[cfg(feature = "alloc")]
use aes_gcm_siv::aead::Aead as _;
use aes_gcm_siv::aead::KeyInit;
use aes_gcm_siv::AeadInPlace as _;

// CryptoProvider traits and types
use crypto_provider::aead::{Aead, AeadError, AeadInit};

pub struct AesGcmSiv<A: BlockCipher<BlockSize = U16> + BlockEncrypt + KeyInit>(
    aes_gcm_siv::AesGcmSiv<A>,
);

impl<A: BlockCipher<BlockSize = U16> + BlockEncrypt + KeyInit> crypto_provider::aead::AesGcmSiv
    for AesGcmSiv<A>
{
}

impl<K: crypto_provider::aes::AesKey, A: BlockCipher<BlockSize = U16> + BlockEncrypt + KeyInit>
    AeadInit<K> for AesGcmSiv<A>
{
    fn new(key: &K) -> Self {
        Self(aes_gcm_siv::AesGcmSiv::<A>::new(key.as_slice().into()))
    }
}

impl<A: aes::cipher::BlockCipher<BlockSize = U16> + BlockEncrypt + KeyInit> Aead for AesGcmSiv<A> {
    const TAG_SIZE: usize = 16;
    type Nonce = [u8; 12];
    type Tag = [u8; 16];

    #[cfg(feature = "alloc")]
    fn encrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0
            .encrypt(aes_gcm_siv::Nonce::from_slice(nonce), Payload { msg, aad })
            .map_err(|_| AeadError)
    }

    fn encrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
    ) -> Result<Self::Tag, AeadError> {
        self.0
            .encrypt_in_place_detached(aes_gcm_siv::Nonce::from_slice(nonce), aad, msg)
            .map(|arr| arr.into())
            .map_err(|_| AeadError)
    }

    #[cfg(feature = "alloc")]
    fn decrypt(&self, msg: &[u8], aad: &[u8], nonce: &Self::Nonce) -> Result<Vec<u8>, AeadError> {
        self.0
            .decrypt(aes_gcm_siv::Nonce::from_slice(nonce), Payload { msg, aad })
            .map_err(|_| AeadError)
    }

    fn decrypt_detached(
        &self,
        msg: &mut [u8],
        aad: &[u8],
        nonce: &Self::Nonce,
        tag: &Self::Tag,
    ) -> Result<(), AeadError> {
        self.0
            .decrypt_in_place_detached(aes_gcm_siv::Nonce::from_slice(nonce), aad, msg, tag.into())
            .map_err(|_| AeadError)
    }
}

#[cfg(test)]
mod tests {
    use core::marker::PhantomData;
    use crypto_provider_test::aead::aes_gcm_siv::*;
    use crypto_provider_test::aes::*;
    use crypto_provider_test::prelude::apply;

    #[apply(aes_128_gcm_siv_test_cases)]
    fn aes_gcm_siv_128_test(
        testcase: CryptoProviderTestCase<crate::aead::aes_gcm_siv::AesGcmSiv<aes::Aes128>>,
    ) {
        testcase(PhantomData);
    }

    #[apply(aes_128_gcm_siv_test_cases_detached)]
    fn aes_gcm_siv_128_test_detached(
        testcase: CryptoProviderTestCase<crate::aead::aes_gcm_siv::AesGcmSiv<aes::Aes128>>,
    ) {
        testcase(PhantomData);
    }

    #[apply(aes_256_gcm_siv_test_cases)]
    fn aes_gcm_siv_256_test(
        testcase: CryptoProviderTestCase<crate::aead::aes_gcm_siv::AesGcmSiv<aes::Aes256>>,
    ) {
        testcase(PhantomData);
    }

    #[apply(aes_256_gcm_siv_test_cases_detached)]
    fn aes_gcm_siv_256_test_detached(
        testcase: CryptoProviderTestCase<crate::aead::aes_gcm_siv::AesGcmSiv<aes::Aes256>>,
    ) {
        testcase(PhantomData);
    }
}
