// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/json_schema/json_schema_validator.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/json_schema/json_schema_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace schema = json_schema_constants;

namespace {

double GetNumberValue(const base::Value* value) {
  double result = 0;
  CHECK(value->GetAsDouble(&result))
      << "Unexpected value type: " << value->GetType();
  return result;
}

bool IsValidType(const std::string& type) {
  static const char* kValidTypes[] = {
    schema::kAny,
    schema::kArray,
    schema::kBoolean,
    schema::kInteger,
    schema::kNull,
    schema::kNumber,
    schema::kObject,
    schema::kString,
  };
  const char** end = kValidTypes + arraysize(kValidTypes);
  return std::find(kValidTypes, end, type) != end;
}

// Maps a schema attribute name to its expected type.
struct ExpectedType {
  const char* key;
  base::Value::Type type;
};

// Helper for std::lower_bound.
bool CompareToString(const ExpectedType& entry, const std::string& key) {
  return entry.key < key;
}

bool IsValidSchema(const base::DictionaryValue* dict, std::string* error) {
  // This array must be sorted, so that std::lower_bound can perform a
  // binary search.
  static const ExpectedType kExpectedTypes[] = {
    // Note: kRef == "$ref", kSchema == "$schema"
    { schema::kRef,                     base::Value::TYPE_STRING      },
    { schema::kSchema,                  base::Value::TYPE_STRING      },

    { schema::kAdditionalProperties,    base::Value::TYPE_DICTIONARY  },
    { schema::kChoices,                 base::Value::TYPE_LIST        },
    { schema::kDescription,             base::Value::TYPE_STRING      },
    { schema::kEnum,                    base::Value::TYPE_LIST        },
    { schema::kId,                      base::Value::TYPE_STRING      },
    { schema::kMaxItems,                base::Value::TYPE_INTEGER     },
    { schema::kMaxLength,               base::Value::TYPE_INTEGER     },
    { schema::kMaximum,                 base::Value::TYPE_DOUBLE      },
    { schema::kMinItems,                base::Value::TYPE_INTEGER     },
    { schema::kMinLength,               base::Value::TYPE_INTEGER     },
    { schema::kMinimum,                 base::Value::TYPE_DOUBLE      },
    { schema::kOptional,                base::Value::TYPE_BOOLEAN     },
    { schema::kProperties,              base::Value::TYPE_DICTIONARY  },
    { schema::kTitle,                   base::Value::TYPE_STRING      },
  };

  bool has_type = false;
  const base::ListValue* list_value = NULL;
  const base::DictionaryValue* dictionary_value = NULL;
  std::string string_value;

  for (base::DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    // Validate the "type" attribute, which may be a string or a list.
    if (it.key() == schema::kType) {
      switch (it.value().GetType()) {
        case base::Value::TYPE_STRING:
          it.value().GetAsString(&string_value);
          if (!IsValidType(string_value)) {
            *error = "Invalid value for type attribute";
            return false;
          }
          break;
        case base::Value::TYPE_LIST:
          it.value().GetAsList(&list_value);
          for (size_t i = 0; i < list_value->GetSize(); ++i) {
            if (!list_value->GetString(i, &string_value) ||
                !IsValidType(string_value)) {
              *error = "Invalid value for type attribute";
              return false;
            }
          }
          break;
        default:
          *error = "Invalid value for type attribute";
          return false;
      }
      has_type = true;
      continue;
    }

    // Validate the "items" attribute, which is a schema or a list of schemas.
    if (it.key() == schema::kItems) {
      if (it.value().GetAsDictionary(&dictionary_value)) {
        if (!IsValidSchema(dictionary_value, error)) {
          DCHECK(!error->empty());
          return false;
        }
      } else if (it.value().GetAsList(&list_value)) {
        for (size_t i = 0; i < list_value->GetSize(); ++i) {
          if (!list_value->GetDictionary(i, &dictionary_value)) {
            *error = base::StringPrintf(
                "Invalid entry in items attribute at index %d",
                static_cast<int>(i));
            return false;
          }
          if (!IsValidSchema(dictionary_value, error)) {
            DCHECK(!error->empty());
            return false;
          }
        }
      } else {
        *error = "Invalid value for items attribute";
        return false;
      }
      continue;
    }

    // All the other attributes have a single valid type.
    const ExpectedType* end = kExpectedTypes + arraysize(kExpectedTypes);
    const ExpectedType* entry = std::lower_bound(
        kExpectedTypes, end, it.key(), CompareToString);
    if (entry == end || entry->key != it.key()) {
      *error = base::StringPrintf("Invalid attribute %s", it.key().c_str());
      return false;
    }
    if (!it.value().IsType(entry->type)) {
      *error = base::StringPrintf("Invalid value for %s attribute",
                                  it.key().c_str());
      return false;
    }

    // base::Value::TYPE_INTEGER attributes must be >= 0.
    // This applies to "minItems", "maxItems", "minLength" and "maxLength".
    if (it.value().IsType(base::Value::TYPE_INTEGER)) {
      int integer_value;
      it.value().GetAsInteger(&integer_value);
      if (integer_value < 0) {
        *error = base::StringPrintf("Value of %s must be >= 0, got %d",
                                    it.key().c_str(), integer_value);
        return false;
      }
    }

    // Validate the "properties" attribute. Each entry maps a key to a schema.
    if (it.key() == schema::kProperties) {
      it.value().GetAsDictionary(&dictionary_value);
      for (base::DictionaryValue::Iterator it(*dictionary_value);
           !it.IsAtEnd(); it.Advance()) {
        if (!it.value().GetAsDictionary(&dictionary_value)) {
          *error = "Invalid value for properties attribute";
          return false;
        }
        if (!IsValidSchema(dictionary_value, error)) {
          DCHECK(!error->empty());
          return false;
        }
      }
    }

    // Validate "additionalProperties" attribute, which is a schema.
    if (it.key() == schema::kAdditionalProperties) {
      it.value().GetAsDictionary(&dictionary_value);
      if (!IsValidSchema(dictionary_value, error)) {
        DCHECK(!error->empty());
        return false;
      }
    }

    // Validate the values contained in an "enum" attribute.
    if (it.key() == schema::kEnum) {
      it.value().GetAsList(&list_value);
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        const base::Value* value = NULL;
        list_value->Get(i, &value);
        switch (value->GetType()) {
          case base::Value::TYPE_NULL:
          case base::Value::TYPE_BOOLEAN:
          case base::Value::TYPE_INTEGER:
          case base::Value::TYPE_DOUBLE:
          case base::Value::TYPE_STRING:
            break;
          default:
            *error = "Invalid value in enum attribute";
            return false;
        }
      }
    }

    // Validate the schemas contained in a "choices" attribute.
    if (it.key() == schema::kChoices) {
      it.value().GetAsList(&list_value);
      for (size_t i = 0; i < list_value->GetSize(); ++i) {
        if (!list_value->GetDictionary(i, &dictionary_value)) {
          *error = "Invalid choices attribute";
          return false;
        }
        if (!IsValidSchema(dictionary_value, error)) {
          DCHECK(!error->empty());
          return false;
        }
      }
    }
  }

