#!/usr/bin/python
#
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

import optparse
import os
import posixpath
import re
import string


class IdlBadFilenameError(Exception):
    """Raised if an IDL filename disagrees with the interface name in the file."""
    pass


class IdlInterfaceFileNotFoundError(Exception):
    """Raised if the IDL file implementing an interface cannot be found."""
    pass


def parse_options():
    parser = optparse.OptionParser()
    parser.add_option('--event-names-file', help='output file')
    parser.add_option('--main-idl-files-list', help='file listing main (compiled to Blink) IDL files')
    parser.add_option('--support-idl-files-list', help='file listing support IDL files (not compiled to Blink, e.g. testing)')
    parser.add_option('--interface-dependencies-file', help='output file')
    parser.add_option('--bindings-derived-sources-file', help='output file')
    parser.add_option('--window-constructors-file', help='output file')
    parser.add_option('--workerglobalscope-constructors-file', help='output file')
    parser.add_option('--sharedworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--dedicatedworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--serviceworkerglobalscope-constructors-file', help='output file')
    parser.add_option('--write-file-only-if-changed', type='int', help='if true, do not write an output file if it would be identical to the existing one, which avoids unnecessary rebuilds in ninja')
    options, args = parser.parse_args()
    if options.event_names_file is None:
        parser.error('Must specify an output file using --event-names-file.')
    if options.interface_dependencies_file is None:
        parser.error('Must specify an output file using --interface-dependencies-file.')
    if options.bindings_derived_sources_file is None:
        parser.error('Must specify an output file using --bindings-derived-sources-file.')
    if options.window_constructors_file is None:
        parser.error('Must specify an output file using --window-constructors-file.')
    if options.workerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --workerglobalscope-constructors-file.')
    if options.sharedworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --sharedworkerglobalscope-constructors-file.')
    if options.dedicatedworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --dedicatedworkerglobalscope-constructors-file.')
    if options.serviceworkerglobalscope_constructors_file is None:
        parser.error('Must specify an output file using --serviceworkerglobalscope-constructors-file.')
    if options.main_idl_files_list is None:
        parser.error('Must specify a file listing main IDL files using --main-idl-files-list.')
    if options.support_idl_files_list is None:
        parser.error('Must specify a file listing support IDL files using --support-idl-files-list.')
    if options.write_file_only_if_changed is None:
        parser.error('Must specify whether file is only written if changed using --write-file-only-if-changed.')
    options.write_file_only_if_changed = bool(options.write_file_only_if_changed)
    if args:
        parser.error('No arguments taken, but "%s" given.' % ' '.join(args))
    return options


def get_file_contents(idl_filename):
    with open(idl_filename) as idl_file:
        lines = idl_file.readlines()
    return ''.join(lines)


def write_file(new_lines, destination_filename, only_if_changed):
    if only_if_changed and os.path.isfile(destination_filename):
        with open(destination_filename) as destination_file:
            old_lines = destination_file.readlines()
        if old_lines == new_lines:
            return
    with open(destination_filename, 'w') as destination_file:
        destination_file.write(''.join(new_lines))


def get_partial_interface_name_from_idl(file_contents):
    match = re.search(r'partial\s+interface\s+(\w+)', file_contents)
    return match and match.group(1)


# identifier-A implements identifier-B;
# http://www.w3.org/TR/WebIDL/#idl-implements-statements
def get_implemented_interfaces_from_idl(file_contents, interface_name):
    def get_implemented(left_identifier, right_identifier):
        # identifier-A must be the current interface
        if left_identifier != interface_name:
            raise IdlBadFilenameError("Identifier on the left of the 'implements' statement should be %s in %s.idl, but found %s" % (interface_name, interface_name, left_identifier))
        return right_identifier

    implements_re = r'^\s*(\w+)\s+implements\s+(\w+)\s*;'
    implements_matches = re.finditer(implements_re, file_contents, re.MULTILINE)
    implements_pairs = [(match.group(1), match.group(2))
                        for match in implements_matches]
    return [get_implemented(left, right) for left, right in implements_pairs]


