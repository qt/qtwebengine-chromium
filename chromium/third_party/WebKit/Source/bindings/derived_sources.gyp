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
#

{
  'includes': [
    '../build/scripts/scripts.gypi',
    '../build/win/precompile.gypi',
    '../build/scripts/scripts.gypi',
    '../core/core.gypi',
    '../modules/modules.gypi',
    'bindings.gypi',
  ],

  'variables': {
    'main_idl_files': [
      '<@(core_idl_files)',
      '<@(modules_idl_files)',
      '<@(svg_idl_files)',
    ],
    'support_idl_files': [
      '<@(webcore_testing_support_idl_files)',
      '<@(modules_testing_support_idl_files)',
    ],
    'generated_support_idl_files': [
      '<@(generated_webcore_testing_support_idl_files)',
    ],
    'compiler_module_files': [
        'scripts/idl_compiler.py',
        '<(DEPTH)/third_party/ply/lex.py',
        '<(DEPTH)/third_party/ply/yacc.py',
        # jinja2/__init__.py contains version string, so sufficient for package
        '<(DEPTH)/third_party/jinja2/__init__.py',
        '<(DEPTH)/third_party/markupsafe/__init__.py',  # jinja2 dep
        '<(DEPTH)/tools/idl_parser/idl_lexer.py',
        '<(DEPTH)/tools/idl_parser/idl_node.py',
        '<(DEPTH)/tools/idl_parser/idl_parser.py',
        'scripts/blink_idl_lexer.py',
        'scripts/blink_idl_parser.py',
        'scripts/code_generator_v8.py',
        'scripts/idl_definitions.py',
        'scripts/idl_definitions_builder.py',
        'scripts/idl_reader.py',
        'scripts/idl_validator.py',
        'scripts/interface_dependency_resolver.py',
        'scripts/v8_attributes.py',
        'scripts/v8_callback_interface.py',
        'scripts/v8_interface.py',
        'scripts/v8_types.py',
        'scripts/v8_utilities.py',
    ],
    'code_generator_template_files': [
        'templates/attributes.cpp',
        'templates/callback_interface.cpp',
        'templates/callback_interface.h',
        'templates/interface_base.cpp',
        'templates/interface.cpp',
        'templates/interface.h',
    ],

    'bindings_output_dir': '<(SHARED_INTERMEDIATE_DIR)/blink/bindings',
    'generated_global_constructors_idl_files': [
         '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
         '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
    ],

    'conditions': [
      ['OS=="win" and buildtype=="Official"', {
        # On windows official release builds, we try to preserve symbol space.
        'derived_sources_aggregate_files': [
          '<(bindings_output_dir)/V8DerivedSourcesAll.cpp',
        ],
      },{
        'derived_sources_aggregate_files': [
          '<(bindings_output_dir)/V8DerivedSources01.cpp',
          '<(bindings_output_dir)/V8DerivedSources02.cpp',
          '<(bindings_output_dir)/V8DerivedSources03.cpp',
          '<(bindings_output_dir)/V8DerivedSources04.cpp',
          '<(bindings_output_dir)/V8DerivedSources05.cpp',
          '<(bindings_output_dir)/V8DerivedSources06.cpp',
          '<(bindings_output_dir)/V8DerivedSources07.cpp',
          '<(bindings_output_dir)/V8DerivedSources08.cpp',
          '<(bindings_output_dir)/V8DerivedSources09.cpp',
          '<(bindings_output_dir)/V8DerivedSources10.cpp',
          '<(bindings_output_dir)/V8DerivedSources11.cpp',
          '<(bindings_output_dir)/V8DerivedSources12.cpp',
          '<(bindings_output_dir)/V8DerivedSources13.cpp',
          '<(bindings_output_dir)/V8DerivedSources14.cpp',
          '<(bindings_output_dir)/V8DerivedSources15.cpp',
          '<(bindings_output_dir)/V8DerivedSources16.cpp',
          '<(bindings_output_dir)/V8DerivedSources17.cpp',
          '<(bindings_output_dir)/V8DerivedSources18.cpp',
          '<(bindings_output_dir)/V8DerivedSources19.cpp',
        ],
      }],
      # The bindings generator can not write generated files if they are identical
      # to the already existing file – that way they don't need to be recompiled.
      # However, a reverse dependency having a newer timestamp than a
      # generated binding can confuse some build systems, so only use this on
      # ninja which explicitly supports this use case (gyp turns all actions into
      # ninja restat rules).
      ['"<(GENERATOR)"=="ninja"', {
        'write_file_only_if_changed': '--write-file-only-if-changed 1',
      },{
        'write_file_only_if_changed': '--write-file-only-if-changed 0',
      }],
    ],
  },

  'target_defaults': {
    'variables': {
      'optimize': 'max',
    },
  },

  'targets': [{
    'target_name': 'interface_dependencies',
    'type': 'none',
    'actions': [{
      'action_name': 'compute_interface_dependencies',
      'variables': {
        # Write sources into a file, so that the action command line won't
        # exceed OS limits.
        'main_idl_files_list': '<|(main_idl_files_list.tmp <@(main_idl_files))',
        'support_idl_files_list': '<|(support_idl_files_list.tmp <@(support_idl_files))',
      },
      'inputs': [
        'scripts/compute_dependencies.py',
        '<(main_idl_files_list)',
        '<@(main_idl_files)',
        '<(support_idl_files_list)',
        '<@(support_idl_files)',
       ],
       'outputs': [
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
         '<(SHARED_INTERMEDIATE_DIR)/blink/BindingsDerivedSources.txt',
         '<@(generated_global_constructors_idl_files)',
         '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
       ],
       'msvs_cygwin_shell': 0,
       'action': [
         'python',
         'scripts/compute_dependencies.py',
         '--main-idl-files-list',
         '<(main_idl_files_list)',
         '--support-idl-files-list',
         '<(support_idl_files_list)',
         '--interface-dependencies-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
         '--bindings-derived-sources-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/BindingsDerivedSources.txt',
         '--window-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WindowConstructors.idl',
         '--workerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/WorkerGlobalScopeConstructors.idl',
         '--sharedworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/SharedWorkerGlobalScopeConstructors.idl',
         '--dedicatedworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/DedicatedWorkerGlobalScopeConstructors.idl',
         '--serviceworkerglobalscope-constructors-file',
         '<(SHARED_INTERMEDIATE_DIR)/ServiceWorkerGlobalScopeConstructors.idl',
         '--event-names-file',
         '<(SHARED_INTERMEDIATE_DIR)/blink/EventInterfaces.in',
         '<@(write_file_only_if_changed)',
       ],
       'message': 'Resolving partial interfaces dependencies in all IDL files',
      }]
    },
    {
      'target_name': 'bindings_sources',
      'type': 'none',
      # The 'binding' rule generates .h files, so mark as hard_dependency, per:
      # https://code.google.com/p/gyp/wiki/InputFormatReference#Linking_Dependencies
      'hard_dependency': 1,
      'dependencies': [
        'interface_dependencies',
        '../config.gyp:config',
        '../core/core_derived_sources.gyp:generate_test_support_idls',
      ],
      'sources': [
        '<@(main_idl_files)',
        '<@(support_idl_files)',
        '<@(generated_support_idl_files)',
      ],
      'rules': [{
        'rule_name': 'binding',
        'extension': 'idl',
        'msvs_external_rule': 1,
        'inputs': [
          'scripts/generate_bindings.pl',
          'scripts/code_generator_v8.pm',
          'scripts/idl_parser.pm',
          'scripts/idl_serializer.pm',
          '../build/scripts/preprocessor.pm',
          'IDLExtendedAttributes.txt',
          # FIXME: If the dependency structure changes, we rebuild all files,
          # since we're not computing dependencies file-by-file in the build.
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
          # FIXME: Similarly, if any partial interface changes, rebuild
          # everything, since every IDL potentially depends on them, because
          # we're not computing dependencies file-by-file.
          #
          # If a new partial interface is added, need to regyp to update these
          # dependencies, as these are computed statically at gyp runtime.
          '<!@pymod_do_main(list_idl_files_with_partial_interface <@(main_idl_files) <@(support_idl_files))',
          # Generated IDLs are all partial interfaces, hence everything
          # potentially depends on them.
          '<@(generated_global_constructors_idl_files)',
        ],
        'outputs': [
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).cpp',
          '<(bindings_output_dir)/V8<(RULE_INPUT_ROOT).h',
        ],
        'variables': {
          # IDL include paths. The generator will search recursively for IDL
          # files under these locations.
          'generator_include_dirs': [
            '--include', '../core',
            '--include', '../modules',
            '--include', '<(SHARED_INTERMEDIATE_DIR)/blink',
          ],
          # Hook for embedders to specify extra directories to find IDL files.
          'extra_blink_generator_include_dirs%': [],
        },
        'msvs_cygwin_shell': 0,
        # sanitize-win-build-log.sed uses a regex which matches this command
        # line (Perl script + .idl file being processed).
        # Update that regex if command line changes (other than changing flags)
        'action': [
          '<(perl_exe)',
          '-w',
          '-Iscripts',
          '-I../build/scripts',
          '-I<(DEPTH)/third_party/JSON/out/lib/perl5',
          'scripts/generate_bindings.pl',
          '--outputDir',
          '<(bindings_output_dir)',
          '--idlAttributesFile',
          'IDLExtendedAttributes.txt',
          '<@(generator_include_dirs)',
          '<@(extra_blink_generator_include_dirs)',
          '--interfaceDependenciesFile',
          '<(SHARED_INTERMEDIATE_DIR)/blink/InterfaceDependencies.txt',
          '--additionalIdlFiles',
          # Generated IDL files aren't listed in InterfaceDependencies.txt,
          # b/c they aren't available at GYP run time, so we need to sort them
          # manually, depending on whether they are (main) interface files,
          # for which we generate bindings, or dependencies, for which we don't.
          # generated_support_idl_files are main interfaces, so generate
          # bindings, but generated_global_constructors_idl_files are all
          # partial interfaces, so don't (b/c generated by dependents).
          '<(generated_support_idl_files)',
          '<@(preprocessor)',
          '<@(write_file_only_if_changed)',
          '<(RULE_INPUT_PATH)',
        ],
        'message': 'Generating binding from <(RULE_INPUT_PATH)',
      }],
    },
    {
      'target_name': 'bindings_derived_sources',
      'type': 'none',
      'dependencies': [
        'interface_dependencies',
        'bindings_sources',
      ],
      'actions': [{
        'action_name': 'derived_sources_all_in_one',
        'inputs': [
          '../build/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/blink/BindingsDerivedSources.txt',
        ],
        'outputs': [
          '<@(derived_sources_aggregate_files)',
        ],
        'action': [
          'python',
          '../build/scripts/action_derivedsourcesallinone.py',
          '<(SHARED_INTERMEDIATE_DIR)/blink/BindingsDerivedSources.txt',
          '--',
          '<@(derived_sources_aggregate_files)',
        ],
        'message': 'Generating bindings derived sources',
      }],
    },
  ],
}
