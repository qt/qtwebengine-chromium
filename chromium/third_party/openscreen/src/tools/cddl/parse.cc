// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/parse.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include "tools/cddl/logging.h"

static_assert(sizeof(std::string_view::size_type) == sizeof(size_t),
              "We assume string_view's size_type is the same as size_t. If "
              "not, the following file needs to be refactored");

// All of the parsing methods in this file that operate on Parser are named
// either Parse* or Skip* and are named according to the CDDL grammar in
// grammar.abnf.  Similarly, methods like ParseMemberKey1 attempt to parse the
// first choice in the memberkey rule.
struct Parser {
  std::string_view data;
  std::vector<std::unique_ptr<AstNode>> nodes;
};

std::string_view StripLeadingAsciiWhitespace(std::string_view view) {
  while (!view.empty() &&
         std::isspace(static_cast<unsigned char>(view.front()))) {
    view.remove_prefix(1);
  }
  return view;
}

AstNode* AddNode(Parser* p,
                 AstNode::Type type,
                 std::string_view text,
                 AstNode* children = nullptr) {
  p->nodes.push_back(std::make_unique<AstNode>());
  AstNode* node = p->nodes.back().get();
  node->children = children;
  node->sibling = nullptr;
  node->type = type;
  node->text = text;
  return node;
}

bool IsBinaryDigit(char x) {
  return '0' == x || x == '1';
}

// Determines if the given character matches regex '[a-zA-Z@_$]'.
bool IsExtendedAlpha(char x) {
  return std::isalpha(static_cast<unsigned char>(x)) || x == '@' || x == '_' ||
         x == '$';
}

bool IsNewline(char x) {
  return x == '\r' || x == '\n';
}

bool IsWhitespaceOrSemicolon(char c) {
  return c == ' ' || c == ';' || c == '\r' || c == '\n';
}

std::string_view SkipNewline(std::string_view view) {
  size_t index = 0;
  while (index < view.length() && IsNewline(view[index])) {
    ++index;
  }

  return view.substr(index);
}

std::optional<std::string_view> ParseTypeKeyFromComment(Parser* p);

// Skips over a comment that makes up the remainder of the current line.
std::string_view SkipComment(std::string_view view, bool skip_type_key = true) {
  size_t index = 0;
  if (!view.empty() && view[index] == ';') {
    if (!skip_type_key) {
      Parser p = {view};
      if (ParseTypeKeyFromComment(&p).has_value()) {
        return view;
      }
    }

    ++index;
    while (index < view.length() && !IsNewline(view[index])) {
      CHECK(std::isprint(static_cast<unsigned char>(view[index])));
      ++index;
    }

    while (index < view.length() && IsNewline(view[index])) {
      ++index;
    }
  }

  return view.substr(index);
}

void SkipWhitespaceImpl(Parser* p, bool skip_type_key = true) {
  std::string_view view = p->data;
  std::string_view new_view;

  while (true) {
    new_view = SkipComment(view, skip_type_key);
    if (new_view.data() == view.data()) {
      new_view = StripLeadingAsciiWhitespace(view);
    }

    if (new_view.data() == view.data() && new_view.size() == view.size()) {
      break;
    }

    view = new_view;
  }

  p->data = new_view;
}

void SkipWhitespace(Parser* p, bool skip_comments = true) {
  if (!skip_comments) {
    p->data = StripLeadingAsciiWhitespace(p->data);
    return;
  }

  SkipWhitespaceImpl(p);
}

// This is only used for the start of the file to avoid losing the first type
// key.
void SkipStartWhitespace(Parser* p) {
  SkipWhitespaceImpl(p, false);
}

bool TrySkipNewline(Parser* p) {
  std::string_view new_view = SkipNewline(p->data);
  bool is_changed = p->data.size() != new_view.size();
  p->data = new_view;
  return is_changed;
}

bool TrySkipCharacter(Parser* p, char c) {
  if (!p->data.empty() && p->data.front() == c) {
    p->data.remove_prefix(1);
    return true;
  }

  return false;
}

enum class AssignType {
  kInvalid = -1,
  kAssign,
  kAssignT,
  kAssignG,
};

