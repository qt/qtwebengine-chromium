/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ExceptionMessages_h
#define ExceptionMessages_h

#include "wtf/text/WTFString.h"

namespace WebCore {

class ExceptionMessages {
public:
    static String failedToConstruct(const String& type, const String& detail = String());
    static String failedToExecute(const String& method, const String& type, const String& detail = String());
    static String failedToGet(const String& property, const String& type, const String& detail);
    static String failedToSet(const String& property, const String& type, const String& detail);
    static String failedToDelete(const String& property, const String& type, const String& detail);

    static String incorrectArgumentType(int argumentIndex, const String& detail);
    static String incorrectPropertyType(const String& property, const String& detail);

    // If  > 0, the argument index that failed type check (1-indexed.)
    // If == 0, a (non-argument) value (e.g., a setter) failed the same check.
    static String notAnArrayTypeArgumentOrValue(int argumentIndex);
    static String notASequenceTypeProperty(const String& propertyName);
    static String notAFiniteNumber(double value);

    static String notEnoughArguments(unsigned expected, unsigned providedleastNumMandatoryParams);

private:
    static String ordinalNumber(int number);
};

} // namespace WebCore

#endif // ExceptionMessages_h
