// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_IC_H_
#define V8_IC_H_

#include "macro-assembler.h"
#include "type-info.h"

namespace v8 {
namespace internal {


// IC_UTIL_LIST defines all utility functions called from generated
// inline caching code. The argument for the macro, ICU, is the function name.
#define IC_UTIL_LIST(ICU)                             \
  ICU(LoadIC_Miss)                                    \
  ICU(KeyedLoadIC_Miss)                               \
  ICU(CallIC_Miss)                                    \
  ICU(KeyedCallIC_Miss)                               \
  ICU(StoreIC_Miss)                                   \
  ICU(StoreIC_ArrayLength)                            \
  ICU(StoreIC_Slow)                                   \
  ICU(SharedStoreIC_ExtendStorage)                    \
  ICU(KeyedStoreIC_Miss)                              \
  ICU(KeyedStoreIC_Slow)                              \
  /* Utilities for IC stubs. */                       \
  ICU(StoreCallbackProperty)                          \
  ICU(LoadPropertyWithInterceptorOnly)                \
  ICU(LoadPropertyWithInterceptorForLoad)             \
  ICU(LoadPropertyWithInterceptorForCall)             \
  ICU(KeyedLoadPropertyWithInterceptor)               \
  ICU(StoreInterceptorProperty)                       \
  ICU(CompareIC_Miss)                                 \
  ICU(BinaryOpIC_Miss)                                \
  ICU(CompareNilIC_Miss)                              \
  ICU(Unreachable)                                    \
  ICU(ToBooleanIC_Miss)
//
// IC is the base class for LoadIC, StoreIC, CallIC, KeyedLoadIC,
// and KeyedStoreIC.
//
class IC {
 public:
  // The ids for utility called from the generated code.
  enum UtilityId {
  #define CONST_NAME(name) k##name,
    IC_UTIL_LIST(CONST_NAME)
  #undef CONST_NAME
    kUtilityCount
  };

  // Looks up the address of the named utility.
  static Address AddressFromUtilityId(UtilityId id);

  // Alias the inline cache state type to make the IC code more readable.
  typedef InlineCacheState State;

  // The IC code is either invoked with no extra frames on the stack
  // or with a single extra frame for supporting calls.
  enum FrameDepth {
    NO_EXTRA_FRAME = 0,
    EXTRA_CALL_FRAME = 1
  };

  // Construct the IC structure with the given number of extra
  // JavaScript frames on the stack.
  IC(FrameDepth depth, Isolate* isolate);
  virtual ~IC() {}

  State state() const { return state_; }
  inline Address address() const;

  // Compute the current IC state based on the target stub, receiver and name.
  void UpdateState(Handle<Object> receiver, Handle<Object> name);
  void MarkMonomorphicPrototypeFailure() {
    state_ = MONOMORPHIC_PROTOTYPE_FAILURE;
  }

  // Clear the inline cache to initial state.
  static void Clear(Isolate* isolate, Address address);

  // Computes the reloc info for this IC. This is a fairly expensive
  // operation as it has to search through the heap to find the code
  // object that contains this IC site.
  RelocInfo::Mode ComputeMode();

  // Returns if this IC is for contextual (no explicit receiver)
  // access to properties.
  bool IsUndeclaredGlobal(Handle<Object> receiver) {
    if (receiver->IsGlobalObject()) {
      return SlowIsUndeclaredGlobal();
    } else {
      ASSERT(!SlowIsUndeclaredGlobal());
      return false;
    }
  }

  bool SlowIsUndeclaredGlobal() {
    return ComputeMode() == RelocInfo::CODE_TARGET_CONTEXT;
  }

#ifdef DEBUG
  bool IsLoadStub() {
    return target()->is_load_stub() || target()->is_keyed_load_stub();
  }

  bool IsStoreStub() {
    return target()->is_store_stub() || target()->is_keyed_store_stub();
  }

  bool IsCallStub() {
    return target()->is_call_stub() || target()->is_keyed_call_stub();
  }
#endif