  if (!has_type) {
    *error = "Schema must have a type attribute";
    return false;
  }

  return true;
}

}  // namespace


JSONSchemaValidator::Error::Error() {
}

JSONSchemaValidator::Error::Error(const std::string& message)
    : path(message) {
}

JSONSchemaValidator::Error::Error(const std::string& path,
                                  const std::string& message)
    : path(path), message(message) {
}


const char JSONSchemaValidator::kUnknownTypeReference[] =
    "Unknown schema reference: *.";
const char JSONSchemaValidator::kInvalidChoice[] =
    "Value does not match any valid type choices.";
const char JSONSchemaValidator::kInvalidEnum[] =
    "Value does not match any valid enum choices.";
const char JSONSchemaValidator::kObjectPropertyIsRequired[] =
    "Property is required.";
const char JSONSchemaValidator::kUnexpectedProperty[] =
    "Unexpected property.";
const char JSONSchemaValidator::kArrayMinItems[] =
    "Array must have at least * items.";
const char JSONSchemaValidator::kArrayMaxItems[] =
    "Array must not have more than * items.";
const char JSONSchemaValidator::kArrayItemRequired[] =
    "Item is required.";
const char JSONSchemaValidator::kStringMinLength[] =
    "String must be at least * characters long.";
const char JSONSchemaValidator::kStringMaxLength[] =
    "String must not be more than * characters long.";