def is_callback_interface_from_idl(file_contents):
    match = re.search(r'callback\s+interface\s+\w+', file_contents)
    return bool(match)


def get_parent_interface(file_contents):
    match = re.search(r'interface\s+\w+\s*:\s*(\w+)\s*', file_contents)
    return match and match.group(1)


def get_interface_extended_attributes_from_idl(file_contents):
    match = re.search(r'\[(.*)\]\s+(callback\s+)?(interface|exception)\s+(\w+)',
                      file_contents, flags=re.DOTALL)
    if not match:
        return {}
    # Strip comments
    # re.compile needed b/c Python 2.6 doesn't support flags in re.sub
    single_line_comment_re = re.compile(r'//.*$', flags=re.MULTILINE)
    block_comment_re = re.compile(r'/\*.*?\*/', flags=re.MULTILINE | re.DOTALL)
    extended_attributes_string = re.sub(single_line_comment_re, '', match.group(1))
    extended_attributes_string = re.sub(block_comment_re, '', extended_attributes_string)
    extended_attributes = {}
    # FIXME: this splitting is WRONG: it fails on ExtendedAttributeArgList like
    # 'NamedConstructor=Foo(a, b)'
    parts = [extended_attribute.strip()
             for extended_attribute in extended_attributes_string.split(',')
             # Discard empty parts, which may exist due to trailing comma
             if extended_attribute.strip()]
    for part in parts:
        name, _, value = map(string.strip, part.partition('='))
        extended_attributes[name] = value
    return extended_attributes


def generate_constructor_attribute_list(interface_name, extended_attributes):
    extended_attributes_list = [
            name + '=' + extended_attributes[name]
            for name in 'Conditional', 'PerContextEnabled', 'RuntimeEnabled'
            if name in extended_attributes]
    if extended_attributes_list:
        extended_string = '[%s] ' % ', '.join(extended_attributes_list)
    else:
        extended_string = ''

    attribute_string = 'attribute {interface_name}Constructor {interface_name}'.format(interface_name=interface_name)
    attributes_list = [extended_string + attribute_string]

    # In addition to the regular property, for every [NamedConstructor]
    # extended attribute on an interface, a corresponding property MUST exist
    # on the ECMAScript global object.
    if 'NamedConstructor' in extended_attributes:
        named_constructor = extended_attributes['NamedConstructor']
        # Extract function name, namely everything before opening '('
        constructor_name = re.sub(r'\(.*', '', named_constructor)
        # Note the reduplicated 'ConstructorConstructor'
        attribute_string = 'attribute %sConstructorConstructor %s' % (interface_name, constructor_name)
        attributes_list.append(extended_string + attribute_string)

    return attributes_list


def generate_event_names_file(destination_filename, event_names, only_if_changed):
    def extended_attribute_string(name):
        value = extended_attributes[name]
        if name == 'RuntimeEnabled':
            value += 'Enabled'
        return name + '=' + value

    source_dir, _ = os.path.split(os.getcwd())
    lines = []
    lines.append('namespace="Event"\n')
    lines.append('\n')
    for filename, extended_attributes in sorted(event_names.iteritems()):
        refined_filename, _ = os.path.splitext(os.path.relpath(filename, source_dir))
        refined_filename = refined_filename.replace(os.sep, posixpath.sep)
        extended_attributes_list = [
                extended_attribute_string(name)
                for name in 'Conditional', 'ImplementedAs', 'RuntimeEnabled'
                if name in extended_attributes]
        lines.append('%s %s\n' % (refined_filename, ', '.join(extended_attributes_list)))
    write_file(lines, destination_filename, only_if_changed)


def generate_global_constructors_partial_interface(interface_name, destination_filename, constructor_attributes_list, only_if_changed):
    lines = (['partial interface %s {\n' % interface_name] +
             ['    %s;\n' % constructor_attribute
              for constructor_attribute in sorted(constructor_attributes_list)] +
             ['};\n'])
    write_file(lines, destination_filename, only_if_changed)


