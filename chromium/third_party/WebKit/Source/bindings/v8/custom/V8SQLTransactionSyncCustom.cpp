/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "V8SQLTransactionSync.h"

#include "V8SQLResultSet.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/V8Binding.h"
#include "core/dom/ExceptionCode.h"
#include "core/platform/sql/SQLValue.h"
#include "modules/webdatabase/DatabaseSync.h"
#include "modules/webdatabase/SQLResultSet.h"
#include "wtf/Vector.h"

using namespace WTF;

namespace WebCore {

void V8SQLTransactionSync::executeSqlMethodCustom(const v8::FunctionCallbackInfo<v8::Value>& args)
{
    if (!args.Length()) {
        setDOMException(SyntaxError, args.GetIsolate());
        return;
    }

    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, statement, args[0]);

    Vector<SQLValue> sqlValues;

    if (args.Length() > 1 && !isUndefinedOrNull(args[1])) {
        if (!args[1]->IsObject()) {
            setDOMException(TypeMismatchError, args.GetIsolate());
            return;
        }

        uint32_t sqlArgsLength = 0;
        v8::Local<v8::Object> sqlArgsObject = args[1]->ToObject();
        V8TRYCATCH_VOID(v8::Local<v8::Value>, length, sqlArgsObject->Get(v8::String::New("length")));

        if (isUndefinedOrNull(length))
            sqlArgsLength = sqlArgsObject->GetPropertyNames()->Length();
        else
            sqlArgsLength = length->Uint32Value();

        for (unsigned int i = 0; i < sqlArgsLength; ++i) {
            v8::Handle<v8::Integer> key = v8::Integer::New(i, args.GetIsolate());
            V8TRYCATCH_VOID(v8::Local<v8::Value>, value, sqlArgsObject->Get(key));

            if (value.IsEmpty() || value->IsNull())
                sqlValues.append(SQLValue());
            else if (value->IsNumber()) {
                V8TRYCATCH_VOID(double, sqlValue, value->NumberValue());
                sqlValues.append(SQLValue(sqlValue));
            } else {
                V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<>, sqlValue, value);
                sqlValues.append(SQLValue(sqlValue));
            }
        }
    }

    SQLTransactionSync* transaction = V8SQLTransactionSync::toNative(args.Holder());

    ExceptionState es(args.GetIsolate());
    v8::Handle<v8::Value> result = toV8(transaction->executeSQL(statement, sqlValues, es), args.Holder(), args.GetIsolate());
    if (es.throwIfNeeded())
        return;

    v8SetReturnValue(args, result);
}

} // namespace WebCore
