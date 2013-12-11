// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/x/selection_utils.h"

#include <set>

#include "base/i18n/icu_string_conversions.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/base/x/x11_util.h"

namespace ui {

const char kMimeTypeMozillaURL[] = "text/x-moz-url";
const char kString[] = "STRING";
const char kText[] = "TEXT";
const char kUtf8String[] = "UTF8_STRING";

const char* kSelectionDataAtoms[] = {
  Clipboard::kMimeTypeHTML,
  kString,
  kText,
  kUtf8String,
  NULL
};

std::vector< ::Atom> GetTextAtomsFrom(const X11AtomCache* atom_cache) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache->GetAtom(kString));
  atoms.push_back(atom_cache->GetAtom(kText));
  atoms.push_back(atom_cache->GetAtom(kUtf8String));
  return atoms;
}

std::vector< ::Atom> GetURLAtomsFrom(const X11AtomCache* atom_cache) {
  std::vector< ::Atom> atoms;
  atoms.push_back(atom_cache->GetAtom(Clipboard::kMimeTypeURIList));
  atoms.push_back(atom_cache->GetAtom(kMimeTypeMozillaURL));
  return atoms;
}

void GetAtomIntersection(const std::vector< ::Atom>& one,
                         const std::vector< ::Atom>& two,
                         std::vector< ::Atom>* output) {
  for (std::vector< ::Atom>::const_iterator it = one.begin(); it != one.end();
       ++it) {
    for (std::vector< ::Atom>::const_iterator jt = two.begin(); jt != two.end();
         ++jt) {
      if (*it == *jt) {
        output->push_back(*it);
        break;
      }
    }
  }
}

void AddString16ToVector(const string16& str,
                         std::vector<unsigned char>* bytes) {
  const unsigned char* front =
      reinterpret_cast<const unsigned char*>(str.data());
  bytes->insert(bytes->end(), front, front + (str.size() * 2));
}

std::string RefCountedMemoryToString(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return std::string();
  }

  size_t size = memory->size();
  if (!size)
    return std::string();

  const unsigned char* front = memory->front();
  return std::string(reinterpret_cast<const char*>(front), size);
}

string16 RefCountedMemoryToString16(
    const scoped_refptr<base::RefCountedMemory>& memory) {
  if (!memory.get()) {
    NOTREACHED();
    return string16();
  }

  size_t size = memory->size();
  if (!size)
    return string16();

  const unsigned char* front = memory->front();
  return string16(reinterpret_cast<const base::char16*>(front), size / 2);
}

///////////////////////////////////////////////////////////////////////////////

SelectionFormatMap::SelectionFormatMap() {}

SelectionFormatMap::~SelectionFormatMap() {}

void SelectionFormatMap::Insert(
    ::Atom atom,
    const scoped_refptr<base::RefCountedMemory>& item) {
  data_.erase(atom);
  data_.insert(std::make_pair(atom, item));
}

ui::SelectionData SelectionFormatMap::GetFirstOf(
    const std::vector< ::Atom>& requested_types) const {
  for (std::vector< ::Atom>::const_iterator it = requested_types.begin();
       it != requested_types.end(); ++it) {
    const_iterator data_it = data_.find(*it);
    if (data_it != data_.end()) {
      return SelectionData(data_it->first, data_it->second);
    }
  }

  return SelectionData();
}

std::vector< ::Atom> SelectionFormatMap::GetTypes() const {
  std::vector< ::Atom> atoms;
  for (const_iterator it = data_.begin(); it != data_.end(); ++it)
    atoms.push_back(it->first);

  return atoms;
}

///////////////////////////////////////////////////////////////////////////////

SelectionData::SelectionData()
    : type_(None),
      atom_cache_(gfx::GetXDisplay(), kSelectionDataAtoms) {
}

SelectionData::SelectionData(
    ::Atom type,
    const scoped_refptr<base::RefCountedMemory>& memory)
    : type_(type),
      memory_(memory),
      atom_cache_(gfx::GetXDisplay(), kSelectionDataAtoms) {
}

SelectionData::SelectionData(const SelectionData& rhs)
    : type_(rhs.type_),
      memory_(rhs.memory_),
      atom_cache_(gfx::GetXDisplay(), kSelectionDataAtoms) {
}

SelectionData::~SelectionData() {}

SelectionData& SelectionData::operator=(const SelectionData& rhs) {
  type_ = rhs.type_;
  memory_ = rhs.memory_;
  // TODO(erg): In some future where we have to support multiple X Displays,
  // the following will also need to deal with the display.
  return *this;
}

bool SelectionData::IsValid() const {
  return type_ != None;
}

::Atom SelectionData::GetType() const {
  return type_;
}

const unsigned char* SelectionData::GetData() const {
  return memory_.get() ? memory_->front() : NULL;
}

size_t SelectionData::GetSize() const {
  return memory_.get() ? memory_->size() : 0;
}

std::string SelectionData::GetText() const {
  if (type_ == atom_cache_.GetAtom(kUtf8String) ||
      type_ == atom_cache_.GetAtom(kText)) {
    return RefCountedMemoryToString(memory_);
  } else if (type_ == atom_cache_.GetAtom(kString)) {
    std::string result;
    base::ConvertToUtf8AndNormalize(RefCountedMemoryToString(memory_),
                                    base::kCodepageLatin1,
                                    &result);
    return result;
  } else {
    // BTW, I looked at COMPOUND_TEXT, and there's no way we're going to
    // support that. Yuck.
    NOTREACHED();
    return std::string();
  }
}

string16 SelectionData::GetHtml() const {
  string16 markup;

  if (type_ == atom_cache_.GetAtom(Clipboard::kMimeTypeHTML)) {
    const unsigned char* data = GetData();
    size_t size = GetSize();

    // If the data starts with 0xFEFF, i.e., Byte Order Mark, assume it is
    // UTF-16, otherwise assume UTF-8.
    if (size >= 2 &&
        reinterpret_cast<const uint16_t*>(data)[0] == 0xFEFF) {
      markup.assign(reinterpret_cast<const uint16_t*>(data) + 1,
                    (size / 2) - 1);
    } else {
      UTF8ToUTF16(reinterpret_cast<const char*>(data), size, &markup);
    }

    // If there is a terminating NULL, drop it.
    if (!markup.empty() && markup.at(markup.length() - 1) == '\0')
      markup.resize(markup.length() - 1);

    return markup;
  } else {
    NOTREACHED();
    return markup;
  }
}

void SelectionData::AssignTo(std::string* result) const {
  *result = RefCountedMemoryToString(memory_);
}

void SelectionData::AssignTo(string16* result) const {
  *result = RefCountedMemoryToString16(memory_);
}

}  // namespace ui
