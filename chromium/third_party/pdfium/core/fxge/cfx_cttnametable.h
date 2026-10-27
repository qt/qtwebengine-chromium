// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CORE_FXGE_CFX_CTTNAMETABLE_H_
#define CORE_FXGE_CFX_CTTNAMETABLE_H_

#include <stdint.h>

#include <vector>

#include "core/fxcrt/span.h"
#include "core/fxcrt/widestring.h"

class CFX_CTTNameTable {
 public:
  CFX_CTTNameTable();
  explicit CFX_CTTNameTable(pdfium::span<const uint8_t> name_table);
  CFX_CTTNameTable(CFX_CTTNameTable&&) noexcept;
  CFX_CTTNameTable& operator=(CFX_CTTNameTable&&) noexcept;
  ~CFX_CTTNameTable();

  pdfium::span<const WideString> GetFamilyNames() const;
  void AddFamilyName(const WideString& name);

 private:
  std::vector<WideString> family_names_;
};

#endif  // CORE_FXGE_CFX_CTTNAMETABLE_H_
