// Copyright 2014 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/layout/fgas_linebreak.h"

#include <stddef.h>

#include <array>
#include <iterator>

#include "core/fxcrt/check.h"
#include "core/fxcrt/compiler_specific.h"
#include "core/fxcrt/fx_unicode.h"

namespace pdfium {

namespace {

// Short aliases used to keep the kLineBreakPairTable rows below readable.
constexpr LineBreakType kLbUn = LineBreakType::kUnknown;
constexpr LineBreakType kLbDb = LineBreakType::kDirectBreak;
constexpr LineBreakType kLbIb = LineBreakType::kIndirectBreak;
constexpr LineBreakType kLbCb = LineBreakType::kCommonIndirectBreak;
constexpr LineBreakType kLbCp = LineBreakType::kCommonProhibitedBreak;
constexpr LineBreakType kLbPb = LineBreakType::kProhibitedBreak;

using LineBreakPairRow = std::array<const LineBreakType, 38>;
constexpr std::array<const LineBreakPairRow, 38> kLineBreakPairTable = {{
    {kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb,
     kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbCp,
     kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbPb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbPb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbIb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbIb, kLbIb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbIb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbIb, kLbIb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbIb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbDb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbDb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbPb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbPb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbIb, kLbIb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbIb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbIb, kLbIb,
     kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbIb, kLbIb, kLbIb, kLbIb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbIb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbDb, kLbPb, kLbPb, kLbPb, kLbDb, kLbPb, kLbDb, kLbIb,
     kLbDb, kLbDb, kLbDb, kLbPb, kLbPb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbPb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
    {kLbDb, kLbPb, kLbIb, kLbDb, kLbIb, kLbPb, kLbPb, kLbPb, kLbDb, kLbDb,
     kLbDb, kLbDb, kLbDb, kLbDb, kLbIb, kLbIb, kLbDb, kLbDb, kLbPb, kLbCb,
     kLbPb, kLbDb, kLbDb, kLbDb, kLbDb, kLbDb, kLbUn, kLbUn, kLbUn, kLbUn,
     kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn, kLbUn},
}};

}  // namespace

LineBreakType GetLineBreakTypeFromPair(BreakProperty curr_char,
                                       BreakProperty next_char) {
  const size_t row = static_cast<size_t>(curr_char);
  const size_t col = static_cast<size_t>(next_char);
  return kLineBreakPairTable[row][col];
}

}  // namespace pdfium
