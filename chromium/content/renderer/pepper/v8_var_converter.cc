// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/v8_var_converter.h"

#include <map>
#include <stack>
#include <string>

#include "base/bind.h"
#include "base/containers/hash_tables.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "content/renderer/pepper/host_array_buffer_var.h"
#include "content/renderer/pepper/resource_converter.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "third_party/WebKit/public/platform/WebArrayBuffer.h"

using ppapi::ArrayBufferVar;
using ppapi::ArrayVar;
using ppapi::DictionaryVar;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using std::make_pair;

namespace {

template <class T>
struct StackEntry {
  StackEntry(T v) : val(v), sentinel(false) {}
  T val;
  // Used to track parent nodes on the stack while traversing the graph.
  bool sentinel;
};

struct HashedHandle {
  HashedHandle(v8::Handle<v8::Object> h) : handle(h) {}
  size_t hash() const { return handle->GetIdentityHash(); }
  bool operator==(const HashedHandle& h) const { return handle == h.handle; }
  bool operator<(const HashedHandle& h) const { return hash() < h.hash(); }
  v8::Handle<v8::Object> handle;
};

}  // namespace

namespace BASE_HASH_NAMESPACE {
#if defined(COMPILER_GCC)
template <>
struct hash<HashedHandle> {
  size_t operator()(const HashedHandle& handle) const {
    return handle.hash();
  }
};
#elif defined(COMPILER_MSVC)
inline size_t hash_value(const HashedHandle& handle) {
  return handle.hash();
}
#endif
}  // namespace BASE_HASH_NAMESPACE

namespace content {

namespace {

// Maps PP_Var IDs to the V8 value handle they correspond to.
typedef base::hash_map<int64_t, v8::Handle<v8::Value> > VarHandleMap;
typedef base::hash_set<int64_t> ParentVarSet;

// Maps V8 value handles to the PP_Var they correspond to.
typedef base::hash_map<HashedHandle, ScopedPPVar> HandleVarMap;
typedef base::hash_set<HashedHandle> ParentHandleSet;

// Returns a V8 value which corresponds to a given PP_Var. If |var| is a
// reference counted PP_Var type, and it exists in |visited_ids|, the V8 value
// associated with it in the map will be returned, otherwise a new V8 value will
// be created and added to the map. |did_create| indicates whether a new v8
// value was created as a result of calling the function.
bool GetOrCreateV8Value(v8::Isolate* isolate,
                        const PP_Var& var,
                        v8::Handle<v8::Value>* result,
                        bool* did_create,
                        VarHandleMap* visited_ids,
                        ParentVarSet* parent_ids) {
  *did_create = false;

  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type)) {
    if (parent_ids->count(var.value.as_id) != 0)
      return false;
    VarHandleMap::iterator it = visited_ids->find(var.value.as_id);
    if (it != visited_ids->end()) {
      *result = it->second;
      return true;
    }
  }

  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      *result = v8::Undefined(isolate);
      break;
    case PP_VARTYPE_NULL:
      *result = v8::Null(isolate);
      break;
    case PP_VARTYPE_BOOL:
      *result = (var.value.as_bool == PP_TRUE)
          ? v8::True(isolate)
          : v8::False(isolate);
      break;
    case PP_VARTYPE_INT32:
      *result = v8::Integer::New(var.value.as_int);
      break;
    case PP_VARTYPE_DOUBLE:
      *result = v8::Number::New(var.value.as_double);
      break;
    case PP_VARTYPE_STRING: {
      StringVar* string = StringVar::FromPPVar(var);
      if (!string) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      const std::string& value = string->value();
      // Create a string object rather than a string primitive. This allows us
      // to have multiple references to the same string in javascript, which
      // matches the reference behavior of PP_Vars.
      *result = v8::String::NewFromUtf8(isolate,
                                        value.c_str(),
                                        v8::String::kNormalString,
                                        value.size())->ToObject();
      break;
    }
    case PP_VARTYPE_ARRAY_BUFFER: {
      ArrayBufferVar* buffer = ArrayBufferVar::FromPPVar(var);
      if (!buffer) {
        NOTREACHED();
        result->Clear();
        return false;
      }
      HostArrayBufferVar* host_buffer =
          static_cast<HostArrayBufferVar*>(buffer);
      *result = host_buffer->webkit_buffer().toV8Value();
      break;
    }
    case PP_VARTYPE_ARRAY:
      *result = v8::Array::New(isolate);
      break;
    case PP_VARTYPE_DICTIONARY:
      *result = v8::Object::New();
      break;
    case PP_VARTYPE_OBJECT:
    case PP_VARTYPE_RESOURCE:
      // TODO(mgiuca): Convert PP_VARTYPE_RESOURCE vars into the correct V8
      // type. (http://crbug.com/177017)
      result->Clear();
      return false;
  }

  *did_create = true;
  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type))
    (*visited_ids)[var.value.as_id] = *result;
  return true;
}