  // Determines which map must be used for keeping the code stub.
  // These methods should not be called with undefined or null.
  static inline InlineCacheHolderFlag GetCodeCacheForObject(Object* object);
  // TODO(verwaest): This currently returns a HeapObject rather than JSObject*
  // since loading the IC for loading the length from strings are stored on
  // the string map directly, rather than on the JSObject-typed prototype.
  static inline HeapObject* GetCodeCacheHolder(Isolate* isolate,
                                               Object* object,
                                               InlineCacheHolderFlag holder);

  static inline InlineCacheHolderFlag GetCodeCacheFlag(Type* type);
  static inline Handle<Map> GetCodeCacheHolder(InlineCacheHolderFlag flag,
                                               Type* type,
                                               Isolate* isolate);

  static bool IsCleared(Code* code) {
    InlineCacheState state = code->ic_state();
    return state == UNINITIALIZED || state == PREMONOMORPHIC;
  }

  // Utility functions to convert maps to types and back. There are two special
  // cases:
  // - The heap_number_map is used as a marker which includes heap numbers as
  //   well as smis.
  // - The oddball map is only used for booleans.
  static Handle<Map> TypeToMap(Type* type, Isolate* isolate);
  static Type* MapToType(Handle<Map> type);
  static Handle<Type> CurrentTypeOf(Handle<Object> object, Isolate* isolate);

 protected:
  // Get the call-site target; used for determining the state.
  Handle<Code> target() const { return target_; }

  Address fp() const { return fp_; }
  Address pc() const { return *pc_address_; }
  Isolate* isolate() const { return isolate_; }

#ifdef ENABLE_DEBUGGER_SUPPORT
  // Computes the address in the original code when the code running is
  // containing break points (calls to DebugBreakXXX builtins).
  Address OriginalCodeAddress() const;
#endif

  // Set the call-site target.
  void set_target(Code* code) {
    SetTargetAtAddress(address(), code);
    target_set_ = true;
  }

  bool is_target_set() { return target_set_; }

#ifdef DEBUG
  char TransitionMarkFromState(IC::State state);

  void TraceIC(const char* type, Handle<Object> name);
#endif

  Failure* TypeError(const char* type,
                     Handle<Object> object,
                     Handle<Object> key);
  Failure* ReferenceError(const char* type, Handle<String> name);

  // Access the target code for the given IC address.
  static inline Code* GetTargetAtAddress(Address address);
  static inline void SetTargetAtAddress(Address address, Code* target);
  static void PostPatching(Address address, Code* target, Code* old_target);

  // Compute the handler either by compiling or by retrieving a cached version.
  Handle<Code> ComputeHandler(LookupResult* lookup,
                              Handle<Object> object,
                              Handle<String> name,
                              Handle<Object> value = Handle<Code>::null());
  virtual Handle<Code> CompileHandler(LookupResult* lookup,
                                      Handle<Object> object,
                                      Handle<String> name,
                                      Handle<Object> value,
                                      InlineCacheHolderFlag cache_holder) {
    UNREACHABLE();
    return Handle<Code>::null();
  }

  void UpdateMonomorphicIC(Handle<Type> type,
                           Handle<Code> handler,
                           Handle<String> name);

  bool UpdatePolymorphicIC(Handle<Type> type,
                           Handle<String> name,
                           Handle<Code> code);

  virtual void UpdateMegamorphicCache(Type* type, Name* name, Code* code);

