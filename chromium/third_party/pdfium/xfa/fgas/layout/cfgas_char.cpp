// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "xfa/fgas/layout/cfgas_char.h"

#include <algorithm>
#include <array>
#include <iterator>

#include "core/fxcrt/check.h"
#include "core/fxcrt/fx_extension.h"
#include "core/fxcrt/stl_util.h"

namespace {

#if DCHECK_IS_ON()
constexpr int32_t kBidiMaxLevel = 61;
#endif

// Weak/neutral actions pack two BidiClass values into the high and low
// nibbles of an integer.
constexpr int32_t kNibbleBits = 4;
constexpr int32_t kNibbleMask = 0x0F;

#undef PACK_NIBBLES
#define PACK_NIBBLES(hi, lo) \
  ((static_cast<uint32_t>(hi) << kNibbleBits) + static_cast<uint32_t>(lo))

enum FX_BIDIWEAKSTATE : uint8_t {
  FX_BWSxa = 0,
  FX_BWSxr,
  FX_BWSxl,
  FX_BWSao,
  FX_BWSro,
  FX_BWSlo,
  FX_BWSrt,
  FX_BWSlt,
  FX_BWScn,
  FX_BWSra,
  FX_BWSre,
  FX_BWSla,
  FX_BWSle,
  FX_BWSac,
  FX_BWSrc,
  FX_BWSrs,
  FX_BWSlc,
  FX_BWSls,
  FX_BWSret,
  FX_BWSlet
};

// NOTE: Range of BidiClass prevents encoding all possible values in this
// manner, but the ones used manage to fit. Except that I suspect that 0xF
// was intended to be used as a sentinel, even though it also means kRLE.
// TODO(tsepez): pick a better representation.
enum FX_BIDIWEAKACTION : uint16_t {
  FX_BWAIX = 0x100,
  FX_BWAXX = 0x0F,
  FX_BWAxxx = 0xFF,
  FX_BWAxIx = 0x100 + FX_BWAxxx,
  FX_BWAxxN = PACK_NIBBLES(0x0F, BidiClass::kON),
  FX_BWAxxE = PACK_NIBBLES(0x0F, BidiClass::kEN),
  FX_BWAxxA = PACK_NIBBLES(0x0F, BidiClass::kAN),
  FX_BWAxxR = PACK_NIBBLES(0x0F, BidiClass::kR),
  FX_BWAxxL = PACK_NIBBLES(0x0F, BidiClass::kL),
  FX_BWANxx = PACK_NIBBLES(BidiClass::kON, 0x0F),
  FX_BWAAxx = PACK_NIBBLES(BidiClass::kAN, 0x0F),
  FX_BWAExE = PACK_NIBBLES(BidiClass::kEN, BidiClass::kEN),
  FX_BWANIx = 0x100 + PACK_NIBBLES(BidiClass::kON, 0x0F),
  FX_BWANxN = PACK_NIBBLES(BidiClass::kON, BidiClass::kON),
  FX_BWANxR = PACK_NIBBLES(BidiClass::kON, BidiClass::kR),
  FX_BWANxE = PACK_NIBBLES(BidiClass::kON, BidiClass::kEN),
  FX_BWAAxA = PACK_NIBBLES(BidiClass::kAN, BidiClass::kAN),
  FX_BWANxL = PACK_NIBBLES(BidiClass::kON, BidiClass::kL),
  FX_BWALxL = PACK_NIBBLES(BidiClass::kL, BidiClass::kL),
  FX_BWAxIL = 0x100 + PACK_NIBBLES(0x0F, BidiClass::kL),
  FX_BWAAxR = PACK_NIBBLES(BidiClass::kAN, BidiClass::kR),
  FX_BWALxx = PACK_NIBBLES(BidiClass::kL, 0x0F),
};

enum FX_BIDINEUTRALSTATE : uint8_t {
  FX_BNSr = 0,
  FX_BNSl,
  FX_BNSrn,
  FX_BNSln,
  FX_BNSa,
  FX_BNSna
};

enum FX_BIDINEUTRALACTION : uint16_t {
  // For placeholders in table.
  FX_BNAZero = 0,