AssignType ParseAssignmentType(Parser* p) {
  std::string_view it = p->data;
  if (it.empty()) {
    return AssignType::kInvalid;
  }
  if (it[0] == '=') {
    p->data.remove_prefix(1);
    return AssignType::kAssign;
  } else if (it[0] == '/') {
    if (it.size() >= 3 && it[1] == '/' && it[2] == '=') {
      p->data.remove_prefix(3);
      return AssignType::kAssignG;
    } else if (it.size() >= 2 && it[1] == '=') {
      p->data.remove_prefix(2);
      return AssignType::kAssignT;
    }
  }
  return AssignType::kInvalid;
}

AstNode* ParseType1(Parser* p);
AstNode* ParseType(Parser* p, bool skip_comments = true);
AstNode* ParseId(Parser* p);

void SkipUint(Parser* p) {
  std::string_view view = p->data;

  bool is_binary = false;
  size_t index = 0;
  if (view.starts_with("0b")) {
    is_binary = true;
    index = 2;
  } else if (view.starts_with("0x")) {
    index = 2;
  }

  while (index < view.length() &&
         std::isdigit(static_cast<unsigned char>(view[index]))) {
    if (is_binary) {
      CHECK(IsBinaryDigit(view[index]));
    }

    ++index;
  }

  p->data = view.substr(index);
}

AstNode* ParseNumber(Parser* p) {
  Parser p_speculative{p->data};
  if (p_speculative.data.empty()) {
    return nullptr;
  }
  if (!std::isdigit(static_cast<unsigned char>(p_speculative.data[0])) &&
      p_speculative.data[0] != '-') {
    // TODO(btolsch): Add support for hexfloat, fraction, exponent.
    return nullptr;
  }
  if (p_speculative.data[0] == '-') {
    p_speculative.data.remove_prefix(1);
  }

  SkipUint(&p_speculative);

  AstNode* node =
      AddNode(p, AstNode::Type::kNumber,
              p->data.substr(0, p->data.size() - p_speculative.data.size()));
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseText(Parser* p) {
  return nullptr;
}

AstNode* ParseBytes(Parser* p) {
  return nullptr;
}

// Returns whether `c` could be the first character in a valid "value" string.
// This is not a guarantee however, since 'h' and 'b' could also indicate the
// start of an ID, but value needs to be tried first.
bool IsValue(char c) {
  return (c == '-' ||
          std::isdigit(static_cast<unsigned char>(c)) ||  // FIRST(number)
          c == '"' ||                                     // FIRST(text)
          c == '\'' || c == 'h' || c == 'b');             // FIRST(bytes)
}

AstNode* ParseValue(Parser* p) {
  AstNode* node = ParseNumber(p);
  if (!node) {
    node = ParseText(p);
  }
  if (!node) {
    ParseBytes(p);
  }
  return node;
}

// Determines whether an occurrence operator (such as '*' or '?') prefacing
// the group definition occurs before the next whitespace character, and
// creates a new Occurrence node if so.
AstNode* ParseOccur(Parser* p) {
  Parser p_speculative{p->data};
  if (p_speculative.data.empty()) {
    return nullptr;
  }

  if (p_speculative.data[0] == '?' || p_speculative.data[0] == '+') {
    p_speculative.data.remove_prefix(1);
  } else {
    SkipUint(&p_speculative);
    if (p_speculative.data.empty() || p_speculative.data[0] != '*') {
      return nullptr;
    }
    p_speculative.data.remove_prefix(1);
    SkipUint(&p_speculative);
  }

  AstNode* node =
      AddNode(p, AstNode::Type::kOccur,
              p->data.substr(0, p->data.size() - p_speculative.data.size()));
  p->data = p_speculative.data;
  return node;
}

std::optional<std::string_view> ParseTypeKeyFromComment(Parser* p) {
  Parser p_speculative{p->data};
  if (!TrySkipCharacter(&p_speculative, ';')) {
    return std::nullopt;
  }

  SkipWhitespace(&p_speculative, false);
  constexpr std::string_view kTypeKeyPrefix = "type key";
  if (!p_speculative.data.starts_with(kTypeKeyPrefix)) {
    return std::nullopt;
  }
  p_speculative.data.remove_prefix(kTypeKeyPrefix.size());

  SkipWhitespace(&p_speculative, false);
  Parser p_speculative2{p_speculative.data};
  while (!p_speculative2.data.empty() &&
         std::isdigit(static_cast<unsigned char>(p_speculative2.data[0]))) {
    p_speculative2.data.remove_prefix(1);
  }
  auto result = p_speculative.data.substr(
      0, p_speculative.data.size() - p_speculative2.data.size());
  p->data = p_speculative2.data;
  return result;
}

AstNode* ParseMemberKeyFromComment(Parser* p) {
  Parser p_speculative{p->data};
  if (!TrySkipCharacter(&p_speculative, ';')) {
    return nullptr;
  }

  SkipWhitespace(&p_speculative, false);

  AstNode* value = ParseId(&p_speculative);
  if (!value) {
    return nullptr;
  }

  while (!p_speculative.data.empty() && (p_speculative.data.front() == ' ' ||
                                         p_speculative.data.front() == '\t')) {
    p_speculative.data.remove_prefix(1);
  }

  if (!TrySkipNewline(&p_speculative)) {
    return nullptr;
  }

  AstNode* node = AddNode(p, AstNode::Type::kMemberKey, value->text, value);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));

  return node;
}

