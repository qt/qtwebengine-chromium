// Copyright 2022 Google LLC
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

//! Nearby Presence-specific usage of LDT.
#![no_std]

#[cfg(feature = "std")]
extern crate std;

#[cfg(test)]
mod np_adv_test_vectors;
#[cfg(test)]
mod tests;

use array_view::ArrayView;
use core::fmt;
use crypto_provider::{aes::BLOCK_SIZE, CryptoProvider};
use ldt::{LdtDecryptCipher, LdtEncryptCipher, LdtError, Mix, Padder, Swap, XorPadder};
use ldt_tbc::TweakableBlockCipher;
use np_hkdf::{legacy_ldt_expanded_salt, NpHmacSha256Key, NpKeySeedHkdf};
use xts_aes::XtsAes128;

/// Max LDT-XTS-AES data size: `(2 * AES block size) - 1`
pub const LDT_XTS_AES_MAX_LEN: usize = 31;

/// Legacy (v0) format uses a 14-byte metadata key
pub const NP_LEGACY_METADATA_KEY_LEN: usize = 14;

/// The salt included in an NP advertisement.
/// LDT does not use an IV but can instead incorporate the 2 byte, regularly rotated,
/// salt from the advertisement payload and XOR it with the padded tweak data.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct LegacySalt {
    /// Salt bytes extracted from the incoming NP advertisement
    bytes: [u8; 2],
}

impl LegacySalt {
    /// Returns the salt as a byte array.
    pub fn bytes(&self) -> &[u8; 2] {
        &self.bytes
    }
}

impl From<[u8; 2]> for LegacySalt {
    fn from(arr: [u8; 2]) -> Self {
        Self { bytes: arr }
    }
}

/// [LdtEncryptCipher] parameterized for XTS-AES-128 with the [Swap] mix function.
pub type LdtEncrypterXtsAes128<C> = LdtEncryptCipher<{ BLOCK_SIZE }, XtsAes128<C>, Swap>;

/// A Nearby Presence specific LDT decrypter which verifies the hmac tag of the given payload
/// parameterized for XTS-AES-128 with the [Swap] mix function.
pub type LdtNpAdvDecrypterXtsAes128<C> =
    LdtNpAdvDecrypter<{ BLOCK_SIZE }, LDT_XTS_AES_MAX_LEN, XtsAes128<C>, Swap, C>;

/// Build a Nearby Presence specific LDT XTS-AES-128 decrypter from a provided [NpKeySeedHkdf] and
/// metadata_key_hmac, with the [Swap] mix function
pub fn build_np_adv_decrypter_from_key_seed<C: CryptoProvider>(
    key_seed: &NpKeySeedHkdf<C>,
    metadata_key_tag: [u8; 32],
) -> LdtNpAdvDecrypterXtsAes128<C> {
    build_np_adv_decrypter(
        &key_seed.legacy_ldt_key(),
        metadata_key_tag,
        key_seed.legacy_metadata_key_hmac_key(),
    )
}

/// Build a Nearby Presence specific LDT XTS-AES-128 decrypter from precalculated cipher components,
/// with the [Swap] mix function
pub fn build_np_adv_decrypter<C: CryptoProvider>(
    ldt_key: &ldt::LdtKey<xts_aes::XtsAes128Key>,
    metadata_key_tag: [u8; 32],
    metadata_key_hmac_key: NpHmacSha256Key<C>,
) -> LdtNpAdvDecrypterXtsAes128<C> {
    LdtNpAdvDecrypter {
        ldt_decrypter: LdtXtsAes128Decrypter::<C>::new(ldt_key),
        metadata_key_tag,
        metadata_key_hmac_key,
    }
}

// [LdtDecryptCipher] parameterized for XTS-AES-128 with the [Swap] mix function.
type LdtXtsAes128Decrypter<C> = LdtDecryptCipher<{ BLOCK_SIZE }, XtsAes128<C>, Swap>;