def generate_dependencies(idl_file_name, interfaces, dependencies, partial_interface_files, implements_interfaces, implemented_somewhere):
    interface_name, _ = os.path.splitext(os.path.basename(idl_file_name))
    full_path = os.path.realpath(idl_file_name)
    idl_file_contents = get_file_contents(full_path)

    # Handle partial interfaces
    partial_interface_name = get_partial_interface_name_from_idl(idl_file_contents)
    if partial_interface_name:
        partial_interface_files[partial_interface_name].append(full_path)
        return partial_interface_name

    interfaces.add(interface_name)
    # Non-partial interfaces default to having bindings generated
    dependencies[full_path] = []

    # Parse 'identifier-A implements identifier-B;' statements
    implemented_interfaces = get_implemented_interfaces_from_idl(idl_file_contents, interface_name)
    implements_interfaces[interface_name] = implemented_interfaces
    implemented_somewhere |= set(implemented_interfaces)

    return partial_interface_name


def remove_interfaces_implemented_somewhere(dependencies, interface_name_to_idl_file, implemented_somewhere):
    # Interfaces that are implemented by another interface do not have
    # their own bindings generated, as this would be redundant with the
    # actual implementation.
    for implemented_interface in implemented_somewhere:
        full_path = interface_name_to_idl_file[implemented_interface]
        del dependencies[full_path]


def record_global_constructors_and_extended_attribute(idl_file_name, global_constructors, interface_extended_attribute, parent_interface):
    interface_name, _ = os.path.splitext(os.path.basename(idl_file_name))
    full_path = os.path.realpath(idl_file_name)
    idl_file_contents = get_file_contents(full_path)
    extended_attributes = get_interface_extended_attributes_from_idl(idl_file_contents)

    # Record global constructors
    if not is_callback_interface_from_idl(idl_file_contents) and 'NoInterfaceObject' not in extended_attributes:
        global_contexts = extended_attributes.get('GlobalContext', 'Window').split('&')
        new_constructor_list = generate_constructor_attribute_list(interface_name, extended_attributes)
        for global_object in global_contexts:
            global_constructors[global_object].extend(new_constructor_list)

    # Record parents and extended attributes for generating event names
    if interface_name == 'Event':
        interface_extended_attribute[interface_name] = extended_attributes
    parent = get_parent_interface(idl_file_contents)
    if parent:
        parent_interface[interface_name] = parent
        interface_extended_attribute[interface_name] = extended_attributes


