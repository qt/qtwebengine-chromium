// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains definitions for CppBoundClass

// Here's the control flow of a JS method getting forwarded to a class.
// - Something calls our NPObject with a function like "Invoke".
// - CppNPObject's static invoke() function forwards it to its attached
//   CppBoundClass's Invoke() method.
// - CppBoundClass has then overridden Invoke() to look up the function
//   name in its internal map of methods, and then calls the appropriate
//   method.

#include "webkit/renderer/cpp_bound_class.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/platform/WebString.h"

using blink::WebBindings;
using blink::WebFrame;

namespace webkit_glue {

namespace {

class CppVariantPropertyCallback : public CppBoundClass::PropertyCallback {
 public:
  CppVariantPropertyCallback(CppVariant* value) : value_(value) { }

  virtual bool GetValue(CppVariant* value) OVERRIDE {
    value->Set(*value_);
    return true;
  }
  virtual bool SetValue(const CppVariant& value) OVERRIDE {
    value_->Set(value);
    return true;
  }

 private:
  CppVariant* value_;
};

class GetterPropertyCallback : public CppBoundClass::PropertyCallback {
public:
  GetterPropertyCallback(const CppBoundClass::GetterCallback& callback)
      : callback_(callback) { }

  virtual bool GetValue(CppVariant* value) OVERRIDE {
    callback_.Run(value);
    return true;
  }

  virtual bool SetValue(const CppVariant& value) OVERRIDE {
    return false;
  }

private:
  CppBoundClass::GetterCallback callback_;
};

}

// Our special NPObject type.  We extend an NPObject with a pointer to a
// CppBoundClass, which is just a C++ interface that we forward all NPObject
// callbacks to.
struct CppNPObject {
  NPObject parent;  // This must be the first field in the struct.
  CppBoundClass* bound_class;

  //
  // All following objects and functions are static, and just used to interface
  // with NPObject/NPClass.
  //

  // An NPClass associates static functions of CppNPObject with the
  // function pointers used by the JS runtime.
  static NPClass np_class_;

  // Allocate a new NPObject with the specified class.
  static NPObject* allocate(NPP npp, NPClass* aClass);

  // Free an object.
  static void deallocate(NPObject* obj);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given property.  Called by the JS runtime.
  static bool hasProperty(NPObject *obj, NPIdentifier ident);

  // Returns true if the C++ class associated with this NPObject exposes the
  // given method.  Called by the JS runtime.
  static bool hasMethod(NPObject *obj, NPIdentifier ident);

  // If the given method is exposed by the C++ class associated with this
  // NPObject, invokes it with the given args and returns a result.  Otherwise,
  // returns "undefined" (in the JavaScript sense).  Called by the JS runtime.
  static bool invoke(NPObject *obj, NPIdentifier ident,
                     const NPVariant *args, uint32_t arg_count,
                     NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, returns its value.  Otherwise, returns "undefined" (in the
  // JavaScript sense).  Called by the JS runtime.
  static bool getProperty(NPObject *obj, NPIdentifier ident,
                          NPVariant *result);

  // If the given property is exposed by the C++ class associated with this
  // NPObject, sets its value.  Otherwise, does nothing. Called by the JS
  // runtime.
  static bool setProperty(NPObject *obj, NPIdentifier ident,
                          const NPVariant *value);
};

// Build CppNPObject's static function pointers into an NPClass, for use
// in constructing NPObjects for the C++ classes.
NPClass CppNPObject::np_class_ = {
  NP_CLASS_STRUCT_VERSION,
  CppNPObject::allocate,
  CppNPObject::deallocate,
  /* NPInvalidateFunctionPtr */ NULL,
  CppNPObject::hasMethod,
  CppNPObject::invoke,
  /* NPInvokeDefaultFunctionPtr */ NULL,
  CppNPObject::hasProperty,
  CppNPObject::getProperty,
  CppNPObject::setProperty,
  /* NPRemovePropertyFunctionPtr */ NULL
};

/* static */ NPObject* CppNPObject::allocate(NPP npp, NPClass* aClass) {
  CppNPObject* obj = new CppNPObject;
  // obj->parent will be initialized by the NPObject code calling this.
  obj->bound_class = NULL;
  return &obj->parent;
}

/* static */ void CppNPObject::deallocate(NPObject* np_obj) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  delete obj;
}

/* static */ bool CppNPObject::hasMethod(NPObject* np_obj,
                                         NPIdentifier ident) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->HasMethod(ident);
}

/* static */ bool CppNPObject::hasProperty(NPObject* np_obj,
                                           NPIdentifier ident) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->HasProperty(ident);
}

/* static */ bool CppNPObject::invoke(NPObject* np_obj, NPIdentifier ident,
                                      const NPVariant* args, uint32_t arg_count,
                                      NPVariant* result) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->Invoke(ident, args, arg_count, result);
}

/* static */ bool CppNPObject::getProperty(NPObject* np_obj,
                                           NPIdentifier ident,
                                           NPVariant* result) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->GetProperty(ident, result);
}

