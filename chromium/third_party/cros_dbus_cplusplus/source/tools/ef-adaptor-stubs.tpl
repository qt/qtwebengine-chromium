/*
 *  {{AUTO_GENERATED_WARNING}}
 */
#ifndef __dbusxx_ef__{{FILE_STRING}}__ADAPTOR_MARSHALL_H
#define __dbusxx_ef__{{FILE_STRING}}__ADAPTOR_MARSHALL_H

#include <dbus-c++/dbus.h>
#include <cassert>{{BI_NEWLINE}}

{{#FOR_EACH_INTERFACE}}
{{#FOR_EACH_NAMESPACE}}
namespace {{NAMESPACE_NAME}} {
{{/FOR_EACH_NAMESPACE}}

{{BI_NEWLINE}}class {{CLASS_NAME}}_adaptor
  : public ::DBus::InterfaceAdaptor
{
public:

    {{CLASS_NAME}}_adaptor()
    : ::DBus::InterfaceAdaptor("{{INTERFACE_NAME}}")
    {
{{#FOR_EACH_PROPERTY}}
        bind_property({{PROP_NAME}}, "{{PROP_SIG}}", {{PROP_READABLE}}, {{PROP_WRITEABLE}});
{{/FOR_EACH_PROPERTY}}
{{#FOR_EACH_METHOD}}
        register_method({{CLASS_NAME}}_adaptor, {{METHOD_NAME}}, _{{METHOD_NAME}}_stub);
{{/FOR_EACH_METHOD}}
    }

    const ::DBus::IntrospectedInterface *introspect() const
    {
{{#FOR_EACH_METHOD}}
        static const ::DBus::IntrospectedArgument {{METHOD_NAME}}_args[] =
        {
    {{#FOR_EACH_METHOD_ARG}}
            { "{{METHOD_ARG_NAME}}", "{{METHOD_ARG_SIG}}", {{METHOD_ARG_IN_OUT}} },
    {{/FOR_EACH_METHOD_ARG}}
            { 0, 0, 0 }
        };
{{/FOR_EACH_METHOD}}
{{#FOR_EACH_SIGNAL}}
        static const ::DBus::IntrospectedArgument {{SIGNAL_NAME}}_args[] =
        {
    {{#FOR_EACH_SIGNAL_ARG}}
            { "{{SIGNAL_ARG_NAME}}", "{{SIGNAL_ARG_SIG}}", false },
    {{/FOR_EACH_SIGNAL_ARG}}
            { 0, 0, 0 }
        };
{{/FOR_EACH_SIGNAL}}
        static const ::DBus::IntrospectedMethod {{CLASS_NAME}}_adaptor_methods[] =
        {
{{#FOR_EACH_METHOD}}
            { "{{METHOD_NAME}}", {{METHOD_NAME}}_args },
{{/FOR_EACH_METHOD}}
            { 0, 0 }
        };
        static const ::DBus::IntrospectedMethod {{CLASS_NAME}}_adaptor_signals[] =
        {
{{#FOR_EACH_SIGNAL}}
            { "{{SIGNAL_NAME}}", {{SIGNAL_NAME}}_args },
{{/FOR_EACH_SIGNAL}}
            { 0, 0 }
        };
        static const ::DBus::IntrospectedProperty {{CLASS_NAME}}_adaptor_properties[] =
        {
{{#FOR_EACH_PROPERTY}}
            { "{{PROP_NAME}}", "{{PROP_SIG}}", {{PROP_READABLE}}, {{PROP_WRITEABLE}} },
{{/FOR_EACH_PROPERTY}}
            { 0, 0, 0, 0 }
        };
        static const ::DBus::IntrospectedInterface {{CLASS_NAME}}_adaptor_interface =
        {
            "{{INTERFACE_NAME}}",
            {{CLASS_NAME}}_adaptor_methods,
            {{CLASS_NAME}}_adaptor_signals,
            {{CLASS_NAME}}_adaptor_properties
        };
        return &{{CLASS_NAME}}_adaptor_interface;
    }

    /* Properties exposed by this interface.
     * Use property() and property(value) to
     * get and set a particular property.
     */
{{#FOR_EACH_PROPERTY}}
    ::DBus::PropertyAdaptor< {{PROP_TYPE}} > {{PROP_NAME}};
{{/FOR_EACH_PROPERTY}}

    /* Methods exported by this interface.
     * You will have to implement them in your ObjectAdaptor.
     *
     * Your code should return in one of 3 ways:
     *
     * Return a result:
     *   return XxxxMakeReply(call, _return_values, ...);
     *
     * Return an error:
     *   return DBus::ErrorMessage(call, "org.foo", "my error");
     *
     * Return a continuation:
     *   DBus::Tag *tag = new DBus::Tag(); // Or tag from other obj
     *   return DBus::TagMessage(tag);
     *
     *  // Later do....
     *  Continuation *ret = find_continuation(tag);
     *  XxxxWriteReply(ret->writer(), _return_values, ...);
     *  return_now(ret);
     *
     *  // Or later do...
     *  Continuation *ret = find_continuation(tag);
     *  return_error(ret, DBus::Error("org.foo", "my error");
     *
     */
{{#FOR_EACH_METHOD}}
    virtual ::DBus::Message {{METHOD_NAME}}(const ::DBus::CallMessage &__call{{#METHOD_IN_ARGS_SECTION}}, {{#FOR_EACH_METHOD_IN_ARG}}const {{METHOD_IN_ARG_TYPE}} &{{METHOD_IN_ARG_NAME}}{{#FOR_EACH_METHOD_IN_ARG_separator}}, {{/FOR_EACH_METHOD_IN_ARG_separator}}{{/FOR_EACH_METHOD_IN_ARG}}{{/METHOD_IN_ARGS_SECTION}}) = 0;
{{/FOR_EACH_METHOD}}

    /* signal emitters for this interface */
{{#FOR_EACH_SIGNAL}}
    void {{SIGNAL_NAME}}({{#CONST_SIGNAL_ARG_LIST}}{{SIGNAL_ARG_DECL}}{{#CONST_SIGNAL_ARG_LIST_separator}}, {{/CONST_SIGNAL_ARG_LIST_separator}}{{/CONST_SIGNAL_ARG_LIST}})
    {
        ::DBus::SignalMessage __sig("{{SIGNAL_NAME}}");
{{#SIGNAL_ARGS_SECTION}}
        ::DBus::MessageIter __wi = __sig.writer();
{{#FOR_EACH_SIGNAL_ARG}}
        __wi << {{SIGNAL_ARG_NAME}};
{{/FOR_EACH_SIGNAL_ARG}}
{{/SIGNAL_ARGS_SECTION}}
        emit_signal(__sig);
    }
{{/FOR_EACH_SIGNAL}}

protected:
    /* The following functions, XxxxMakeReply and XxxxWriteReply,
     * are helper functions which can be used when returning a result in an
     * adaptor implemented method or in a continuation returning a result
     * asynchronously.  See the examples above.
     */
{{#FOR_EACH_METHOD}}
    ::DBus::Message {{METHOD_NAME}}MakeReply(const ::DBus::CallMessage &__call{{#METHOD_OUT_ARGS_SECTION}}, {{#FOR_EACH_METHOD_OUT_ARG}}const {{METHOD_OUT_ARG_TYPE}} &{{METHOD_OUT_ARG_NAME}}{{#FOR_EACH_METHOD_OUT_ARG_separator}}, {{/FOR_EACH_METHOD_OUT_ARG_separator}}{{/FOR_EACH_METHOD_OUT_ARG}}{{/METHOD_OUT_ARGS_SECTION}})
    {
        ::DBus::ReturnMessage __reply(__call);
        // Maybe this should just call XxxxWriteReply();
{{#METHOD_OUT_ARGS_SECTION}}
        ::DBus::MessageIter __wi = __reply.writer();
{{#FOR_EACH_METHOD_OUT_ARG}}
        __wi << {{METHOD_OUT_ARG_NAME}};
{{/FOR_EACH_METHOD_OUT_ARG}}
{{/METHOD_OUT_ARGS_SECTION}}
        return __reply;
    }

    void {{METHOD_NAME}}WriteReply(::DBus::MessageIter &__wi{{#METHOD_OUT_ARGS_SECTION}}, {{#FOR_EACH_METHOD_OUT_ARG}}const {{METHOD_OUT_ARG_TYPE}} &{{METHOD_OUT_ARG_NAME}}{{#FOR_EACH_METHOD_OUT_ARG_separator}}, {{/FOR_EACH_METHOD_OUT_ARG_separator}}{{/FOR_EACH_METHOD_OUT_ARG}}{{/METHOD_OUT_ARGS_SECTION}})
    {
{{#METHOD_OUT_ARGS_SECTION}}
{{#FOR_EACH_METHOD_OUT_ARG}}
        __wi << {{METHOD_OUT_ARG_NAME}};
{{/FOR_EACH_METHOD_OUT_ARG}}
{{/METHOD_OUT_ARGS_SECTION}}
    }
{{/FOR_EACH_METHOD}}


private:
    /* unmarshallers (to unpack the DBus message before calling the actual
     * interface method)
     */
{{#FOR_EACH_METHOD}}
    ::DBus::Message _{{METHOD_NAME}}_stub(const ::DBus::CallMessage &__call)
    {
{{#METHOD_IN_ARGS_SECTION}}
        ::DBus::MessageIter __ri = __call.reader();
{{#FOR_EACH_METHOD_IN_ARG}}
        {{METHOD_IN_ARG_TYPE}} {{METHOD_IN_ARG_NAME}}; __ri >> {{METHOD_IN_ARG_NAME}};
{{/FOR_EACH_METHOD_IN_ARG}}
{{/METHOD_IN_ARGS_SECTION}}
        return {{METHOD_NAME}}(__call{{#METHOD_IN_ARGS_SECTION}}, {{#FOR_EACH_METHOD_IN_ARG}}{{METHOD_IN_ARG_NAME}}{{#FOR_EACH_METHOD_IN_ARG_separator}}, {{/FOR_EACH_METHOD_IN_ARG_separator}}{{/FOR_EACH_METHOD_IN_ARG}}{{/METHOD_IN_ARGS_SECTION}});

    }
{{/FOR_EACH_METHOD}}
};
{{#FOR_EACH_NAMESPACE}}}{{/FOR_EACH_NAMESPACE}}
{{/FOR_EACH_INTERFACE}}

#endif  // __dbusxx_ef__{{FILE_STRING}}__ADAPTOR_MARSHALL_H
