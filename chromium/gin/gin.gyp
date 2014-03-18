# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'gin',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../v8/tools/gyp/v8.gyp:v8',
      ],
      'defines': [
        'GIN_IMPLEMENTATION',
      ],
      'sources': [
        'arguments.cc',
        'arguments.h',
        'array_buffer.cc',
        'array_buffer.h',
        'context_holder.cc',
        'converter.cc',
        'converter.h',
        'dictionary.cc',
        'dictionary.h',
        'function_template.cc',
        'function_template.h',
        'gin_export.h',
        'handle.h',
        'isolate_holder.cc',
        'modules/console.cc',
        'modules/console.h',
        'modules/file_module_provider.cc',
        'modules/file_module_provider.h',
        'modules/module_registry.cc',
        'modules/module_registry.h',
        'modules/module_runner_delegate.cc',
        'modules/module_runner_delegate.h',
        'object_template_builder.cc',
        'object_template_builder.h',
        'per_context_data.cc',
        'per_context_data.h',
        'per_isolate_data.cc',
        'per_isolate_data.h',
        'public/context_holder.h',
        'public/gin_embedders.h',
        'public/isolate_holder.h',
        'public/wrapper_info.h',
        'runner.cc',
        'runner.h',
        'try_catch.cc',
        'try_catch.h',
        'wrappable.cc',
        'wrappable.h',
        'wrapper_info.cc',
      ],
    },
    {
      'target_name': 'gin_shell',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../v8/tools/gyp/v8.gyp:v8',
        'gin',
      ],
      'sources': [
        'shell/gin_main.cc',
      ],
      'msvs_settings': {
        'VCLinkerTool': {
          'SubSystem': '1', # /SUBSYSTEM:CONSOLE
        },
      },
    },
    {
      'target_name': 'gin_test',
      'type': 'static_library',
      'dependencies': [
        '../testing/gtest.gyp:gtest',
        '../v8/tools/gyp/v8.gyp:v8',
        'gin',
      ],
      'export_dependent_settings': [
        '../testing/gtest.gyp:gtest',
        'gin',
      ],
      'sources': [
        'test/file_runner.cc',
        'test/file_runner.h',
        'test/gtest.cc',
        'test/gtest.h',
        'test/v8_test.cc',
        'test/v8_test.h',
      ],
    },
    {
      'target_name': 'gin_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../v8/tools/gyp/v8.gyp:v8',
        'gin_test',
      ],
      'sources': [
        'converter_unittest.cc',
        'test/run_all_unittests.cc',
        'test/run_js_tests.cc',
        'runner_unittest.cc',
        'wrappable_unittest.cc',
      ],
    },
  ],
}