// For a given V8 value handle, this returns a PP_Var which corresponds to it.
// If the handle already exists in |visited_handles|, the PP_Var associated with
// it will be returned, otherwise a new V8 value will be created and added to
// the map. |did_create| indicates if a new PP_Var was created as a result of
// calling the function.
bool GetOrCreateVar(v8::Handle<v8::Value> val,
                    v8::Handle<v8::Context> context,
                    PP_Var* result,
                    bool* did_create,
                    HandleVarMap* visited_handles,
                    ParentHandleSet* parent_handles,
                    ResourceConverter* resource_converter) {
  CHECK(!val.IsEmpty());
  *did_create = false;

  // Even though every v8 string primitive encountered will be a unique object,
  // we still add them to |visited_handles| so that the corresponding string
  // PP_Var created will be properly refcounted.
  if (val->IsObject() || val->IsString()) {
    if (parent_handles->count(HashedHandle(val->ToObject())) != 0)
      return false;

    HandleVarMap::const_iterator it = visited_handles->find(
        HashedHandle(val->ToObject()));
    if (it != visited_handles->end()) {
      *result = it->second.get();
      return true;
    }
  }

  if (val->IsUndefined()) {
    *result = PP_MakeUndefined();
  } else if (val->IsNull()) {
    *result = PP_MakeNull();
  } else if (val->IsBoolean() || val->IsBooleanObject()) {
    *result = PP_MakeBool(PP_FromBool(val->ToBoolean()->Value()));
  } else if (val->IsInt32()) {
    *result = PP_MakeInt32(val->ToInt32()->Value());
  } else if (val->IsNumber() || val->IsNumberObject()) {
    *result = PP_MakeDouble(val->ToNumber()->Value());
  } else if (val->IsString() || val->IsStringObject()) {
    v8::String::Utf8Value utf8(val->ToString());
    *result = StringVar::StringToPPVar(std::string(*utf8, utf8.length()));
  } else if (val->IsArray()) {
    *result = (new ArrayVar())->GetPPVar();
  } else if (val->IsObject()) {
    scoped_ptr<blink::WebArrayBuffer> web_array_buffer(
        blink::WebArrayBuffer::createFromV8Value(val));
    if (web_array_buffer.get()) {
      scoped_refptr<HostArrayBufferVar> buffer_var(new HostArrayBufferVar(
          *web_array_buffer));
      *result = buffer_var->GetPPVar();
    } else {
      bool was_resource;
      if (!resource_converter->FromV8Value(val->ToObject(), context, result,
                                           &was_resource))
        return false;
      if (!was_resource) {
        *result = (new DictionaryVar())->GetPPVar();
      }
    }
  } else {
    // Silently ignore the case where we can't convert to a Var as we may
    // be trying to convert a type that doesn't have a corresponding
    // PP_Var type.
    return true;
  }

  *did_create = true;
  if (val->IsObject() || val->IsString()) {
    visited_handles->insert(make_pair(
        HashedHandle(val->ToObject()),
        ScopedPPVar(ScopedPPVar::PassRef(), *result)));
  }
  return true;
}

bool CanHaveChildren(PP_Var var) {
  return var.type == PP_VARTYPE_ARRAY || var.type == PP_VARTYPE_DICTIONARY;
}

}  // namespace

V8VarConverter::V8VarConverter(PP_Instance instance)
    : message_loop_proxy_(base::MessageLoopProxy::current()) {
  resource_converter_.reset(new ResourceConverterImpl(
      instance, RendererPpapiHost::GetForPPInstance(instance)));
}

V8VarConverter::V8VarConverter(
    PP_Instance instance,
    scoped_ptr<ResourceConverter> resource_converter)
    : message_loop_proxy_(base::MessageLoopProxy::current()),
      resource_converter_(resource_converter.release()) {
}

V8VarConverter::~V8VarConverter() {
}