  void CopyICToMegamorphicCache(Handle<String> name);
  bool IsTransitionOfMonomorphicTarget(Type* type);
  void PatchCache(Handle<Type> type,
                  Handle<String> name,
                  Handle<Code> code);
  virtual Code::Kind kind() const {
    UNREACHABLE();
    return Code::STUB;
  }
  virtual Handle<Code> slow_stub() const {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> megamorphic_stub() {
    UNREACHABLE();
    return Handle<Code>::null();
  }
  virtual Handle<Code> generic_stub() const {
    UNREACHABLE();
    return Handle<Code>::null();
  }

  bool TryRemoveInvalidPrototypeDependentStub(Handle<Object> receiver,
                                              Handle<String> name);
  void TryRemoveInvalidHandlers(Handle<Map> map, Handle<String> name);

  virtual ExtraICState extra_ic_state() { return kNoExtraICState; }

 private:
  Code* raw_target() const { return GetTargetAtAddress(address()); }

  // Frame pointer for the frame that uses (calls) the IC.
  Address fp_;

  // All access to the program counter of an IC structure is indirect
  // to make the code GC safe. This feature is crucial since
  // GetProperty and SetProperty are called and they in turn might
  // invoke the garbage collector.
  Address* pc_address_;

  Isolate* isolate_;

  // The original code target that missed.
  Handle<Code> target_;
  State state_;
  bool target_set_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IC);
};


// An IC_Utility encapsulates IC::UtilityId. It exists mainly because you
// cannot make forward declarations to an enum.
class IC_Utility {
 public:
  explicit IC_Utility(IC::UtilityId id)
    : address_(IC::AddressFromUtilityId(id)), id_(id) {}

  Address address() const { return address_; }

  IC::UtilityId id() const { return id_; }
 private:
  Address address_;
  IC::UtilityId id_;
};


class CallICBase: public IC {
 public:
  // ExtraICState bits
  class Contextual: public BitField<ContextualMode, 0, 1> {};
  class StringStubState: public BitField<StringStubFeedback, 1, 1> {};
  static ExtraICState ComputeExtraICState(ContextualMode mode,
                                          StringStubFeedback feedback) {
    return Contextual::encode(mode) | StringStubState::encode(feedback);
  }

  // Returns a JSFunction or a Failure.
  MUST_USE_RESULT MaybeObject* LoadFunction(Handle<Object> object,
                                            Handle<String> name);

 protected:
  CallICBase(Code::Kind kind, Isolate* isolate)
      : IC(EXTRA_CALL_FRAME, isolate), kind_(kind) {}

  // Compute a monomorphic stub if possible, otherwise return a null handle.
  Handle<Code> ComputeMonomorphicStub(LookupResult* lookup,
                                      Handle<Object> object,
                                      Handle<String> name);

  // Update the inline cache and the global stub cache based on the lookup
  // result.
  void UpdateCaches(LookupResult* lookup,
                    Handle<Object> object,
                    Handle<String> name);

  // Returns a JSFunction if the object can be called as a function, and
  // patches the stack to be ready for the call.  Otherwise, it returns the
  // undefined value.
  Handle<Object> TryCallAsFunction(Handle<Object> object);

  void ReceiverToObjectIfRequired(Handle<Object> callee, Handle<Object> object);

  static void Clear(Address address, Code* target);

  // Platform-specific code generation functions used by both call and
  // keyed call.
  static void GenerateMiss(MacroAssembler* masm,
                           int argc,
                           IC::UtilityId id,
                           ExtraICState extra_state);

  static void GenerateNormal(MacroAssembler* masm, int argc);

  static void GenerateMonomorphicCacheProbe(MacroAssembler* masm,
                                            int argc,
                                            Code::Kind kind,
                                            ExtraICState extra_state);

  virtual Handle<Code> megamorphic_stub();
  virtual Handle<Code> pre_monomorphic_stub();

  Code::Kind kind_;

  friend class IC;
};


class CallIC: public CallICBase {
 public:
  explicit CallIC(Isolate* isolate)
      : CallICBase(Code::CALL_IC, isolate),
        extra_ic_state_(target()->extra_ic_state()) {
    ASSERT(target()->is_call_stub());
  }

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm,
                                 int argc,
                                 ExtraICState extra_state) {
    GenerateMiss(masm, argc, extra_state);
  }

  static void GenerateMiss(MacroAssembler* masm,
                           int argc,
                           ExtraICState extra_state) {
    CallICBase::GenerateMiss(masm, argc, IC::kCallIC_Miss, extra_state);
  }

  static void GenerateMegamorphic(MacroAssembler* masm,
                                  int argc,
                                  ExtraICState extra_ic_state);

  static void GenerateNormal(MacroAssembler* masm, int argc) {
    CallICBase::GenerateNormal(masm, argc);
    GenerateMiss(masm, argc, kNoExtraICState);
  }
  bool TryUpdateExtraICState(LookupResult* lookup, Handle<Object> object);