  // Other values.
  FX_BNAnL = PACK_NIBBLES(0, BidiClass::kL),
  FX_BNAEn = PACK_NIBBLES(BidiClass::kAN, 0),
  FX_BNARn = PACK_NIBBLES(BidiClass::kR, 0),
  FX_BNALn = PACK_NIBBLES(BidiClass::kL, 0),
  FX_BNAIn = FX_BWAIX,
  FX_BNALnL = PACK_NIBBLES(BidiClass::kL, BidiClass::kL),
};
#undef PACK_NIBBLES

constexpr auto kNTypes = std::to_array<const BidiClass>(
    {BidiClass::kN,   BidiClass::kL,   BidiClass::kR,   BidiClass::kAN,
     BidiClass::kEN,  BidiClass::kAL,  BidiClass::kNSM, BidiClass::kCS,
     BidiClass::kES,  BidiClass::kET,  BidiClass::kBN,  BidiClass::kBN,
     BidiClass::kN,   BidiClass::kB,   BidiClass::kRLO, BidiClass::kRLE,
     BidiClass::kLRO, BidiClass::kLRE, BidiClass::kPDF, BidiClass::kON});

using WeakStateRow = std::array<const FX_BIDIWEAKSTATE, 10>;
constexpr std::array<const WeakStateRow, 20> kWeakStateTable = {{
    {FX_BWSao, FX_BWSxl, FX_BWSxr, FX_BWScn, FX_BWScn, FX_BWSxa, FX_BWSxa,
     FX_BWSao, FX_BWSao, FX_BWSao},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSxr,
     FX_BWSro, FX_BWSro, FX_BWSrt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSxl,
     FX_BWSlo, FX_BWSlo, FX_BWSlt},
    {FX_BWSao, FX_BWSxl, FX_BWSxr, FX_BWScn, FX_BWScn, FX_BWSxa, FX_BWSao,
     FX_BWSao, FX_BWSao, FX_BWSao},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSro,
     FX_BWSro, FX_BWSro, FX_BWSrt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSlo,
     FX_BWSlo, FX_BWSlo, FX_BWSlt},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSrt,
     FX_BWSro, FX_BWSro, FX_BWSrt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSlt,
     FX_BWSlo, FX_BWSlo, FX_BWSlt},
    {FX_BWSao, FX_BWSxl, FX_BWSxr, FX_BWScn, FX_BWScn, FX_BWSxa, FX_BWScn,
     FX_BWSac, FX_BWSao, FX_BWSao},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSra,
     FX_BWSrc, FX_BWSro, FX_BWSrt},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSre,
     FX_BWSrs, FX_BWSrs, FX_BWSret},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSla,
     FX_BWSlc, FX_BWSlo, FX_BWSlt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSle,
     FX_BWSls, FX_BWSls, FX_BWSlet},
    {FX_BWSao, FX_BWSxl, FX_BWSxr, FX_BWScn, FX_BWScn, FX_BWSxa, FX_BWSao,
     FX_BWSao, FX_BWSao, FX_BWSao},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSro,
     FX_BWSro, FX_BWSro, FX_BWSrt},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSro,
     FX_BWSro, FX_BWSro, FX_BWSrt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSlo,
     FX_BWSlo, FX_BWSlo, FX_BWSlt},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSlo,
     FX_BWSlo, FX_BWSlo, FX_BWSlt},
    {FX_BWSro, FX_BWSxl, FX_BWSxr, FX_BWSra, FX_BWSre, FX_BWSxa, FX_BWSret,
     FX_BWSro, FX_BWSro, FX_BWSret},
    {FX_BWSlo, FX_BWSxl, FX_BWSxr, FX_BWSla, FX_BWSle, FX_BWSxa, FX_BWSlet,
     FX_BWSlo, FX_BWSlo, FX_BWSlet},
}};