AstNode* ParseMemberKey1(Parser* p) {
  Parser p_speculative{p->data};
  if (!ParseType1(&p_speculative)) {
    return nullptr;
  }

  SkipWhitespace(&p_speculative);

  if (p_speculative.data.size() < 2 || p_speculative.data[0] != '=' ||
      p_speculative.data[1] != '>') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(2);

  AstNode* node =
      AddNode(p, AstNode::Type::kMemberKey,
              p->data.substr(0, p->data.size() - p_speculative.data.size()));
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseMemberKey2(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* id = ParseId(&p_speculative);
  if (!id) {
    return nullptr;
  }

  SkipWhitespace(&p_speculative);

  if (p_speculative.data.empty() || p_speculative.data[0] != ':') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(1);

  AstNode* node = AddNode(
      p, AstNode::Type::kMemberKey,
      p->data.substr(0, p->data.size() - p_speculative.data.size()), id);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseMemberKey3(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* value = ParseValue(&p_speculative);
  if (!value) {
    return nullptr;
  }

  SkipWhitespace(&p_speculative);

  if (p_speculative.data.empty() || p_speculative.data[0] != ':') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(1);

  AstNode* node = AddNode(
      p, AstNode::Type::kMemberKey,
      p->data.substr(0, p->data.size() - p_speculative.data.size()), value);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

// Iteratively tries all valid member key formats, retuning a Node representing
// the member key if found or nullptr if not.
AstNode* ParseMemberKey(Parser* p) {
  AstNode* node = ParseMemberKey1(p);
  if (!node) {
    node = ParseMemberKey2(p);
  }
  if (!node) {
    node = ParseMemberKey3(p);
  }
  return node;
}

AstNode* ParseGroupEntry(Parser* p);

bool SkipOptionalComma(Parser* p) {
  SkipWhitespace(p);
  if (!p->data.empty() && p->data[0] == ',') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
  }
  return true;
}

// Parse the group contained inside of other brackets. Since the brackets around
// the group are optional inside of other brackets, we can't directly call
// ParseGroupEntry(...) and instead need this wrapper around it.
AstNode* ParseGroupChoice(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* tail = nullptr;
  AstNode* group_node =
      AddNode(&p_speculative, AstNode::Type::kGrpchoice, std::string_view());
  std::string_view group_node_text = p_speculative.data;
  while (true) {
    std::string_view orig = p_speculative.data;
    AstNode* group_entry = ParseGroupEntry(&p_speculative);
    if (!group_entry) {
      p_speculative.data = orig;
      if (!group_node->children) {
        return nullptr;
      }
      group_node->text = group_node_text.substr(
          0, group_node_text.size() - p_speculative.data.size());
      p->data = p_speculative.data;
      std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
                std::back_inserter(p->nodes));
      return group_node;
    }
    if (!group_node->children) {
      group_node->children = group_entry;
    }
    if (tail) {
      tail->sibling = group_entry;
    }
    tail = group_entry;
    if (!SkipOptionalComma(&p_speculative)) {
      return nullptr;
    }
  }
}

AstNode* ParseGroup(Parser* p) {
  std::string_view orig = p->data;
  AstNode* group_choice = ParseGroupChoice(p);
  if (!group_choice) {
    return nullptr;
  }
  return AddNode(p, AstNode::Type::kGroup,
                 orig.substr(0, orig.size() - p->data.size()), group_choice);
}

// Parse optional range operator .. (inlcusive) or ... (exclusive)
// ABNF rule: rangeop = "..." / ".."
AstNode* ParseRangeop(Parser* p) {
  std::string_view view = p->data;
  if (view.starts_with("...")) {
    // rangeop ...
    p->data.remove_prefix(3);
    return AddNode(p, AstNode::Type::kRangeop, view.substr(0, 3));
  } else if (view.starts_with("..")) {
    // rangeop ..
    p->data.remove_prefix(2);
    return AddNode(p, AstNode::Type::kRangeop, view.substr(0, 2));
  }
  return nullptr;
}

// Parse optional control operator .id
// ABNF rule: ctlop = "." id
AstNode* ParseCtlop(Parser* p) {
  std::string_view view = p->data;
  std::string_view initial_data = p->data;
  if (!view.starts_with(".")) {
    return nullptr;
  }
  p->data.remove_prefix(1);
  AstNode* id = ParseId(p);
  if (!id) {
    return nullptr;
  }
  return AddNode(p, AstNode::Type::kCtlop,
                 view.substr(0, initial_data.size() - p->data.size()), id);
}

AstNode* ParseType2(Parser* p) {
  std::string_view orig = p->data;
  std::string_view it = p->data;
  if (it.empty()) {
    return nullptr;
  }

  AstNode* node = AddNode(p, AstNode::Type::kType2, std::string_view());
  if (IsValue(it[0])) {
    AstNode* value = ParseValue(p);
    if (!value) {
      if (it[0] == 'h' || it[0] == 'b') {
        AstNode* id = ParseId(p);
        if (!id) {
          return nullptr;
        }
        id->type = AstNode::Type::kTypename;
        node->children = id;
      } else {
        return nullptr;
      }
    } else {
      node->children = value;
    }
  } else if (IsExtendedAlpha(it[0])) {
    AstNode* id = ParseId(p);
    if (!id) {
      return nullptr;
    }
    if (!p->data.empty() && p->data[0] == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
    id->type = AstNode::Type::kTypename;
    node->children = id;
  } else if (it[0] == '(') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
    if (!p->data.empty() && p->data[0] == ')') {
      std::cerr << "It looks like you're trying to provide an empty Type (), "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
    AstNode* type = ParseType(p);
    if (!type) {
      return nullptr;
    }
    SkipWhitespace(p);
    if (p->data.empty() || p->data[0] != ')') {
      return nullptr;
    }
    p->data.remove_prefix(1);
    node->children = type;
  } else if (it[0] == '{') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
    if (!p->data.empty() && p->data[0] == '}') {
      std::cerr << "It looks like you're trying to provide an empty Group {}, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    SkipWhitespace(p);
    if (p->data.empty() || p->data[0] != '}') {
      return nullptr;
    }
    p->data.remove_prefix(1);
    node->children = group;
  } else if (it[0] == '[') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
    AstNode* group = ParseGroup(p);
    if (!group) {
      return nullptr;
    }
    SkipWhitespace(p);
    if (p->data.empty() || p->data[0] != ']') {
      return nullptr;
    }
    p->data.remove_prefix(1);
    node->children = group;
  } else if (it[0] == '~') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
    if (!ParseId(p)) {
      return nullptr;
    }
    if (!p->data.empty() && p->data[0] == '<') {
      std::cerr << "It looks like you're trying to use a generic argument, "
                   "which we don't support"
                << std::endl;
      return nullptr;
    }
  } else if (it[0] == '&') {
    p->data.remove_prefix(1);
    SkipWhitespace(p);
    if (!p->data.empty() && p->data[0] == '(') {
      p->data.remove_prefix(1);
      SkipWhitespace(p);
      if (!p->data.empty() && p->data[0] == ')') {
        std::cerr << "It looks like you're trying to provide an empty Type &(),"
                     " which we don't support"
                  << std::endl;
        return nullptr;
      }
      AstNode* group = ParseGroup(p);
      if (!group) {
        return nullptr;
      }
      SkipWhitespace(p);
      if (p->data.empty() || p->data[0] != ')') {
        return nullptr;
      }
      p->data.remove_prefix(1);
      node->children = group;
    } else {
      AstNode* id = ParseId(p);
      if (id) {
        if (!p->data.empty() && p->data[0] == '<') {
          std::cerr << "It looks like you're trying to use a generic argument, "
                       "which we don't support"
                    << std::endl;
          return nullptr;
        }
        id->type = AstNode::Type::kGroupname;
        node->children = id;
      } else {
        return nullptr;
      }
    }
  } else if (it[0] == '#') {
    p->data.remove_prefix(1);
    if (!p->data.empty() && p->data[0] == '6') {
      p->data.remove_prefix(1);
      if (!p->data.empty() && p->data[0] == '.') {
        p->data.remove_prefix(1);
        SkipUint(p);
      }
      if (p->data.empty() || p->data[0] != '(') {
        return nullptr;
      }
      p->data.remove_prefix(1);
      SkipWhitespace(p);
      AstNode* type = ParseType(p);
      if (!type) {
        return nullptr;
      }
      SkipWhitespace(p);
      if (p->data.empty() || p->data[0] != ')') {
        return nullptr;
      }
      p->data.remove_prefix(1);
      node->children = type;
    } else if (!p->data.empty() &&
               std::isdigit(static_cast<unsigned char>(p->data[0]))) {
      std::cerr << "# MAJOR unimplemented" << std::endl;
      return nullptr;
    } else {
      // '#' followed by something else? "p->data = it" logic in original was:
      // it was incremented.
      // If we fall through, p->data is updated.
      // Original: p->data = it; (it was incremented at start of # block)
      // Here p->data is already incremented.
    }
  } else {
    return nullptr;
  }
  node->text = orig.substr(0, orig.size() - p->data.size());
  return node;
}