 protected:
  virtual ExtraICState extra_ic_state() { return extra_ic_state_; }

 private:
  ExtraICState extra_ic_state_;
};


class KeyedCallIC: public CallICBase {
 public:
  explicit KeyedCallIC(Isolate* isolate)
      : CallICBase(Code::KEYED_CALL_IC, isolate) {
    ASSERT(target()->is_keyed_call_stub());
  }

  MUST_USE_RESULT MaybeObject* LoadFunction(Handle<Object> object,
                                            Handle<Object> key);

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm, int argc) {
    GenerateMiss(masm, argc);
  }

  static void GenerateMiss(MacroAssembler* masm, int argc) {
    CallICBase::GenerateMiss(masm, argc, IC::kKeyedCallIC_Miss,
                             kNoExtraICState);
  }

  static void GenerateMegamorphic(MacroAssembler* masm, int argc);
  static void GenerateNormal(MacroAssembler* masm, int argc);
  static void GenerateNonStrictArguments(MacroAssembler* masm, int argc);
};


class LoadIC: public IC {
 public:
  explicit LoadIC(FrameDepth depth, Isolate* isolate) : IC(depth, isolate) {
    ASSERT(IsLoadStub());
  }

  // Code generator routines.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);

  MUST_USE_RESULT MaybeObject* Load(Handle<Object> object,
                                    Handle<String> name);

 protected:
  virtual Code::Kind kind() const { return Code::LOAD_IC; }

  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->LoadIC_Slow();
  }

  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->LoadIC_Megamorphic();
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    Handle<Object> object,
                    Handle<String> name);

  virtual Handle<Code> CompileHandler(LookupResult* lookup,
                                      Handle<Object> object,
                                      Handle<String> name,
                                      Handle<Object> unused,
                                      InlineCacheHolderFlag cache_holder);

 private:
  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->LoadIC_Initialize();
  }

  static Handle<Code> pre_monomorphic_stub(Isolate* isolate) {
    return isolate->builtins()->LoadIC_PreMonomorphic();
  }

  virtual Handle<Code> pre_monomorphic_stub() {
    return pre_monomorphic_stub(isolate());
  }

  Handle<Code> SimpleFieldLoad(int offset,
                               bool inobject = true,
                               Representation representation =
                                    Representation::Tagged());

  static void Clear(Isolate* isolate, Address address, Code* target);

  friend class IC;
};


class KeyedLoadIC: public LoadIC {
 public:
  explicit KeyedLoadIC(FrameDepth depth, Isolate* isolate)
      : LoadIC(depth, isolate) {
    ASSERT(target()->is_keyed_load_stub());
  }

  MUST_USE_RESULT MaybeObject* Load(Handle<Object> object,
                                    Handle<Object> key);

  // Code generator routines.
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateRuntimeGetProperty(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateGeneric(MacroAssembler* masm);
  static void GenerateString(MacroAssembler* masm);
  static void GenerateIndexedInterceptor(MacroAssembler* masm);
  static void GenerateNonStrictArguments(MacroAssembler* masm);

  // Bit mask to be tested against bit field for the cases when
  // generic stub should go into slow case.
  // Access check is necessary explicitly since generic stub does not perform
  // map checks.
  static const int kSlowCaseBitFieldMask =
      (1 << Map::kIsAccessCheckNeeded) | (1 << Map::kHasIndexedInterceptor);

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_LOAD_IC; }

  Handle<Code> LoadElementStub(Handle<JSObject> receiver);

