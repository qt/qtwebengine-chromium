// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accessibility/accessible_text_utils.h"

#include "base/logging.h"
#include "base/strings/string_util.h"

namespace ui {

size_t FindAccessibleTextBoundary(const base::string16& text,
                                  const std::vector<int>& line_breaks,
                                  TextBoundaryType boundary,
                                  size_t start_offset,
                                  TextBoundaryDirection direction) {
  size_t text_size = text.size();
  DCHECK(start_offset <= text_size);

  if (boundary == CHAR_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION && start_offset < text_size)
      return start_offset + 1;
    else
      return start_offset;
  } else if (boundary == LINE_BOUNDARY) {
    if (direction == FORWARDS_DIRECTION) {
      for (size_t j = 0; j < line_breaks.size(); ++j) {
          size_t line_break = line_breaks[j] >= 0 ? line_breaks[j] : 0;
        if (line_break > start_offset)
          return line_break;
      }
      return text_size;
    } else {
      // Note: j is unsigned, so for loop continues until j wraps around
      // and becomes greater than the starting value.
      for (size_t j = line_breaks.size() - 1;
           j < line_breaks.size();
           --j) {
        size_t line_break = line_breaks[j] >= 0 ? line_breaks[j] : 0;
        if (line_break <= start_offset)
          return line_break;
      }
      return 0;
    }
  }

  size_t result = start_offset;
  for (;;) {
    size_t pos;
    if (direction == FORWARDS_DIRECTION) {
      if (result >= text_size)
        return text_size;
      pos = result;
    } else {
      if (result == 0)
        return 0;
      pos = result - 1;
    }

    switch (boundary) {
      case CHAR_BOUNDARY:
      case LINE_BOUNDARY:
        NOTREACHED();  // These are handled above.
        break;
      case WORD_BOUNDARY:
        if (IsWhitespace(text[pos]))
          return result;
        break;
      case PARAGRAPH_BOUNDARY:
        if (text[pos] == '\n')
          return result;
      case SENTENCE_BOUNDARY:
        if ((text[pos] == '.' || text[pos] == '!' || text[pos] == '?') &&
            (pos == text_size - 1 || IsWhitespace(text[pos + 1]))) {
          return result;
        }
      case ALL_BOUNDARY:
      default:
        break;
    }

    if (direction == FORWARDS_DIRECTION) {
      result++;
    } else {
      result--;
    }
  }
}

}  // Namespace ui