AstNode* ParseType1(Parser* p) {
  std::string_view orig = p->data;
  AstNode* type2 = ParseType2(p);
  if (!type2) {
    return nullptr;
  }
  SkipWhitespace(p, false);
  AstNode* rangeop_or_ctlop = ParseRangeop(p);
  if (!rangeop_or_ctlop) {
    rangeop_or_ctlop = ParseCtlop(p);
  }
  if (rangeop_or_ctlop) {
    SkipWhitespace(p, false);
    AstNode* param = ParseType2(p);
    if (!param) {
      return nullptr;
    }
    type2->sibling = rangeop_or_ctlop;
    rangeop_or_ctlop->sibling = param;
  }
  return AddNode(p, AstNode::Type::kType1,
                 orig.substr(0, orig.size() - p->data.size()), type2);
}

// Different valid types for a call are specified as type1 / type2, so we split
// at the '/' character and process each allowed type separately.
AstNode* ParseType(Parser* p, bool skip_comments) {
  Parser p_speculative{p->data};

  // Parse all allowed types into a linked list starting in type1's sibling ptr.
  AstNode* type1 = ParseType1(&p_speculative);
  if (!type1) {
    return nullptr;
  }
  SkipWhitespace(&p_speculative, skip_comments);

  AstNode* tail = type1;
  while (!p_speculative.data.empty() && p_speculative.data[0] == '/') {
    p_speculative.data.remove_prefix(1);
    SkipWhitespace(&p_speculative, skip_comments);

    AstNode* next_type1 = ParseType1(&p_speculative);
    if (!next_type1) {
      return nullptr;
    }
    tail->sibling = next_type1;
    tail = next_type1;
    SkipWhitespace(&p_speculative, skip_comments);
  }

  // Create a new AstNode with all parsed types.
  AstNode* node = AddNode(
      p, AstNode::Type::kType,
      p->data.substr(0, p->data.size() - p_speculative.data.size()), type1);
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

AstNode* ParseId(Parser* p) {
  std::string_view id = p->data;

  // If the id doesnt start with a valid name character, return null.
  if (id.empty() || !IsExtendedAlpha(id[0])) {
    return nullptr;
  }

  // Read through the name character by character and make sure it's valid.
  size_t index = 1;
  while (true) {
    if (index < id.size() && (id[index] == '-' || id[index] == '.')) {
      ++index;
      if (index < id.size() && !IsExtendedAlpha(id[index]) &&
          !std::isdigit(static_cast<unsigned char>(id[index]))) {
        return nullptr;
      }
      ++index;
    } else if (index < id.size() &&
               (IsExtendedAlpha(id[index]) ||
                std::isdigit(static_cast<unsigned char>(id[index])))) {
      ++index;
    } else {
      break;
    }
  }

  // Create and return a new node with the parsed data.
  AstNode* node = AddNode(p, AstNode::Type::kId, p->data.substr(0, index));
  p->data.remove_prefix(index);
  return node;
}

AstNode* UpdateNodesForGroupEntry(Parser* p,
                                  Parser* p_speculative,
                                  AstNode* occur,
                                  AstNode* member_key,
                                  AstNode* type) {
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string_view());
  if (occur) {
    node->children = occur;
    if (member_key) {
      occur->sibling = member_key;
      member_key->sibling = type;
    } else {
      occur->sibling = type;
    }
  } else if (member_key) {
    node->children = member_key;
    member_key->sibling = type;
  } else {
    node->children = type;
  }
  node->text = p->data.substr(0, p->data.size() - p_speculative->data.size());
  p->data = p_speculative->data;
  std::move(p_speculative->nodes.begin(), p_speculative->nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

// Parse a group entry of form <id_num>: <type> ; <name>
AstNode* ParseGroupEntryWithNameInComment(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespace(&p_speculative, false);
  }
  AstNode* member_key_num = ParseValue(&p_speculative);
  if (!member_key_num) {
    return nullptr;
  }
  SkipWhitespace(&p_speculative, false);
  if (p_speculative.data.empty() || p_speculative.data[0] != ':') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(1);
  SkipWhitespace(&p_speculative, false);
  AstNode* type = ParseType(&p_speculative, false);
  if (!type) {
    return nullptr;
  }
  SkipWhitespace(&p_speculative, false);
  AstNode* member_key = ParseMemberKeyFromComment(&p_speculative);
  if (!member_key) {
    return nullptr;
  }

  member_key->integer_member_key_text = member_key_num->text;

  return UpdateNodesForGroupEntry(p, &p_speculative, occur, member_key, type);
}

AstNode* ParseGroupEntryWithNameAsId(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespace(&p_speculative);
  }
  AstNode* member_key = ParseMemberKey(&p_speculative);
  if (member_key) {
    SkipWhitespace(&p_speculative);
  }
  AstNode* type = ParseType(&p_speculative);
  if (!type) {
    return nullptr;
  }
  return UpdateNodesForGroupEntry(p, &p_speculative, occur, member_key, type);
}