  virtual Handle<Code> megamorphic_stub() {
    return isolate()->builtins()->KeyedLoadIC_Generic();
  }
  virtual Handle<Code> generic_stub() const {
    return isolate()->builtins()->KeyedLoadIC_Generic();
  }
  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->KeyedLoadIC_Slow();
  }

  virtual void UpdateMegamorphicCache(Type* type, Name* name, Code* code) { }

 private:
  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate) {
    return isolate->builtins()->KeyedLoadIC_Initialize();
  }
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate) {
    return isolate->builtins()->KeyedLoadIC_PreMonomorphic();
  }
  virtual Handle<Code> pre_monomorphic_stub() {
    return pre_monomorphic_stub(isolate());
  }
  Handle<Code> indexed_interceptor_stub() {
    return isolate()->builtins()->KeyedLoadIC_IndexedInterceptor();
  }
  Handle<Code> non_strict_arguments_stub() {
    return isolate()->builtins()->KeyedLoadIC_NonStrictArguments();
  }
  Handle<Code> string_stub() {
    return isolate()->builtins()->KeyedLoadIC_String();
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  friend class IC;
};


class StoreIC: public IC {
 public:
  // ExtraICState bits
  class StrictModeState: public BitField<StrictModeFlag, 0, 1> {};
  static ExtraICState ComputeExtraICState(StrictModeFlag flag) {
    return StrictModeState::encode(flag);
  }

  static StrictModeFlag GetStrictMode(ExtraICState state) {
    return StrictModeState::decode(state);
  }

  // For convenience, a statically declared encoding of strict mode extra
  // IC state.
  static const ExtraICState kStrictModeState =
      1 << StrictModeState::kShift;

  StoreIC(FrameDepth depth, Isolate* isolate)
      : IC(depth, isolate),
        strict_mode_(GetStrictMode(target()->extra_ic_state())) {
    ASSERT(IsStoreStub());
  }

  StrictModeFlag strict_mode() const { return strict_mode_; }

  // Code generators for stub routines. Only called once at startup.
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateMegamorphic(MacroAssembler* masm,
                                  ExtraICState extra_ic_state);
  static void GenerateNormal(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode);

  MUST_USE_RESULT MaybeObject* Store(
      Handle<Object> object,
      Handle<String> name,
      Handle<Object> value,
      JSReceiver::StoreFromKeyed store_mode =
          JSReceiver::CERTAINLY_NOT_STORE_FROM_KEYED);

 protected:
  virtual Code::Kind kind() const { return Code::STORE_IC; }
  virtual Handle<Code> megamorphic_stub() {
    if (strict_mode() == kStrictMode) {
      return isolate()->builtins()->StoreIC_Megamorphic_Strict();
    } else {
      return isolate()->builtins()->StoreIC_Megamorphic();
    }
  }
  // Stub accessors.
  virtual Handle<Code> generic_stub() const {
    if (strict_mode() == kStrictMode) {
      return isolate()->builtins()->StoreIC_Generic_Strict();
    } else {
      return isolate()->builtins()->StoreIC_Generic();
    }
  }

  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->StoreIC_Slow();
  }

  virtual Handle<Code> pre_monomorphic_stub() {
    return pre_monomorphic_stub(isolate(), strict_mode());
  }

  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           StrictModeFlag strict_mode) {
    if (strict_mode == kStrictMode) {
      return isolate->builtins()->StoreIC_PreMonomorphic_Strict();
    } else {
      return isolate->builtins()->StoreIC_PreMonomorphic();
    }
  }

  // Update the inline cache and the global stub cache based on the
  // lookup result.
  void UpdateCaches(LookupResult* lookup,
                    Handle<JSObject> receiver,
                    Handle<String> name,
                    Handle<Object> value);
  virtual Handle<Code> CompileHandler(LookupResult* lookup,
                                      Handle<Object> object,
                                      Handle<String> name,
                                      Handle<Object> value,
                                      InlineCacheHolderFlag cache_holder);

  virtual ExtraICState extra_ic_state() {
    return ComputeExtraICState(strict_mode());
  }

 private:
  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT(GetStrictMode(code->extra_ic_state()) ==
           GetStrictMode(target()->extra_ic_state()));
    IC::set_target(code);
  }

  static Handle<Code> initialize_stub(Isolate* isolate,
                                      StrictModeFlag strict_mode) {
    if (strict_mode == kStrictMode) {
      return isolate->builtins()->StoreIC_Initialize_Strict();
    } else {
      return isolate->builtins()->StoreIC_Initialize();
    }
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  StrictModeFlag strict_mode_;

  friend class IC;
};


