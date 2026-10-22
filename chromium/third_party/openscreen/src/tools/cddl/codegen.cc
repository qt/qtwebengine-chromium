// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/cddl/codegen.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <format>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

// Convert '-' to '_' to use a CDDL identifier as a C identifier.
std::string ToUnderscoreId(const std::string& x) {
  std::string result(x);
  for (auto& c : result) {
    if (c == '-')
      c = '_';
  }
  return result;
}

// Return default value for each type. The default value is used to
// avoid undefined behavior when struct is initialized on the stack.
std::string GetTypeDefaultValue(const std::string& type) {
  if (type == "uint64_t") {
    return " = 0ull";
  } else if (type == "int64_t") {
    return " = 0ll";
  } else if (type == "bool") {
    return " = false";
  } else if (type == "float") {
    return " = 0.0f";
  } else if (type == "double") {
    return " = 0.0";
  } else if (type.find("std::array") != std::string::npos) {
    return "{}";
  } else if (type.find("std::optional") != std::string::npos) {
    return "";
  } else {
    return "";
  }
}

// Convert a CDDL identifier to camel case for use as a C typename.  E.g.
// presentation-connection-message to PresentationConnectionMessage.
std::string ToCamelCase(const std::string& x) {
  std::string result(x);
  result[0] = toupper(result[0]);
  size_t new_size = 1;
  size_t result_size = result.size();
  for (size_t i = 1; i < result_size; ++i, ++new_size) {
    if (result[i] == '-') {
      ++i;
      if (i < result_size)
        result[new_size] = toupper(result[i]);
    } else {
      result[new_size] = result[i];
    }
  }
  result.resize(new_size);
  return result;
}

// Returns a string which represents the C++ type of `cpp_type`.  Returns an
// empty string if there is no valid representation for `cpp_type` (e.g. a
// vector with an invalid element type).
std::string CppTypeToString(const CppType& cpp_type) {
  switch (cpp_type.which) {
    case CppType::Which::kBool:
      return "bool";
    case CppType::Which::kFloat:
      return "float";
    case CppType::Which::kFloat64:
      return "double";
    case CppType::Which::kInt64:
      return "int64_t";
    case CppType::Which::kUint64:
      return "uint64_t";
    case CppType::Which::kString:
      return "std::string";
    case CppType::Which::kBytes: {
      if (cpp_type.bytes_type.fixed_size) {
        std::string size_string =
            std::to_string(cpp_type.bytes_type.fixed_size.value());
        return "std::array<uint8_t, " + size_string + ">";
      } else {
        return "std::vector<uint8_t>";
      }
    }
    case CppType::Which::kVector: {
      std::string element_string =
          CppTypeToString(*cpp_type.vector_type.element_type);
      if (element_string.empty())
        return std::string();
      return "std::vector<" + element_string + ">";
    }
    case CppType::Which::kEnum:
      return ToCamelCase(cpp_type.name);
    case CppType::Which::kStruct:
      return ToCamelCase(cpp_type.name);
    case CppType::Which::kTaggedType:
      return CppTypeToString(*cpp_type.tagged_type.real_type);
    default:
      return std::string();
  }
}

template <typename... Args>
void Write(std::ostream& os, std::format_string<Args...> fmt, Args&&... args) {
  os << std::format(fmt, std::forward<Args>(args)...);
}

bool WriteEnumEqualityOperatorSwitchCases(std::ostream& os,
                                          const CppType& parent,
                                          std::string child_name,
                                          std::string parent_name) {
  for (const auto& x : parent.enum_type.members) {
    std::string enum_value = "k" + ToCamelCase(x.first);
    Write(os, "    case {}::{}: return parent == {}::{};\n", child_name,
          enum_value, parent_name, enum_value);
  }

  return std::all_of(parent.enum_type.sub_members.cbegin(),
                     parent.enum_type.sub_members.cend(),
                     [&os, &child_name, &parent_name](CppType* new_parent) {
                       return WriteEnumEqualityOperatorSwitchCases(
                           os, *new_parent, child_name, parent_name);
                     });
}

// Write the equality operators for comparing an enum and its parent types.
bool WriteEnumEqualityOperator(std::ostream& os,
                               const CppType& type,
                               const CppType& parent) {
  std::string name = ToCamelCase(type.name);
  std::string parent_name = ToCamelCase(parent.name);

  // Define type == parentType.
  Write(os, "inline bool operator==(const {}& child, const {}& parent) {{\n",
        name, parent_name);
  Write(os, "  switch (child) {{\n");
  if (!WriteEnumEqualityOperatorSwitchCases(os, parent, name, parent_name)) {
    return false;
  }
  Write(os, "    default: return false;\n");
  Write(os, "  }}\n}}\n");

  // Define parentType == type.
  Write(os, "inline bool operator==(const {}& parent, const {}& child) {{\n",
        parent_name, name);
  Write(os, "  return child == parent;\n}}\n");

  // Define type != parentType.
  Write(os, "inline bool operator!=(const {}& child, const {}& parent) {{\n",
        name, parent_name);
  Write(os, "  return !(child == parent);\n}}\n");

  // Define parentType != type.
  Write(os, "inline bool operator!=(const {}& parent, const {}& child) {{\n",
        parent_name, name);
  Write(os, "  return !(parent == child);\n}}\n");

  return true;
}

bool WriteEnumStreamOperatorSwitchCases(std::ostream& os,
                                        const CppType& type,
                                        std::string name) {
  for (const auto& x : type.enum_type.members) {
    std::string enum_value = "k" + ToCamelCase(x.first);
    Write(os, "    case {}::{}: os << \"{}\"; break;\n", name, enum_value,
          enum_value);
  }

  return std::all_of(
      type.enum_type.sub_members.cbegin(), type.enum_type.sub_members.cend(),
      [&os, &name](CppType* parent) {
        return WriteEnumStreamOperatorSwitchCases(os, *parent, name);
      });
}

bool WriteEnumOperators(std::ostream& os, const CppType& type) {
  // Write << operator.
  std::string name = ToCamelCase(type.name);
  Write(os,
        "inline std::ostream& operator<<(std::ostream& os, const {}& val) {{\n",
        name);
  Write(os, "  switch (val) {{\n");
  if (!WriteEnumStreamOperatorSwitchCases(os, type, name)) {
    return false;
  }
  Write(os,
        "    default: os << \"Unknown Value: \" << static_cast<int>(val);"
        "\n      break;\n    }}\n  return os;\n}}\n");

  // Write equality operators.
  return std::all_of(type.enum_type.sub_members.cbegin(),
                     type.enum_type.sub_members.cend(),
                     [&os, &type](CppType* parent) {
                       return WriteEnumEqualityOperator(os, type, *parent);
                     });
}

// Writes the equality operator for a specific Discriminated Union.
bool WriteDiscriminatedUnionEqualityOperator(
    std::ostream& os,
    const CppType& type,
    const std::string& name_prefix = "") {
  const std::string name = name_prefix + ToCamelCase(type.name);
  Write(os, "\nbool {}::operator==(const {}& other) const {{\n", name, name);
  Write(os, "  return this->which == other.which");
  for (auto* union_member : type.discriminated_union.members) {
    Write(os, " &&\n         ");
    switch (union_member->which) {
      case CppType::Which::kBool:
        Write(os,
              "(this->which != Which::kBool || this->bool_var == "
              "other.bool_var)");
        break;
      case CppType::Which::kFloat:
        Write(os,
              "(this->which != Which::kFloat || this->float_var == "
              "other.float_var)");
        break;
      case CppType::Which::kFloat64:
        Write(os,
              "(this->which != Which::kFloat64 || this->double_var == "
              "other.double_var)");
        break;
      case CppType::Which::kInt64:
        Write(
            os,
            "(this->which != Which::kInt64 || this->int_var == other.int_var)");
        break;
      case CppType::Which::kUint64:
        Write(os,
              "(this->which != Which::kUint64 || this->uint == other.uint)");
        break;
      case CppType::Which::kString:
        Write(os, "(this->which != Which::kString || this->str == other.str)");
        break;
      case CppType::Which::kBytes:
        Write(os,
              "(this->which != Which::kBytes || this->bytes == other.bytes)");
        break;
      default:
        return false;
    }
  }
  Write(os, ";\n}}\n");
  Write(os, "\nbool {}::operator!=(const {}& other) const {{\n", name, name);
  Write(os, "  return !(*this == other);\n}}\n");
  return true;
}