const char JSONSchemaValidator::kStringPattern[] =
    "String must match the pattern: *.";
const char JSONSchemaValidator::kNumberMinimum[] =
    "Value must not be less than *.";
const char JSONSchemaValidator::kNumberMaximum[] =
    "Value must not be greater than *.";
const char JSONSchemaValidator::kInvalidType[] =
    "Expected '*' but got '*'.";
const char JSONSchemaValidator::kInvalidTypeIntegerNumber[] =
    "Expected 'integer' but got 'number', consider using Math.round().";


// static
std::string JSONSchemaValidator::GetJSONSchemaType(const base::Value* value) {
  switch (value->GetType()) {
    case base::Value::TYPE_NULL:
      return schema::kNull;
    case base::Value::TYPE_BOOLEAN:
      return schema::kBoolean;
    case base::Value::TYPE_INTEGER:
      return schema::kInteger;
    case base::Value::TYPE_DOUBLE: {
      double double_value = 0;
      value->GetAsDouble(&double_value);
      if (std::abs(double_value) <= std::pow(2.0, DBL_MANT_DIG) &&
          double_value == floor(double_value)) {
        return schema::kInteger;
      } else {
        return schema::kNumber;
      }
    }
    case base::Value::TYPE_STRING:
      return schema::kString;
    case base::Value::TYPE_DICTIONARY:
      return schema::kObject;
    case base::Value::TYPE_LIST:
      return schema::kArray;
    default:
      NOTREACHED() << "Unexpected value type: " << value->GetType();
      return std::string();
  }
}

// static
std::string JSONSchemaValidator::FormatErrorMessage(const std::string& format,
                                                    const std::string& s1) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  return ret_val;
}

// static
std::string JSONSchemaValidator::FormatErrorMessage(const std::string& format,
                                                    const std::string& s1,
                                                    const std::string& s2) {
  std::string ret_val = format;
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s1);
  ReplaceFirstSubstringAfterOffset(&ret_val, 0, "*", s2);
  return ret_val;
}

// static
scoped_ptr<base::DictionaryValue> JSONSchemaValidator::IsValidSchema(
    const std::string& schema,
    std::string* error) {
  base::JSONParserOptions options = base::JSON_PARSE_RFC;
  scoped_ptr<base::Value> json(
      base::JSONReader::ReadAndReturnError(schema, options, NULL, error));
  if (!json)
    return scoped_ptr<base::DictionaryValue>();
  base::DictionaryValue* dict = NULL;
  if (!json->GetAsDictionary(&dict)) {
    *error = "Schema must be a JSON object";
    return scoped_ptr<base::DictionaryValue>();
  }
  if (!::IsValidSchema(dict, error))
    return scoped_ptr<base::DictionaryValue>();
  ignore_result(json.release());
  return make_scoped_ptr(dict);
}

JSONSchemaValidator::JSONSchemaValidator(base::DictionaryValue* schema)
    : schema_root_(schema), default_allow_additional_properties_(false) {
}

JSONSchemaValidator::JSONSchemaValidator(base::DictionaryValue* schema,
                                         base::ListValue* types)
    : schema_root_(schema), default_allow_additional_properties_(false) {
  if (!types)
    return;

  for (size_t i = 0; i < types->GetSize(); ++i) {
    base::DictionaryValue* type = NULL;
    CHECK(types->GetDictionary(i, &type));

    std::string id;
    CHECK(type->GetString(schema::kId, &id));

    CHECK(types_.find(id) == types_.end());
    types_[id] = type;
  }
}

JSONSchemaValidator::~JSONSchemaValidator() {}

bool JSONSchemaValidator::Validate(const base::Value* instance) {
  errors_.clear();
  Validate(instance, schema_root_, std::string());
  return errors_.empty();
}