using WeakActionRow = std::array<const FX_BIDIWEAKACTION, 10>;
constexpr std::array<const WeakActionRow, 20> kWeakActionTable = {{
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxA, FX_BWAxxR,
     FX_BWAxxR, FX_BWAxxN, FX_BWAxxN, FX_BWAxxN},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxE, FX_BWAxxR,
     FX_BWAxxR, FX_BWAxxN, FX_BWAxxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxL, FX_BWAxxR,
     FX_BWAxxL, FX_BWAxxN, FX_BWAxxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxA, FX_BWAxxR,
     FX_BWAxxN, FX_BWAxxN, FX_BWAxxN, FX_BWAxxN},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxE, FX_BWAxxR,
     FX_BWAxxN, FX_BWAxxN, FX_BWAxxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxL, FX_BWAxxR,
     FX_BWAxxN, FX_BWAxxN, FX_BWAxxN, FX_BWAxIx},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWAExE, FX_BWANxR,
     FX_BWAxIx, FX_BWANxN, FX_BWANxN, FX_BWAxIx},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWALxL, FX_BWANxR,
     FX_BWAxIx, FX_BWANxN, FX_BWANxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxA, FX_BWAxxR,
     FX_BWAxxA, FX_BWAxIx, FX_BWAxxN, FX_BWAxxN},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxE, FX_BWAxxR,
     FX_BWAxxA, FX_BWAxIx, FX_BWAxxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxE, FX_BWAxxR,
     FX_BWAxxE, FX_BWAxIx, FX_BWAxIx, FX_BWAxxE},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxL, FX_BWAxxR,
     FX_BWAxxA, FX_BWAxIx, FX_BWAxxN, FX_BWAxIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxL, FX_BWAxxR,
     FX_BWAxxL, FX_BWAxIx, FX_BWAxIx, FX_BWAxxL},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWAAxx, FX_BWAAxA, FX_BWANxR,
     FX_BWANxN, FX_BWANxN, FX_BWANxN, FX_BWANxN},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWAAxx, FX_BWANxE, FX_BWANxR,
     FX_BWANxN, FX_BWANxN, FX_BWANxN, FX_BWANIx},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWAExE, FX_BWANxR,
     FX_BWANxN, FX_BWANxN, FX_BWANxN, FX_BWANIx},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWAAxx, FX_BWANxL, FX_BWANxR,
     FX_BWANxN, FX_BWANxN, FX_BWANxN, FX_BWANIx},
    {FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWANxx, FX_BWALxL, FX_BWANxR,
     FX_BWANxN, FX_BWANxN, FX_BWANxN, FX_BWANIx},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxE, FX_BWAxxR,
     FX_BWAxxE, FX_BWAxxN, FX_BWAxxN, FX_BWAxxE},
    {FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxx, FX_BWAxxL, FX_BWAxxR,
     FX_BWAxxL, FX_BWAxxN, FX_BWAxxN, FX_BWAxxL},
}};

using NeutralStateRow = std::array<const FX_BIDINEUTRALSTATE, 5>;
constexpr std::array<const NeutralStateRow, 6> kNeutralStateTable = {{
    {FX_BNSrn, FX_BNSl, FX_BNSr, FX_BNSr, FX_BNSr},
    {FX_BNSln, FX_BNSl, FX_BNSr, FX_BNSa, FX_BNSl},
    {FX_BNSrn, FX_BNSl, FX_BNSr, FX_BNSr, FX_BNSr},
    {FX_BNSln, FX_BNSl, FX_BNSr, FX_BNSa, FX_BNSl},
    {FX_BNSna, FX_BNSl, FX_BNSr, FX_BNSa, FX_BNSl},
    {FX_BNSna, FX_BNSl, FX_BNSr, FX_BNSa, FX_BNSl},
}};

using NeutralActionRow = std::array<const FX_BIDINEUTRALACTION, 5>;
constexpr std::array<const NeutralActionRow, 6> kNeutralActionTable = {{
    {FX_BNAIn, FX_BNAZero, FX_BNAZero, FX_BNAZero, FX_BNAZero},
    {FX_BNAIn, FX_BNAZero, FX_BNAZero, FX_BNAZero, FX_BNAnL},
    {FX_BNAIn, FX_BNAEn, FX_BNARn, FX_BNARn, FX_BNARn},
    {FX_BNAIn, FX_BNALn, FX_BNAEn, FX_BNAEn, FX_BNALnL},
    {FX_BNAIn, FX_BNAZero, FX_BNAZero, FX_BNAZero, FX_BNAnL},
    {FX_BNAIn, FX_BNAEn, FX_BNARn, FX_BNARn, FX_BNAEn},
}};