// Writes the equality operator for a specific C++ struct.
bool WriteStructEqualityOperator(std::ostream& os,
                                 const CppType& type,
                                 const std::string& name_prefix = "") {
  const std::string name = name_prefix + ToCamelCase(type.name);
  Write(os, "\nbool {}::operator==(const {}& other) const {{\n", name, name);
  for (size_t i = 0; i < type.struct_type.members.size(); i++) {
    if (i == 0) {
      Write(os, "  return ");
    } else {
      Write(os, " &&\n         ");
    }
    const auto member_name = ToUnderscoreId(type.struct_type.members[i].name);
    Write(os, "this->{} == other.{}", member_name, member_name);
  }
  Write(os, ";\n}}\n");
  Write(os, "\nbool {}::operator!=(const {}& other) const {{\n", name, name);
  Write(os, "  return !(*this == other);\n}}\n");
  std::string new_prefix = name_prefix + ToCamelCase(type.name) + "::";
  for (const auto& x : type.struct_type.members) {
    // NOTE: Don't need to call recursively on struct members, since all structs
    // are handled in the calling method.
    if (x.type->which == CppType::Which::kDiscriminatedUnion) {
      if (!WriteDiscriminatedUnionEqualityOperator(os, *x.type, new_prefix)) {
        return false;
      }
    }
  }
  return true;
}

// Write the C++ struct member definitions of every type in `members` to the
// file descriptor `os`.
bool WriteStructMembers(
    std::ostream& os,
    const std::string& name,
    const std::vector<CppType::Struct::CppMember>& members) {
  for (const auto& x : members) {
    std::string type_string;
    switch (x.type->which) {
      case CppType::Which::kStruct: {
        if (x.type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
          if (!WriteStructMembers(os, x.type->name,
                                  x.type->struct_type.members))
            return false;
          continue;
        } else {
          type_string = ToCamelCase(x.type->name);
        }
      } break;
      case CppType::Which::kOptional: {
        type_string =
            "std::optional<" + CppTypeToString(*x.type->optional_type) + ">";
      } break;
      case CppType::Which::kDiscriminatedUnion: {
        std::string cid = ToUnderscoreId(x.name);
        type_string = ToCamelCase(x.name);
        Write(os, "  struct {} {{\n", type_string);
        Write(os, "    {}();\n    ~{}();\n\n", type_string, type_string);

        Write(os, "  bool operator==(const {}& other) const;\n", type_string);
        Write(os, "  bool operator!=(const {}& other) const;\n\n", type_string);
        Write(os, "  enum class Which {{\n");
        for (auto* union_member : x.type->discriminated_union.members) {
          switch (union_member->which) {
            case CppType::Which::kBool:
              Write(os, "    kBool,\n");
              break;
            case CppType::Which::kFloat:
              Write(os, "    kFloat,\n");
              break;
            case CppType::Which::kFloat64:
              Write(os, "    kFloat64,\n");
              break;
            case CppType::Which::kInt64:
              Write(os, "    kInt64,\n");
              break;
            case CppType::Which::kUint64:
              Write(os, "    kUint64,\n");
              break;
            case CppType::Which::kString:
              Write(os, "    kString,\n");
              break;
            case CppType::Which::kBytes:
              Write(os, "    kBytes,\n");
              break;
            default:
              return false;
          }
        }
        Write(os, "    kUninitialized,\n");
        Write(os, "  }} which;\n");
        Write(os, "  union {{\n");
        for (auto* union_member : x.type->discriminated_union.members) {
          switch (union_member->which) {
            case CppType::Which::kBool:
              Write(os, "    bool bool_var;\n");
              break;
            case CppType::Which::kFloat:
              Write(os, "    float float_var;\n");
              break;
            case CppType::Which::kFloat64:
              Write(os, "    double double_var;\n");
              break;
            case CppType::Which::kInt64:
              Write(os, "    int64_t int_var;\n");
              break;
            case CppType::Which::kUint64:
              Write(os, "    uint64_t uint;\n");
              break;
            case CppType::Which::kString:
              Write(os, "    std::string str;\n");
              break;
            case CppType::Which::kBytes:
              Write(os, "    std::vector<uint8_t> bytes;\n");
              break;
            default:
              return false;
          }
        }
        // NOTE: This member allows the union to be easily constructed in an
        // effectively uninitialized state.  Its value should never be used.
        Write(os, "    bool placeholder_;\n");
        Write(os, "  }};\n");
        Write(os, "  }};\n");
      } break;
      default:
        type_string = CppTypeToString(*x.type);
        break;
    }
    if (type_string.empty())
      return false;
    Write(os, "  {} {}{};\n", type_string, ToUnderscoreId(x.name),
          GetTypeDefaultValue(type_string));
  }
  return true;
}

void WriteEnumMembers(std::ostream& os, const CppType& type) {
  for (const auto& x : type.enum_type.members) {
    Write(os, "  k{} = {}ull,\n", ToCamelCase(x.first), x.second);
  }
  for (const auto* x : type.enum_type.sub_members) {
    WriteEnumMembers(os, *x);
  }
}

// Writes a C++ type definition for `type` to the file descriptor `os`.  This
// only generates a definition for enums and structs.
bool WriteTypeDefinition(std::ostream& os, const CppType& type) {
  std::string name = ToCamelCase(type.name);
  switch (type.which) {
    case CppType::Which::kEnum: {
      Write(os, "\nenum class {} : uint64_t {{\n", name);
      WriteEnumMembers(os, type);
      Write(os, "}};\n");
      if (!WriteEnumOperators(os, type))
        return false;
    } break;
    case CppType::Which::kStruct: {
      Write(os, "\nstruct {} {{\n", name);
      if (type.type_key != std::nullopt) {
        Write(os, "  // type key: {}\n", type.type_key.value());
      }
      Write(os, "  bool operator==(const {}& other) const;\n", name);
      Write(os, "  bool operator!=(const {}& other) const;\n\n", name);
      if (!WriteStructMembers(os, type.name, type.struct_type.members))
        return false;
      Write(os, "}};\n");
    } break;
    default:
      break;
  }
  return true;
}

// Ensures that any dependencies within `cpp_type` are written to the file
// descriptor `os` before writing `cpp_type` to the file descriptor `os`.  This
// is done by walking the tree of types defined by `cpp_type` (e.g. all the
// members for a struct).  `defs` contains the names of types that have already
// been written.  If a type hasn't been written and needs to be, its name will
// also be added to `defs`.
bool EnsureDependentTypeDefinitionsWritten(std::ostream& os,
                                           const CppType& cpp_type,
                                           std::set<std::string>* defs) {
  switch (cpp_type.which) {
    case CppType::Which::kVector: {
      return EnsureDependentTypeDefinitionsWritten(
          os, *cpp_type.vector_type.element_type, defs);
    }
    case CppType::Which::kEnum: {
      if (defs->find(cpp_type.name) != defs->end())
        return true;
      for (const auto* x : cpp_type.enum_type.sub_members)
        if (!EnsureDependentTypeDefinitionsWritten(os, *x, defs))
          return false;
      defs->emplace(cpp_type.name);
      WriteTypeDefinition(os, cpp_type);
    } break;
    case CppType::Which::kStruct: {
      if (cpp_type.struct_type.key_type !=
          CppType::Struct::KeyType::kPlainGroup) {
        if (defs->find(cpp_type.name) != defs->end())
          return true;
        for (const auto& x : cpp_type.struct_type.members)
          if (!EnsureDependentTypeDefinitionsWritten(os, *x.type, defs))
            return false;
        defs->emplace(cpp_type.name);
        WriteTypeDefinition(os, cpp_type);
      }
    } break;
    case CppType::Which::kOptional: {
      return EnsureDependentTypeDefinitionsWritten(os, *cpp_type.optional_type,
                                                   defs);
    }
    case CppType::Which::kDiscriminatedUnion: {
      for (const auto* x : cpp_type.discriminated_union.members)
        if (!EnsureDependentTypeDefinitionsWritten(os, *x, defs))
          return false;
    } break;
    case CppType::Which::kTaggedType: {
      if (!EnsureDependentTypeDefinitionsWritten(
              os, *cpp_type.tagged_type.real_type, defs)) {
        return false;
      }
    } break;
    default:
      break;
  }
  return true;
}