void JSONSchemaValidator::Validate(const base::Value* instance,
                                   const base::DictionaryValue* schema,
                                   const std::string& path) {
  // If this schema defines itself as reference type, save it in this.types.
  std::string id;
  if (schema->GetString(schema::kId, &id)) {
    TypeMap::iterator iter = types_.find(id);
    if (iter == types_.end())
      types_[id] = schema;
    else
      DCHECK(iter->second == schema);
  }

  // If the schema has a $ref property, the instance must validate against
  // that schema. It must be present in types_ to be referenced.
  std::string ref;
  if (schema->GetString(schema::kRef, &ref)) {
    TypeMap::iterator type = types_.find(ref);
    if (type == types_.end()) {
      errors_.push_back(
          Error(path, FormatErrorMessage(kUnknownTypeReference, ref)));
    } else {
      Validate(instance, type->second, path);
    }
    return;
  }

  // If the schema has a choices property, the instance must validate against at
  // least one of the items in that array.
  const base::ListValue* choices = NULL;
  if (schema->GetList(schema::kChoices, &choices)) {
    ValidateChoices(instance, choices, path);
    return;
  }

  // If the schema has an enum property, the instance must be one of those
  // values.
  const base::ListValue* enumeration = NULL;
  if (schema->GetList(schema::kEnum, &enumeration)) {
    ValidateEnum(instance, enumeration, path);
    return;
  }

  std::string type;
  schema->GetString(schema::kType, &type);
  CHECK(!type.empty());
  if (type != schema::kAny) {
    if (!ValidateType(instance, type, path))
      return;

    // These casts are safe because of checks in ValidateType().
    if (type == schema::kObject) {
      ValidateObject(static_cast<const base::DictionaryValue*>(instance),
                     schema,
                     path);
    } else if (type == schema::kArray) {
      ValidateArray(static_cast<const base::ListValue*>(instance),
                    schema, path);
    } else if (type == schema::kString) {
      // Intentionally NOT downcasting to StringValue*. TYPE_STRING only implies
      // GetAsString() can safely be carried out, not that it's a StringValue.
      ValidateString(instance, schema, path);
    } else if (type == schema::kNumber || type == schema::kInteger) {
      ValidateNumber(instance, schema, path);
    } else if (type != schema::kBoolean && type != schema::kNull) {
      NOTREACHED() << "Unexpected type: " << type;
    }
  }
}

void JSONSchemaValidator::ValidateChoices(const base::Value* instance,
                                          const base::ListValue* choices,
                                          const std::string& path) {
  size_t original_num_errors = errors_.size();

  for (size_t i = 0; i < choices->GetSize(); ++i) {
    const base::DictionaryValue* choice = NULL;
    CHECK(choices->GetDictionary(i, &choice));

    Validate(instance, choice, path);
    if (errors_.size() == original_num_errors)
      return;

    // We discard the error from each choice. We only want to know if any of the
    // validations succeeded.
    errors_.resize(original_num_errors);
  }

  // Now add a generic error that no choices matched.
  errors_.push_back(Error(path, kInvalidChoice));
  return;
}

void JSONSchemaValidator::ValidateEnum(const base::Value* instance,
                                       const base::ListValue* choices,
                                       const std::string& path) {
  for (size_t i = 0; i < choices->GetSize(); ++i) {
    const base::Value* choice = NULL;
    CHECK(choices->Get(i, &choice));
    switch (choice->GetType()) {
      case base::Value::TYPE_NULL:
      case base::Value::TYPE_BOOLEAN:
      case base::Value::TYPE_STRING:
        if (instance->Equals(choice))
          return;
        break;

      case base::Value::TYPE_INTEGER:
      case base::Value::TYPE_DOUBLE:
        if (instance->IsType(base::Value::TYPE_INTEGER) ||
            instance->IsType(base::Value::TYPE_DOUBLE)) {
          if (GetNumberValue(choice) == GetNumberValue(instance))
            return;
        }
        break;

      default:
       NOTREACHED() << "Unexpected type in enum: " << choice->GetType();
    }
  }

  errors_.push_back(Error(path, kInvalidEnum));
}

