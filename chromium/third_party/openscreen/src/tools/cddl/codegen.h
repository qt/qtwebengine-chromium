// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CDDL_CODEGEN_H_
#define TOOLS_CDDL_CODEGEN_H_

#include <iostream>
#include <string>

#include "tools/cddl/sema.h"

bool WriteTypeDefinitions(std::ostream& os, CppSymbolTable* table);
bool WriteFunctionDeclarations(std::ostream& os, CppSymbolTable* table);
bool WriteEncoders(std::ostream& os, CppSymbolTable* table);
bool WriteDecoders(std::ostream& os, CppSymbolTable* table);
bool WriteEqualityOperators(std::ostream& os, CppSymbolTable* table);
bool WriteHeaderPrologue(std::ostream& os, const std::string& header_filename);
bool WriteHeaderEpilogue(std::ostream& os, const std::string& header_filename);
bool WriteSourcePrologue(std::ostream& os, const std::string& header_filename);
bool WriteSourceEpilogue(std::ostream& os);

#endif  // TOOLS_CDDL_CODEGEN_H_