using AddLevelRow = std::array<const uint8_t, 4>;
constexpr std::array<const AddLevelRow, 2> kAddLevelTable = {{
    {0, 1, 2, 2},
    {1, 0, 1, 1},
}};

BidiClass Direction(int32_t val) {
  return FX_IsOdd(val) ? BidiClass::kR : BidiClass::kL;
}

BidiClass GetDeferredType(int32_t val) {
  return static_cast<BidiClass>((val >> kNibbleBits) & kNibbleMask);
}

BidiClass GetResolvedType(int32_t val) {
  return static_cast<BidiClass>(val & kNibbleMask);
}

BidiClass GetDeferredNeutrals(int32_t iAction, int32_t iLevel) {
  BidiClass eClass = GetDeferredType(iAction);
  return eClass == BidiClass::kAN ? Direction(iLevel) : eClass;
}

BidiClass GetResolvedNeutrals(int32_t iAction) {
  return GetResolvedType(iAction);
}

FX_BIDIWEAKSTATE GetWeakState(FX_BIDIWEAKSTATE eState, BidiClass eClass) {
  return kWeakStateTable[static_cast<size_t>(eState)]
                        [static_cast<size_t>(eClass)];
}

FX_BIDIWEAKACTION GetWeakAction(FX_BIDIWEAKSTATE eState, BidiClass eClass) {
  return kWeakActionTable[static_cast<size_t>(eState)]
                         [static_cast<size_t>(eClass)];
}

FX_BIDINEUTRALSTATE GetNeutralState(FX_BIDINEUTRALSTATE eState,
                                    BidiClass eClass) {
  return kNeutralStateTable[static_cast<size_t>(eState)]
                           [static_cast<size_t>(eClass)];
}

FX_BIDINEUTRALACTION GetNeutralAction(FX_BIDINEUTRALSTATE eState,
                                      BidiClass eClass) {
  return kNeutralActionTable[static_cast<size_t>(eState)]
                            [static_cast<size_t>(eClass)];
}

void ReverseString(std::vector<CFGAS_Char>* chars,
                   size_t iStart,
                   size_t iCount) {
  DCHECK(fxcrt::IndexInBounds(*chars, iStart));
  DCHECK(iStart + iCount <= chars->size());

  std::reverse(chars->begin() + iStart, chars->begin() + iStart + iCount);
}

void SetDeferredRunClass(std::vector<CFGAS_Char>* chars,
                         size_t iStart,
                         size_t iCount,
                         BidiClass eValue) {
  DCHECK(iStart <= chars->size());
  DCHECK(iStart >= iCount);

  size_t iLast = iStart - iCount;
  for (size_t i = iStart; i > iLast; --i) {
    (*chars)[i - 1].bidi_class_ = eValue;
  }
}

void SetDeferredRunLevel(std::vector<CFGAS_Char>* chars,
                         size_t iStart,
                         size_t iCount,
                         int32_t iValue) {
  DCHECK(iStart <= chars->size());
  DCHECK(iStart >= iCount);

  size_t iLast = iStart - iCount;
  for (size_t i = iStart; i > iLast; --i) {
    (*chars)[i - 1].bidi_level_ = static_cast<int16_t>(iValue);
  }
}

void Classify(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount; ++i) {
    CFGAS_Char& cur = (*chars)[i];
    cur.bidi_class_ = pdfium::unicode::GetBidiClass(cur.char_code());
  }
}

void ClassifyWithTransform(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount; ++i) {
    CFGAS_Char& cur = (*chars)[i];
    cur.bidi_class_ = kNTypes[static_cast<size_t>(
        pdfium::unicode::GetBidiClass(cur.char_code()))];
  }
}

void ResolveExplicit(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount; ++i) {
    (*chars)[i].bidi_level_ = 0;
  }
}