// To/FromV8Value use a stack-based DFS search to traverse V8/Var graph. Each
// iteration, the top node on the stack examined. If the node has not been
// visited yet (i.e. sentinel == false) then it is added to the list of parents
// which contains all of the nodes on the path from the start node to the
// current node. Each of the current nodes children are examined. If they appear
// in the list of parents it means we have a cycle and we return NULL.
// Otherwise, if they can have children, we add them to the stack. If the
// node at the top of the stack has already been visited, then we pop it off the
// stack and erase it from the list of parents.
// static
bool V8VarConverter::ToV8Value(const PP_Var& var,
                               v8::Handle<v8::Context> context,
                               v8::Handle<v8::Value>* result) {
  v8::Context::Scope context_scope(context);
  v8::Isolate* isolate = context->GetIsolate();
  v8::EscapableHandleScope handle_scope(isolate);

  VarHandleMap visited_ids;
  ParentVarSet parent_ids;

  std::stack<StackEntry<PP_Var> > stack;
  stack.push(StackEntry<PP_Var>(var));
  v8::Local<v8::Value> root;
  bool is_root = true;

  while (!stack.empty()) {
    const PP_Var& current_var = stack.top().val;
    v8::Handle<v8::Value> current_v8;

    if (stack.top().sentinel) {
      stack.pop();
      if (CanHaveChildren(current_var))
        parent_ids.erase(current_var.value.as_id);
      continue;
    } else {
      stack.top().sentinel = true;
    }

    bool did_create = false;
    if (!GetOrCreateV8Value(isolate, current_var, &current_v8, &did_create,
                            &visited_ids, &parent_ids)) {
      return false;
    }

    if (is_root) {
      is_root = false;
      root = current_v8;
    }

    // Add child nodes to the stack.
    if (current_var.type == PP_VARTYPE_ARRAY) {
      parent_ids.insert(current_var.value.as_id);
      ArrayVar* array_var = ArrayVar::FromPPVar(current_var);
      if (!array_var) {
        NOTREACHED();
        return false;
      }
      DCHECK(current_v8->IsArray());
      v8::Handle<v8::Array> v8_array = current_v8.As<v8::Array>();

      for (size_t i = 0; i < array_var->elements().size(); ++i) {
        const PP_Var& child_var = array_var->elements()[i].get();
        v8::Handle<v8::Value> child_v8;
        if (!GetOrCreateV8Value(isolate, child_var, &child_v8, &did_create,
                                &visited_ids, &parent_ids)) {
          return false;
        }
        if (did_create && CanHaveChildren(child_var))
          stack.push(child_var);
        v8::TryCatch try_catch;
        v8_array->Set(static_cast<uint32>(i), child_v8);
        if (try_catch.HasCaught()) {
          LOG(ERROR) << "Setter for index " << i << " threw an exception.";
          return false;
        }
      }
    } else if (current_var.type == PP_VARTYPE_DICTIONARY) {
      parent_ids.insert(current_var.value.as_id);
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(current_var);
      if (!dict_var) {
        NOTREACHED();
        return false;
      }
      DCHECK(current_v8->IsObject());
      v8::Handle<v8::Object> v8_object = current_v8->ToObject();

      for (DictionaryVar::KeyValueMap::const_iterator iter =
               dict_var->key_value_map().begin();
           iter != dict_var->key_value_map().end();
           ++iter) {
        const std::string& key = iter->first;
        const PP_Var& child_var = iter->second.get();
        v8::Handle<v8::Value> child_v8;
        if (!GetOrCreateV8Value(isolate, child_var, &child_v8, &did_create,
                                &visited_ids, &parent_ids)) {
          return false;
        }
        if (did_create && CanHaveChildren(child_var))
          stack.push(child_var);
        v8::TryCatch try_catch;
        v8_object->Set(v8::String::NewFromUtf8(isolate,
                                               key.c_str(),
                                               v8::String::kNormalString,
                                               key.length()),
                       child_v8);
        if (try_catch.HasCaught()) {
          LOG(ERROR) << "Setter for property " << key.c_str() << " threw an "
              << "exception.";
          return false;
        }
      }
    }
  }

  *result = handle_scope.Escape(root);
  return true;
}