/* static */ bool CppNPObject::setProperty(NPObject* np_obj,
                                           NPIdentifier ident,
                                           const NPVariant* value) {
  CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
  return obj->bound_class->SetProperty(ident, value);
}

CppBoundClass::CppBoundClass() : bound_to_frame_(false), npp_(new NPP_t) {
  WebBindings::registerObjectOwner(npp_.get());
}

CppBoundClass::~CppBoundClass() {
  STLDeleteValues(&properties_);

  // TODO(wez): Remove once crrev.com/14019005 lands.
  if (bound_to_frame_)
    WebBindings::unregisterObject(NPVARIANT_TO_OBJECT(self_variant_));

  WebBindings::unregisterObjectOwner(npp_.get());
}

bool CppBoundClass::HasMethod(NPIdentifier ident) const {
  return (methods_.find(ident) != methods_.end());
}

bool CppBoundClass::HasProperty(NPIdentifier ident) const {
  return (properties_.find(ident) != properties_.end());
}

bool CppBoundClass::Invoke(NPIdentifier ident,
                           const NPVariant* args,
                           size_t arg_count,
                           NPVariant* result) {
  MethodList::const_iterator method = methods_.find(ident);
  Callback callback;
  if (method == methods_.end()) {
    if (!fallback_callback_.is_null()) {
      callback = fallback_callback_;
    } else {
      VOID_TO_NPVARIANT(*result);
      return false;
    }
  } else {
    callback = method->second;
  }

  // Build a CppArgumentList argument vector from the NPVariants coming in.
  CppArgumentList cpp_args(arg_count);
  for (size_t i = 0; i < arg_count; i++)
    cpp_args[i].Set(args[i]);

  CppVariant cpp_result;
  callback.Run(cpp_args, &cpp_result);

  cpp_result.CopyToNPVariant(result);
  return true;
}

bool CppBoundClass::GetProperty(NPIdentifier ident, NPVariant* result) const {
  PropertyList::const_iterator callback = properties_.find(ident);
  if (callback == properties_.end()) {
    VOID_TO_NPVARIANT(*result);
    return false;
  }

  CppVariant cpp_value;
  if (!callback->second->GetValue(&cpp_value))
    return false;
  cpp_value.CopyToNPVariant(result);
  return true;
}

bool CppBoundClass::SetProperty(NPIdentifier ident,
                                const NPVariant* value) {
  PropertyList::iterator callback = properties_.find(ident);
  if (callback == properties_.end())
    return false;

  CppVariant cpp_value;
  cpp_value.Set(*value);
  return (*callback).second->SetValue(cpp_value);
}

void CppBoundClass::BindCallback(const std::string& name,
                                 const Callback& callback) {
  NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
  if (callback.is_null()) {
    methods_.erase(ident);
    return;
  }

  methods_[ident] = callback;
}

void CppBoundClass::BindGetterCallback(const std::string& name,
                                       const GetterCallback& callback) {
  PropertyCallback* property_callback = callback.is_null() ?
      NULL : new GetterPropertyCallback(callback);

  BindProperty(name, property_callback);
}

void CppBoundClass::BindProperty(const std::string& name, CppVariant* prop) {
  PropertyCallback* property_callback = prop == NULL ?
      NULL : new CppVariantPropertyCallback(prop);

  BindProperty(name, property_callback);
}

void CppBoundClass::BindProperty(const std::string& name,
                                 PropertyCallback* callback) {
  NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
  PropertyList::iterator old_callback = properties_.find(ident);
  if (old_callback != properties_.end()) {
    delete old_callback->second;
    if (callback == NULL) {
      properties_.erase(old_callback);
      return;
    }
  }

  properties_[ident] = callback;
}

bool CppBoundClass::IsMethodRegistered(const std::string& name) const {
  NPIdentifier ident = WebBindings::getStringIdentifier(name.c_str());
  MethodList::const_iterator callback = methods_.find(ident);
  return (callback != methods_.end());
}

CppVariant* CppBoundClass::GetAsCppVariant() {
  if (!self_variant_.isObject()) {
    // Create an NPObject using our static NPClass.  The first argument has type
    // NPP, but is only used to track object ownership, so passing this is fine.
    NPObject* np_obj = WebBindings::createObject(
        npp_.get(), &CppNPObject::np_class_);
    CppNPObject* obj = reinterpret_cast<CppNPObject*>(np_obj);
    obj->bound_class = this;
    self_variant_.Set(np_obj);
    WebBindings::releaseObject(np_obj);  // CppVariant takes the reference.
  }
  DCHECK(self_variant_.isObject());
  return &self_variant_;
}

void CppBoundClass::BindToJavascript(WebFrame* frame,
                                     const std::string& classname) {
  // BindToWindowObject will take its own reference to the NPObject, and clean
  // up after itself. It will also (indirectly) register the object with V8,
  // against an owner pointer we supply, so we must register that as an owner,
  // and unregister when we teardown.
  frame->bindToWindowObject(ASCIIToUTF16(classname),
                            NPVARIANT_TO_OBJECT(*GetAsCppVariant()));
  bound_to_frame_ = true;
}

}  // namespace webkit_glue