// Writes the type definition for every C++ type in `table`.  This function
// makes sure to write them in such an order that all type dependencies are
// written before they are need so the resulting text in the file descriptor
// `os` will compile without modification.  For example, the following would be
// bad output:
//
// struct Foo {
//   Bar bar;
//   int x;
// };
//
// struct Bar {
//   int alpha;
// };
//
// This function ensures that Bar would be written sometime before Foo.
bool WriteTypeDefinitions(std::ostream& os, CppSymbolTable* table) {
  std::set<std::string> defs;
  for (const std::unique_ptr<CppType>& real_type : table->cpp_types) {
    if (real_type->which != CppType::Which::kStruct ||
        real_type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    if (!EnsureDependentTypeDefinitionsWritten(os, *real_type, &defs))
      return false;
  }

  Write(os, "\nenum class Type : uint64_t {{\n");
  Write(os, "  kUnknown = 0ull,\n");
  for (CppType* type : table->TypesWithId()) {
    Write(os, "  k{} = {}ull,\n", ToCamelCase(type->name),
          type->type_key.value());
  }
  Write(os, "}};\n");
  return true;
}

// Writes a parser that takes in a uint64_t and outputs the corresponding Type
// if one matches up, or Type::kUnknown if none does.
// NOTE: In future, this could be changes to use a Trie, which would allow for
// manufacturers to more easily add their own type ids to ours.
bool WriteTypeParserDefinition(std::ostream& os, CppSymbolTable* table) {
  Write(os, "\n//static\n");
  Write(os, "Type TypeEnumValidator::SafeCast(uint64_t type_id) {{\n");
  Write(os, "  switch (type_id) {{\n");
  for (CppType* type : table->TypesWithId()) {
    Write(os, "    case uint64_t{{{}}}: return Type::k{};\n",
          type->type_key.value(), ToCamelCase(type->name));
  }
  Write(os, "    default: return Type::kUnknown;\n");
  Write(os, "  }}\n}}\n");
  return true;
}

// Writes the function prototypes for the encode and decode functions for each
// type in `table` to the file descriptor `os`.
bool WriteFunctionDeclarations(std::ostream& os, CppSymbolTable* table) {
  for (CppType* real_type : table->TypesWithId()) {
    const auto& name = real_type->name;
    if (real_type->which != CppType::Which::kStruct ||
        real_type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
      return false;
    }
    std::string cpp_name = ToCamelCase(name);
    Write(os, "\nbool Encode{}(\n", cpp_name);
    Write(os, "    const {}& data,\n", cpp_name);
    Write(os, "    CborEncodeBuffer* buffer);\n");
    Write(os, "CborResult Encode{}(\n", cpp_name);
    Write(os, "    const {}& data,\n", cpp_name);
    Write(os, "    uint8_t* buffer,\n    size_t length);\n");
    Write(os, "CborResult Decode{}(\n", cpp_name);
    Write(os, "    const uint8_t* buffer,\n    size_t length,\n");
    Write(os, "    {}& data);\n", cpp_name);
  }
  return true;
}

bool WriteMapEncoder(std::ostream& os,
                     const std::string& name,
                     const std::vector<CppType::Struct::CppMember>& members,
                     const std::string& nested_type_scope,
                     int encoder_depth = 1);
bool WriteArrayEncoder(std::ostream& os,
                       const std::string& name,
                       const std::vector<CppType::Struct::CppMember>& members,
                       const std::string& nested_type_scope,
                       int encoder_depth = 1);

// Writes the encoding function for the C++ type `cpp_type` to the file
// descriptor `os`.  `name` is the C++ variable name that needs to be encoded.
// `nested_type_scope` is the closest C++ scope name (i.e. struct name), which
// may be used to access local enum constants.  `encoder_depth` is used to
// independently name independent cbor encoders that need to be created.
bool WriteEncoder(std::ostream& os,
                  const std::string& name,
                  const CppType& cpp_type,
                  const std::string& nested_type_scope,
                  int encoder_depth) {
  switch (cpp_type.which) {
    case CppType::Which::kStruct:
      if (cpp_type.struct_type.key_type == CppType::Struct::KeyType::kMap) {
        if (!WriteMapEncoder(os, name, cpp_type.struct_type.members,
                             cpp_type.name, encoder_depth + 1)) {
          return false;
        }
        return true;
      } else if (cpp_type.struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        if (!WriteArrayEncoder(os, name, cpp_type.struct_type.members,
                               cpp_type.name, encoder_depth + 1)) {
          return false;
        }
        return true;
      } else {
        for (const auto& x : cpp_type.struct_type.members) {
          if (x.integer_key.has_value()) {
            Write(os,
                  "  CBOR_RETURN_ON_ERROR(cbor_encode_uint("
                  "&encoder{}, {}));\n",
                  encoder_depth, x.integer_key.value());
          } else {
            Write(os,
                  "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string("
                  "&encoder{}, \"{}\", sizeof(\"{}\") - 1));\n",
                  encoder_depth, x.name, x.name);
          }
          if (!WriteEncoder(os, name + "." + ToUnderscoreId(x.name), *x.type,
                            nested_type_scope, encoder_depth)) {
            return false;
          }
        }
        return true;
      }
    case CppType::Which::kBool:
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encode_boolean(&encoder{}, {}));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    case CppType::Which::kFloat:
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_encode_float(&encoder{}, {}));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    case CppType::Which::kFloat64:
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_encode_double(&encoder{}, {}));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    case CppType::Which::kInt64:
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_encode_int(&encoder{}, {}));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    case CppType::Which::kUint64:
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder{}, {}));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    case CppType::Which::kString: {
      std::string cid = ToUnderscoreId(name);
      Write(os, "  if (!IsValidUtf8({})) {{\n", cid);
      Write(os, "    return -CborErrorInvalidUtf8TextString;\n");
      Write(os, "  }}\n");
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder{}, "
            "{}.c_str(), {}.size()));\n",
            encoder_depth, cid, cid);
      return true;
    }
    case CppType::Which::kBytes: {
      std::string cid = ToUnderscoreId(name);
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encode_byte_string(&encoder{}, "
            "{}.data(), "
            "{}.size()));\n",
            encoder_depth, cid, cid);
      return true;
    }
    case CppType::Which::kVector: {
      std::string cid = ToUnderscoreId(name);
      Write(os, "  {{\n");
      if (cpp_type.vector_type.min_length !=
          CppType::Vector::kMinLengthUnbounded) {
        Write(os, "  if ({}.size() < {}) {{\n", cid,
              cpp_type.vector_type.min_length);
        Write(os, "    return -CborErrorTooFewItems;\n");
        Write(os, "  }}\n");
      }
      if (cpp_type.vector_type.max_length !=
          CppType::Vector::kMaxLengthUnbounded) {
        Write(os, "  if ({}.size() > {}) {{\n", cid,
              cpp_type.vector_type.max_length);
        Write(os, "    return -CborErrorTooManyItems;\n");
        Write(os, "  }}\n");
      }
      Write(os, "  CborEncoder encoder{};\n", encoder_depth + 1);
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder{}, "
            "&encoder{}, {}.size()));\n",
            encoder_depth, encoder_depth + 1, cid);
      std::string loop_variable = "x" + std::to_string(encoder_depth + 1);
      Write(os, "  for (const auto& {} : {}) {{\n", loop_variable, cid);
      if (!WriteEncoder(os, loop_variable, *cpp_type.vector_type.element_type,
                        nested_type_scope, encoder_depth + 1)) {
        return false;
      }
      Write(os, "  }}\n");
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder{}, "
            "&encoder{}));\n",
            encoder_depth, encoder_depth + 1);
      Write(os, "  }}\n");
      return true;
    }
    case CppType::Which::kEnum: {
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder{}, "
            "static_cast<uint64_t>({})));\n",
            encoder_depth, ToUnderscoreId(name));
      return true;
    }
    case CppType::Which::kDiscriminatedUnion: {
      for (const auto* union_member : cpp_type.discriminated_union.members) {
        switch (union_member->which) {
          case CppType::Which::kBool:
            Write(os, "  case {}::{}::Which::kBool:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".bool_var"),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kFloat:
            Write(os, "  case {}::{}::Which::kFloat:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".float_var"),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kFloat64:
            Write(os, "  case {}::{}::Which::kFloat64:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".double_var"),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kInt64:
            Write(os, "  case {}::{}::Which::kInt64:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".int_var"),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kUint64:
            Write(os, "  case {}::{}::Which::kUint64:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".uint"), *union_member,
                              nested_type_scope, encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kString:
            Write(os, "  case {}::{}::Which::kString:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".str"), *union_member,
                              nested_type_scope, encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          case CppType::Which::kBytes:
            Write(os, "  case {}::{}::Which::kBytes:\n",
                  ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
            if (!WriteEncoder(os, ToUnderscoreId(name + ".bytes"),
                              *union_member, nested_type_scope,
                              encoder_depth)) {
              return false;
            }
            Write(os, "    break;\n");
            break;
          default:
            return false;
        }
      }
      Write(os, "  case {}::{}::Which::kUninitialized:\n",
            ToCamelCase(nested_type_scope), ToCamelCase(cpp_type.name));
      Write(os, "    return -CborUnknownError;\n");
      return true;
    }
    case CppType::Which::kTaggedType: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_encode_tag(&encoder{}, {}ull));\n",
            encoder_depth, cpp_type.tagged_type.tag);
      if (!WriteEncoder(os, name, *cpp_type.tagged_type.real_type,
                        nested_type_scope, encoder_depth)) {
        return false;
      }
      return true;
    }
    default:
      break;
  }
  return false;
}

struct MemberCountResult {
  int num_required;
  int num_optional;
};

MemberCountResult CountMemberTypes(
    std::ostream& os,
    const std::string& name_id,
    const std::vector<CppType::Struct::CppMember>& members) {
  int num_required = 0;
  int num_optional = 0;
  for (const auto& x : members) {
    if (x.type->which == CppType::Which::kOptional) {
      std::string x_id = ToUnderscoreId(x.name);
      if (num_optional == 0) {
        Write(os, "  int num_optionals_present = {}.{}.has_value() ? 1 : 0;\n",
              name_id, x_id);
      } else {
        Write(os, "  num_optionals_present += {}.{}.has_value() ? 1 : 0;\n",
              name_id, x_id);
      }
      ++num_optional;
    } else {
      ++num_required;
    }
  }
  return MemberCountResult{num_required, num_optional};
}

// Writes the encoding function for a CBOR map with the C++ type members in
// `members` to the file descriptor `os`.  `name` is the C++ variable name that
// needs to be encoded.  `nested_type_scope` is the closest C++ scope name (i.e.
// struct name), which may be used to access local enum constants.
// `encoder_depth` is used to independently name independent cbor encoders that
// need to be created.
bool WriteMapEncoder(std::ostream& os,
                     const std::string& name,
                     const std::vector<CppType::Struct::CppMember>& members,
                     const std::string& nested_type_scope,
                     int encoder_depth) {
  std::string name_id = ToUnderscoreId(name);
  Write(os, "  {{\n  CborEncoder encoder{};\n", encoder_depth);
  MemberCountResult member_counts = CountMemberTypes(os, name_id, members);
  if (member_counts.num_optional == 0) {
    Write(os,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder{}, "
          "&encoder{}, "
          "{}));\n",
          encoder_depth - 1, encoder_depth, member_counts.num_required);
  } else {
    Write(os,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_map(&encoder{}, "
          "&encoder{}, "
          "{} + num_optionals_present));\n",
          encoder_depth - 1, encoder_depth, member_counts.num_required);
  }

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.type;
    if (x.type->which != CppType::Which::kStruct ||
        x.type->struct_type.key_type != CppType::Struct::KeyType::kPlainGroup) {
      if (x.type->which == CppType::Which::kOptional) {
        member_type = x.type->optional_type;
        Write(os, "  if ({}.{}.has_value()) {{\n", name_id,
              ToUnderscoreId(x.name));
      }

      if (x.integer_key.has_value()) {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_uint(&encoder{}, {}"
              "));\n",
              encoder_depth, x.integer_key.value());
      } else {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(cbor_encode_text_string(&encoder{}, "
              "\"{}\", sizeof(\"{}\") - 1));\n",
              encoder_depth, x.name, x.name);
      }
      if (x.type->which == CppType::Which::kDiscriminatedUnion) {
        Write(os, "  switch ({}.{}.which) {{\n", fullname, x.name);
      }
      fullname = fullname + "." + x.name;
    }
    if (x.type->which == CppType::Which::kOptional) {
      fullname = "(*(" + fullname + "))";
    }
    if (!WriteEncoder(os, fullname, *member_type, nested_type_scope,
                      encoder_depth)) {
      return false;
    }
    if (x.type->which == CppType::Which::kOptional ||
        x.type->which == CppType::Which::kDiscriminatedUnion) {
      Write(os, "  }}\n");
    }
  }

  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder{}, "
        "&encoder{}));\n  }}\n",
        encoder_depth - 1, encoder_depth);
  return true;
}

// Writes the encoding function for a CBOR array with the C++ type members in
// `members` to the file descriptor `os`.  `name` is the C++ variable name that
// needs to be encoded.  `nested_type_scope` is the closest C++ scope name (i.e.
// struct name), which may be used to access local enum constants.
// `encoder_depth` is used to independently name independent cbor encoders that
// need to be created.
bool WriteArrayEncoder(std::ostream& os,
                       const std::string& name,
                       const std::vector<CppType::Struct::CppMember>& members,
                       const std::string& nested_type_scope,
                       int encoder_depth) {
  std::string name_id = ToUnderscoreId(name);
  Write(os, "  {{\n  CborEncoder encoder{};\n", encoder_depth);
  MemberCountResult member_counts = CountMemberTypes(os, name_id, members);
  if (member_counts.num_optional == 0) {
    Write(os,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder{}, "
          "&encoder{}, {}));\n",
          encoder_depth - 1, encoder_depth, member_counts.num_required);
  } else {
    Write(os,
          "  CBOR_RETURN_ON_ERROR(cbor_encoder_create_array(&encoder{}, "
          "&encoder{}, {} + num_optionals_present));\n",
          encoder_depth - 1, encoder_depth, member_counts.num_required);
  }

  for (const auto& x : members) {
    std::string fullname = name;
    CppType* member_type = x.type;
    if (x.type->which != CppType::Which::kStruct ||
        x.type->struct_type.key_type != CppType::Struct::KeyType::kPlainGroup) {
      if (x.type->which == CppType::Which::kOptional) {
        member_type = x.type->optional_type;
        Write(os, "  if ({}.{}.has_value()) {{\n", name_id,
              ToUnderscoreId(x.name));
      }
      if (x.type->which == CppType::Which::kDiscriminatedUnion) {
        Write(os, "  switch ({}.{}.which) {{\n", fullname, x.name);
      }
      fullname = fullname + "." + x.name;
    }
    if (x.type->which == CppType::Which::kOptional) {
      fullname = "(*(" + fullname + "))";
    }
    if (!WriteEncoder(os, fullname, *member_type, nested_type_scope,
                      encoder_depth)) {
      return false;
    }
    if (x.type->which == CppType::Which::kOptional ||
        x.type->which == CppType::Which::kDiscriminatedUnion) {
      Write(os, "  }}\n");
    }
  }

  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_encoder_close_container(&encoder{}, "
        "&encoder{}));\n  }}\n",
        encoder_depth - 1, encoder_depth);
  return true;
}

uint8_t GetByte(uint64_t value, size_t byte) {
  return static_cast<uint8_t>((value >> (byte * 8)) & 0xFF);
}

std::string GetEncodedTypeKey(const CppType& type) {
  if (type.type_key == std::nullopt) {
    return "";
  }

  // Determine all constants needed for calculating the encoded id bytes.
  uint64_t type_id = type.type_key.value();
  uint8_t encoding_size;
  uint8_t start_processing_byte;
  if (type_id < 0x1 << 6) {
    encoding_size = 0x0;
    start_processing_byte = 0;
  } else if (type_id < 0x1 << 14) {
    encoding_size = 0x01;
    start_processing_byte = 1;
  } else if (type_id < 0x1 << 30) {
    encoding_size = 0x02;
    start_processing_byte = 3;
  } else if (type_id < uint64_t{0x1} << 62) {
    encoding_size = 0x03;
    start_processing_byte = 7;
  } else {
    return "";
  }

  // Parse the encoded id into a string;
  uint8_t first_byte =
      encoding_size << 6 | GetByte(type_id, start_processing_byte);
  std::string result = std::format("{{0x{:x}", uint32_t{first_byte});
  for (int i = start_processing_byte - 1; i >= 0; i--) {
    result += std::format(", 0x{:x}", uint32_t{GetByte(type_id, i)});
  }
  result += "}";
  return result;
}

// Writes encoding functions for each type in `table` to the file descriptor
// `os`.
bool WriteEncoders(std::ostream& os, CppSymbolTable* table) {
  for (CppType* real_type : table->TypesWithId()) {
    const auto& name = real_type->name;
    if (real_type->which != CppType::Which::kStruct ||
        real_type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
      return false;
    }
    std::string cpp_name = ToCamelCase(name);

    for (const auto& x : real_type->struct_type.members) {
      if (x.type->which != CppType::Which::kDiscriminatedUnion)
        continue;
      std::string dunion_cpp_name = ToCamelCase(x.name);
      Write(os, "\n{}::{}::{}()\n", cpp_name, dunion_cpp_name, dunion_cpp_name);
      std::string cid = ToUnderscoreId(x.name);
      std::string type_name = ToCamelCase(x.name);
      Write(os,
            "    : which(Which::kUninitialized), placeholder_(false) {{}}\n");

      Write(os, "\n{}::{}::~{}() {{\n", cpp_name, dunion_cpp_name,
            dunion_cpp_name);
      Write(os, "  switch (which) {{\n");
      for (const auto* y : x.type->discriminated_union.members) {
        switch (y->which) {
          case CppType::Which::kBool: {
            Write(os, "    case Which::kBool: break;\n");
          } break;
          case CppType::Which::kFloat: {
            Write(os, "    case Which::kFloat: break;\n");
          } break;
          case CppType::Which::kFloat64: {
            Write(os, "    case Which::kFloat64: break;\n");
          } break;
          case CppType::Which::kInt64: {
            Write(os, "    case Which::kInt64: break;\n");
          } break;
          case CppType::Which::kUint64: {
            Write(os, "    case Which::kUint64: break;\n");
          } break;
          case CppType::Which::kString: {
            Write(os, "    case Which::kString:\n");
            Write(os, "      str.std::string::~basic_string();\n");
            Write(os, "      break;\n");
          } break;
          case CppType::Which::kBytes: {
            Write(os, "    case Which::kBytes:\n");
            Write(os, "      bytes.std::vector<uint8_t>::~vector();\n");
            Write(os, "      break;\n");
          } break;
          default:
            return false;
        }
      }
      Write(os, "    case Which::kUninitialized: break;\n");
      Write(os, "  }}\n");
      Write(os, "}}\n");
    }

    static constexpr char vector_encode_function[] =
        R"(
bool Encode{}(
    const {}& data,
    CborEncodeBuffer* buffer) {{
  if (buffer->AvailableLength() == 0 &&
      !buffer->ResizeBy(CborEncodeBuffer::kDefaultInitialEncodeBufferSize)) {{
    return false;
  }}
  const uint8_t type_id[] = {};
  if (!buffer->SetType(type_id, sizeof(type_id))) {{
    return false;
  }}
  while (true) {{
    size_t available_length = buffer->AvailableLength();
    int64_t error_or_size = msgs::Encode{}(
        data, buffer->Position(), available_length);
    if (IsError(error_or_size)) {{
      return false;
    }} else if (error_or_size > static_cast<int64_t>(available_length)) {{
      if (!buffer->ResizeBy(error_or_size - available_length))
        return false;
    }} else {{
      buffer->ResizeBy(error_or_size - available_length);
      return true;
    }}
  }}
}}
)";

    std::string encoded_id = GetEncodedTypeKey(*real_type);
    if (encoded_id.empty()) {
      return false;
    }

    Write(os, vector_encode_function, cpp_name, cpp_name, encoded_id, cpp_name);
    Write(os, "\nCborResult Encode{}(\n", cpp_name);
    Write(os, "    const {}& data,\n", cpp_name);
    Write(os, "    uint8_t* buffer,\n    size_t length) {{\n");
    Write(os, "  CborEncoder encoder0;\n");
    Write(os, "  cbor_encoder_init(&encoder0, buffer, length, 0);\n");

    if (real_type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!WriteMapEncoder(os, "data", real_type->struct_type.members, name))
        return false;
    } else {
      if (!WriteArrayEncoder(os, "data", real_type->struct_type.members,
                             name)) {
        return false;
      }
    }

    Write(os,
          "  int64_t extra_bytes_needed = "
          "cbor_encoder_get_extra_bytes_needed(&encoder0);\n");
    Write(os, "  if (extra_bytes_needed) {{\n");
    Write(os,
          "    return static_cast<int64_t>(length + extra_bytes_needed);\n");
    Write(os, "  }} else {{\n");
    Write(os,
          "    return "
          "static_cast<int64_t>(cbor_encoder_get_buffer_size(&encoder0, "
          "buffer));\n");
    Write(os, "  }}\n");
    Write(os, "}}\n");
  }
  return true;
}

