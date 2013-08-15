/*
 *  {{AUTO_GENERATED_WARNING}}
 */
#ifndef __dbusxx__{{FILE_STRING}}__PROXY_MARSHALL_H
#define __dbusxx__{{FILE_STRING}}__PROXY_MARSHALL_H

#include <dbus-c++/dbus.h>
#include <cassert>{{BI_NEWLINE}}

{{#FOR_EACH_INTERFACE}}
{{#FOR_EACH_NAMESPACE}}
namespace {{NAMESPACE_NAME}} {
{{/FOR_EACH_NAMESPACE}}

{{BI_NEWLINE}}class {{CLASS_NAME}}_proxy
  : public ::DBus::InterfaceProxy
{
public:

    {{CLASS_NAME}}_proxy()
    : ::DBus::InterfaceProxy("{{INTERFACE_NAME}}")
    {
{{#FOR_EACH_SIGNAL}}
        connect_signal({{CLASS_NAME}}_proxy, {{SIGNAL_NAME}}, _{{SIGNAL_NAME}}_stub);
{{/FOR_EACH_SIGNAL}}
    }{{BI_NEWLINE}}

    /* properties exported by this interface */
{{#FOR_EACH_PROPERTY}}

    {{#PROPERTY_GETTER}}
    {{#PROP_CONST}}const {{/PROP_CONST}}{{PROP_TYPE}} {{PROP_NAME}}() {
        ::DBus::CallMessage __call ;
         __call.member("Get"); __call.interface("org.freedesktop.DBus.Properties");
        ::DBus::MessageIter __wi = __call.writer();
        const std::string interface_name = "{{INTERFACE_NAME}}";
        const std::string property_name  = "{{PROP_NAME}}";
        __wi << interface_name;
        __wi << property_name;
        ::DBus::Message __ret = this->invoke_method (__call);
        ::DBus::MessageIter __ri = __ret.reader ();
        ::DBus::Variant argout;
        __ri >> argout;
        return argout;
    }{{BI_NEWLINE}}
    {{/PROPERTY_GETTER}}

    {{#PROPERTY_SETTER}}
    void {{PROP_NAME}}(const {{PROP_TYPE}} &input) {
        ::DBus::CallMessage __call ;
         __call.member("Set");  __call.interface( "org.freedesktop.DBus.Properties");
        ::DBus::MessageIter __wi = __call.writer();
        ::DBus::Variant __value;
        ::DBus::MessageIter vi = __value.writer ();
        vi << input;
        const std::string interface_name = "{{INTERFACE_NAME}}";
        const std::string property_name  = "{{PROP_NAME}}";
        __wi << interface_name;
        __wi << property_name;
        __wi << __value;
        ::DBus::Message __ret = this->invoke_method (__call);
    }{{BI_NEWLINE}}
    {{/PROPERTY_SETTER}}

{{/FOR_EACH_PROPERTY}}

{{#SYNC_SECTION}}
    /* methods exported by this interface.
     * these functions will invoke the corresponding methods
     * on the remote objects.
     */
{{#FOR_EACH_METHOD}}

    {{METHOD_RETURN_TYPE}} {{METHOD_NAME}}({{#METHOD_ARG_LIST}}{{METHOD_ARG_DECL}}{{#METHOD_ARG_LIST_separator}}, {{/METHOD_ARG_LIST_separator}}{{/METHOD_ARG_LIST}})
    {
        ::DBus::CallMessage __call;

    {{#METHOD_IN_ARGS_SECTION}}
        ::DBus::MessageIter __wi = __call.writer();

        {{#FOR_EACH_METHOD_IN_ARG}}
        __wi << {{METHOD_IN_ARG_NAME}};
        {{/FOR_EACH_METHOD_IN_ARG}}

    {{/METHOD_IN_ARGS_SECTION}}

        __call.member("{{METHOD_NAME}}");
        {{METHOD_INVOKE_RET}}invoke_method(__call);

    {{#METHOD_OUT_ARGS_SECTION}}
        ::DBus::MessageIter __ri = __ret.reader();

        {{#FOR_EACH_METHOD_OUT_ARG}}

        {{#METHOD_OUT_ARG_DECL}}
        {{METHOD_OUT_ARG_TYPE}} {{METHOD_OUT_ARG_NAME}};
        {{/METHOD_OUT_ARG_DECL}}

        __ri >> {{METHOD_OUT_ARG_NAME}};
        {{/FOR_EACH_METHOD_OUT_ARG}}

    {{/METHOD_OUT_ARGS_SECTION}}

    {{#METHOD_RETURN}}
        return {{METHOD_RETURN_NAME}};
    {{/METHOD_RETURN}}

    }{{BI_NEWLINE}}
{{/FOR_EACH_METHOD}}

{{/SYNC_SECTION}}

{{#ASYNC_SECTION}}
    /* non-blocking versions of the methods exported by this interface.
     * these functions will invoke the corresponding methods
     * on the remote objects.
     */
{{#FOR_EACH_ASYNC_METHOD}}
    void {{METHOD_NAME}}({{#METHOD_ARG_LIST}}{{METHOD_ARG_DECL}}{{#METHOD_ARG_LIST_separator}}, {{/METHOD_ARG_LIST_separator}}{{/METHOD_ARG_LIST}})
    {
        ::DBus::CallMessage __call;

    {{#METHOD_IN_ARGS_SECTION}}
        ::DBus::MessageIter __wi = __call.writer();

        {{#FOR_EACH_METHOD_IN_ARG}}
        __wi << {{METHOD_IN_ARG_NAME}};
        {{/FOR_EACH_METHOD_IN_ARG}}

    {{/METHOD_IN_ARGS_SECTION}}

        __call.member("{{METHOD_NAME}}");
        ::DBus::PendingCall *__pending = invoke_method_async(__call, __timeout);
        ::DBus::AsyncReplyHandler __handler;
        __handler = new ::DBus::Callback<{{CLASS_NAME}}_proxy, void, ::DBus::PendingCall *>(this, &{{CLASS_NAME}}_proxy::_{{METHOD_NAME}}Callback_stub);
        __pending->reply_handler(__handler);
        __pending->data(__data);
    }{{BI_NEwLINE}}{{BI_NEWLINE}}

{{/FOR_EACH_ASYNC_METHOD}}

{{/ASYNC_SECTION}}

    /* signal handlers for this interface.
     * you will have to implement them in your ObjectProxy.
     */

{{#FOR_EACH_SIGNAL}}
    virtual void {{SIGNAL_NAME}}({{#CONST_SIGNAL_ARG_LIST}}{{SIGNAL_ARG_DECL}}{{#CONST_SIGNAL_ARG_LIST_separator}}, {{/CONST_SIGNAL_ARG_LIST_separator}}{{/CONST_SIGNAL_ARG_LIST}}) = 0;
{{/FOR_EACH_SIGNAL}}

{{BI_NEWLINE}}protected:

{{#ASYNC_SECTION}}
    /* async method reply handlers for this interface.
     * you will have to implement them in your ObjectProxy.
     */

{{#FOR_EACH_ASYNC_METHOD}}

    virtual void {{METHOD_NAME}}Callback({{#METHOD_CALLBACK_ARG_LIST}}{{METHOD_CALLBACK_ARG}}{{#METHOD_CALLBACK_ARG_LIST_separator}}, {{/METHOD_CALLBACK_ARG_LIST_separator}}{{/METHOD_CALLBACK_ARG_LIST}})
    {
        assert(!"Implement {{METHOD_NAME}}Callback");
    }{{BI_NEWLINE}}

{{/FOR_EACH_ASYNC_METHOD}}

{{/ASYNC_SECTION}}
private:

{{#ASYNC_SECTION}}
    /* unmarshallers (to steal the PendingCall reply and unmarshall args
     * before invoking the reply callback)
     */
{{#FOR_EACH_ASYNC_METHOD}}
    void _{{METHOD_NAME}}Callback_stub(::DBus::PendingCall *__call)
    {
        ::DBus::Message __reply = __call->steal_reply();
        void *__data = __call->data();
        remove_pending_call(__call);
        ::DBus::Error __error(__reply);

    {{#METHOD_OUT_ARGS_SECTION}}
        ::DBus::MessageIter __ri = __reply.reader();

        {{#FOR_EACH_METHOD_USER_OUT_ARG}}
        {{METHOD_OUT_ARG_TYPE}} {{METHOD_OUT_ARG_NAME}};
        {{/FOR_EACH_METHOD_USER_OUT_ARG}}

        if (!__error.is_set()) {

        {{#FOR_EACH_METHOD_USER_OUT_ARG}}
            __ri >> {{METHOD_OUT_ARG_NAME}};
        {{/FOR_EACH_METHOD_USER_OUT_ARG}}

        }
    {{/METHOD_OUT_ARGS_SECTION}}

        {{METHOD_NAME}}Callback({{#METHOD_OUT_ARG_LIST}}{{METHOD_OUT_ARG_NAME}}{{#METHOD_OUT_ARG_LIST_separator}}, {{/METHOD_OUT_ARG_LIST_separator}}{{/METHOD_OUT_ARG_LIST}});

    }{{BI_NEWLINE}}
{{/FOR_EACH_ASYNC_METHOD}}

{{/ASYNC_SECTION}}

    /* unmarshallers (to unpack the DBus message before
     * calling the actual signal handler)
     */

{{#FOR_EACH_SIGNAL}}
    void _{{SIGNAL_NAME}}_stub(const ::DBus::SignalMessage &__sig)
    {

    {{#SIGNAL_ARGS_SECTION}}
        ::DBus::MessageIter __ri = __sig.reader();

        {{#FOR_EACH_SIGNAL_ARG}}
        {{SIGNAL_ARG_DECL}}; __ri >> {{SIGNAL_ARG_NAME}};
        {{/FOR_EACH_SIGNAL_ARG}}

    {{/SIGNAL_ARGS_SECTION}}

        {{SIGNAL_NAME}}({{#SIGNAL_ARG_LIST}}{{SIGNAL_ARG_NAME}}{{#SIGNAL_ARG_LIST_separator}}, {{/SIGNAL_ARG_LIST_separator}}{{/SIGNAL_ARG_LIST}});
    }{{BI_NEWLINE}}

{{/FOR_EACH_SIGNAL}}

};
{{#FOR_EACH_NAMESPACE}}}{{/FOR_EACH_NAMESPACE}}
{{/FOR_EACH_INTERFACE}}

#endif  // __dbusxx__{{FILE_STRING}}__PROXY_MARSHALL_H