def parse_idl_files(main_idl_files, support_idl_files, global_constructors_filenames):
    """Return dependencies between IDL files, constructors on global objects, and events.

    Returns:
        interfaces:
            set of all interfaces
        bindings_derived_sources:
            list of main IDL file names (except support IDL file names)
        dependencies:
            dict of main IDL filename (for a given interface) -> list of partial IDL filenames (for that interface)
            The keys (main IDL files) are the files for which bindings are
            generated. This does not include IDL files for interfaces
            implemented by another interface.
        global_constructors:
            dict of global objects -> list of constructors on that object
        event_names:
            dict of interfaces that inherit from Event -> list of extended attributes for the interface
    """
    interfaces = set()
    dependencies = {}
    partial_interface_files = {}
    implements_interfaces = {}
    implemented_somewhere = set()

    global_constructors = {}
    for global_object in global_constructors_filenames.keys():
        global_constructors[global_object] = []

    # Parents and extended attributes (of interfaces with parents) are
    # used in generating event names
    parent_interface = {}
    interface_extended_attribute = {}

    interface_name_to_idl_file = {}
    for idl_file_name in main_idl_files + support_idl_files:
        full_path = os.path.realpath(idl_file_name)
        interface_name, _ = os.path.splitext(os.path.basename(idl_file_name))
        interface_name_to_idl_file[interface_name] = full_path
        partial_interface_files[interface_name] = []

    # Generate dependencies, global_constructors and interface_extended_attributes for main IDL files
    for idl_file_name in main_idl_files:
        if not generate_dependencies(idl_file_name, interfaces, dependencies, partial_interface_files, implements_interfaces, implemented_somewhere):
            record_global_constructors_and_extended_attribute(idl_file_name, global_constructors, interface_extended_attribute, parent_interface)

    bindings_derived_sources = dependencies.copy()
    remove_interfaces_implemented_somewhere(bindings_derived_sources, interface_name_to_idl_file, implemented_somewhere)

    # Add constructors on global objects to partial interfaces
    for global_object, filename in global_constructors_filenames.iteritems():
        if global_object in interfaces:
            partial_interface_files[global_object].append(filename)

    # Add support IDL files to the dependencies for supporting partial interface
    for idl_file_name in support_idl_files:
        generate_dependencies(idl_file_name, interfaces, dependencies, partial_interface_files, implements_interfaces, implemented_somewhere)
    remove_interfaces_implemented_somewhere(dependencies, interface_name_to_idl_file, implemented_somewhere)

    # An IDL file's dependencies are partial interface files that extend it,
    # and files for other interfaces that this interfaces implements.
    for idl_file_path in dependencies.iterkeys():
        interface_name, _ = os.path.splitext(os.path.basename(idl_file_path))
        implemented_interfaces = implements_interfaces[interface_name]
        try:
            interface_paths = map(lambda x: interface_name_to_idl_file[x], implemented_interfaces)
        except KeyError as key_name:
            raise IdlInterfaceFileNotFoundError('Could not find the IDL file where the following implemented interface is defined: %s' % key_name)
        dependencies[idl_file_path] = sorted(partial_interface_files[interface_name] + interface_paths)

    # Generate event names for all interfaces that inherit from Event,
    # including Event itself.
    event_names = {}
    if 'Event' in interfaces:
        event_names[interface_name_to_idl_file['Event']] = interface_extended_attribute['Event']
    for interface, parent in parent_interface.iteritems():
        while parent in parent_interface:
            parent = parent_interface[parent]
        if parent == 'Event':
            event_names[interface_name_to_idl_file[interface]] = interface_extended_attribute[interface]

    return interfaces, dependencies, bindings_derived_sources, global_constructors, event_names


def write_dependency_file(filename, dependencies, only_if_changed):
    """Write the interface dependencies file.

    The format is as follows:

    Document.idl P.idl
    Event.idl
    Window.idl Q.idl R.idl S.idl
    ...

    The above indicates that:
    Document.idl depends on P.idl,
    Event.idl depends on no other IDL files, and
    Window.idl depends on Q.idl, R.idl, and S.idl.

    An IDL that is a dependency of another IDL (e.g. P.idl) does not have its
    own line in the dependency file.
    """
    lines = ['%s %s\n' % (idl_file, ' '.join(sorted(dependency_files)))
             for idl_file, dependency_files in sorted(dependencies.iteritems())]
    write_file(lines, filename, only_if_changed)


def main():
    options = parse_options()
    with open(options.main_idl_files_list) as idl_files_list:
        main_idl_files = [string.rstrip(line, '\n') for line in idl_files_list]
    with open(options.support_idl_files_list) as idl_files_list:
        support_idl_files = [string.rstrip(line, '\n') for line in idl_files_list]
    only_if_changed = options.write_file_only_if_changed
    global_constructors_filenames = {
        'Window': options.window_constructors_file,
        'WorkerGlobalScope': options.workerglobalscope_constructors_file,
        'SharedWorkerGlobalScope': options.sharedworkerglobalscope_constructors_file,
        'DedicatedWorkerGlobalScope': options.dedicatedworkerglobalscope_constructors_file,
        'ServiceWorkerGlobalScope': options.serviceworkerglobalscope_constructors_file,
        }

    interfaces, dependencies, bindings_derived_sources, global_constructors, event_names = parse_idl_files(main_idl_files, support_idl_files, global_constructors_filenames)

    write_dependency_file(options.interface_dependencies_file, dependencies, only_if_changed)
    write_dependency_file(options.bindings_derived_sources_file, bindings_derived_sources, only_if_changed)
    for interface_name, filename in global_constructors_filenames.iteritems():
        if interface_name in interfaces:
            generate_global_constructors_partial_interface(interface_name, filename, global_constructors[interface_name], only_if_changed)
    generate_event_names_file(options.event_names_file, event_names, only_if_changed)


if __name__ == '__main__':
    main()