// NOTE: This should probably never be hit, why is it in the grammar?
AstNode* ParseGroupEntryWithGroupReference(Parser* p) {
  Parser p_speculative{p->data};

  // Check for an occurance indicator ('?', '*', "+") before the sub-group
  // definition.
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespace(&p_speculative);
  }

  // Parse the ID of the sub-group.
  AstNode* id = ParseId(&p_speculative);
  if (!id) {
    return nullptr;
  }
  id->type = AstNode::Type::kGroupname;
  if (!p_speculative.data.empty() && p_speculative.data[0] == '<') {
    std::cerr << "It looks like you're trying to use a generic argument, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }

  // Create a new node containing this sub-group reference.
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string_view());
  if (occur) {
    occur->sibling = id;
    node->children = occur;
  } else {
    node->children = id;
  }
  node->text = p->data.substr(0, p->data.size() - p_speculative.data.size());
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

// Recursively parse a group entry that's an inline-defined group of the form
// '(...<some contents>...)'.
AstNode* ParseGroupEntryWithInlineGroupDefinition(Parser* p) {
  Parser p_speculative{p->data};
  AstNode* occur = ParseOccur(&p_speculative);
  if (occur) {
    SkipWhitespace(&p_speculative);
  }
  if (p_speculative.data.empty() || p_speculative.data[0] != '(') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(1);
  SkipWhitespace(&p_speculative);
  AstNode* group = ParseGroup(&p_speculative);  // Recursive call here.
  if (!group) {
    return nullptr;
  }

  SkipWhitespace(&p_speculative);
  if (p_speculative.data.empty() || p_speculative.data[0] != ')') {
    return nullptr;
  }
  p_speculative.data.remove_prefix(1);
  AstNode* node = AddNode(p, AstNode::Type::kGrpent, std::string_view());
  if (occur) {
    node->children = occur;
    occur->sibling = group;
  } else {
    node->children = group;
  }
  node->text = p->data.substr(0, p->data.size() - p_speculative.data.size());
  p->data = p_speculative.data;
  std::move(p_speculative.nodes.begin(), p_speculative.nodes.end(),
            std::back_inserter(p->nodes));
  return node;
}