/// Decrypts and validates a NP legacy format advertisement encrypted with LDT.
///
/// A NP legacy advertisement will always be in the format of:
///
/// Header (1 byte) | Identity DE header (1 byte) | Salt (2 bytes) | Identity (14 bytes) | repeated
/// { DE header | DE payload }
///
/// Example:
/// Header (1 byte) | Identity DE header (1 byte) | Salt (2 bytes) | Identity (14 bytes) |
/// Tx power DE header (1 byte) | Tx power (1 byte) | Action DE header(1 byte) | action (1-3 bytes)
///
/// The ciphertext bytes will always start with the Identity through the end of the
/// advertisement, for example in the above [ Identity (14 bytes) | Tx power DE header (1 byte) |
/// Tx power (1 byte) | Action DE header(1 byte) | action (1-3 bytes) ] will be the ciphertext section
/// passed as the input to `decrypt_and_verify`
///
/// `B` is the underlying block cipher block size.
/// `O` is the max output size (must be 2 * B - 1).
/// `T` is the tweakable block cipher used by LDT.
/// `M` is the mix function used by LDT.
pub struct LdtNpAdvDecrypter<
    const B: usize,
    const O: usize,
    T: TweakableBlockCipher<B>,
    M: Mix,
    C: CryptoProvider,
> {
    ldt_decrypter: LdtDecryptCipher<B, T, M>,
    metadata_key_tag: [u8; 32],
    metadata_key_hmac_key: NpHmacSha256Key<C>,
}

impl<const B: usize, const O: usize, T, M, C> LdtNpAdvDecrypter<B, O, T, M, C>
where
    T: TweakableBlockCipher<B>,
    M: Mix,
    C: CryptoProvider,
{
    /// Decrypt an advertisement payload using the provided padder.
    ///
    /// If the plaintext's metadata key matches this item's MAC, return the plaintext, otherwise `None`.
    ///
    /// NOTE: because LDT acts as a PRP over the entire message, tampering with any bit scrambles
    /// the whole message, so we can leverage the MAC on just the metadata key to ensure integrity
    /// for the whole message.
    ///
    /// # Errors
    /// - If `payload` has a length outside of `[B, B * 2)`.
    /// - If the decrypted plaintext fails its HMAC validation
    pub fn decrypt_and_verify<P: Padder<B, T>>(
        &self,
        payload: &[u8],
        padder: &P,
    ) -> Result<ArrayView<u8, O>, LdtAdvDecryptError> {
        assert_eq!(B * 2 - 1, O); // should be compiled away

        // have to check length before passing to LDT to ensure copying into the buffer is safe
        if payload.len() < B || payload.len() > O {
            return Err(LdtAdvDecryptError::InvalidLength(payload.len()));
        }

        // we copy to avoid exposing plaintext that hasn't been validated w/ hmac
        let mut buffer = [0_u8; O];
        buffer[..payload.len()].copy_from_slice(payload);

        #[allow(clippy::expect_used)]
        self.ldt_decrypter
            .decrypt(&mut buffer[..payload.len()], padder)
            .map_err(|e| match e {
                LdtError::InvalidLength(l) => LdtAdvDecryptError::InvalidLength(l),
            })
            .and_then(|_| {
                self.metadata_key_hmac_key
                    .verify_hmac(&buffer[..NP_LEGACY_METADATA_KEY_LEN], self.metadata_key_tag)
                    .map_err(|_| LdtAdvDecryptError::MacMismatch)
                    .map(|_| {
                        ArrayView::try_from_array(buffer, payload.len())
                            .expect("this will never be hit because the length is validated above")
                    })
            })
    }
}

/// Errors that can occur during [LdtNpAdvDecrypter::decrypt_and_verify].
#[derive(Debug, PartialEq, Eq)]
pub enum LdtAdvDecryptError {
    /// The ciphertext data was an invalid length.
    InvalidLength(usize),
    /// The MAC calculated from the plaintext did not match the expected value
    MacMismatch,
}

impl fmt::Display for LdtAdvDecryptError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LdtAdvDecryptError::InvalidLength(len) => {
                write!(f, "Adv decrypt error: invalid length ({len})")
            }
            LdtAdvDecryptError::MacMismatch => write!(f, "Adv decrypt error: MAC mismatch"),
        }
    }
}
/// Build a XorPadder by HKDFing the NP advertisement salt
pub fn salt_padder<const B: usize, C: CryptoProvider>(salt: LegacySalt) -> XorPadder<{ B }> {
    // Assuming that the tweak size == the block size here, which it is for XTS.
    // If that's ever not true, yet another generic parameter will address that.
    XorPadder::from(legacy_ldt_expanded_salt::<B, C>(&salt.bytes))
}
