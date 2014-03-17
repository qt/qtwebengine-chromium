{##############################################################################}
{% macro generate_method(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}{{method.overload_index}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% if method.is_raises_exception or method.is_check_security_for_frame or
          method.name in ['addEventListener', 'removeEventListener'] %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{method.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% if method.name in ['addEventListener', 'removeEventListener'] %}
    {{add_remove_event_listener_method(method.name) | indent}}
    {% else %}
    {% if method.number_of_required_arguments %}
    if (UNLIKELY(info.Length() < {{method.number_of_required_arguments}})) {
        {{throw_type_error(method,
              'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
                  method.number_of_required_arguments)}}
        return;
    }
    {% endif %}
    {% if not method.is_static %}
    {{cpp_class}}* imp = {{v8_class}}::toNative(info.Holder());
    {% endif %}
    {% if method.is_custom_element_callbacks %}
    CustomElementCallbackDispatcher::CallbackDeliveryScope deliveryScope;
    {% endif %}
    {% if method.is_check_security_for_frame %}
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% if method.is_check_security_for_node %}
    if (!BindingSecurity::shouldAllowAccessToNode(imp->{{method.name}}(exceptionState), exceptionState)) {
        v8SetReturnValueNull(info);
        exceptionState.throwIfNeeded();
        return;
    }
    {% endif %}
    {% for argument in method.arguments %}
    {{generate_argument(method, argument) | indent}}
    {% endfor %}
    {{cpp_method_call(method, method.v8_set_return_value, method.cpp_value) | indent}}
    {% endif %}{# addEventListener, removeEventListener #}
}
{% endfilter %}
{% endmacro %}


{######################################}
{% macro add_remove_event_listener_method(method_name) %}
{# Set template values for addEventListener vs. removeEventListener #}
{% set listener_lookup_type, listener, hidden_dependency_action =
    ('ListenerFindOrCreate', 'listener', 'createHiddenDependency')
    if method_name == 'addEventListener' else
    ('ListenerFindOnly', 'listener.get()', 'removeHiddenDependency')
%}
EventTarget* impl = {{v8_class}}::toNative(info.Holder());
if (DOMWindow* window = impl->toDOMWindow()) {
    if (!BindingSecurity::shouldAllowAccessToFrame(window->frame(), exceptionState)) {
        exceptionState.throwIfNeeded();
        return;
    }
    if (!window->document())
        return;
}
RefPtr<EventListener> listener = V8EventListenerList::getEventListener(info[1], false, {{listener_lookup_type}});
if (listener) {
    V8TRYCATCH_FOR_V8STRINGRESOURCE_VOID(V8StringResource<WithNullCheck>, eventName, info[0]);
    impl->{{method_name}}(eventName, {{listener}}, info[2]->BooleanValue());
    if (!impl->toNode())
        {{hidden_dependency_action}}(info.Holder(), info[1], {{v8_class}}::eventListenerCacheIndex, info.GetIsolate());
}
{% endmacro %}


{######################################}
{% macro generate_argument(method, argument) %}
{% if argument.is_optional and not argument.has_default and
      argument.idl_type != 'Dictionary' %}
{# Optional arguments without a default value generate an early call with
   fewer arguments if they are omitted.
   Optional Dictionary arguments default to empty dictionary. #}
if (UNLIKELY(info.Length() <= {{argument.index}})) {
    {{cpp_method_call(method, argument.v8_set_return_value, argument.cpp_value) | indent}}
    return;
}
{% endif %}
{% if method.is_strict_type_checking and argument.is_wrapper_type %}
{# Type checking for wrapper interface types (if interface not implemented,
   throw TypeError), per http://www.w3.org/TR/WebIDL/#es-interface #}
if (info.Length() > {{argument.index}} && !isUndefinedOrNull(info[{{argument.index}}]) && !V8{{argument.idl_type}}::hasInstance(info[{{argument.index}}], info.GetIsolate(), worldType(info.GetIsolate()))) {
    {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                               (argument.index + 1, argument.idl_type))}}
    return;
}
{% endif %}
{% if argument.is_clamp %}
{# NaN is treated as 0: http://www.w3.org/TR/WebIDL/#es-type-mapping #}
{{argument.cpp_type}} {{argument.name}} = 0;
V8TRYCATCH_VOID(double, {{argument.name}}NativeValue, info[{{argument.index}}]->NumberValue());
if (!std::isnan({{argument.name}}NativeValue))
    {# IDL type is used for clamping, for the right bounds, since different
       IDL integer types have same internal C++ type (int or unsigned) #}
    {{argument.name}} = clampTo<{{argument.idl_type}}>({{argument.name}}NativeValue);
{% elif argument.idl_type == 'SerializedScriptValue' %}
{% set did_throw = argument.name + 'DidThrow' %}
bool {{did_throw}} = false;
{{argument.cpp_type}} {{argument.name}} = SerializedScriptValue::create(info[{{argument.index}}], 0, 0, {{did_throw}}, info.GetIsolate());
if ({{did_throw}})
    return;
{% elif argument.is_variadic_wrapper_type %}
Vector<{{argument.cpp_type}} > {{argument.name}};
for (int i = {{argument.index}}; i < info.Length(); ++i) {
    if (!V8{{argument.idl_type}}::hasInstance(info[i], info.GetIsolate(), worldType(info.GetIsolate()))) {
        {{throw_type_error(method, '"parameter %s is not of type \'%s\'."' %
                                   (argument.index + 1, argument.idl_type))}}
        return;
    }
    {{argument.name}}.append(V8{{argument.idl_type}}::toNative(v8::Handle<v8::Object>::Cast(info[i])));
}
{% else %}
{{argument.v8_value_to_local_cpp_value}};
{% endif %}
{% if argument.enum_validation_expression %}
{# Methods throw on invalid enum values: http://www.w3.org/TR/WebIDL/#idl-enums #}
String string = {{argument.name}};
if (!({{argument.enum_validation_expression}})) {
    {{throw_type_error(method,
          '"parameter %s (\'" + string + "\') is not a valid enum value."' %
              (argument.index + 1))}}
    return;
}
{% endif %}
{% if argument.idl_type in ['Dictionary', 'Promise'] %}
if (!{{argument.name}}.isUndefinedOrNull() && !{{argument.name}}.isObject()) {
    {{throw_type_error(method, '"parameter %s (\'%s\') is not an object."' %
                               (argument.index + 1, argument.name))}}
    return;
}
{% endif %}
{% endmacro %}


{######################################}
{% macro cpp_method_call(method, v8_set_return_value, cpp_value) %}
{% if method.is_call_with_script_state %}
ScriptState* currentState = ScriptState::current();
if (!currentState)
    return;
ScriptState& state = *currentState;
{% endif %}
{% if method.is_call_with_execution_context %}
ExecutionContext* scriptContext = getExecutionContext();
{% endif %}
{% if method.is_call_with_script_arguments %}
RefPtr<ScriptArguments> scriptArguments(createScriptArguments(info, {{method.number_of_arguments}}));
{% endif %}
{% if method.idl_type == 'void' %}
{{cpp_value}};
{% elif method.is_call_with_script_state %}
{# FIXME: consider always using a local variable #}
{{method.cpp_type}} result = {{cpp_value}};
{% endif %}
{% if method.is_raises_exception %}
if (exceptionState.throwIfNeeded())
    return;
{% endif %}
{% if method.is_call_with_script_state %}
if (state.hadException()) {
    v8::Local<v8::Value> exception = state.exception();
    state.clearException();
    throwError(exception, info.GetIsolate());
    return;
}
{% endif %}
{% if v8_set_return_value %}{{v8_set_return_value}};{% endif %}{# None for void #}
{% endmacro %}


{######################################}
{% macro throw_type_error(method, error_message) %}
{% if method.is_constructor %}
throwTypeError(ExceptionMessages::failedToConstruct("{{interface_name}}", {{error_message}}), info.GetIsolate());
{%- else %}
throwTypeError(ExceptionMessages::failedToExecute("{{method.name}}", "{{interface_name}}", {{error_message}}), info.GetIsolate());
{%- endif %}
{% endmacro %}


{##############################################################################}
{% macro overload_resolution_method(overloads, world_suffix) %}
static void {{overloads.name}}Method{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% for method in overloads.methods %}
    if ({{method.overload_resolution_expression}}) {
        {{method.name}}{{method.overload_index}}Method{{world_suffix}}(info);
        return;
    }
    {% endfor %}
    {% if overloads.minimum_number_of_required_arguments %}
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "{{overloads.name}}", "{{interface_name}}", info.Holder(), info.GetIsolate());
    if (UNLIKELY(info.Length() < {{overloads.minimum_number_of_required_arguments}})) {
        exceptionState.throwTypeError(ExceptionMessages::notEnoughArguments({{overloads.minimum_number_of_required_arguments}}, info.Length()));
        exceptionState.throwIfNeeded();
        return;
    }
    exceptionState.throwTypeError("No function was found that matched the signature provided.");
    exceptionState.throwIfNeeded();
    {% else %}
    {{throw_type_error(overloads, '"No function was found that matched the signature provided."')}}
    {% endif %}
}
{% endmacro %}


{##############################################################################}
{% macro method_callback(method, world_suffix) %}
{% filter conditional(method.conditional_string) %}
static void {{method.name}}MethodCallback{{world_suffix}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMMethod");
    {% if method.measure_as %}
    UseCounter::count(activeDOMWindow(), UseCounter::{{method.measure_as}});
    {% endif %}
    {% if method.deprecate_as %}
    UseCounter::countDeprecation(activeExecutionContext(), UseCounter::{{method.deprecate_as}});
    {% endif %}
    {% if world_suffix in method.activity_logging_world_list %}
    V8PerContextData* contextData = V8PerContextData::from(info.GetIsolate()->GetCurrentContext());
    if (contextData && contextData->activityLogger()) {
        {# FIXME: replace toVectorOfArguments with toNativeArguments(info, 0)
           and delete toVectorOfArguments #}
        Vector<v8::Handle<v8::Value> > loggerArgs = toNativeArguments<v8::Handle<v8::Value> >(info, 0);
        contextData->activityLogger()->log("{{interface_name}}.{{method.name}}", info.Length(), loggerArgs.data(), "Method");
    }
    {% endif %}
    {% if method.is_custom %}
    {{v8_class}}::{{method.name}}MethodCustom(info);
    {% else %}
    {{cpp_class}}V8Internal::{{method.name}}Method{{world_suffix}}(info);
    {% endif %}
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endfilter %}
{% endmacro %}


{##############################################################################}
{% macro origin_safe_method_getter(method, world_suffix) %}
static void {{method.name}}OriginSafeMethodGetter{{world_suffix}}(const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {# FIXME: don't call GetIsolate() so often #}
    // This is only for getting a unique pointer which we can pass to privateTemplate.
    static int privateTemplateUniqueKey;
    WrapperWorldType currentWorldType = worldType(info.GetIsolate());
    V8PerIsolateData* data = V8PerIsolateData::from(info.GetIsolate());
    {# FIXME: 1 case of [DoNotCheckSignature] in Window.idl may differ #}
    v8::Handle<v8::FunctionTemplate> privateTemplate = data->privateTemplate(currentWorldType, &privateTemplateUniqueKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), v8::Signature::New(info.GetIsolate(), V8PerIsolateData::from(info.GetIsolate())->rawDOMTemplate(&{{v8_class}}::wrapperTypeInfo, currentWorldType)), {{method.number_of_required_or_variadic_arguments}});

    v8::Handle<v8::Object> holder = info.This()->FindInstanceInPrototypeChain({{v8_class}}::domTemplate(info.GetIsolate(), currentWorldType));
    if (holder.IsEmpty()) {
        // This is only reachable via |object.__proto__.func|, in which case it
        // has already passed the same origin security check
        v8SetReturnValue(info, privateTemplate->GetFunction());
        return;
    }
    {{cpp_class}}* imp = {{v8_class}}::toNative(holder);
    if (!BindingSecurity::shouldAllowAccessToFrame(imp->frame(), DoNotReportSecurityError)) {
        static int sharedTemplateUniqueKey;
        v8::Handle<v8::FunctionTemplate> sharedTemplate = data->privateTemplate(currentWorldType, &sharedTemplateUniqueKey, {{cpp_class}}V8Internal::{{method.name}}MethodCallback{{world_suffix}}, v8Undefined(), v8::Signature::New(info.GetIsolate(), V8PerIsolateData::from(info.GetIsolate())->rawDOMTemplate(&{{v8_class}}::wrapperTypeInfo, currentWorldType)), {{method.number_of_required_or_variadic_arguments}});
        v8SetReturnValue(info, sharedTemplate->GetFunction());
        return;
    }

    v8::Local<v8::Value> hiddenValue = info.This()->GetHiddenValue(v8::String::NewFromUtf8(info.GetIsolate(), "{{method.name}}", v8::String::kInternalizedString));
    if (!hiddenValue.IsEmpty()) {
        v8SetReturnValue(info, hiddenValue);
        return;
    }

    v8SetReturnValue(info, privateTemplate->GetFunction());
}

static void {{method.name}}OriginSafeMethodGetterCallback{{world_suffix}}(v8::Local<v8::String>, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {{cpp_class}}V8Internal::{{method.name}}OriginSafeMethodGetter{{world_suffix}}(info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endmacro %}


{##############################################################################}
{% macro generate_constructor(constructor) %}
static void constructor{{constructor.overload_index}}(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    {% if interface_length and not constructor.overload_index %}
    {# FIXME: remove this UNLIKELY: constructors are heavy, so no difference. #}
    if (UNLIKELY(info.Length() < {{interface_length}})) {
        {{throw_type_error({'name': 'Constructor'},
            'ExceptionMessages::notEnoughArguments(%s, info.Length())' %
                interface_length)}}
        return;
    }
    {% endif %}
    {% if is_constructor_raises_exception %}
    ExceptionState exceptionState(ExceptionState::ConstructionContext, "{{interface_name}}", info.Holder(), info.GetIsolate());
    {% endif %}
    {% for argument in constructor.arguments %}
    {{generate_argument(constructor, argument) | indent}}
    {% endfor %}
    {% if is_constructor_call_with_execution_context %}
    ExecutionContext* context = getExecutionContext();
    {% endif %}
    {% if is_constructor_call_with_document %}
    Document& document = *toDocument(getExecutionContext());
    {% endif %}
    RefPtr<{{cpp_class}}> impl = {{cpp_class}}::create({{constructor.argument_list | join(', ')}});
    v8::Handle<v8::Object> wrapper = info.Holder();
    {% if is_constructor_raises_exception %}
    if (exceptionState.throwIfNeeded())
        return;
    {% endif %}

    {# FIXME: Should probably be Independent unless [ActiveDOMObject]
              or [DependentLifetime]. #}
    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{v8_class}}::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}
{% endmacro %}


{##############################################################################}
{% macro named_constructor_callback(constructor) %}
static void {{v8_class}}ConstructorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (!info.IsConstructCall()) {
        throwTypeError(ExceptionMessages::failedToConstruct("{{constructor.name}}", "Please use the 'new' operator, this DOM object constructor cannot be called as a function."), info.GetIsolate());
        return;
    }

    if (ConstructorMode::current() == ConstructorMode::WrapExistingObject) {
        v8SetReturnValue(info, info.Holder());
        return;
    }

    Document* document = currentDocument();
    ASSERT(document);

    // Make sure the document is added to the DOM Node map. Otherwise, the {{cpp_class}} instance
    // may end up being the only node in the map and get garbage-collected prematurely.
    toV8(document, info.Holder(), info.GetIsolate());

    {# FIXME: arguments #}
    {% set argument_list = ['*document'] %}
    RefPtr<{{cpp_class}}> impl = {{cpp_class}}::createForJSConstructor({{argument_list | join(', ')}});
    v8::Handle<v8::Object> wrapper = info.Holder();

    V8DOMWrapper::associateObjectWithWrapper<{{v8_class}}>(impl.release(), &{{v8_class}}Constructor::wrapperTypeInfo, wrapper, info.GetIsolate(), WrapperConfiguration::Dependent);
    v8SetReturnValue(info, wrapper);
}
{% endmacro %}