// Recursively parse the group assignemnt.
AstNode* ParseGroupEntry(Parser* p) {
  // Parse a group entry of form '#: type ; name'
  AstNode* node = ParseGroupEntryWithNameInComment(p);

  // Parse a group entry of form 'id: type'.
  if (!node) {
    node = ParseGroupEntryWithNameAsId(p);
  }

  // Parse a group entry of form 'subgroupName'.
  if (!node) {
    node = ParseGroupEntryWithGroupReference(p);
  }

  // Parse a group entry of the form: '(' <some contents> ')'.
  // NOTE: This is the method hit during the top-level group parsing, and the
  // recursive call occurs inside this method.
  if (!node) {
    node = ParseGroupEntryWithInlineGroupDefinition(p);
  }

  // Return the results of the recursive call.
  return node;
}

AstNode* ParseRule(Parser* p) {
  std::string_view start = p->data;

  // Parse the type key, if it's present
  std::optional<std::string_view> type_key = ParseTypeKeyFromComment(p);
  SkipWhitespace(p);

  // Use the parser to extract the id and data.
  AstNode* id = ParseId(p);
  if (!id) {
    Logger::Error("No id found!");
    return nullptr;
  }
  if (!p->data.empty() && p->data[0] == '<') {
    std::cerr << "It looks like you're trying to use a generic parameter, "
                 "which we don't support"
              << std::endl;
    return nullptr;
  }

  // Determine the type of assignment being done to this variable name (ie '=').
  SkipWhitespace(p);
  std::string_view assign_start = p->data;
  AssignType assign_type = ParseAssignmentType(p);
  if (assign_type != AssignType::kAssign) {
    Logger::Error("No assignment operator found! assign_type: " +
                  std::to_string(static_cast<int>(assign_type)));
    return nullptr;
  }
  AstNode* assign_node =
      AddNode(p,
              (assign_type == AssignType::kAssign)    ? AstNode::Type::kAssign
              : (assign_type == AssignType::kAssignT) ? AstNode::Type::kAssignT
                                                      : AstNode::Type::kAssignG,
              assign_start.substr(0, assign_start.size() - p->data.size()));
  id->sibling = assign_node;

  // Parse the object type being assigned.
  SkipWhitespace(p);
  AstNode* type = ParseType(p, false);  // Try to parse it as a type.
  id->type = AstNode::Type::kTypename;
  if (!type) {  // If it's not a type, try and parse it as a group.
    type = ParseGroupEntry(p);
    id->type = AstNode::Type::kGroupname;
  }
  if (!type) {  // if it's not a type or a group, exit as failure.
    Logger::Error("No node type found!");
    return nullptr;
  }
  assign_node->sibling = type;
  SkipWhitespace(p, false);

  // Return the results.
  auto rule_node = AddNode(p, AstNode::Type::kRule,
                           start.substr(0, start.size() - p->data.size()), id);
  rule_node->type_key = type_key;
  return rule_node;
}

