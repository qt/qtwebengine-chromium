// Copyright 2023 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_UTILS_BYTES_READER_H_
#define SRC_TINT_UTILS_BYTES_READER_H_

#include <algorithm>
#include <cstdint>
#include <string>

#include "src/tint/utils/bytes/endianness.h"
#include "src/tint/utils/bytes/swap.h"
#include "src/tint/utils/containers/slice.h"
#include "src/tint/utils/result/result.h"

namespace tint::bytes {

/// A binary stream reader interface
class Reader {
  public:
    /// Destructor
    virtual ~Reader();

    /// Read reads bytes from the stream, blocking until there are @p count bytes available, or the
    /// end of the stream has been reached.
    /// @param out a pointer to the byte buffer that will be filled with the read data. Must be at
    /// least @p count size.
    /// @param count the number of bytes to read. Must be greater than 0.
    /// @returns the number of bytes read from the stream. If Read() returns less than @p count,
    /// then the end of the stream has been reached.
    virtual size_t Read(std::byte* out, size_t count) = 0;

    /// Reads an integer from the stream, performing byte swapping if the stream's endianness
    /// differs from the native endianness.
    /// If there are too few bytes remaining in the stream, then a failure is returned.
    /// @param endianness the encoded endianness of the integer
    /// @return the deserialized integer
    template <typename T>
    Result<T> Int(Endianness endianness = Endianness::kLittle) {
        static_assert(std::is_integral_v<T>);
        T out = 0;
        if (size_t n = Read(reinterpret_cast<std::byte*>(&out), sizeof(T)); n != sizeof(T)) {
            return Failure{"EOF"};
        }
        if (NativeEndianness() != endianness) {
            out = Swap(out);
        }
        return out;
    }

    /// Reads a float from the stream.
    /// If there are too few bytes remaining in the stream, then a failure is returned.
    /// @return the deserialized floating point number
    template <typename T>
    Result<T> Float() {
        static_assert(std::is_floating_point_v<T>);
        T out = 0;
        if (size_t n = Read(reinterpret_cast<std::byte*>(&out), sizeof(T)); n != sizeof(T)) {
            return Failure{"EOF"};
        }
        return out;
    }

    /// Reads a boolean from the stream
    /// If there are too few bytes remaining in the stream, then a failure is returned.
    /// @returns true if the next byte is non-zero
    Result<bool> Bool() {
        std::byte b{0};
        if (size_t n = Read(&b, 1); n != 1) {
            return Failure{"EOF"};
        }
        return b != std::byte{0};
    }

    /// Reads a string of @p len bytes from the stream.
    /// If there are too few bytes remaining in the stream, then a failure is returned.
    /// @param len the length of the returned string in bytes
    /// @return the deserialized string
    Result<std::string> String(size_t len) {
        std::string out;
        out.resize(len);
        if (size_t n = Read(reinterpret_cast<std::byte*>(out.data()), sizeof(char) * len);
            n != len) {
            return Failure{"EOF"};
        }
        return out;
    }
};

/// BufferReader is an implementation of the Reader interface backed by a buffer.
class BufferReader final : public Reader {
  public:
    // Destructor
    ~BufferReader() override;

    /// Constructor
    /// @param data the data to read from
    /// @param size the number of bytes in the buffer
    BufferReader(const std::byte* data, size_t size) : data_(data), bytes_remaining_(size) {
        TINT_ASSERT(data);
    }

    /// Constructor
    /// @param string the string to read from
    explicit BufferReader(std::string_view string)
        : data_(reinterpret_cast<const std::byte*>(string.data())),
          bytes_remaining_(string.length()) {}

    /// Constructor
    /// @param slice the byte slice to read from
    explicit BufferReader(Slice<const std::byte> slice)
        : data_(slice.data), bytes_remaining_(slice.len) {
        TINT_ASSERT(slice.data);
    }

    /// @copydoc Reader::Read
    size_t Read(std::byte* out, size_t count) override;

  private:
    /// The data to read from
    const std::byte* data_ = nullptr;

    /// The number of bytes remaining
    size_t bytes_remaining_ = 0;
};

}  // namespace tint::bytes

#endif  // SRC_TINT_UTILS_BYTES_READER_H_
