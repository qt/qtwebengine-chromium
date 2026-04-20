// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gn/value.h"

#include <stddef.h>
#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "gn/scope.h"

ValueList::ValueList() = default;
ValueList::ValueList(std::vector<Value> v) : values_(std::move(v)) {}
ValueList::~ValueList() = default;

// NOTE: Cannot use = default here due to the use of a union member.
Value::Value() {}

Value::Value(const ParseNode* origin, Type t) : type_(t), origin_(origin) {
  switch (type_) {
    case NONE:
      break;
    case BOOLEAN:
      boolean_value_ = false;
      break;
    case INTEGER:
      int_value_ = 0;
      break;
    case STRING:
      new (&string_value_) std::string();
      break;
    case LIST:
      new (&list_ptr_) scoped_refptr<ValueList>();
      break;
    case SCOPE:
      new (&scope_value_) std::unique_ptr<Scope>();
      break;
  }
}

Value::Value(const ParseNode* origin, bool bool_val)
    : type_(BOOLEAN), origin_(origin), boolean_value_(bool_val) {}

Value::Value(const ParseNode* origin, int64_t int_val)
    : type_(INTEGER), origin_(origin), int_value_(int_val) {}

Value::Value(const ParseNode* origin, std::string str_val)
    : type_(STRING), origin_(origin), string_value_(std::move(str_val)) {}

Value::Value(const ParseNode* origin, const char* str_val)
    : type_(STRING), origin_(origin), string_value_(str_val) {}

Value::Value(const ParseNode* origin, std::unique_ptr<Scope> scope)
    : type_(SCOPE), origin_(origin), scope_value_(std::move(scope)) {}

Value::Value(const Value& other) : type_(other.type_), origin_(other.origin_) {
  switch (type_) {
    case NONE:
      break;
    case BOOLEAN:
      boolean_value_ = other.boolean_value_;
      break;
    case INTEGER:
      int_value_ = other.int_value_;
      break;
    case STRING:
      new (&string_value_) std::string(other.string_value_);
      break;
    case LIST:
      new (&list_ptr_) scoped_refptr<ValueList>(other.list_ptr_);
      break;
    case SCOPE:
      new (&scope_value_) std::unique_ptr<Scope>(
          other.scope_value_.get() ? other.scope_value_->MakeClosure()
                                   : nullptr);
      break;
  }
}

Value::Value(Value&& other) noexcept
    : type_(other.type_), origin_(other.origin_) {
  switch (type_) {
    case NONE:
      break;
    case BOOLEAN:
      boolean_value_ = other.boolean_value_;
      break;
    case INTEGER:
      int_value_ = other.int_value_;
      break;
    case STRING:
      new (&string_value_) std::string(std::move(other.string_value_));
      break;
    case LIST:
      new (&list_ptr_) scoped_refptr<ValueList>(std::move(other.list_ptr_));
      break;
    case SCOPE:
      new (&scope_value_) std::unique_ptr<Scope>(std::move(other.scope_value_));
      break;
  }
}

Value& Value::operator=(const Value& other) {
  if (this != &other) {
    this->~Value();
    new (this) Value(other);
  }
  return *this;
}

Value& Value::operator=(Value&& other) noexcept {
  if (this != &other) {
    this->~Value();
    new (this) Value(std::move(other));
  }
  return *this;
}

Value::~Value() {
  using namespace std;
  switch (type_) {
    case STRING:
      string_value_.~string();
      break;
    case LIST:
      list_ptr_.~scoped_refptr();
      break;
    case SCOPE:
      scope_value_.~unique_ptr();
      break;
    default:;
  }
}

// static
const char* Value::DescribeType(Type t) {
  switch (t) {
    case NONE:
      return "none";
    case BOOLEAN:
      return "boolean";
    case INTEGER:
      return "integer";
    case STRING:
      return "string";
    case LIST:
      return "list";
    case SCOPE:
      return "scope";
    default:
      NOTREACHED();
      return "UNKNOWN";
  }
}