bool WriteMapDecoder(std::ostream& os,
                     const std::string& name,
                     const std::vector<CppType::Struct::CppMember>& members,
                     int decoder_depth,
                     int* temporary_count);
bool WriteArrayDecoder(std::ostream& os,
                       const std::string& name,
                       const std::vector<CppType::Struct::CppMember>& members,
                       int decoder_depth,
                       int* temporary_count);

// Writes the decoding function for the C++ type `cpp_type` to the file
// descriptor `os`.  `name` is the C++ variable name that needs to be decoded.
// `decoder_depth` is used to independently name independent cbor
// decoders that need to be created.  `temporary_count` is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteDecoder(std::ostream& os,
                  const std::string& name,
                  const CppType& cpp_type,
                  int decoder_depth,
                  int* temporary_count) {
  switch (cpp_type.which) {
    case CppType::Which::kBool: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_get_boolean(&it{}, &{}));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kFloat: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_get_float(&it{}, &{}));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kFloat64: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_get_double(&it{}, &{}));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kInt64: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_get_int64(&it{}, &{}));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kUint64: {
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it{}, &{}));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kString: {
      int temp_length = (*temporary_count)++;
      Write(os, "  size_t length{} = 0;\n", temp_length);
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_value_validate(&it{}, "
            "CborValidateUtf8));\n",
            decoder_depth);
      Write(os, "  if (cbor_value_is_length_known(&it{})) {{\n", decoder_depth);
      Write(os,
            "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it{}, "
            "&length{}));\n",
            decoder_depth, temp_length);
      Write(os, "  }} else {{\n");
      Write(
          os,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it{}, "
          "&length{}));\n",
          decoder_depth, temp_length);
      Write(os, "  }}\n");
      Write(os, "  {}.resize(length{});\n", name, temp_length);
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_value_copy_text_string(&it{}, "
            "const_cast<char*>({}.data()), &length{}, nullptr));\n",
            decoder_depth, name, temp_length);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kBytes: {
      int temp_length = (*temporary_count)++;
      Write(os, "  size_t length{} = 0;\n", temp_length);
      Write(os, "  if (cbor_value_is_length_known(&it{})) {{\n", decoder_depth);
      Write(os,
            "    CBOR_RETURN_ON_ERROR(cbor_value_get_string_length(&it{}, "
            "&length{}));\n",
            decoder_depth, temp_length);
      Write(os, "  }} else {{\n");
      Write(
          os,
          "    CBOR_RETURN_ON_ERROR(cbor_value_calculate_string_length(&it{}, "
          "&length{}));\n",
          decoder_depth, temp_length);
      Write(os, "  }}\n");
      if (!cpp_type.bytes_type.fixed_size) {
        Write(os, "  {}.resize(length{});\n", name, temp_length);
      } else {
        Write(os, "  if (length{} < {}) {{\n", temp_length,
              static_cast<int>(cpp_type.bytes_type.fixed_size.value()));
        Write(os, "    return -CborErrorTooFewItems;\n");
        Write(os, "  }} else if (length{} > {}) {{\n", temp_length,
              static_cast<int>(cpp_type.bytes_type.fixed_size.value()));
        Write(os, "    return -CborErrorTooManyItems;\n");
        Write(os, "  }}\n");
      }
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_value_copy_byte_string(&it{}, "
            "const_cast<uint8_t*>({}.data()), &length{}, nullptr));\n",
            decoder_depth, name, temp_length);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance(&it{}));\n",
            decoder_depth);
      return true;
    }
    case CppType::Which::kVector: {
      Write(os, "  if (cbor_value_get_type(&it{}) != CborArrayType) {{\n",
            decoder_depth);
      Write(os, "    return -1;\n");
      Write(os, "  }}\n");
      Write(os, "  {{\n");
      Write(os, "  CborValue it{};\n", decoder_depth + 1);
      Write(os, "  size_t it{}_length = 0;\n", decoder_depth + 1);
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it{}, "
            "&it{}_length));\n",
            decoder_depth, decoder_depth + 1);
      if (cpp_type.vector_type.min_length !=
          CppType::Vector::kMinLengthUnbounded) {
        Write(os, "  if (it{}_length < {}) {{\n", decoder_depth + 1,
              cpp_type.vector_type.min_length);
        Write(os, "    return -CborErrorTooFewItems;\n");
        Write(os, "  }}\n");
      }
      if (cpp_type.vector_type.max_length !=
          CppType::Vector::kMaxLengthUnbounded) {
        Write(os, "  if (it{}_length > {}) {{\n", decoder_depth + 1,
              cpp_type.vector_type.max_length);
        Write(os, "    return -CborErrorTooManyItems;\n");
        Write(os, "  }}\n");
      }
      Write(os, "  {}.resize(it{}_length);\n", name, decoder_depth + 1);
      Write(
          os,
          "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it{}, &it{}));\n",
          decoder_depth, decoder_depth + 1);
      std::string loop_variable = "x" + std::to_string(decoder_depth + 1);
      Write(os, " for (auto& {} : {}) {{\n", loop_variable, name);
      if (!WriteDecoder(os, loop_variable, *cpp_type.vector_type.element_type,
                        decoder_depth + 1, temporary_count)) {
        return false;
      }
      Write(os, "  }}\n");
      Write(
          os,
          "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it{}, &it{}));\n",
          decoder_depth, decoder_depth + 1);
      Write(os, "  }}\n");
      return true;
    }
    case CppType::Which::kEnum: {
      Write(os,
            "  CBOR_RETURN_ON_ERROR(cbor_value_get_uint64(&it{}, "
            "reinterpret_cast<uint64_t*>(&{})));\n",
            decoder_depth, name);
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      // TODO(btolsch): Validate against enum members.
      return true;
    }
    case CppType::Which::kStruct: {
      if (cpp_type.struct_type.key_type == CppType::Struct::KeyType::kMap) {
        return WriteMapDecoder(os, name, cpp_type.struct_type.members,
                               decoder_depth + 1, temporary_count);
      } else if (cpp_type.struct_type.key_type ==
                 CppType::Struct::KeyType::kArray) {
        return WriteArrayDecoder(os, name, cpp_type.struct_type.members,
                                 decoder_depth + 1, temporary_count);
      }
    } break;
    case CppType::Which::kDiscriminatedUnion: {
      int temp_value_type = (*temporary_count)++;
      Write(os, "  CborType type{} = cbor_value_get_type(&it{});\n",
            temp_value_type, decoder_depth);
      bool first = true;
      for (const auto* x : cpp_type.discriminated_union.members) {
        if (first)
          first = false;
        else
          Write(os, " else ");
        switch (x->which) {
          case CppType::Which::kBool:
            Write(os, "  if (type{} == CborBooleanType) {{\n", temp_value_type);
            Write(os, "  {}.which = decltype({})::Which::kBool;\n", name, name);
            if (!WriteDecoder(os, name + ".bool_var", *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kFloat:
            Write(os, "  if (type{} == CborFloatType) {{\n", temp_value_type);
            Write(os, "  {}.which = decltype({})::Which::kFloat;\n", name,
                  name);
            if (!WriteDecoder(os, name + ".float_var", *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kFloat64:
            Write(os, "  if (type{} == CborDoublType) {{\n", temp_value_type);
            Write(os, "  {}.which = decltype({})::Which::kFloat64;\n", name,
                  name);
            if (!WriteDecoder(os, name + ".double_var", *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kInt64:
            Write(os,
                  "  if (type{} == CborIntegerType && (it{}.flags & "
                  "CborIteratorFlag_NegativeInteger) != 0) {{\n",
                  temp_value_type, decoder_depth);
            Write(os, "  {}.which = decltype({})::Which::kInt64;\n", name,
                  name);
            if (!WriteDecoder(os, name + ".int_var", *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kUint64:
            Write(os,
                  "  if (type{} == CborIntegerType && (it{}.flags & "
                  "CborIteratorFlag_NegativeInteger) == 0) {{\n",
                  temp_value_type, decoder_depth);
            Write(os, "  {}.which = decltype({})::Which::kUint64;\n", name,
                  name);
            if (!WriteDecoder(os, name + ".uint", *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
            break;
          case CppType::Which::kString: {
            Write(os, "  if (type{} == CborTextStringType) {{\n",
                  temp_value_type);
            Write(os, "  {}.which = decltype({})::Which::kString;\n", name,
                  name);
            std::string str_name = name + ".str";
            Write(os, "  new (&{}) std::string();\n", str_name);
            if (!WriteDecoder(os, str_name, *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
          } break;
          case CppType::Which::kBytes: {
            Write(os, "  if (type{} == CborByteStringType) {{\n",
                  temp_value_type);
            std::string bytes_name = name + ".bytes";
            Write(os, "  {}.which = decltype({})::Which::kBytes;\n", name,
                  name);
            Write(os, "  new (&{}) std::vector<uint8_t>();\n", bytes_name);
            if (!WriteDecoder(os, bytes_name, *x, decoder_depth,
                              temporary_count)) {
              return false;
            }
          } break;
          default:
            return false;
        }
        Write(os, "  }}\n");
      }
      Write(os, " else {{ return -1; }}\n");
      return true;
    }
    case CppType::Which::kTaggedType: {
      int temp_tag = (*temporary_count)++;
      Write(os, "  uint64_t tag{} = 0;\n", temp_tag);
      Write(os, "  cbor_value_get_tag(&it{}, &tag{});\n", decoder_depth,
            temp_tag);
      Write(os, "  if (tag{} != {}ull) {{\n", temp_tag,
            cpp_type.tagged_type.tag);
      Write(os, "    return -1;\n");
      Write(os, "  }}\n");
      Write(os, "  CBOR_RETURN_ON_ERROR(cbor_value_advance_fixed(&it{}));\n",
            decoder_depth);
      if (!WriteDecoder(os, name, *cpp_type.tagged_type.real_type,
                        decoder_depth, temporary_count)) {
        return false;
      }
      return true;
    }
    default:
      break;
  }
  return false;
}

// Writes the decoding function for the CBOR map with members in `members` to
// the file descriptor `os`.  `name` is the C++ variable name that needs to be
// decoded.  `decoder_depth` is used to independently name independent
// cbor decoders that need to be created.  `temporary_count` is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteMapDecoder(std::ostream& os,
                     const std::string& name,
                     const std::vector<CppType::Struct::CppMember>& members,
                     int decoder_depth,
                     int* temporary_count) {
  Write(os, "  if (cbor_value_get_type(&it{}) != CborMapType) {{\n",
        decoder_depth - 1);
  Write(os, "    return -1;\n");
  Write(os, "  }}\n");
  Write(os, "  CborValue it{};\n", decoder_depth);
  Write(os, "  size_t it{}_length = 0;\n", decoder_depth);
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_get_map_length(&it{}, "
        "&it{}_length));\n",
        decoder_depth - 1, decoder_depth);
  int optional_members = 0;
  for (const auto& member : members) {
    if (member.type->which == CppType::Which::kOptional) {
      ++optional_members;
    }
  }
  Write(os, "  if (it{}_length != {}", decoder_depth,
        static_cast<int>(members.size()));
  for (int i = 0; i < optional_members; ++i) {
    Write(os, " && it{}_length != {}", decoder_depth,
          static_cast<int>(members.size()) - i - 1);
  }
  Write(os, ") {{\n");
  Write(os, "    return -1;\n");
  Write(os, "  }}\n");
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it{}, &it{}));\n",
        decoder_depth - 1, decoder_depth);
  for (const auto& x : members) {
    std::string cid = ToUnderscoreId(x.name);
    std::string fullname = name + "." + cid;
    if (x.type->which == CppType::Which::kOptional) {
      std::string found_var = "found_" + std::to_string((*temporary_count)++);
      Write(os, "  bool {} = false;\n", found_var);
      if (x.integer_key.has_value()) {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(CHECK_INT_KEY_CONSTANT(&it{}, {}, "
              "&{}));\n",
              decoder_depth, x.integer_key.value(), found_var);
      } else {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(CHECK_KEY_CONSTANT(&it{}, \"{}\", "
              "&{}));\n",
              decoder_depth, x.name, found_var);
      }

      Write(os, "  if ({}) {{\n", found_var);
      std::string temp_val = "val_" + std::to_string((*temporary_count)++);
      Write(os, "    auto& {} = {}.{}.emplace();\n", temp_val, name, cid);
      if (!WriteDecoder(os, temp_val, *x.type->optional_type, decoder_depth,
                        temporary_count)) {
        return false;
      }
      Write(os, "  }} else {{\n");
      Write(os, "    {}.{}.reset();\n", name, cid);
      Write(os, "  }}\n");
    } else {
      if (x.integer_key.has_value()) {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(EXPECT_INT_KEY_CONSTANT(&it{}, {}"
              "));\n",
              decoder_depth, x.integer_key.value());
      } else {
        Write(os,
              "  CBOR_RETURN_ON_ERROR(EXPECT_KEY_CONSTANT(&it{}, \"{}\"));\n",
              decoder_depth, x.name);
      }
      if (!WriteDecoder(os, fullname, *x.type, decoder_depth,
                        temporary_count)) {
        return false;
      }
    }
  }
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it{}, &it{}));\n",
        decoder_depth - 1, decoder_depth);
  return true;
}

// Writes the decoding function for the CBOR array with members in `members` to
// the file descriptor `os`.  `name` is the C++ variable name that needs to be
// decoded.  `decoder_depth` is used to independently name independent
// cbor decoders that need to be created.  `temporary_count` is used to ensure
// temporaries get unique names by appending an automatically incremented
// integer.
bool WriteArrayDecoder(std::ostream& os,
                       const std::string& name,
                       const std::vector<CppType::Struct::CppMember>& members,
                       int decoder_depth,
                       int* temporary_count) {
  Write(os, "  if (cbor_value_get_type(&it{}) != CborArrayType) {{\n",
        decoder_depth - 1);
  Write(os, "    return -1;\n");
  Write(os, "  }}\n");
  Write(os, "  CborValue it{};\n", decoder_depth);
  Write(os, "  size_t it{}_length = 0;\n", decoder_depth);
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_get_array_length(&it{}, "
        "&it{}_length));\n",
        decoder_depth - 1, decoder_depth);
  int optional_members = 0;
  for (const auto& member : members) {
    if (member.type->which == CppType::Which::kOptional) {
      ++optional_members;
    }
  }
  Write(os, "  if (it{}_length != {}", decoder_depth,
        static_cast<int>(members.size()));
  for (int i = 0; i < optional_members; ++i) {
    Write(os, " && it{}_length != {}", decoder_depth,
          static_cast<int>(members.size()) - i - 1);
  }
  Write(os, ") {{\n");
  Write(os, "    return -1;\n");
  Write(os, "  }}\n");
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_enter_container(&it{}, &it{}));\n",
        decoder_depth - 1, decoder_depth);
  int member_pos = 0;
  for (const auto& x : members) {
    std::string cid = ToUnderscoreId(x.name);
    std::string fullname = name + "." + cid;
    if (x.type->which == CppType::Which::kOptional) {
      // TODO(btolsch): This only handles a single block of optionals and only
      // the ones present form a contiguous range from the start of the block.
      // However, we likely don't really need more than one optional for arrays
      // for the foreseeable future.  The proper approach would be to have a set
      // of possible types for the next element and a map for the member to
      // which each corresponds.
      Write(os, "  if (it{}_length > {}) {{\n", decoder_depth, member_pos);

      std::string temp_val = "val_" + std::to_string((*temporary_count)++);
      Write(os, "    auto& {} = {}.{}.emplace();\n", temp_val, name, cid);
      if (!WriteDecoder(os, temp_val, *x.type->optional_type, decoder_depth,
                        temporary_count)) {
        return false;
      }
      Write(os, "  }} else {{\n");
      Write(os, "    {}.{}.reset();\n", name, cid);
      Write(os, "  }}\n");
    } else {
      if (!WriteDecoder(os, fullname, *x.type, decoder_depth,
                        temporary_count)) {
        return false;
      }
    }
    ++member_pos;
  }
  Write(os,
        "  CBOR_RETURN_ON_ERROR(cbor_value_leave_container(&it{}, &it{}));\n",
        decoder_depth - 1, decoder_depth);
  return true;
}

// Writes the equality operators for all structs.
bool WriteEqualityOperators(std::ostream& os, CppSymbolTable* table) {
  for (const auto& pair : table->cpp_type_map) {
    CppType* real_type = pair.second;
    if (real_type->which == CppType::Which::kStruct &&
        real_type->struct_type.key_type !=
            CppType::Struct::KeyType::kPlainGroup) {
      if (!WriteStructEqualityOperator(os, *real_type)) {
        return false;
      }
    }
  }
  return true;
}

// Writes a decoder function definition for every type in `table` to the file
// descriptor `os`.
bool WriteDecoders(std::ostream& os, CppSymbolTable* table) {
  if (!WriteTypeParserDefinition(os, table)) {
    return false;
  }
  for (CppType* real_type : table->TypesWithId()) {
    const auto& name = real_type->name;
    int temporary_count = 0;
    if (real_type->which != CppType::Which::kStruct ||
        real_type->struct_type.key_type ==
            CppType::Struct::KeyType::kPlainGroup) {
      continue;
    }
    std::string cpp_name = ToCamelCase(name);
    Write(os, "\nint64_t Decode{}(\n", cpp_name);
    Write(os, "    const uint8_t* buffer,\n    size_t length,\n");
    Write(os, "    {}& data) {{\n", cpp_name);
    Write(os, "  CborParser parser;\n");
    Write(os, "  CborValue it0;\n");
    Write(os,
          "  CBOR_RETURN_ON_ERROR(cbor_parser_init(buffer, length, 0, &parser, "
          "&it0));\n");
    if (real_type->struct_type.key_type == CppType::Struct::KeyType::kMap) {
      if (!WriteMapDecoder(os, "data", real_type->struct_type.members, 1,
                           &temporary_count)) {
        return false;
      }
    } else {
      if (!WriteArrayDecoder(os, "data", real_type->struct_type.members, 1,
                             &temporary_count)) {
        return false;
      }
    }
    Write(
        os,
        "  auto result = static_cast<int64_t>(cbor_value_get_next_byte(&it0) - "
        "buffer);\n");
    Write(os, "  return result;\n");
    Write(os, "}}\n");
  }
  return true;
}

// Converts the filename `header_filename` to a preprocessor token that can be
// used as a header guard macro name.
std::string ToHeaderGuard(const std::string& header_filename) {
  std::string result = header_filename;
  for (auto& c : result) {
    if (c == '/' || c == '.')
      c = '_';
    else
      c = toupper(c);
  }
  result += "_";
  return result;
}

bool WriteHeaderPrologue(std::ostream& os, const std::string& header_filename) {
  static constexpr char prologue[] =
      R"(#ifndef {}
#define {}

#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "third_party/tinycbor/src/src/cbor.h"

namespace openscreen::msgs {{

enum CborErrors {{
  kParserEOF = -CborErrorUnexpectedEOF,
}};

using CborResult = int64_t;

class CborEncodeBuffer;
)";
  std::string header_guard = ToHeaderGuard(header_filename);
  Write(os, prologue, header_guard, header_guard);
  return true;
}

bool WriteHeaderEpilogue(std::ostream& os, const std::string& header_filename) {
  static constexpr char epilogue[] = R"(
class TypeEnumValidator {{
 public:
  static Type SafeCast(uint64_t type_id);
}};

class CborEncodeBuffer {{
 public:
  static constexpr size_t kDefaultInitialEncodeBufferSize = 250;
  static constexpr size_t kDefaultMaxEncodeBufferSize = 64000;

  CborEncodeBuffer();
  CborEncodeBuffer(size_t initial_size, size_t max_size);
  ~CborEncodeBuffer();

  bool ResizeBy(int64_t length);
  bool SetType(const uint8_t encoded_id[], size_t size);

  const uint8_t* data() const {{ return data_.data(); }}
  size_t size() const {{ return data_.size(); }}

  uint8_t* Position();
  size_t AvailableLength() {{ return data_.size() - position_; }}

 private:
  size_t max_size_;
  size_t position_;
  std::vector<uint8_t> data_;
}};

CborError ExpectKey(CborValue* it, const uint64_t key);
CborError ExpectKey(CborValue* it, const char* key, size_t key_length);

}}  // namespace openscreen::msgs

#endif  // {})";
  std::string header_guard = ToHeaderGuard(header_filename);
  Write(os, epilogue, header_guard);
  return true;
}

bool WriteSourcePrologue(std::ostream& os, const std::string& header_filename) {
  static constexpr char prologue[] =
      R"(#include "{}"

#include "third_party/tinycbor/src/src/utf8_p.h"
#include "{}"

namespace openscreen::msgs {{
namespace {{


/*
 * Encoder-specific errors, so it's fine to check these even in the
 * parser.
 */
#define CBOR_RETURN_WHAT_ON_ERROR(stmt, what)                           \
  {{                                                                     \
    CborError error = stmt;                                             \
    OSP_DCHECK_NE(error, CborErrorTooFewItems);                         \
    OSP_DCHECK_NE(error, CborErrorTooManyItems);                        \
    OSP_DCHECK_NE(error, CborErrorDataTooLarge);                        \
    if (error != CborNoError && error != CborErrorOutOfMemory)          \
      return what;                                                      \
  }}
#define CBOR_RETURN_ON_ERROR_INTERNAL(stmt) \
  CBOR_RETURN_WHAT_ON_ERROR(stmt, error)
#define CBOR_RETURN_ON_ERROR(stmt) CBOR_RETURN_WHAT_ON_ERROR(stmt, -error)

#define EXPECT_KEY_CONSTANT(it, key) ExpectKey(it, key, sizeof(key) - 1)
#define EXPECT_INT_KEY_CONSTANT(it, key) ExpectKey(it, key)
#define CHECK_KEY_CONSTANT(it, key, found) CheckKey(it, key, sizeof(key) - 1, found)
#define CHECK_INT_KEY_CONSTANT(it, key, found) CheckKey(it, key, found)

bool IsValidUtf8(const std::string& s) {{
  const uint8_t* buffer = reinterpret_cast<const uint8_t*>(s.data());
  const uint8_t* end = buffer + s.size();
  while (buffer < end) {{
    // TODO(btolsch): This is an implementation detail of tinycbor so we should
    // eventually replace this call with our own utf8 validation.
    if (get_utf8(&buffer, end) == ~0u) {{
      return false;
    }}
  }}
  return true;
}}

}}  // namespace

CborError ExpectKey(CborValue* it, const uint64_t key) {{
  if (!cbor_value_is_unsigned_integer(it)) {{
    return CborErrorImproperValue;
  }}
  uint64_t observed_key;
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_get_uint64(it, &observed_key));
  if (observed_key != key) {{
    return CborErrorImproperValue;
  }}
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance_fixed(it));
  return CborNoError;
}}

CborError ExpectKey(CborValue* it, const char* key, size_t key_length) {{
  if (!cbor_value_is_text_string(it)) {{
    return CborErrorImproperValue;
  }}
  size_t observed_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(
      cbor_value_get_string_length(it, &observed_length));
  if (observed_length != key_length) {{
    return CborErrorImproperValue;
  }}
  std::string observed_key(key_length, 0);
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
      it, const_cast<char*>(observed_key.data()), &observed_length, nullptr));
  if (observed_key != key) {{
    return CborErrorImproperValue;
  }}
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(it));
  return CborNoError;
}}

CborError CheckKey(CborValue* it, const uint64_t key, bool* found) {{
  *found = false;
  if (!cbor_value_is_unsigned_integer(it)) {{
    return CborNoError;
  }}
  uint64_t observed_key;
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_get_uint64(it, &observed_key));
  if (observed_key == key) {{
    *found = true;
    CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance_fixed(it));
  }}
  return CborNoError;
}}