void V8VarConverter::FromV8Value(
    v8::Handle<v8::Value> val,
    v8::Handle<v8::Context> context,
    const base::Callback<void(const ScopedPPVar&, bool)>& callback) {
  v8::Context::Scope context_scope(context);
  v8::HandleScope handle_scope(context->GetIsolate());

  HandleVarMap visited_handles;
  ParentHandleSet parent_handles;

  std::stack<StackEntry<v8::Handle<v8::Value> > > stack;
  stack.push(StackEntry<v8::Handle<v8::Value> >(val));
  ScopedPPVar root;
  bool is_root = true;

  while (!stack.empty()) {
    v8::Handle<v8::Value> current_v8 = stack.top().val;
    PP_Var current_var;

    if (stack.top().sentinel) {
      stack.pop();
      if (current_v8->IsObject())
        parent_handles.erase(HashedHandle(current_v8->ToObject()));
      continue;
    } else {
      stack.top().sentinel = true;
    }

    bool did_create = false;
    if (!GetOrCreateVar(current_v8, context, &current_var, &did_create,
                        &visited_handles, &parent_handles,
                        resource_converter_.get())) {
      message_loop_proxy_->PostTask(FROM_HERE,
          base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
      return;
    }

    if (is_root) {
      is_root = false;
      root = current_var;
    }

    // Add child nodes to the stack.
    if (current_var.type == PP_VARTYPE_ARRAY) {
      DCHECK(current_v8->IsArray());
      v8::Handle<v8::Array> v8_array = current_v8.As<v8::Array>();
      parent_handles.insert(HashedHandle(v8_array));

      ArrayVar* array_var = ArrayVar::FromPPVar(current_var);
      if (!array_var) {
        NOTREACHED();
        message_loop_proxy_->PostTask(FROM_HERE,
            base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
        return;
      }

      for (uint32 i = 0; i < v8_array->Length(); ++i) {
        v8::TryCatch try_catch;
        v8::Handle<v8::Value> child_v8 = v8_array->Get(i);
        if (try_catch.HasCaught()) {
          message_loop_proxy_->PostTask(FROM_HERE,
              base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
          return;
        }

        if (!v8_array->HasRealIndexedProperty(i))
          continue;

        PP_Var child_var;
        if (!GetOrCreateVar(child_v8, context, &child_var, &did_create,
                            &visited_handles, &parent_handles,
                            resource_converter_.get())) {
          message_loop_proxy_->PostTask(FROM_HERE,
              base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
          return;
        }
        if (did_create && child_v8->IsObject())
          stack.push(child_v8);

        array_var->Set(i, child_var);
      }
    } else if (current_var.type == PP_VARTYPE_DICTIONARY) {
      DCHECK(current_v8->IsObject());
      v8::Handle<v8::Object> v8_object = current_v8->ToObject();
      parent_handles.insert(HashedHandle(v8_object));

      DictionaryVar* dict_var = DictionaryVar::FromPPVar(current_var);
      if (!dict_var) {
        NOTREACHED();
        message_loop_proxy_->PostTask(FROM_HERE,
            base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
        return;
      }

      v8::Handle<v8::Array> property_names(v8_object->GetOwnPropertyNames());
      for (uint32 i = 0; i < property_names->Length(); ++i) {
        v8::Handle<v8::Value> key(property_names->Get(i));

        // Extend this test to cover more types as necessary and if sensible.
        if (!key->IsString() && !key->IsNumber()) {
          NOTREACHED() << "Key \"" << *v8::String::Utf8Value(key) << "\" "
                          "is neither a string nor a number";
          message_loop_proxy_->PostTask(FROM_HERE,
              base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
          return;
        }

        // Skip all callbacks: crbug.com/139933
        if (v8_object->HasRealNamedCallbackProperty(key->ToString()))
          continue;

        v8::String::Utf8Value name_utf8(key->ToString());

        v8::TryCatch try_catch;
        v8::Handle<v8::Value> child_v8 = v8_object->Get(key);
        if (try_catch.HasCaught()) {
          message_loop_proxy_->PostTask(FROM_HERE,
              base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
          return;
        }

        PP_Var child_var;
        if (!GetOrCreateVar(child_v8, context, &child_var, &did_create,
                            &visited_handles, &parent_handles,
                            resource_converter_.get())) {
          message_loop_proxy_->PostTask(FROM_HERE,
              base::Bind(callback, ScopedPPVar(PP_MakeUndefined()), false));
          return;
        }
        if (did_create && child_v8->IsObject())
          stack.push(child_v8);

        bool success = dict_var->SetWithStringKey(
            std::string(*name_utf8, name_utf8.length()), child_var);
        DCHECK(success);
      }
    }
  }
  resource_converter_->Flush(base::Bind(callback, root));
}

}  // namespace content
