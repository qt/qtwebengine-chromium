{##############################################################################}
{% macro attribute_getter(attribute) %}
static void {{attribute.name}}AttributeGetter(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    {{cpp_class_name}}* imp = {{v8_class_name}}::toNative(info.Holder());
{% if attribute.should_keep_attribute_alive %}
    {{attribute.cpp_type}} result = imp->{{attribute.cpp_method_name}}();
    if (result.get() && DOMDataStore::setReturnValueFromWrapper<{{attribute.v8_type}}>(info.GetReturnValue(), result.get()))
        return;
    v8::Handle<v8::Value> wrapper = toV8(result.get(), info.Holder(), info.GetIsolate());
    if (!wrapper.IsEmpty()) {
        V8HiddenPropertyName::setNamedHiddenReference(info.Holder(), "{{attribute.name}}", wrapper);
        v8SetReturnValue(info, wrapper);
    }
{% else %}
    {{attribute.return_v8_value_statement | indent}}
{% endif %}
    return;
}
{% endmacro %}


{##############################################################################}
{% macro attribute_getter_callback(attribute) %}
static void {{attribute.name}}AttributeGetterCallback(v8::Local<v8::String> name, const v8::PropertyCallbackInfo<v8::Value>& info)
{
    TRACE_EVENT_SET_SAMPLING_STATE("Blink", "DOMGetter");
    {{cpp_class_name}}V8Internal::{{attribute.name}}AttributeGetter(name, info);
    TRACE_EVENT_SET_SAMPLING_STATE("V8", "Execution");
}
{% endmacro %}
