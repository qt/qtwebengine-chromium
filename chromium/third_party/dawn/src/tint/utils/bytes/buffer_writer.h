// Copyright 2024 The Dawn & Tint Authors
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

#ifndef SRC_TINT_UTILS_BYTES_BUFFER_WRITER_H_
#define SRC_TINT_UTILS_BYTES_BUFFER_WRITER_H_

#include <stdint.h>

#include "src/tint/utils/bytes/writer.h"

namespace tint::bytes {

/// BufferWriter is an implementation of the Writer interface backed by a buffer.
template <size_t N>
class BufferWriter final : public Writer {
  public:
    /// @copydoc Writer::Write
    Result<SuccessType> Write(const std::byte* in, size_t count) override {
        size_t at = buffer.Length();
        buffer.Resize(at + count);
        memcpy(&buffer[at], in, count);
        return Success;
    }

    /// @returns the buffer content as a string view
    std::string_view BufferString() const {
        if (buffer.IsEmpty()) {
            return "";
        }
        auto* data = reinterpret_cast<const char*>(&buffer[0]);
        static_assert(sizeof(std::byte) == sizeof(char), "length needs calculation");
        return std::string_view(data, buffer.Length());
    }

    /// The buffer holding the written data
    Vector<std::byte, N> buffer;
};

/// Deduction guide for BufferWriter
BufferWriter() -> BufferWriter<32>;

}  // namespace tint::bytes

#endif  // SRC_TINT_UTILS_BYTES_BUFFER_WRITER_H_
