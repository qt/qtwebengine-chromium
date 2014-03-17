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

"""Generate Blink V8 bindings (.h and .cpp files).

FIXME: Not currently used in build.
This is a rewrite of the Perl IDL compiler in Python, but is not complete.
Once it is complete, we will switch all IDL files over to Python at once.
Until then, please work on the Perl IDL compiler.
For details, see bug http://crbug.com/239771

Input: An object of class IdlDefinitions, containing an IDL interface X
Output: V8X.h and V8X.cpp
"""

import os
import posixpath
import re
import sys

# jinja2 is in chromium's third_party directory.
module_path, module_name = os.path.split(__file__)
third_party = os.path.join(module_path, os.pardir, os.pardir, os.pardir, os.pardir, os.pardir)
# Insert at front to override system libraries, and after path[0] == script dir
sys.path.insert(1, third_party)
import jinja2

templates_dir = os.path.join(module_path, os.pardir, os.pardir, 'templates')

import v8_callback_interface
from v8_globals import includes
import v8_interface
import v8_types
from v8_utilities import capitalize, cpp_name, conditional_string, v8_class_name


class CodeGeneratorV8:
    def __init__(self, definitions, interface_name, output_directory, relative_dir_posix, idl_directories, verbose=False):
        self.idl_definitions = definitions
        self.interface_name = interface_name
        self.idl_directories = idl_directories
        self.output_directory = output_directory
        self.verbose = verbose
        # FIXME: remove definitions check when remove write_dummy_header_and_cpp
        if not definitions:
            return
        try:
            self.interface = definitions.interfaces[interface_name]
        except KeyError:
            raise Exception('%s not in IDL definitions' % interface_name)
        if self.interface.is_callback:
            header_template_filename = 'callback_interface.h'
            cpp_template_filename = 'callback_interface.cpp'
            self.generate_contents = v8_callback_interface.generate_callback_interface
        else:
            header_template_filename = 'interface.h'
            cpp_template_filename = 'interface.cpp'
            self.generate_contents = v8_interface.generate_interface
        jinja_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(templates_dir),
            keep_trailing_newline=True,  # newline-terminate generated files
            lstrip_blocks=True,  # so can indent control flow tags
            trim_blocks=True)
        jinja_env.filters.update({
            'blink_capitalize': capitalize,
            'conditional': conditional_if_endif,
            'runtime_enabled': runtime_enabled_if,
            })
        self.header_template = jinja_env.get_template(header_template_filename)
        self.cpp_template = jinja_env.get_template(cpp_template_filename)

        class_name = cpp_name(self.interface)
        self.include_for_cpp_class = posixpath.join(relative_dir_posix, class_name + '.h')
        enumerations = definitions.enumerations
        if enumerations:
            v8_types.set_enum_types(enumerations)

    def write_dummy_header_and_cpp(self):
        # FIXME: fix GYP so these files aren't needed and remove this method
        target_interface_name = self.interface_name
        header_basename = 'V8%s.h' % target_interface_name
        cpp_basename = 'V8%s.cpp' % target_interface_name
        contents = """/*
    This file is generated just to tell build scripts that {header_basename} and
    {cpp_basename} are created for {target_interface_name}.idl, and thus
    prevent the build scripts from trying to generate {header_basename} and
    {cpp_basename} at every build. This file must not be tried to compile.
*/
""".format(**locals())
        self.write_file(header_basename, contents)
        self.write_file(cpp_basename, contents)

    def write_header_and_cpp(self):
        interface = self.interface
        template_contents = self.generate_contents(interface)
        template_contents['header_includes'].add(self.include_for_cpp_class)
        template_contents['header_includes'] = sorted(template_contents['header_includes'])
        template_contents['cpp_includes'] = sorted(includes)

        header_basename = v8_class_name(interface) + '.h'
        header_file_text = self.header_template.render(template_contents)
        self.write_file(header_basename, header_file_text)

        cpp_basename = v8_class_name(interface) + '.cpp'
        cpp_file_text = self.cpp_template.render(template_contents)
        self.write_file(cpp_basename, cpp_file_text)

    def write_file(self, basename, file_text):
        filename = os.path.join(self.output_directory, basename)
        with open(filename, 'w') as output_file:
            output_file.write(file_text)


# [Conditional]
def conditional_if_endif(code, conditional_string):
    # Jinja2 filter to generate if/endif directive blocks
    if not conditional_string:
        return code
    return ('#if %s\n' % conditional_string +
            code +
            '#endif // %s\n' % conditional_string)


# [RuntimeEnabled]
def runtime_enabled_if(code, runtime_enabled_function_name):
    if not runtime_enabled_function_name:
        return code
    # Indent if statement to level of original code
    indent = re.match(' *', code).group(0)
    return ('%sif (%s())\n' % (indent, runtime_enabled_function_name) +
            '    %s' % code)
