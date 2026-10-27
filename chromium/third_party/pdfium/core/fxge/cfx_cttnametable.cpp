// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxge/cfx_cttnametable.h"

#include "core/fxcrt/byteorder.h"

namespace {

uint16_t ReadUInt16FromSpanAtOffset(pdfium::span<const uint8_t> data,
                                    size_t offset) {
  return fxcrt::GetUInt16MSBFirst(data.subspan(offset).first<2u>());
}

}  // namespace

CFX_CTTNameTable::CFX_CTTNameTable(pdfium::span<const uint8_t> name_table) {
  if (name_table.empty()) {
    return;
  }

  uint16_t nNameCount = ReadUInt16FromSpanAtOffset(name_table, 2);
  pdfium::span<const uint8_t> str =
      name_table.subspan(ReadUInt16FromSpanAtOffset(name_table, 4));
  pdfium::span<const uint8_t> name_record = name_table.subspan<6u>();
  for (uint16_t i = 0; i < nNameCount; ++i) {
    uint16_t nNameID = ReadUInt16FromSpanAtOffset(name_table, i * 12 + 6);
    if (nNameID != 1) {
      continue;
    }

    uint16_t nPlatformID = ReadUInt16FromSpanAtOffset(name_record, i * 12);
    uint16_t nNameLength = ReadUInt16FromSpanAtOffset(name_record, i * 12 + 8);
    uint16_t nNameOffset = ReadUInt16FromSpanAtOffset(name_record, i * 12 + 10);
    if (nPlatformID != 1) {
      WideString wsFamily;
      for (uint16_t j = 0; j < nNameLength / 2; ++j) {
        wchar_t wcTemp = ReadUInt16FromSpanAtOffset(str, nNameOffset + j * 2);
        wsFamily += wcTemp;
      }
      family_names_.push_back(wsFamily);
      continue;
    }

    // Avoid out of bounds crashes if the length and/or offset are wrong.
    if (static_cast<size_t>(nNameLength) + nNameOffset >= str.size()) {
      continue;
    }

    WideString wsFamily;
    for (uint16_t j = 0; j < nNameLength; ++j) {
      wchar_t wcTemp = str[nNameOffset + j];
      wsFamily += wcTemp;
    }
    family_names_.push_back(wsFamily);
  }
}

CFX_CTTNameTable::CFX_CTTNameTable() = default;

CFX_CTTNameTable::CFX_CTTNameTable(CFX_CTTNameTable&&) noexcept = default;

CFX_CTTNameTable& CFX_CTTNameTable::operator=(CFX_CTTNameTable&&) noexcept =
    default;

CFX_CTTNameTable::~CFX_CTTNameTable() = default;

pdfium::span<const WideString> CFX_CTTNameTable::GetFamilyNames() const {
  return family_names_;
}

void CFX_CTTNameTable::AddFamilyName(const WideString& name) {
  family_names_.push_back(name);
}