enum KeyedStoreCheckMap {
  kDontCheckMap,
  kCheckMap
};


enum KeyedStoreIncrementLength {
  kDontIncrementLength,
  kIncrementLength
};


class KeyedStoreIC: public StoreIC {
 public:
  // ExtraICState bits (building on IC)
  // ExtraICState bits
  class ExtraICStateKeyedAccessStoreMode:
      public BitField<KeyedAccessStoreMode, 1, 4> {};  // NOLINT

  static ExtraICState ComputeExtraICState(StrictModeFlag flag,
                                                KeyedAccessStoreMode mode) {
    return StrictModeState::encode(flag) |
        ExtraICStateKeyedAccessStoreMode::encode(mode);
  }

  static KeyedAccessStoreMode GetKeyedAccessStoreMode(
      ExtraICState extra_state) {
    return ExtraICStateKeyedAccessStoreMode::decode(extra_state);
  }

  KeyedStoreIC(FrameDepth depth, Isolate* isolate)
      : StoreIC(depth, isolate) {
    ASSERT(target()->is_keyed_store_stub());
  }

  MUST_USE_RESULT MaybeObject* Store(Handle<Object> object,
                                     Handle<Object> name,
                                     Handle<Object> value);

  // Code generators for stub routines.  Only called once at startup.
  static void GenerateInitialize(MacroAssembler* masm) { GenerateMiss(masm); }
  static void GeneratePreMonomorphic(MacroAssembler* masm) {
    GenerateMiss(masm);
  }
  static void GenerateMiss(MacroAssembler* masm);
  static void GenerateSlow(MacroAssembler* masm);
  static void GenerateRuntimeSetProperty(MacroAssembler* masm,
                                         StrictModeFlag strict_mode);
  static void GenerateGeneric(MacroAssembler* masm, StrictModeFlag strict_mode);
  static void GenerateNonStrictArguments(MacroAssembler* masm);

 protected:
  virtual Code::Kind kind() const { return Code::KEYED_STORE_IC; }

  virtual void UpdateMegamorphicCache(Type* type, Name* name, Code* code) { }

  virtual ExtraICState extra_ic_state() {
    return ComputeExtraICState(strict_mode(), STANDARD_STORE);
  }

  virtual Handle<Code> pre_monomorphic_stub() {
    return pre_monomorphic_stub(isolate(), strict_mode());
  }
  static Handle<Code> pre_monomorphic_stub(Isolate* isolate,
                                           StrictModeFlag strict_mode) {
    if (strict_mode == kStrictMode) {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic_Strict();
    } else {
      return isolate->builtins()->KeyedStoreIC_PreMonomorphic();
    }
  }
  virtual Handle<Code> slow_stub() const {
    return isolate()->builtins()->KeyedStoreIC_Slow();
  }
  virtual Handle<Code> megamorphic_stub() {
    if (strict_mode() == kStrictMode) {
      return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
    } else {
      return isolate()->builtins()->KeyedStoreIC_Generic();
    }
  }

  Handle<Code> StoreElementStub(Handle<JSObject> receiver,
                                KeyedAccessStoreMode store_mode);

 private:
  void set_target(Code* code) {
    // Strict mode must be preserved across IC patching.
    ASSERT(GetStrictMode(code->extra_ic_state()) == strict_mode());
    IC::set_target(code);
  }

  // Stub accessors.
  static Handle<Code> initialize_stub(Isolate* isolate,
                                      StrictModeFlag strict_mode) {
    if (strict_mode == kStrictMode) {
      return isolate->builtins()->KeyedStoreIC_Initialize_Strict();
    } else {
      return isolate->builtins()->KeyedStoreIC_Initialize();
    }
  }

  virtual Handle<Code> generic_stub() const {
    if (strict_mode() == kStrictMode) {
      return isolate()->builtins()->KeyedStoreIC_Generic_Strict();
    } else {
      return isolate()->builtins()->KeyedStoreIC_Generic();
    }
  }

  Handle<Code> non_strict_arguments_stub() {
    return isolate()->builtins()->KeyedStoreIC_NonStrictArguments();
  }

