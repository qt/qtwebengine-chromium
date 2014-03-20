# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Generate template values for a callback interface.

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771
"""

from v8_globals import includes
import v8_types
import v8_utilities

CALLBACK_INTERFACE_H_INCLUDES = set([
    'bindings/v8/ActiveDOMCallback.h',
    'bindings/v8/DOMWrapperWorld.h',
    'bindings/v8/ScopedPersistent.h',
])
CALLBACK_INTERFACE_CPP_INCLUDES = set([
    'bindings/v8/V8Binding.h',
    'bindings/v8/V8Callback.h',
    'core/dom/ExecutionContext.h',
    'wtf/Assertions.h',
])


def cpp_to_v8_conversion(idl_type, name):
    # FIXME: setting creation_context=v8::Handle<v8::Object>() is wrong,
    # as toV8 then implicitly uses the current context, which causes leaks
    # between isolate worlds if a different context should be used.
    cpp_value_to_v8_value = v8_types.cpp_value_to_v8_value(idl_type, name,
        isolate='isolate', creation_context='v8::Handle<v8::Object>()')
    return 'v8::Handle<v8::Value> {name}Handle = {cpp_to_v8};'.format(
        name=name, cpp_to_v8=cpp_value_to_v8_value)


def cpp_type(idl_type):
    # FIXME: remove this function by making callback types consistent
    # (always use usual v8_types.cpp_type)
    if idl_type == 'DOMString':
        return 'const String&'
    if idl_type == 'void':
        return 'void'
    # Callbacks use raw pointers, so used_as_argument=True
    usual_cpp_type = v8_types.cpp_type(idl_type, used_as_argument=True)
    if usual_cpp_type.startswith('Vector'):
        return 'const %s&' % usual_cpp_type
    return usual_cpp_type


def generate_callback_interface(callback_interface):
    includes.clear()
    includes.update(CALLBACK_INTERFACE_CPP_INCLUDES)
    name = callback_interface.name

    methods = [generate_method(operation)
               for operation in callback_interface.operations]
    template_contents = {
        'conditional_string': v8_utilities.conditional_string(callback_interface),
        'cpp_class': name,
        'v8_class': v8_utilities.v8_class_name(callback_interface),
        'header_includes': CALLBACK_INTERFACE_H_INCLUDES,
        'methods': methods,
    }
    return template_contents


def add_includes_for_operation(operation):
    v8_types.add_includes_for_type(operation.idl_type)
    for argument in operation.arguments:
        v8_types.add_includes_for_type(argument.idl_type)


def generate_method(operation):
    extended_attributes = operation.extended_attributes
    idl_type = operation.idl_type
    if idl_type not in ['boolean', 'void']:
        raise Exception('We only support callbacks that return boolean or void values.')
    is_custom = 'Custom' in extended_attributes
    if not is_custom:
        add_includes_for_operation(operation)
    call_with = extended_attributes.get('CallWith')
    call_with_this_handle = v8_utilities.extended_attribute_value_contains(call_with, 'ThisValue')
    contents = {
        'call_with_this_handle': call_with_this_handle,
        'custom': is_custom,
        'name': operation.name,
        'return_cpp_type': cpp_type(idl_type),
        'return_idl_type': idl_type,
    }
    contents.update(generate_arguments_contents(operation.arguments, call_with_this_handle))
    return contents


def generate_arguments_contents(arguments, call_with_this_handle):
    def generate_argument(argument):
        return {
            'name': argument.name,
            'cpp_to_v8_conversion': cpp_to_v8_conversion(argument.idl_type, argument.name),
        }

    argument_declarations = [
            '%s %s' % (cpp_type(argument.idl_type), argument.name)
            for argument in arguments]
    if call_with_this_handle:
        argument_declarations.insert(0, 'ScriptValue thisValue')
    return  {
        'argument_declarations': argument_declarations,
        'arguments': [generate_argument(argument) for argument in arguments],
        'handles': ['%sHandle' % argument.name for argument in arguments],
    }
