// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/registry_dict_win.h"

#include "base/json/json_reader.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/values.h"
#include "base/win/registry.h"
#include "components/policy/core/common/schema.h"

using base::win::RegistryKeyIterator;
using base::win::RegistryValueIterator;

namespace policy {

namespace {

// Converts a value (as read from the registry) to meet |schema|, converting
// types as necessary. Unconvertible types will show up as NULL values in the
// result.
scoped_ptr<base::Value> ConvertValue(const base::Value& value,
                                     const Schema& schema) {
  if (!schema.valid())
    return make_scoped_ptr(value.DeepCopy());

  // If the type is good already, go with it.
  if (value.IsType(schema.type())) {
    // Recurse for complex types.
    const base::DictionaryValue* dict = NULL;
    const base::ListValue* list = NULL;
    if (value.GetAsDictionary(&dict)) {
      scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
      for (base::DictionaryValue::Iterator entry(*dict); !entry.IsAtEnd();
           entry.Advance()) {
        scoped_ptr<base::Value> converted =
            ConvertValue(entry.value(), schema.GetProperty(entry.key()));
        if (converted)
          result->SetWithoutPathExpansion(entry.key(), converted.release());
      }
      return result.Pass();
    } else if (value.GetAsList(&list)) {
      scoped_ptr<base::ListValue> result(new base::ListValue());
      for (base::ListValue::const_iterator entry(list->begin());
           entry != list->end(); ++entry) {
        scoped_ptr<base::Value> converted =
            ConvertValue(**entry, schema.GetItems());
        if (converted)
          result->Append(converted.release());
      }
      return result.Pass();
    }
    return make_scoped_ptr(value.DeepCopy());
  }

  // Else, do some conversions to map windows registry data types to JSON types.
  std::string string_value;
  int int_value = 0;
  switch (schema.type()) {
    case base::Value::TYPE_NULL: {
      return make_scoped_ptr(base::Value::CreateNullValue());
    }
    case base::Value::TYPE_BOOLEAN: {
      // Accept booleans encoded as either string or integer.
      if (value.GetAsInteger(&int_value) ||
          (value.GetAsString(&string_value) &&
           base::StringToInt(string_value, &int_value))) {
        return make_scoped_ptr(Value::CreateBooleanValue(int_value != 0));
      }
      break;
    }
    case base::Value::TYPE_INTEGER: {
      // Integers may be string-encoded.
      if (value.GetAsString(&string_value) &&
          base::StringToInt(string_value, &int_value)) {
        return make_scoped_ptr(base::Value::CreateIntegerValue(int_value));
      }
      break;
    }
    case base::Value::TYPE_DOUBLE: {
      // Doubles may be string-encoded or integer-encoded.
      double double_value = 0;
      if (value.GetAsInteger(&int_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(int_value));
      } else if (value.GetAsString(&string_value) &&
                 base::StringToDouble(string_value, &double_value)) {
        return make_scoped_ptr(base::Value::CreateDoubleValue(double_value));
      }
      break;
    }
    case base::Value::TYPE_LIST: {
      // Lists are encoded as subkeys with numbered value in the registry.
      const base::DictionaryValue* dict = NULL;
      if (value.GetAsDictionary(&dict)) {
        scoped_ptr<base::ListValue> result(new base::ListValue());
        for (int i = 1; ; ++i) {
          const base::Value* entry = NULL;
          if (!dict->Get(base::IntToString(i), &entry))
            break;
          scoped_ptr<base::Value> converted =
              ConvertValue(*entry, schema.GetItems());
          if (converted)
            result->Append(converted.release());
        }
        return result.Pass();
      }
      // Fall through in order to accept lists encoded as JSON strings.
    }
    case base::Value::TYPE_DICTIONARY: {
      // Dictionaries may be encoded as JSON strings.
      if (value.GetAsString(&string_value)) {
        scoped_ptr<base::Value> result(base::JSONReader::Read(string_value));
        if (result && result->IsType(schema.type()))
          return result.Pass();
      }
      break;
    }
    case base::Value::TYPE_STRING:
    case base::Value::TYPE_BINARY:
      // No conversion possible.
      break;
  }

  LOG(WARNING) << "Failed to convert " << value.GetType()
               << " to " << schema.type();
  return scoped_ptr<base::Value>();
}

}  // namespace

bool CaseInsensitiveStringCompare::operator()(const std::string& a,
                                              const std::string& b) const {
  return base::strcasecmp(a.c_str(), b.c_str()) < 0;
}

RegistryDict::RegistryDict() {}

RegistryDict::~RegistryDict() {
  ClearKeys();
  ClearValues();
}

RegistryDict* RegistryDict::GetKey(const std::string& name) {
  KeyMap::iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

const RegistryDict* RegistryDict::GetKey(const std::string& name) const {
  KeyMap::const_iterator entry = keys_.find(name);
  return entry != keys_.end() ? entry->second : NULL;
}

void RegistryDict::SetKey(const std::string& name,
                          scoped_ptr<RegistryDict> dict) {
  if (!dict) {
    RemoveKey(name);
    return;
  }

  RegistryDict*& entry = keys_[name];
  delete entry;
  entry = dict.release();
}

scoped_ptr<RegistryDict> RegistryDict::RemoveKey(const std::string& name) {
  scoped_ptr<RegistryDict> result;
  KeyMap::iterator entry = keys_.find(name);
  if (entry != keys_.end()) {
    result.reset(entry->second);
    keys_.erase(entry);
  }
  return result.Pass();
}

void RegistryDict::ClearKeys() {
  STLDeleteValues(&keys_);
}

base::Value* RegistryDict::GetValue(const std::string& name) {
  ValueMap::iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

const base::Value* RegistryDict::GetValue(const std::string& name) const {
  ValueMap::const_iterator entry = values_.find(name);
  return entry != values_.end() ? entry->second : NULL;
}

void RegistryDict::SetValue(const std::string& name,
                            scoped_ptr<base::Value> dict) {
  if (!dict) {
    RemoveValue(name);
    return;
  }

  Value*& entry = values_[name];
  delete entry;
  entry = dict.release();
}

scoped_ptr<base::Value> RegistryDict::RemoveValue(const std::string& name) {
  scoped_ptr<base::Value> result;
  ValueMap::iterator entry = values_.find(name);
  if (entry != values_.end()) {
    result.reset(entry->second);
    values_.erase(entry);
  }
  return result.Pass();
}

void RegistryDict::ClearValues() {
  STLDeleteValues(&values_);
}

void RegistryDict::Merge(const RegistryDict& other) {
  for (KeyMap::const_iterator entry(other.keys_.begin());
       entry != other.keys_.end(); ++entry) {
    RegistryDict*& subdict = keys_[entry->first];
    if (!subdict)
      subdict = new RegistryDict();
    subdict->Merge(*entry->second);
  }

  for (ValueMap::const_iterator entry(other.values_.begin());
       entry != other.values_.end(); ++entry) {
    SetValue(entry->first, make_scoped_ptr(entry->second->DeepCopy()));
  }
}

void RegistryDict::Swap(RegistryDict* other) {
  keys_.swap(other->keys_);
  values_.swap(other->values_);
}

void RegistryDict::ReadRegistry(HKEY hive, const base::string16& root) {
  ClearKeys();
  ClearValues();

  // First, read all the values of the key.
  for (RegistryValueIterator it(hive, root.c_str()); it.Valid(); ++it) {
    const std::string name = UTF16ToUTF8(it.Name());
    switch (it.Type()) {
      case REG_SZ:
      case REG_EXPAND_SZ:
        SetValue(
            name,
            make_scoped_ptr(new base::StringValue(UTF16ToUTF8(it.Value()))));
        continue;
      case REG_DWORD_LITTLE_ENDIAN:
      case REG_DWORD_BIG_ENDIAN:
        if (it.ValueSize() == sizeof(DWORD)) {
          DWORD dword_value = *(reinterpret_cast<const DWORD*>(it.Value()));
          if (it.Type() == REG_DWORD_BIG_ENDIAN)
            dword_value = base::NetToHost32(dword_value);
          else
            dword_value = base::ByteSwapToLE32(dword_value);
          SetValue(
              name,
              make_scoped_ptr(base::Value::CreateIntegerValue(dword_value)));
          continue;
        }
      case REG_NONE:
      case REG_LINK:
      case REG_MULTI_SZ:
      case REG_RESOURCE_LIST:
      case REG_FULL_RESOURCE_DESCRIPTOR:
      case REG_RESOURCE_REQUIREMENTS_LIST:
      case REG_QWORD_LITTLE_ENDIAN:
        // Unsupported type, message gets logged below.
        break;
    }

    LOG(WARNING) << "Failed to read hive " << hive << " at "
                 << root << "\\" << name
                 << " type " << it.Type();
  }

  // Recurse for all subkeys.
  for (RegistryKeyIterator it(hive, root.c_str()); it.Valid(); ++it) {
    std::string name(UTF16ToUTF8(it.Name()));
    scoped_ptr<RegistryDict> subdict(new RegistryDict());
    subdict->ReadRegistry(hive, root + L"\\" + it.Name());
    SetKey(name, subdict.Pass());
  }
}

scoped_ptr<base::Value> RegistryDict::ConvertToJSON(
    const Schema& schema) const {
  base::Value::Type type =
      schema.valid() ? schema.type() : base::Value::TYPE_DICTIONARY;
  switch (type) {
    case base::Value::TYPE_DICTIONARY: {
      scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue());
      for (RegistryDict::ValueMap::const_iterator entry(values_.begin());
           entry != values_.end(); ++entry) {
        Schema subschema =
            schema.valid() ? schema.GetProperty(entry->first) : Schema();
        scoped_ptr<base::Value> converted =
            ConvertValue(*entry->second, subschema);
        if (converted)
          result->SetWithoutPathExpansion(entry->first, converted.release());
      }
      for (RegistryDict::KeyMap::const_iterator entry(keys_.begin());
           entry != keys_.end(); ++entry) {
        Schema subschema =
            schema.valid() ? schema.GetProperty(entry->first) : Schema();
        scoped_ptr<base::Value> converted =
            entry->second->ConvertToJSON(subschema);
        if (converted)
          result->SetWithoutPathExpansion(entry->first, converted.release());
      }
      return result.Pass();
    }
    case base::Value::TYPE_LIST: {
      scoped_ptr<base::ListValue> result(new base::ListValue());
      Schema item_schema = schema.valid() ? schema.GetItems() : Schema();
      for (int i = 1; ; ++i) {
        const std::string name(base::IntToString(i));
        const RegistryDict* key = GetKey(name);
        if (key) {
          scoped_ptr<base::Value> converted = key->ConvertToJSON(item_schema);
          if (converted)
            result->Append(converted.release());
          continue;
        }
        const base::Value* value = GetValue(name);
        if (value) {
          scoped_ptr<base::Value> converted = ConvertValue(*value, item_schema);
          if (converted)
            result->Append(converted.release());
          continue;
        }
        break;
      }
      return result.Pass();
    }
    default:
      LOG(WARNING) << "Can't convert registry key to schema type " << type;
  }

  return scoped_ptr<base::Value>();
}

}  // namespace policy