void JSONSchemaValidator::ValidateObject(const base::DictionaryValue* instance,
                                         const base::DictionaryValue* schema,
                                         const std::string& path) {
  const base::DictionaryValue* properties = NULL;
  schema->GetDictionary(schema::kProperties, &properties);
  if (properties) {
    for (base::DictionaryValue::Iterator it(*properties); !it.IsAtEnd();
         it.Advance()) {
      std::string prop_path = path.empty() ? it.key() : (path + "." + it.key());
      const base::DictionaryValue* prop_schema = NULL;
      CHECK(it.value().GetAsDictionary(&prop_schema));

      const base::Value* prop_value = NULL;
      if (instance->Get(it.key(), &prop_value)) {
        Validate(prop_value, prop_schema, prop_path);
      } else {
        // Properties are required unless there is an optional field set to
        // 'true'.
        bool is_optional = false;
        prop_schema->GetBoolean(schema::kOptional, &is_optional);
        if (!is_optional) {
          errors_.push_back(Error(prop_path, kObjectPropertyIsRequired));
        }
      }
    }
  }

  const base::DictionaryValue* additional_properties_schema = NULL;
  if (SchemaAllowsAnyAdditionalItems(schema, &additional_properties_schema))
    return;

  // Validate additional properties.
  for (base::DictionaryValue::Iterator it(*instance); !it.IsAtEnd();
       it.Advance()) {
    if (properties && properties->HasKey(it.key()))
      continue;

    std::string prop_path = path.empty() ? it.key() : path + "." + it.key();
    if (!additional_properties_schema) {
      errors_.push_back(Error(prop_path, kUnexpectedProperty));
    } else {
      Validate(&it.value(), additional_properties_schema, prop_path);
    }
  }
}

void JSONSchemaValidator::ValidateArray(const base::ListValue* instance,
                                        const base::DictionaryValue* schema,
                                        const std::string& path) {
  const base::DictionaryValue* single_type = NULL;
  size_t instance_size = instance->GetSize();
  if (schema->GetDictionary(schema::kItems, &single_type)) {
    int min_items = 0;
    if (schema->GetInteger(schema::kMinItems, &min_items)) {
      CHECK(min_items >= 0);
      if (instance_size < static_cast<size_t>(min_items)) {
        errors_.push_back(Error(path, FormatErrorMessage(
            kArrayMinItems, base::IntToString(min_items))));
      }
    }

    int max_items = 0;
    if (schema->GetInteger(schema::kMaxItems, &max_items)) {
      CHECK(max_items >= 0);
      if (instance_size > static_cast<size_t>(max_items)) {
        errors_.push_back(Error(path, FormatErrorMessage(
            kArrayMaxItems, base::IntToString(max_items))));
      }
    }

    // If the items property is a single schema, each item in the array must
    // validate against that schema.
    for (size_t i = 0; i < instance_size; ++i) {
      const base::Value* item = NULL;
      CHECK(instance->Get(i, &item));
      std::string i_str = base::Uint64ToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      Validate(item, single_type, item_path);
    }

    return;
  }

  // Otherwise, the list must be a tuple type, where each item in the list has a
  // particular schema.
  ValidateTuple(instance, schema, path);
}