std::vector<Value>& Value::list_value() {
  DCHECK(type_ == LIST);
  if (!list_ptr_) {
    list_ptr_ = new ValueList();
  } else if (!list_ptr_->HasOneRef()) {
    // Copy-On-Write (COW): If this ValueList is shared with other Value objects
    // (reference count is > 1), we must not modify the shared vector directly.
    // Doing so would incorrectly mutate the other Values sharing this data.
    // Instead, we create a deep copy of the vector for this Value to safely
    // modify.
    list_ptr_ = new ValueList(list_ptr_->values_);
  }
  return list_ptr_->values_;
}

const std::vector<Value>& Value::list_value() const {
  DCHECK(type_ == LIST);
  if (!list_ptr_) {
    static const std::vector<Value>* empty_list = new std::vector<Value>();
    return *empty_list;
  }
  return list_ptr_->values_;
}

void Value::SetScopeValue(std::unique_ptr<Scope> scope) {
  DCHECK(type_ == SCOPE);
  scope_value_ = std::move(scope);
}

std::string Value::ToString(bool quote_string) const {
  switch (type_) {
    case NONE:
      return "<void>";
    case BOOLEAN:
      return boolean_value_ ? "true" : "false";
    case INTEGER:
      return base::Int64ToString(int_value_);
    case STRING:
      if (quote_string) {
        std::string result = "\"";
        bool hanging_backslash = false;
        for (char ch : string_value_) {
          // If the last character was a literal backslash and the next
          // character could form a valid escape sequence, we need to insert
          // an extra backslash to prevent that.
          if (hanging_backslash && (ch == '$' || ch == '"' || ch == '\\'))
            result += '\\';
          // If the next character is a dollar sign or double quote, it needs
          // to be escaped; otherwise it can be printed as is.
          if (ch == '$' || ch == '"')
            result += '\\';
          result += ch;
          hanging_backslash = (ch == '\\');
        }
        // Again, we need to prevent the closing double quotes from becoming
        // an escape sequence.
        if (hanging_backslash)
          result += '\\';
        result += '"';
        return result;
      }
      return string_value_;
    case LIST: {
      std::string result = "[";
      if (list_ptr_) {
        const std::vector<Value>& list = list_value();
        for (size_t i = 0; i < list.size(); i++) {
          if (i > 0)
            result += ", ";
          result += list[i].ToString(true);
        }
      }
      result.push_back(']');
      return result;
    }
    case SCOPE: {
      Scope::KeyValueMap scope_values;
      scope_value_->GetCurrentScopeValues(&scope_values);
      if (scope_values.empty())
        return std::string("{ }");

      std::string result = "{\n";
      for (const auto& pair : scope_values) {
        result += "  " + std::string(pair.first) + " = " +
                  pair.second.ToString(true) + "\n";
      }
      result += "}";

      return result;
    }
  }
  return std::string();
}

bool Value::VerifyTypeIs(Type t, Err* err) const {
  if (type_ == t)
    return true;

  *err = Err(origin(), std::string("This is not a ") + DescribeType(t) + ".",
             std::string("Instead I see a ") + DescribeType(type_) + " = " +
                 ToString(true));
  return false;
}

bool Value::operator==(const Value& other) const {
  if (type_ != other.type_)
    return false;

  switch (type_) {
    case Value::BOOLEAN:
      return boolean_value() == other.boolean_value();
    case Value::INTEGER:
      return int_value() == other.int_value();
    case Value::STRING:
      return string_value() == other.string_value();
    case Value::LIST:
      if (list_ptr_ == other.list_ptr_)
        return true;
      return list_value() == other.list_value();
    case Value::SCOPE:
      return scope_value()->CheckCurrentScopeValuesEqual(other.scope_value());
    case Value::NONE:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

bool Value::operator!=(const Value& other) const {
  return !operator==(other);
}