void ResolveWeak(std::vector<CFGAS_Char>* chars, size_t iCount) {
  if (iCount <= 1) {
    return;
  }
  --iCount;

  int32_t iLevelCur = 0;
  size_t iNum = 0;
  FX_BIDIWEAKSTATE eState = FX_BWSxl;
  BidiClass eClsCur;
  BidiClass eClsRun;
  BidiClass eClsNew;
  size_t i = 0;
  for (; i <= iCount; ++i) {
    CFGAS_Char* pTC = &(*chars)[i];
    eClsCur = pTC->bidi_class_;
    if (eClsCur == BidiClass::kBN) {
      pTC->bidi_level_ = (int16_t)iLevelCur;
      if (i == iCount && iLevelCur != 0) {
        eClsCur = Direction(iLevelCur);
        pTC->bidi_class_ = eClsCur;
      } else if (i < iCount) {
        CFGAS_Char* pTCNext = &(*chars)[i + 1];
        eClsNew = pTCNext->bidi_class_;
        int32_t iLevelNext = pTCNext->bidi_level_;
        if (eClsNew != BidiClass::kBN && iLevelCur != iLevelNext) {
          int32_t iLevelNew = std::max(iLevelNext, iLevelCur);
          pTC->bidi_level_ = static_cast<int16_t>(iLevelNew);
          eClsCur = Direction(iLevelNew);
          pTC->bidi_class_ = eClsCur;
          iLevelCur = iLevelNext;
        } else {
          if (iNum > 0) {
            ++iNum;
          }
          continue;
        }
      } else {
        if (iNum > 0) {
          ++iNum;
        }
        continue;
      }
    }
    if (eClsCur > BidiClass::kBN) {
      continue;
    }

    FX_BIDIWEAKACTION eAction = GetWeakAction(eState, eClsCur);
    eClsRun = GetDeferredType(eAction);
    if (eClsRun != static_cast<BidiClass>(0x0F) && iNum > 0) {
      SetDeferredRunClass(chars, i, iNum, eClsRun);
      iNum = 0;
    }
    eClsNew = GetResolvedType(eAction);
    if (eClsNew != static_cast<BidiClass>(0x0F)) {
      pTC->bidi_class_ = eClsNew;
    }
    if (FX_BWAIX & eAction) {
      ++iNum;
    }

    eState = GetWeakState(eState, eClsCur);
  }
  if (iNum == 0) {
    return;
  }

  eClsCur = Direction(0);
  eClsRun = GetDeferredType(GetWeakAction(eState, eClsCur));
  if (eClsRun != static_cast<BidiClass>(0x0F)) {
    SetDeferredRunClass(chars, i, iNum, eClsRun);
  }
}

void ResolveNeutrals(std::vector<CFGAS_Char>* chars, size_t iCount) {
  if (iCount <= 1) {
    return;
  }
  --iCount;

  CFGAS_Char* pTC;
  int32_t iLevel = 0;
  size_t i = 0;
  size_t iNum = 0;
  FX_BIDINEUTRALSTATE eState = FX_BNSl;
  BidiClass eClsCur;
  BidiClass eClsRun;
  BidiClass eClsNew;
  for (; i <= iCount; ++i) {
    pTC = &(*chars)[i];
    eClsCur = pTC->bidi_class_;
    if (eClsCur == BidiClass::kBN) {
      if (iNum) {
        ++iNum;
      }
      continue;
    }
    if (eClsCur >= BidiClass::kAL) {
      continue;
    }

    FX_BIDINEUTRALACTION eAction = GetNeutralAction(eState, eClsCur);
    eClsRun = GetDeferredNeutrals(eAction, iLevel);
    if (eClsRun != BidiClass::kN && iNum > 0) {
      SetDeferredRunClass(chars, i, iNum, eClsRun);
      iNum = 0;
    }

    eClsNew = GetResolvedNeutrals(eAction);
    if (eClsNew != BidiClass::kN) {
      pTC->bidi_class_ = eClsNew;
    }
    if (FX_BNAIn & eAction) {
      ++iNum;
    }

    eState = GetNeutralState(eState, eClsCur);
    iLevel = pTC->bidi_level_;
  }
  if (iNum == 0) {
    return;
  }

  eClsCur = Direction(iLevel);
  eClsRun = GetDeferredNeutrals(GetNeutralAction(eState, eClsCur), iLevel);
  if (eClsRun != BidiClass::kN) {
    SetDeferredRunClass(chars, i, iNum, eClsRun);
  }
}