  static void Clear(Isolate* isolate, Address address, Code* target);

  KeyedAccessStoreMode GetStoreMode(Handle<JSObject> receiver,
                                    Handle<Object> key,
                                    Handle<Object> value);

  Handle<Map> ComputeTransitionedMap(Handle<JSObject> receiver,
                                     KeyedAccessStoreMode store_mode);

  friend class IC;
};


// Mode to overwrite BinaryExpression values.
enum OverwriteMode { NO_OVERWRITE, OVERWRITE_LEFT, OVERWRITE_RIGHT };

// Type Recording BinaryOpIC, that records the types of the inputs and outputs.
class BinaryOpIC: public IC {
 public:
  class State V8_FINAL BASE_EMBEDDED {
   public:
    explicit State(ExtraICState extra_ic_state);

    State(Token::Value op, OverwriteMode mode)
        : op_(op), mode_(mode), left_kind_(NONE), right_kind_(NONE),
          result_kind_(NONE) {
      ASSERT_LE(FIRST_TOKEN, op);
      ASSERT_LE(op, LAST_TOKEN);
    }

    InlineCacheState GetICState() const {
      if (Max(left_kind_, right_kind_) == NONE) {
        return ::v8::internal::UNINITIALIZED;
      }
      if (Max(left_kind_, right_kind_) == GENERIC) {
        return ::v8::internal::MEGAMORPHIC;
      }
      if (Min(left_kind_, right_kind_) == GENERIC) {
        return ::v8::internal::GENERIC;
      }
      return ::v8::internal::MONOMORPHIC;
    }

    ExtraICState GetExtraICState() const;

    static void GenerateAheadOfTime(
        Isolate*, void (*Generate)(Isolate*, const State&));

    bool CanReuseDoubleBox() const {
      return (result_kind_ > SMI && result_kind_ <= NUMBER) &&
          ((mode_ == OVERWRITE_LEFT &&
            left_kind_ > SMI && left_kind_ <= NUMBER) ||
           (mode_ == OVERWRITE_RIGHT &&
            right_kind_ > SMI && right_kind_ <= NUMBER));
    }

    bool HasSideEffects() const {
      return Max(left_kind_, right_kind_) == GENERIC;
    }

    bool UseInlinedSmiCode() const {
      return KindMaybeSmi(left_kind_) || KindMaybeSmi(right_kind_);
    }

    static const int FIRST_TOKEN = Token::BIT_OR;
    static const int LAST_TOKEN = Token::MOD;

    Token::Value op() const { return op_; }
    OverwriteMode mode() const { return mode_; }
    Maybe<int> fixed_right_arg() const { return fixed_right_arg_; }

    Handle<Type> GetLeftType(Isolate* isolate) const {
      return KindToType(left_kind_, isolate);
    }
    Handle<Type> GetRightType(Isolate* isolate) const {
      return KindToType(right_kind_, isolate);
    }
    Handle<Type> GetResultType(Isolate* isolate) const;

    void Print(StringStream* stream) const;

    void Update(Handle<Object> left,
                Handle<Object> right,
                Handle<Object> result);

   private:
    enum Kind { NONE, SMI, INT32, NUMBER, STRING, GENERIC };

    Kind UpdateKind(Handle<Object> object, Kind kind) const;

    static const char* KindToString(Kind kind);
    static Handle<Type> KindToType(Kind kind, Isolate* isolate);
    static bool KindMaybeSmi(Kind kind) {
      return (kind >= SMI && kind <= NUMBER) || kind == GENERIC;
    }

    // We truncate the last bit of the token.
    STATIC_ASSERT(LAST_TOKEN - FIRST_TOKEN < (1 << 4));
    class OpField:                 public BitField<int, 0, 4> {};
    class OverwriteModeField:      public BitField<OverwriteMode, 4, 2> {};
    class SSE2Field:               public BitField<bool, 6, 1> {};
    class ResultKindField:         public BitField<Kind, 7, 3> {};
    class LeftKindField:           public BitField<Kind, 10,  3> {};
    // When fixed right arg is set, we don't need to store the right kind.
    // Thus the two fields can overlap.
    class HasFixedRightArgField:   public BitField<bool, 13, 1> {};
    class FixedRightArgValueField: public BitField<int,  14, 4> {};
    class RightKindField:          public BitField<Kind, 14, 3> {};