CborError CheckKey(CborValue* it, const char* key,
                   size_t key_length, bool* found) {{
  *found = false;
  if (!cbor_value_is_text_string(it)) {{
    return CborNoError;
  }}
  size_t observed_length = 0;
  CBOR_RETURN_ON_ERROR_INTERNAL(
      cbor_value_get_string_length(it, &observed_length));
  if (observed_length != key_length) {{
    return CborNoError;
  }}
  std::string observed_key(key_length, 0);
  CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_copy_text_string(
      it, const_cast<char*>(observed_key.data()), &observed_length, nullptr));
  if (observed_key == key) {{
    *found = true;
    CBOR_RETURN_ON_ERROR_INTERNAL(cbor_value_advance(it));
  }}
  return CborNoError;
}}

CborEncodeBuffer::CborEncodeBuffer()
    : max_size_(kDefaultMaxEncodeBufferSize),
      position_(0),
      data_(kDefaultInitialEncodeBufferSize) {{}}
CborEncodeBuffer::CborEncodeBuffer(size_t initial_size, size_t max_size)
    : max_size_(max_size), position_(0), data_(initial_size) {{}}
CborEncodeBuffer::~CborEncodeBuffer() = default;

bool CborEncodeBuffer::SetType(const uint8_t encoded_id[], size_t size) {{
  if (this->AvailableLength() < size) {{
    if (!this->ResizeBy(size)) {{
      return false;
    }}
  }}
  memcpy(&data_[position_], encoded_id, size);
  position_ += size;
  return true;
}}

bool CborEncodeBuffer::ResizeBy(int64_t delta) {{
  if (delta == 0) {{
    return true;
  }}
  if (data_.size() + delta < 0) {{
    return false;
  }}
  if (data_.size() + delta > max_size_) {{
    return false;
  }}
  data_.resize(data_.size() + delta);

  // Update `position_` if it is outside the valid range.
  if (position_ > data_.size()) {{
    position_ = data_.size();
  }}
  return true;
}}

uint8_t* CborEncodeBuffer::Position() {{
  if (data_.empty() || position_ >= data_.size()) {{
    return nullptr;
  }}
  return &data_[0] + position_;
}}

bool IsError(int64_t x) {{
  return x < 0;
}}
)";
  // Construct the util/osp_logging.h #include dynamically to avoid a false
  // positive from checkdeps, which disallows including util/ in this file.
  Write(os, prologue, header_filename, "util/osp_logging.h");
  return true;
}

bool WriteSourceEpilogue(std::ostream& os) {
  static constexpr char epilogue[] = R"(
}}  // namespace openscreen::msgs
)";
  Write(os, epilogue);
  return true;
}