void JSONSchemaValidator::ValidateTuple(const base::ListValue* instance,
                                        const base::DictionaryValue* schema,
                                        const std::string& path) {
  const base::ListValue* tuple_type = NULL;
  schema->GetList(schema::kItems, &tuple_type);
  size_t tuple_size = tuple_type ? tuple_type->GetSize() : 0;
  if (tuple_type) {
    for (size_t i = 0; i < tuple_size; ++i) {
      std::string i_str = base::Uint64ToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      const base::DictionaryValue* item_schema = NULL;
      CHECK(tuple_type->GetDictionary(i, &item_schema));
      const base::Value* item_value = NULL;
      instance->Get(i, &item_value);
      if (item_value && item_value->GetType() != base::Value::TYPE_NULL) {
        Validate(item_value, item_schema, item_path);
      } else {
        bool is_optional = false;
        item_schema->GetBoolean(schema::kOptional, &is_optional);
        if (!is_optional) {
          errors_.push_back(Error(item_path, kArrayItemRequired));
          return;
        }
      }
    }
  }

  const base::DictionaryValue* additional_properties_schema = NULL;
  if (SchemaAllowsAnyAdditionalItems(schema, &additional_properties_schema))
    return;

  size_t instance_size = instance->GetSize();
  if (additional_properties_schema) {
    // Any additional properties must validate against the additionalProperties
    // schema.
    for (size_t i = tuple_size; i < instance_size; ++i) {
      std::string i_str = base::Uint64ToString(i);
      std::string item_path = path.empty() ? i_str : (path + "." + i_str);
      const base::Value* item_value = NULL;
      CHECK(instance->Get(i, &item_value));
      Validate(item_value, additional_properties_schema, item_path);
    }
  } else if (instance_size > tuple_size) {
    errors_.push_back(Error(path, FormatErrorMessage(
        kArrayMaxItems, base::Uint64ToString(tuple_size))));
  }
}

void JSONSchemaValidator::ValidateString(const base::Value* instance,
                                         const base::DictionaryValue* schema,
                                         const std::string& path) {
  std::string value;
  CHECK(instance->GetAsString(&value));

  int min_length = 0;
  if (schema->GetInteger(schema::kMinLength, &min_length)) {
    CHECK(min_length >= 0);
    if (value.size() < static_cast<size_t>(min_length)) {
      errors_.push_back(Error(path, FormatErrorMessage(
          kStringMinLength, base::IntToString(min_length))));
    }
  }

  int max_length = 0;
  if (schema->GetInteger(schema::kMaxLength, &max_length)) {
    CHECK(max_length >= 0);
    if (value.size() > static_cast<size_t>(max_length)) {
      errors_.push_back(Error(path, FormatErrorMessage(
          kStringMaxLength, base::IntToString(max_length))));
    }
  }

  CHECK(!schema->HasKey(schema::kPattern)) << "Pattern is not supported.";
}

void JSONSchemaValidator::ValidateNumber(const base::Value* instance,
                                         const base::DictionaryValue* schema,
                                         const std::string& path) {
  double value = GetNumberValue(instance);

  // TODO(aa): It would be good to test that the double is not infinity or nan,
  // but isnan and isinf aren't defined on Windows.

  double minimum = 0;
  if (schema->GetDouble(schema::kMinimum, &minimum)) {
    if (value < minimum)
      errors_.push_back(Error(path, FormatErrorMessage(
          kNumberMinimum, base::DoubleToString(minimum))));
  }

  double maximum = 0;
  if (schema->GetDouble(schema::kMaximum, &maximum)) {
    if (value > maximum)
      errors_.push_back(Error(path, FormatErrorMessage(
          kNumberMaximum, base::DoubleToString(maximum))));
  }
}

bool JSONSchemaValidator::ValidateType(const base::Value* instance,
                                       const std::string& expected_type,
                                       const std::string& path) {
  std::string actual_type = GetJSONSchemaType(instance);
  if (expected_type == actual_type ||
      (expected_type == schema::kNumber && actual_type == schema::kInteger)) {
    return true;
  } else if (expected_type == schema::kInteger &&
             actual_type == schema::kNumber) {
    errors_.push_back(Error(path, kInvalidTypeIntegerNumber));
    return false;
  } else {
    errors_.push_back(Error(path, FormatErrorMessage(
        kInvalidType, expected_type, actual_type)));
    return false;
  }
}

bool JSONSchemaValidator::SchemaAllowsAnyAdditionalItems(
    const base::DictionaryValue* schema,
    const base::DictionaryValue** additional_properties_schema) {
  // If the validator allows additional properties globally, and this schema
  // doesn't override, then we can exit early.
  schema->GetDictionary(schema::kAdditionalProperties,
                        additional_properties_schema);

  if (*additional_properties_schema) {
    std::string additional_properties_type(schema::kAny);
    CHECK((*additional_properties_schema)->GetString(
        schema::kType, &additional_properties_type));
    return additional_properties_type == schema::kAny;
  } else {
    return default_allow_additional_properties_;
  }
}