    Token::Value op_;
    OverwriteMode mode_;
    Kind left_kind_;
    Kind right_kind_;
    Kind result_kind_;
    Maybe<int> fixed_right_arg_;
  };

  explicit BinaryOpIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) { }

  static Builtins::JavaScript TokenToJSBuiltin(Token::Value op);

  MUST_USE_RESULT MaybeObject* Transition(Handle<Object> left,
                                          Handle<Object> right);
};


class CompareIC: public IC {
 public:
  // The type/state lattice is defined by the following inequations:
  //   UNINITIALIZED < ...
  //   ... < GENERIC
  //   SMI < NUMBER
  //   INTERNALIZED_STRING < STRING
  //   KNOWN_OBJECT < OBJECT
  enum State {
    UNINITIALIZED,
    SMI,
    NUMBER,
    STRING,
    INTERNALIZED_STRING,
    UNIQUE_NAME,    // Symbol or InternalizedString
    OBJECT,         // JSObject
    KNOWN_OBJECT,   // JSObject with specific map (faster check)
    GENERIC
  };

  static State NewInputState(State old_state, Handle<Object> value);

  static Handle<Type> StateToType(Isolate* isolate,
                                  State state,
                                  Handle<Map> map = Handle<Map>());

  static void StubInfoToType(int stub_minor_key,
                             Handle<Type>* left_type,
                             Handle<Type>* right_type,
                             Handle<Type>* overall_type,
                             Handle<Map> map,
                             Isolate* isolate);

  CompareIC(Isolate* isolate, Token::Value op)
      : IC(EXTRA_CALL_FRAME, isolate), op_(op) { }

  // Update the inline cache for the given operands.
  Code* UpdateCaches(Handle<Object> x, Handle<Object> y);


  // Factory method for getting an uninitialized compare stub.
  static Handle<Code> GetUninitialized(Isolate* isolate, Token::Value op);

  // Helper function for computing the condition for a compare operation.
  static Condition ComputeCondition(Token::Value op);

  static const char* GetStateName(State state);

 private:
  static bool HasInlinedSmiCode(Address address);

  State TargetState(State old_state,
                    State old_left,
                    State old_right,
                    bool has_inlined_smi_code,
                    Handle<Object> x,
                    Handle<Object> y);

  bool strict() const { return op_ == Token::EQ_STRICT; }
  Condition GetCondition() const { return ComputeCondition(op_); }

  static Code* GetRawUninitialized(Isolate* isolate, Token::Value op);

  static void Clear(Isolate* isolate, Address address, Code* target);

  Token::Value op_;

  friend class IC;
};


class CompareNilIC: public IC {
 public:
  explicit CompareNilIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) {}

  MUST_USE_RESULT MaybeObject* CompareNil(Handle<Object> object);

  static Handle<Code> GetUninitialized();

  static void Clear(Address address, Code* target);

  static MUST_USE_RESULT MaybeObject* DoCompareNilSlow(NilValue nil,
                                                       Handle<Object> object);
};


class ToBooleanIC: public IC {
 public:
  explicit ToBooleanIC(Isolate* isolate) : IC(EXTRA_CALL_FRAME, isolate) { }

  MaybeObject* ToBoolean(Handle<Object> object);
};


// Helper for BinaryOpIC and CompareIC.
enum InlinedSmiCheck { ENABLE_INLINED_SMI_CHECK, DISABLE_INLINED_SMI_CHECK };
void PatchInlinedSmiCode(Address address, InlinedSmiCheck check);

DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedLoadIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedStoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, UnaryOpIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, StoreIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, KeyedCallIC_MissFromStubFailure);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, ElementsTransitionAndStoreIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, BinaryOpIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, CompareNilIC_Miss);
DECLARE_RUNTIME_FUNCTION(MaybeObject*, ToBooleanIC_Miss);


} }  // namespace v8::internal

#endif  // V8_IC_H_
