# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # This turns on e.g. the filename-based detection of which
    # platforms to include source files on (e.g. files ending in
    # _mac.h or _mac.cc are only compiled on MacOSX).
    'chromium_code': 1,
   },
  'conditions': [
    ['android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'components_unittests',
          'type': '<(gtest_target_type)',
          'sources': [
            'auto_login_parser/auto_login_parser_unittest.cc',
            'autofill/core/browser/webdata/autofill_entry_unittest.cc',
            'autofill/core/browser/webdata/web_data_service_unittest.cc',
            'autofill/core/common/form_data_unittest.cc',
            'autofill/core/common/form_field_data_unittest.cc',
            'autofill/core/common/password_form_fill_data_unittest.cc',
            'browser_context_keyed_service/browser_context_dependency_manager_unittest.cc',
            'browser_context_keyed_service/dependency_graph_unittest.cc',
            'dom_distiller/core/article_entry_unittest.cc',
            'dom_distiller/core/distiller_unittest.cc',
            'dom_distiller/core/distiller_url_fetcher_unittest.cc',
            'dom_distiller/core/dom_distiller_database_unittest.cc',
            'dom_distiller/core/dom_distiller_model_unittest.cc',
            'dom_distiller/core/dom_distiller_service_unittest.cc',
            'dom_distiller/core/dom_distiller_store_unittest.cc',
            'dom_distiller/core/dom_distiller_test_util.cc',
            'dom_distiller/core/dom_distiller_test_util.h',
            'dom_distiller/core/fake_db.cc',
            'dom_distiller/core/fake_db.h',
            'dom_distiller/core/fake_distiller.cc',
            'dom_distiller/core/fake_distiller.h',
            'dom_distiller/core/task_tracker_unittest.cc',
            'json_schema/json_schema_validator_unittest.cc',
            'json_schema/json_schema_validator_unittest_base.cc',
            'json_schema/json_schema_validator_unittest_base.h',
            'navigation_interception/intercept_navigation_resource_throttle_unittest.cc',
            'precache/core/precache_database_unittest.cc',
            'precache/core/precache_fetcher_unittest.cc',
            'precache/core/precache_url_table_unittest.cc',
            'sessions/serialized_navigation_entry_unittest.cc',
            'test/run_all_unittests.cc',
            'translate/common/translate_metrics_unittest.cc',
            'translate/common/translate_util_unittest.cc',
            'translate/language_detection/language_detection_util_unittest.cc',
            'url_matcher/regex_set_matcher_unittest.cc',
            'url_matcher/string_pattern_unittest.cc',
            'url_matcher/substring_set_matcher_unittest.cc',
            'url_matcher/url_matcher_factory_unittest.cc',
            'url_matcher/url_matcher_unittest.cc',
            # TODO(asvitkine): These should be tested on iOS too.
            'variations/entropy_provider_unittest.cc',
            'variations/metrics_util_unittest.cc',
            'variations/variations_associated_data_unittest.cc',
            'variations/variations_seed_processor_unittest.cc',
            'visitedlink/test/visitedlink_unittest.cc',
            'webdata/encryptor/encryptor_password_mac_unittest.cc',
            'webdata/encryptor/encryptor_unittest.cc',
            'webdata/encryptor/ie7_password_unittest_win.cc',
            'web_modal/web_contents_modal_dialog_manager_unittest.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'dependencies': [
            '../base/base.gyp:base_prefs_test_support',
            '../base/base.gyp:test_support_base',
            # TODO(blundell): Eliminate the need for this dependency in code
            # that iOS shares. crbug.com/325243
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../sync/sync.gyp:sync',
            '../testing/gmock.gyp:gmock',
            '../testing/gtest.gyp:gtest',

            # Dependencies of auto_login_parser
            'components.gyp:auto_login_parser',

            # Dependencies of autofill
            'components.gyp:autofill_core_browser',
            'components.gyp:autofill_core_common',
            'components.gyp:autofill_core_test_support',

            # Dependencies of dom_distiller
            'components.gyp:distilled_page_proto',
            'components.gyp:dom_distiller_core',

            # Dependencies of encryptor
            'components.gyp:encryptor',

            # Dependencies of json_schema
            'components.gyp:json_schema',

            # Dependencies of precache
            'components.gyp:precache_core',

            # Dependencies of translate.
            'components.gyp:translate_common',
            'components.gyp:translate_language_detection',

            # Dependencies of variations
            'components.gyp:variations',
          ],
          'conditions': [
            ['OS != "ios"', {
              'dependencies': [
                # Dependencies of browser_context_keyed_service
                'components.gyp:browser_context_keyed_service',

                # Dependencies of 
                # intercept_navigation_resource_throttle_unittest.cc
                '../skia/skia.gyp:skia',
                'components.gyp:navigation_interception',

                # Dependencies of sessions
                '../third_party/protobuf/protobuf.gyp:protobuf_lite',
                'components.gyp:sessions',
                'components.gyp:sessions_test_support',

                # Dependencies of url_matcher.
                'components.gyp:url_matcher',

                # Dependencies of visitedlink
                'components.gyp:visitedlink_browser',
                'components.gyp:visitedlink_renderer',
                '../content/content_resources.gyp:content_resources',

                # Dependencies of web_modal
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }, { # 'OS == "ios"'
              'sources/': [
                ['exclude', '\\.cc$'],
                ['include', '^test/run_all_unittests\\.cc$'],
                # TODO(ios): Include files here as they are made to work, see
                # http://crbug.com/303011.
                # TODO(asvitkine): Bring up variations/ unittests on iOS.
                # TODO(blundell): Bring up json_schema/ unittests on iOS.
                ['include', '^auto_login_parser/'],
                ['include', '^autofill/'],
                ['include', '^dom_distiller/'],
                ['include', '^precache/'],
                ['include', '^translate/'],
              ],
            }],
            ['disable_nacl==0', {
              'sources': [
                'nacl/browser/nacl_file_host_unittest.cc',
                'nacl/browser/nacl_process_host_unittest.cc',
                'nacl/browser/nacl_validation_cache_unittest.cc',
                'nacl/browser/pnacl_host_unittest.cc',
                'nacl/browser/pnacl_translation_cache_unittest.cc',
                'nacl/browser/test_nacl_browser_delegate.cc',
              ],
              'dependencies': [
                'nacl.gyp:nacl_browser',
                'nacl.gyp:nacl_common',
              ],
            }],
            ['OS == "mac"', {
              'link_settings': {
                'libraries': [
                  '$(SDKROOT)/System/Library/Frameworks/AddressBook.framework',
                ],
              },
            }],
            ['OS == "android"', {
              'sources!': [
                'web_modal/web_contents_modal_dialog_manager_unittest.cc',
              ],
              'dependencies!': [
                'components.gyp:web_modal',
                'components.gyp:web_modal_test_support',
              ],
            }],
            ['OS == "android" and gtest_target_type == "shared_library"', {
              'dependencies': [
                '../testing/android/native_test.gyp:native_test_native_code',
              ]
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['OS=="linux" and component=="shared_library" and linux_use_tcmalloc==1', {
            'dependencies': [
                '<(DEPTH)/base/allocator/allocator.gyp:allocator',
            ],
            'link_settings': {
                'ldflags': ['-rdynamic'],
            },
            }],
            ['configuration_policy==1', {
              'dependencies': [
                'components.gyp:policy_component',
                'components.gyp:policy_component_test_support',
                'components.gyp:policy_test_support',
              ],
              'sources': [
                'policy/core/common/async_policy_provider_unittest.cc',
                'policy/core/common/cloud/cloud_policy_client_unittest.cc',
                'policy/core/common/cloud/cloud_policy_core_unittest.cc',
                'policy/core/common/cloud/cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/cloud_policy_refresh_scheduler_unittest.cc',
                'policy/core/common/cloud/cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/cloud_policy_validator_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_service_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/component_cloud_policy_updater_unittest.cc',
                'policy/core/common/cloud/device_management_service_unittest.cc',
                'policy/core/common/cloud/external_policy_data_fetcher_unittest.cc',
                'policy/core/common/cloud/external_policy_data_updater_unittest.cc',
                'policy/core/common/cloud/policy_header_io_helper_unittest.cc',
                'policy/core/common/cloud/policy_header_service_unittest.cc',
                'policy/core/common/cloud/rate_limiter_unittest.cc',
                'policy/core/common/cloud/resource_cache_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                'policy/core/common/cloud/user_info_fetcher_unittest.cc',
                'policy/core/common/config_dir_policy_loader_unittest.cc',
                'policy/core/common/forwarding_policy_provider_unittest.cc',
                'policy/core/common/generate_policy_source_unittest.cc',
                'policy/core/common/policy_bundle_unittest.cc',
                'policy/core/common/policy_loader_mac_unittest.cc',
                'policy/core/common/policy_loader_win_unittest.cc',
                'policy/core/common/policy_map_unittest.cc',
                'policy/core/common/policy_service_impl_unittest.cc',
                'policy/core/common/policy_statistics_collector_unittest.cc',
                'policy/core/common/preg_parser_win_unittest.cc',
                'policy/core/common/registry_dict_win_unittest.cc',
                'policy/core/common/schema_map_unittest.cc',
                'policy/core/common/schema_registry_unittest.cc',
                'policy/core/common/schema_unittest.cc',
              ],
              'conditions': [
                ['OS=="android"', {
                  'sources!': [
                    'policy/core/common/async_policy_provider_unittest.cc',
                    'policy/core/common/cloud/component_cloud_policy_service_unittest.cc',
                    'policy/core/common/cloud/component_cloud_policy_store_unittest.cc',
                    'policy/core/common/cloud/component_cloud_policy_updater_unittest.cc',
                    'policy/core/common/cloud/external_policy_data_fetcher_unittest.cc',
                    'policy/core/common/cloud/external_policy_data_updater_unittest.cc',
                    'policy/core/common/cloud/resource_cache_unittest.cc',
                    'policy/core/common/config_dir_policy_loader_unittest.cc',
                  ],
                }],
                ['chromeos==1', {
                  'sources!': [
                    'policy/core/common/cloud/user_cloud_policy_manager_unittest.cc',
                    'policy/core/common/cloud/user_cloud_policy_store_unittest.cc',
                  ],
                }],
              ],
            }],
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [4267, ],
        },
      ],
    }],
    ['OS != "ios" and android_webview_build == 0', {
      'targets': [
        {
          'target_name': 'components_perftests',
          'type': '<(gtest_target_type)',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_perf',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../testing/gtest.gyp:gtest',
            '../ui/compositor/compositor.gyp:compositor',
            'components.gyp:visitedlink_browser',
          ],
         'include_dirs': [
           '..',
         ],
         'sources': [
           'visitedlink/test/visitedlink_perftest.cc',
         ],
         'conditions': [
           ['OS == "android" and gtest_target_type == "shared_library"', {
             'dependencies': [
               '../testing/android/native_test.gyp:native_test_native_code',
             ],
           }],
         ],
         # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
         'msvs_disabled_warnings': [ 4267, ],
        },
      ],
      'conditions': [
        ['OS == "android" and gtest_target_type == "shared_library"', {
          'targets': [
            {
              'target_name': 'components_unittests_apk',
              'type': 'none',
              'dependencies': [
                'components_unittests',
              ],
              'variables': {
                'test_suite_name': 'components_unittests',
                'input_shlib_path': '<(SHARED_LIB_DIR)/<(SHARED_LIB_PREFIX)components_unittests<(SHARED_LIB_SUFFIX)',
              },
              'includes': [ '../build/apk_test.gypi' ],
            },
          ],
        }],
      ],
    }],
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'components_browsertests',
          'type': '<(gtest_target_type)',
          'defines!': ['CONTENT_IMPLEMENTATION'],
          'dependencies': [
            '../content/content_shell_and_tests.gyp:content_browser_test_support',
            '../content/content_shell_and_tests.gyp:test_support_content',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            'components.gyp:dom_distiller_content',
            'components.gyp:dom_distiller_core',
          ],
          'include_dirs': [
            '..',
          ],
          'defines': [
            'HAS_OUT_OF_PROC_TEST_RUNNER',
          ],
          'sources': [
            '../content/test/content_test_launcher.cc',
            'dom_distiller/content/distiller_page_web_contents_browsertest.cc',
          ],
          'conditions': [
            ['OS=="win"', {
              'resource_include_dirs': [
                '<(SHARED_INTERMEDIATE_DIR)/webkit',
              ],
              'sources': [
                '../content/shell/app/resource.h',
                '../content/shell/app/shell.rc',
                # TODO:  It would be nice to have these pulled in
                # automatically from direct_dependent_settings in
                # their various targets (net.gyp:net_resources, etc.),
                # but that causes errors in other targets when
                # resulting .res files get referenced multiple times.
                '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',
                '<(SHARED_INTERMEDIATE_DIR)/webkit/webkit_strings_en-US.rc',
              ],
              'dependencies': [
                '<(DEPTH)/net/net.gyp:net_resources',
                '<(DEPTH)/third_party/iaccessible2/iaccessible2.gyp:iaccessible2',
                '<(DEPTH)/third_party/isimpledom/isimpledom.gyp:isimpledom',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_strings',
                '<(DEPTH)/webkit/webkit_resources.gyp:webkit_resources',
              ],
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
              'msvs_disabled_warnings': [ 4267, ],
            }],
            ['OS=="win" and win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        },
      ],
    }],
  ],
}