void ResolveImplicit(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount; ++i) {
    BidiClass eCls = (*chars)[i].bidi_class_;
    if (eCls == BidiClass::kBN || eCls <= BidiClass::kON ||
        eCls >= BidiClass::kAL) {
      continue;
    }
    (*chars)[i].bidi_level_ += kAddLevelTable[FX_IsOdd((*chars)[i].bidi_level_)]
                                             [static_cast<size_t>(eCls) - 1];
  }
}

void ResolveWhitespace(std::vector<CFGAS_Char>* chars, size_t iCount) {
  if (iCount <= 1) {
    return;
  }
  iCount--;

  int32_t iLevel = 0;
  size_t i = 0;
  size_t iNum = 0;
  for (; i <= iCount; ++i) {
    switch (static_cast<BidiClass>((*chars)[i].bidi_class_)) {
      case BidiClass::kWS:
        ++iNum;
        break;
      case BidiClass::kRLE:
      case BidiClass::kLRE:
      case BidiClass::kLRO:
      case BidiClass::kRLO:
      case BidiClass::kPDF:
      case BidiClass::kBN:
        (*chars)[i].bidi_level_ = static_cast<int16_t>(iLevel);
        ++iNum;
        break;
      case BidiClass::kS:
      case BidiClass::kB:
        if (iNum > 0) {
          SetDeferredRunLevel(chars, i, iNum, 0);
        }

        (*chars)[i].bidi_level_ = 0;
        iNum = 0;
        break;
      default:
        iNum = 0;
        break;
    }
    iLevel = (*chars)[i].bidi_level_;
  }
  if (iNum > 0) {
    SetDeferredRunLevel(chars, i, iNum, 0);
  }
}

size_t ReorderLevel(std::vector<CFGAS_Char>* chars,
                    size_t iCount,
                    int32_t iBaseLevel,
                    size_t iStart,
                    bool bReverse) {
  DCHECK(iBaseLevel >= 0);
  DCHECK(iBaseLevel <= kBidiMaxLevel);
  DCHECK(iStart < iCount);

  if (iCount < 1) {
    return 0;
  }

  bReverse = bReverse || FX_IsOdd(iBaseLevel);
  size_t i = iStart;
  for (; i < iCount; ++i) {
    int32_t iLevel = (*chars)[i].bidi_level_;
    if (iLevel == iBaseLevel) {
      continue;
    }
    if (iLevel < iBaseLevel) {
      break;
    }

    i += ReorderLevel(chars, iCount, iBaseLevel + 1, i, bReverse) - 1;
  }

  size_t iNum = i - iStart;
  if (bReverse && iNum > 1) {
    ReverseString(chars, iStart, iNum);
  }

  return iNum;
}

void Reorder(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount;) {
    i += ReorderLevel(chars, iCount, 0, i, false);
  }
}

void Position(std::vector<CFGAS_Char>* chars, size_t iCount) {
  for (size_t i = 0; i < iCount; ++i) {
    if ((*chars)[i].bidi_pos_ > iCount) {
      continue;
    }

    (*chars)[(*chars)[i].bidi_pos_].bidi_order_ = i;
  }
}

}  // namespace

// static
void CFGAS_Char::BidiLine(std::vector<CFGAS_Char>* chars, size_t iCount) {
  DCHECK(iCount <= chars->size());
  if (iCount < 2) {
    return;
  }

  ClassifyWithTransform(chars, iCount);
  ResolveExplicit(chars, iCount);
  ResolveWeak(chars, iCount);
  ResolveNeutrals(chars, iCount);
  ResolveImplicit(chars, iCount);
  Classify(chars, iCount);
  ResolveWhitespace(chars, iCount);
  Reorder(chars, iCount);
  Position(chars, iCount);
}

CFGAS_Char::CFGAS_Char(uint16_t wCharCode) : CFGAS_Char(wCharCode, 100, 100) {}

CFGAS_Char::CFGAS_Char(uint16_t wCharCode,
                       int32_t iHorizontalScale,
                       int32_t iVerticalScale)
    : char_code_(wCharCode),
      horizontal_scale_(iHorizontalScale),
      vertical_scale_(iVerticalScale) {}

CFGAS_Char::CFGAS_Char(const CFGAS_Char& other) = default;

CFGAS_Char::~CFGAS_Char() = default;

CharType CFGAS_Char::GetCharType() const {
  return pdfium::unicode::GetCharType(char_code_);
}