// Iteratively parse the CDDL spec into a tree structure.
ParseResult ParseCddl(std::string_view data) {
  while (!data.empty() && data.back() == 0) {
    data.remove_suffix(1);
  }

  if (data.empty()) {
    return {nullptr, {}};
  }
  Parser p = {data};

  SkipStartWhitespace(&p);
  AstNode* root = nullptr;
  AstNode* tail = nullptr;
  do {
    AstNode* next = ParseRule(&p);
    if (!next) {
      Logger::Error(
          std::string("Failed to parse next node. Failed starting at: '") +
          std::string(p.data) + "'");
      return {nullptr, {}};
    } else {
      Logger::Log(std::string("Processed text \"") + std::string(next->text) +
                  "\" into node: ");
      DumpAst(next);
    }

    if (!root) {
      root = next;
    }
    if (tail) {
      tail->sibling = next;
    }
    tail = next;
  } while (!p.data.empty());
  return {root, std::move(p.nodes)};
}

// Recursively print out the AstNode graph.
void DumpAst(AstNode* node, int indent_level) {
  while (node) {
    // Prefix with '-'s so the levels of the graph are clear.
    std::string node_text = "";
    for (int i = 0; i <= indent_level; ++i) {
      node_text += "--";
    }

    // Print the type.
    switch (node->type) {
      case AstNode::Type::kRule:
        node_text += "kRule";
        break;
      case AstNode::Type::kTypename:
        node_text += "kTypename";
        break;
      case AstNode::Type::kGroupname:
        node_text += "kGroupname";
        break;
      case AstNode::Type::kAssign:
        node_text += "kAssign";
        break;
      case AstNode::Type::kAssignT:
        node_text += "kAssignT";
        break;
      case AstNode::Type::kAssignG:
        node_text += "kAssignG";
        break;
      case AstNode::Type::kType:
        node_text += "kType";
        break;
      case AstNode::Type::kGrpent:
        node_text += "kGrpent";
        break;
      case AstNode::Type::kType1:
        node_text += "kType1";
        break;
      case AstNode::Type::kType2:
        node_text += "kType2";
        break;
      case AstNode::Type::kValue:
        node_text += "kValue";
        break;
      case AstNode::Type::kGroup:
        node_text += "kGroup";
        break;
      case AstNode::Type::kUint:
        node_text += "kUint";
        break;
      case AstNode::Type::kDigit:
        node_text += "kDigit";
        break;
      case AstNode::Type::kRangeop:
        node_text += "kRangeop";
        break;
      case AstNode::Type::kCtlop:
        node_text += "kCtlop";
        break;
      case AstNode::Type::kGrpchoice:
        node_text += "kGrpchoice";
        break;
      case AstNode::Type::kOccur:
        node_text += "kOccur";
        break;
      case AstNode::Type::kMemberKey:
        node_text += "kMemberKey";
        break;
      case AstNode::Type::kId:
        node_text += "kId";
        break;
      case AstNode::Type::kNumber:
        node_text += "kNumber";
        break;
      case AstNode::Type::kText:
        node_text += "kText";
        break;
      case AstNode::Type::kBytes:
        node_text += "kBytes";
        break;
      case AstNode::Type::kBool:
        node_text += "kBool";
        break;
      case AstNode::Type::kFloat:
        node_text += "kFloat";
        break;
      case AstNode::Type::kFloat64:
        node_text += "kFloat64";
        break;
      case AstNode::Type::kInt:
        node_text += "kInt";
        break;
      case AstNode::Type::kOther:
        node_text += "kOther";
        break;
    }
    if (node->type_key != std::nullopt) {
      node_text +=
          " (type key=\"" + std::string(node->type_key.value()) + "\")";
    }
    node_text += ": ";

    // Print the contents.
    int size = static_cast<int>(node->text.size());
    std::string_view text = node->text.data();
    for (int i = 0; i < size; ++i) {
      if (text[i] == ' ' || text[i] == '\n') {
        node_text += " ";
        while (i < size - 1 && (text[i + 1] == ' ' || text[i + 1] == '\n')) {
          ++i;
        }
        continue;
      } else {
        node_text += text[i];
      }
    }
    Logger::Log(node_text);

    // Recursively print children then iteratively print siblings.
    DumpAst(node->children, indent_level + 1);
    node = node->sibling;
  }
}
