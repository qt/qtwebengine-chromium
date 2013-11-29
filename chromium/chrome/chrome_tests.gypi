# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables' : {
    'pyautolib_sources': [
      'app/chrome_command_ids.h',
      'app/chrome_dll_resource.h',
      'common/automation_constants.h',
      'common/pref_names.cc',
      'common/pref_names.h',
      'browser/chromeos/cros/network_constants.h',
      'test/automation/browser_proxy.cc',
      'test/automation/browser_proxy.h',
      'test/automation/tab_proxy.cc',
      'test/automation/tab_proxy.h',
      '../content/public/common/page_type.h',
      '../content/public/common/security_style.h',
      # Must come before cert_status_flags.h
      '../net/base/net_export.h',
      '../net/cert/cert_status_flags.h',
    ],
    'conditions': [
      ['asan==1', {
        'pyautolib_sources': [
          'test/pyautolib/asan_stub.c',
        ]
      }],
    ],
  },
  'includes': [
    'js_unittest_vars.gypi',
  ],
  'targets': [
    {
      'target_name': 'test_support_ui_runner',
      'type': 'static_library',
      'dependencies': [
        'test_support_common',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/ui/run_all_unittests.cc',
      ],
    },
    {
      'target_name': 'test_support_ui',
      'type': 'static_library',
      'dependencies': [
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:theme_resources',
        'test_support_common',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'export_dependent_settings': [
        'test_support_common',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/automation/proxy_launcher.cc',
        'test/automation/proxy_launcher.h',
        'test/reliability/automated_ui_test_base.cc',
        'test/reliability/automated_ui_test_base.h',
        'test/ui/javascript_test_util.cc',
        'test/ui/run_all_unittests.cc',
        'test/ui/ui_perf_test.cc',
        'test/ui/ui_perf_test.h',
        'test/ui/ui_test.cc',
        'test/ui/ui_test.h',
        'test/ui/ui_test_suite.cc',
        'test/ui/ui_test_suite.h',
      ],
      'conditions': [
        ['OS=="win"', {
          'dependencies': [
            'chrome.gyp:crash_service',  # run time dependency
          ],
        }],
        ['OS=="win" and target_arch=="ia32"', {
          'dependencies': [
            'chrome.gyp:crash_service_win64',  # run time dependency
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'automated_ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_resources.gyp:theme_resources',
        'renderer',
        'test_support_common',
        'test_support_ui',
        'test_support_ui_runner',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../third_party/libxml/libxml.gyp:libxml',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/automated_ui_tests.cc',
        'test/reliability/automated_ui_tests.h',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        },],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            'automated_ui_tests_exe_pdb_workaround',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
        }],
        ['OS=="linux" and enable_webrtc==1', {
          'dependencies': [
            # TODO(tommi): Figure out why the 32bit lucid builder fails to
            # find this dependency for this target (other builders pick it up
            # correctly).  crbug.com/231068.
            '../third_party/libjingle/libjingle.gyp:libpeerconnection',
          ],
        }],
      ],
    },
    {
      'target_name': 'interactive_ui_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common/extensions/api/api.gyp:api',
        'debugger',
        'renderer',
        'test_support_common',
        # NOTE: don't add test_support_ui, no more UITests. See
        # http://crbug.com/137365
        '../third_party/hunspell/hunspell.gyp:hunspell',
        '../net/net.gyp:net',
        '../net/net.gyp:net_resources',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/npapi/npapi.gyp:npapi',
        # Runtime dependencies
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        '../webkit/webkit_resources.gyp:webkit_resources',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
        'INTERACTIVE_TESTS',
      ],
      'sources': [
        '../apps/app_shim/app_shim_quit_interactive_uitest_mac.mm',
        'browser/autofill/autofill_interactive_uitest.cc',
        'browser/browser_keyevents_browsertest.cc',
        'browser/extensions/api/omnibox/omnibox_api_interactive_test.cc',
        'browser/extensions/api/tabs/tabs_interactive_test.cc',
        'browser/extensions/extension_apitest.cc',
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_crash_recovery_browsertest.cc',
        'browser/extensions/extension_function_test_utils.cc',
        'browser/extensions/extension_keybinding_apitest.cc',
        'browser/extensions/extension_test_message_listener.cc',
        'browser/extensions/extension_test_message_listener.h',
        'browser/extensions/notifications_apitest.cc',
        'browser/extensions/platform_app_browsertest_util.cc',
        'browser/extensions/platform_app_browsertest_util.h',
        'browser/extensions/web_view_interactive_browsertest.cc',
        'browser/extensions/window_open_interactive_apitest.cc',
        'browser/extensions/extension_fullscreen_apitest.cc',
        'browser/extensions/extension_pointer_lock_apitest.cc',
        'browser/mouseleave_browsertest.cc',
        'browser/notifications/desktop_notifications_unittest.cc',
        'browser/notifications/desktop_notifications_unittest.h',
        'browser/notifications/notification_browsertest.cc',
        'browser/printing/print_dialog_cloud_interative_uitest.cc',
        'browser/task_manager/task_manager_browsertest_util.cc',
        'browser/ui/browser_focus_uitest.cc',
        'browser/ui/cocoa/panels/panel_cocoa_browsertest.mm',
        'browser/ui/fullscreen/fullscreen_controller_interactive_browsertest.cc',
        'browser/ui/fullscreen/fullscreen_controller_state_interactive_browsertest.cc',
        'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
        'browser/ui/gtk/bookmarks/bookmark_bubble_gtk_browsertest.cc',
        'browser/ui/omnibox/omnibox_view_browsertest.cc',
        'browser/ui/panels/base_panel_browser_test.cc',
        'browser/ui/panels/base_panel_browser_test.h',
        'browser/ui/panels/detached_panel_browsertest.cc',
        'browser/ui/panels/docked_panel_browsertest.cc',
        'browser/ui/panels/panel_browsertest.cc',
        'browser/ui/panels/panel_drag_browsertest.cc',
        'browser/ui/panels/panel_resize_browsertest.cc',
        'browser/ui/panels/stacked_panel_browsertest.cc',
        'browser/ui/panels/test_panel_active_state_observer.cc',
        'browser/ui/panels/test_panel_active_state_observer.h',
        'browser/ui/panels/test_panel_mouse_watcher.cc',
        'browser/ui/panels/test_panel_mouse_watcher.h',
        'browser/ui/panels/test_panel_notification_observer.cc',
        'browser/ui/panels/test_panel_notification_observer.h',
        'browser/ui/panels/test_panel_collection_squeeze_observer.cc',
        'browser/ui/panels/test_panel_collection_squeeze_observer.h',
        'browser/ui/search/instant_extended_interactive_uitest.cc',
        'browser/ui/search/instant_extended_manual_interactive_uitest.cc',
        'browser/ui/search/instant_test_utils.h',
        'browser/ui/search/instant_test_utils.cc',
        'browser/ui/search/local_ntp_browsertest.cc',
        'browser/ui/startup/startup_browser_creator_interactive_uitest.cc',
        'browser/ui/views/ash/tab_scrubber_browsertest.cc',
        'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
        'browser/ui/views/button_dropdown_test.cc',
        'browser/ui/views/constrained_window_views_browsertest.cc',
        'browser/ui/views/find_bar_controller_interactive_uitest.cc',
        'browser/ui/views/find_bar_host_interactive_uitest.cc',
        'browser/ui/views/frame/browser_view_focus_uitest.cc',
        'browser/ui/views/frame/browser_view_interactive_uitest.cc',
        'browser/ui/views/keyboard_access_browsertest.cc',
        'browser/ui/views/location_bar/star_view_browsertest.cc',
        'browser/ui/views/menu_item_view_test.cc',
        'browser/ui/views/menu_model_adapter_test.cc',
        'browser/ui/views/message_center/web_notification_tray_browsertest.cc',
        'browser/ui/views/native_widget_win_interactive_uitest.cc',
        'browser/ui/views/omnibox/omnibox_view_views_browsertest.cc',
        'browser/ui/views/panels/panel_view_browsertest.cc',
        'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.h',
        'browser/ui/views/tabs/tab_drag_controller_interactive_uitest_win.cc',
        'test/base/chrome_test_launcher.cc',
        'test/base/interactive_test_utils.cc',
        'test/base/interactive_test_utils.h',
        'test/base/interactive_test_utils_aura.cc',
        'test/base/interactive_test_utils_aura.h',
        'test/base/interactive_test_utils_gtk.cc',
        'test/base/interactive_test_utils_mac.mm',
        'test/base/interactive_test_utils_views.cc',
        'test/base/interactive_test_utils_win.cc',
        'test/base/view_event_test_base.cc',
        'test/base/view_event_test_base.h',
        'test/ppapi/ppapi_interactive_browsertest.cc',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['toolkit_uses_gtk == 1 and toolkit_views == 0', {
          'sources!': [
            # TODO(port)
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/constrained_window_views_browsertest.cc',
            'browser/ui/views/crypto_module_password_dialog_view_unittest.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/keyboard_access_browsertest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'test/base/view_event_test_base.cc',
            'test/base/view_event_test_base.h',
          ],
        }],
        ['use_ash==1', {
          'sources': [
            '../ash/drag_drop/drag_drop_interactive_uitest.cc',
            'browser/ui/window_sizer/window_sizer_ash_uitest.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'sources!': [
            # TODO(port)
            'browser/ui/gtk/bookmarks/bookmark_bar_gtk_interactive_uitest.cc',
          ],
        }],
        ['OS=="mac"', {
          'sources!': [
            # TODO(port)
            'browser/ui/views/bookmarks/bookmark_bar_view_test.cc',
            'browser/ui/views/button_dropdown_test.cc',
            'browser/ui/views/constrained_window_views_browsertest.cc',
            'browser/ui/views/find_bar_host_interactive_uitest.cc',
            'browser/ui/views/keyboard_access_browsertest.cc',
            'browser/ui/views/menu_item_view_test.cc',
            'browser/ui/views/menu_model_adapter_test.cc',
            'browser/ui/views/tabs/tab_drag_controller_interactive_uitest.cc',
            'test/base/view_event_test_base.cc',
            'test/base/view_event_test_base.h',
          ],
          'dependencies': [
            'chrome'
          ],
          # See comment about the same line in chrome/chrome_tests.gypi.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
        }],  # OS=="mac"
        ['notifications==0', {
          'sources/': [
            ['exclude', '^browser/notifications/'],
            ['exclude', '^browser/extensions/notifications_apitest.cc'],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_test_support',
          ],
        }, { # else: toolkit_views == 0
          'sources/': [
            ['exclude', '^browser/ui/views/'],
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:ash_test_support',
          ],
        }],
        ['use_aura==1', {
          'sources!': [
            'browser/ui/views/tabs/tab_drag_controller_interactive_uitest_win.cc',
          ],
        }],
        ['chromeos==1', {
          'dependencies': [
            '../chromeos/chromeos.gyp:chromeos',
          ],
          'sources': [
            'browser/chromeos/cros/cros_in_process_browser_test.cc',
            'browser/chromeos/cros/cros_in_process_browser_test.h',
            'browser/chromeos/input_method/textinput_browsertest.cc',
            'browser/chromeos/input_method/textinput_surroundingtext_browsertest.cc',
            'browser/chromeos/input_method/textinput_test_helper.cc',
            'browser/chromeos/input_method/textinput_test_helper.h',
            'browser/chromeos/login/captive_portal_window_browsertest.cc',
            'browser/chromeos/login/eula_browsertest.cc',
            'browser/chromeos/login/login_browsertest.cc',
            'browser/chromeos/login/oobe_browsertest.cc',
            'browser/chromeos/login/screen_locker_browsertest.cc',
            'browser/chromeos/login/screen_locker_tester.cc',
            'browser/chromeos/login/screen_locker_tester.h',
            'browser/chromeos/login/wallpaper_manager_browsertest.cc',
            'test/data/chromeos/service_login.html',
          ],
          'sources!': [
            # chromeos does not use cross-platform panels
            'browser/ui/panels/detached_panel_browsertest.cc',
            'browser/ui/panels/docked_panel_browsertest.cc',
            'browser/ui/panels/panel_browsertest.cc',
            'browser/ui/panels/panel_drag_browsertest.cc',
            'browser/ui/panels/panel_resize_browsertest.cc',
            'browser/ui/panels/stacked_panel_browsertest.cc',
            'browser/ui/views/message_center/web_notification_tray_browsertest.cc',
            'browser/ui/views/panels/panel_view_browsertest.cc',
            'browser/notifications/desktop_notifications_unittest.cc',
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/wtl/include',
          ],
          'dependencies': [
            'chrome.gyp:chrome_version_resources',
            '../third_party/isimpledom/isimpledom.gyp:isimpledom',
            '../ui/ui.gyp:ui_resources',
          ],
          'sources': [
            '../ui/resources/cursors/aliasb.cur',
            '../ui/resources/cursors/cell.cur',
            '../ui/resources/cursors/col_resize.cur',
            '../ui/resources/cursors/copy.cur',
            '../ui/resources/cursors/none.cur',
            '../ui/resources/cursors/row_resize.cur',
            '../ui/resources/cursors/vertical_text.cur',
            '../ui/resources/cursors/zoom_in.cur',
            '../ui/resources/cursors/zoom_out.cur',

            'app/chrome_dll.rc',
            'test/data/resource.rc',

            # TODO:  It would be nice to have these pulled in
            # automatically from direct_dependent_settings in
            # their various targets (net.gyp:net_resources, etc.),
            # but that causes errors in other targets when
            # resulting .res files get referenced multiple times.
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',

            'browser/ui/views/accessibility/browser_views_accessibility_browsertest.cc',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                 '../base/allocator/allocator.gyp:allocator',
              ],
            }],
            ['use_aura==1', {
              'sources/': [
                ['exclude', '^browser/ui/views/accessibility/browser_views_accessibility_browsertest.cc'],
                ['exclude', '^browser/ui/views/native_widget_win_interactive_uitest.cc'],
              ],
            }],
          ],
          'msvs_settings': {
            'VCLinkerTool': {
              'conditions': [
                ['incremental_chrome_dll==1', {
                  'UseLibraryDependencyInputs': "true",
                }],
              ],
            },
          },
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267, ],
        }, { # else: OS != "win"
          'sources!': [
            'browser/ui/views/ssl_client_certificate_selector_browsertest.cc',
            'browser/ui/views/native_widget_win_interactive_uitest.cc',
          ],
        }],  # OS != "win"
      ],  # conditions
    },
    {
      # Third-party support sources for chromedriver_lib.
      'target_name': 'chromedriver_support',
      'type': 'static_library',
      'sources': [
        '../third_party/mongoose/mongoose.c',
        '../third_party/mongoose/mongoose.h',
        '../third_party/webdriver/atoms.cc',
        '../third_party/webdriver/atoms.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      # Although this one is probably safe to ignore.
      # Mongose casts -1 directly as HANDLE (definitely safe to ignore).
      'msvs_disabled_warnings': [4267, 4306, ],
   },
    {
      # chromedriver is the chromium implementation of WebDriver.
      # See http://www.chromium.org/developers/testing/webdriver-for-chrome
      'target_name': 'chromedriver_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libxml/libxml.gyp:libxml',
        '../url/url.gyp:url_lib',
        'browser',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chromedriver_support',
        'common',
        'test_support_ui',
        'test_support_ui_runner',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/commands/alert_commands.cc',
        'test/webdriver/commands/alert_commands.h',
        'test/webdriver/commands/appcache_status_command.cc',
        'test/webdriver/commands/appcache_status_command.h',
        'test/webdriver/commands/browser_connection_commands.cc',
        'test/webdriver/commands/browser_connection_commands.h',
        'test/webdriver/commands/chrome_commands.cc',
        'test/webdriver/commands/chrome_commands.h',
        'test/webdriver/commands/command.cc',
        'test/webdriver/commands/command.h',
        'test/webdriver/commands/cookie_commands.cc',
        'test/webdriver/commands/cookie_commands.h',
        'test/webdriver/commands/create_session.cc',
        'test/webdriver/commands/create_session.h',
        'test/webdriver/commands/execute_async_script_command.cc',
        'test/webdriver/commands/execute_async_script_command.h',
        'test/webdriver/commands/execute_command.cc',
        'test/webdriver/commands/execute_command.h',
        'test/webdriver/commands/file_upload_command.cc',
        'test/webdriver/commands/file_upload_command.h',
        'test/webdriver/commands/find_element_commands.cc',
        'test/webdriver/commands/find_element_commands.h',
        'test/webdriver/commands/html5_location_commands.cc',
        'test/webdriver/commands/html5_location_commands.h',
        'test/webdriver/commands/html5_storage_commands.cc',
        'test/webdriver/commands/html5_storage_commands.h',
        'test/webdriver/commands/keys_command.cc',
        'test/webdriver/commands/keys_command.h',
        'test/webdriver/commands/log_command.cc',
        'test/webdriver/commands/log_command.h',
        'test/webdriver/commands/navigate_commands.cc',
        'test/webdriver/commands/navigate_commands.h',
        'test/webdriver/commands/mouse_commands.cc',
        'test/webdriver/commands/mouse_commands.h',
        'test/webdriver/commands/response.h',
        'test/webdriver/commands/response.cc',
        'test/webdriver/commands/screenshot_command.cc',
        'test/webdriver/commands/screenshot_command.h',
        'test/webdriver/commands/session_with_id.cc',
        'test/webdriver/commands/session_with_id.h',
        'test/webdriver/commands/set_timeout_commands.cc',
        'test/webdriver/commands/set_timeout_commands.h',
        'test/webdriver/commands/source_command.cc',
        'test/webdriver/commands/source_command.h',
        'test/webdriver/commands/target_locator_commands.cc',
        'test/webdriver/commands/target_locator_commands.h',
        'test/webdriver/commands/title_command.cc',
        'test/webdriver/commands/title_command.h',
        'test/webdriver/commands/url_command.cc',
        'test/webdriver/commands/url_command.h',
        'test/webdriver/commands/webdriver_command.cc',
        'test/webdriver/commands/webdriver_command.h',
        'test/webdriver/commands/webelement_commands.cc',
        'test/webdriver/commands/webelement_commands.h',
        'test/webdriver/commands/window_commands.cc',
        'test/webdriver/commands/window_commands.h',
        'test/webdriver/frame_path.cc',
        'test/webdriver/frame_path.h',
        'test/webdriver/http_response.cc',
        'test/webdriver/http_response.h',
        'test/webdriver/keycode_text_conversion.h',
        'test/webdriver/keycode_text_conversion_gtk.cc',
        'test/webdriver/keycode_text_conversion_mac.mm',
        'test/webdriver/keycode_text_conversion_win.cc',
        'test/webdriver/keycode_text_conversion_x.cc',
        'test/webdriver/webdriver_automation.cc',
        'test/webdriver/webdriver_automation.h',
        'test/webdriver/webdriver_basic_types.cc',
        'test/webdriver/webdriver_basic_types.h',
        'test/webdriver/webdriver_capabilities_parser.cc',
        'test/webdriver/webdriver_capabilities_parser.h',
        'test/webdriver/webdriver_dispatch.cc',
        'test/webdriver/webdriver_dispatch.h',
        'test/webdriver/webdriver_element_id.cc',
        'test/webdriver/webdriver_element_id.h',
        'test/webdriver/webdriver_error.cc',
        'test/webdriver/webdriver_error.h',
        'test/webdriver/webdriver_key_converter.cc',
        'test/webdriver/webdriver_key_converter.h',
        'test/webdriver/webdriver_logging.cc',
        'test/webdriver/webdriver_logging.h',
        'test/webdriver/webdriver_session.cc',
        'test/webdriver/webdriver_session.h',
        'test/webdriver/webdriver_session_manager.cc',
        'test/webdriver/webdriver_session_manager.h',
        'test/webdriver/webdriver_switches.cc',
        'test/webdriver/webdriver_switches.h',
        'test/webdriver/webdriver_util.cc',
        'test/webdriver/webdriver_util.h',
        'test/webdriver/webdriver_util_mac.mm',
      ],
      'conditions': [
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
          'sources!': [
            'test/webdriver/keycode_text_conversion_x.cc',
          ],
        }],
        ['toolkit_uses_gtk == 0', {
          'sources!': [
            'test/webdriver/keycode_text_conversion_gtk.cc',
          ],
        }],
        ['OS=="linux" and toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],
    },
    {
      'target_name': 'chromedriver',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/webdriver_server.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
              '-lws2_32.lib',
            ],
          },
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        }],
      ]
    },
    {
      'target_name': 'chromedriver_unittests',
      'type': 'executable',
      'dependencies': [
        'chromedriver_lib',
        '../base/base.gyp:run_all_unittests',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../skia/skia.gyp:skia',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/webdriver/commands/set_timeout_commands_unittest.cc',
        'test/webdriver/frame_path_unittest.cc',
        'test/webdriver/http_response_unittest.cc',
        'test/webdriver/keycode_text_conversion_unittest.cc',
        'test/webdriver/webdriver_capabilities_parser_unittest.cc',
        'test/webdriver/webdriver_dispatch_unittest.cc',
        'test/webdriver/webdriver_key_converter_unittest.cc',
        'test/webdriver/webdriver_test_util.cc',
        'test/webdriver/webdriver_test_util.h',
        'test/webdriver/webdriver_util_unittest.cc',
      ],
      'conditions': [
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'link_settings': {
            'libraries': [
              '-lOleAcc.lib',
              '-lws2_32.lib',
            ],
          },
          'configurations': {
            'Debug': {
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
      ],
    },
    {
      'target_name': 'chrome_devtools_lib',
      'type': 'static_library',
      'hard_dependency': 1,
      'dependencies': [
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../third_party/zlib/zlib.gyp:minizip',
        '../third_party/zlib/zlib.gyp:zlib',
        '../ui/ui.gyp:ui',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
        '<(SHARED_INTERMEDIATE_DIR)',
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)',
        ],
      },
      'sources': [
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
        '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
        'test/chromedriver/chrome/adb.h',
        'test/chromedriver/chrome/adb_impl.cc',
        'test/chromedriver/chrome/adb_impl.h',
        'test/chromedriver/chrome/automation_extension.cc',
        'test/chromedriver/chrome/automation_extension.h',
        'test/chromedriver/chrome/chrome.h',
        'test/chromedriver/chrome/chrome_android_impl.cc',
        'test/chromedriver/chrome/chrome_android_impl.h',
        'test/chromedriver/chrome/chrome_desktop_impl.cc',
        'test/chromedriver/chrome/chrome_desktop_impl.h',
        'test/chromedriver/chrome/chrome_finder.cc',
        'test/chromedriver/chrome/chrome_finder.h',
        'test/chromedriver/chrome/chrome_finder_mac.mm',
        'test/chromedriver/chrome/chrome_impl.cc',
        'test/chromedriver/chrome/chrome_impl.h',
        'test/chromedriver/chrome/console_logger.cc',
        'test/chromedriver/chrome/console_logger.h',
        'test/chromedriver/chrome/device_manager.cc',
        'test/chromedriver/chrome/device_manager.h',
        'test/chromedriver/chrome/devtools_client.h',
        'test/chromedriver/chrome/devtools_client_impl.cc',
        'test/chromedriver/chrome/devtools_client_impl.h',
        'test/chromedriver/chrome/devtools_event_listener.cc',
        'test/chromedriver/chrome/devtools_event_listener.h',
        'test/chromedriver/chrome/devtools_http_client.cc',
        'test/chromedriver/chrome/devtools_http_client.h',
        'test/chromedriver/chrome/dom_tracker.cc',
        'test/chromedriver/chrome/dom_tracker.h',
        'test/chromedriver/chrome/frame_tracker.cc',
        'test/chromedriver/chrome/frame_tracker.h',
        'test/chromedriver/chrome/geolocation_override_manager.cc',
        'test/chromedriver/chrome/geolocation_override_manager.h',
        'test/chromedriver/chrome/geoposition.h',
        'test/chromedriver/chrome/javascript_dialog_manager.cc',
        'test/chromedriver/chrome/javascript_dialog_manager.h',
        'test/chromedriver/chrome/log.h',
        'test/chromedriver/chrome/log.cc',
        'test/chromedriver/chrome/navigation_tracker.cc',
        'test/chromedriver/chrome/navigation_tracker.h',
        'test/chromedriver/chrome/performance_logger.h',
        'test/chromedriver/chrome/performance_logger.cc',
        'test/chromedriver/chrome/status.cc',
        'test/chromedriver/chrome/status.h',
        'test/chromedriver/chrome/ui_events.cc',
        'test/chromedriver/chrome/ui_events.h',
        'test/chromedriver/chrome/version.cc',
        'test/chromedriver/chrome/version.h',
        'test/chromedriver/chrome/web_view.h',
        'test/chromedriver/chrome/web_view_impl.cc',
        'test/chromedriver/chrome/web_view_impl.h',
        'test/chromedriver/chrome/zip.cc',
        'test/chromedriver/chrome/zip.h',
        'test/chromedriver/chrome/zip_internal.cc',
        'test/chromedriver/chrome/zip_internal.h',
        'test/chromedriver/chrome/zip_reader.cc',
        'test/chromedriver/chrome/zip_reader.h',
        'test/chromedriver/net/adb_client_socket.cc',
        'test/chromedriver/net/adb_client_socket.h',
        'test/chromedriver/net/net_util.cc',
        'test/chromedriver/net/net_util.h',
        'test/chromedriver/net/sync_websocket.h',
        'test/chromedriver/net/sync_websocket_factory.cc',
        'test/chromedriver/net/sync_websocket_factory.h',
        'test/chromedriver/net/sync_websocket_impl.cc',
        'test/chromedriver/net/sync_websocket_impl.h',
        'test/chromedriver/net/url_request_context_getter.cc',
        'test/chromedriver/net/url_request_context_getter.h',
        'test/chromedriver/net/websocket.cc',
        'test/chromedriver/net/websocket.h',
      ],
      'actions': [
        {
          'action_name': 'embed_js_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_js_in_cpp.py',
            'test/chromedriver/js/add_cookie.js',
            'test/chromedriver/js/call_function.js',
            'test/chromedriver/js/dispatch_context_menu_event.js',
            'test/chromedriver/js/execute_async_script.js',
            'test/chromedriver/js/focus.js',
            'test/chromedriver/js/get_element_region.js',
            'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/js.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_js_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/js/add_cookie.js',
                      'test/chromedriver/js/call_function.js',
                      'test/chromedriver/js/dispatch_context_menu_event.js',
                      'test/chromedriver/js/execute_async_script.js',
                      'test/chromedriver/js/focus.js',
                      'test/chromedriver/js/get_element_region.js',
                      'test/chromedriver/js/is_option_element_toggleable.js',
          ],
          'message': 'Generating sources for embedding js in chromedriver',
        },
        {
          'action_name': 'embed_user_data_dir_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_user_data_dir_in_cpp.py',
            'test/chromedriver/chrome/preferences.txt',
            'test/chromedriver/chrome/local_state.txt',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/user_data_dir.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_user_data_dir_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/chrome/preferences.txt',
                      'test/chromedriver/chrome/local_state.txt',
          ],
          'message': 'Generating sources for embedding user data dir in chromedriver',
        },
        {
          'action_name': 'embed_extension_in_cpp',
          'inputs': [
            'test/chromedriver/cpp_source.py',
            'test/chromedriver/embed_extension_in_cpp.py',
            'test/chromedriver/extension/background.js',
            'test/chromedriver/extension/manifest.json',
          ],
          'outputs': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.cc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome/embedded_automation_extension.h',
          ],
          'action': [ 'python',
                      'test/chromedriver/embed_extension_in_cpp.py',
                      '--directory',
                      '<(SHARED_INTERMEDIATE_DIR)/chrome/test/chromedriver/chrome',
                      'test/chromedriver/extension/background.js',
                      'test/chromedriver/extension/manifest.json',
          ],
          'message': 'Generating sources for embedding automation extension',
        },
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    # This is the new ChromeDriver based on DevTools.
    {
      'target_name': 'chromedriver2_lib',
      'type': 'static_library',
      'dependencies': [
        'chrome_devtools_lib',
        '../base/base.gyp:base',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../ui/ui.gyp:ui',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        '../third_party/webdriver/atoms.cc',
        '../third_party/webdriver/atoms.h',
        'test/chromedriver/alert_commands.cc',
        'test/chromedriver/alert_commands.h',
        'test/chromedriver/basic_types.cc',
        'test/chromedriver/basic_types.h',
        'test/chromedriver/capabilities.cc',
        'test/chromedriver/capabilities.h',
        'test/chromedriver/chrome_launcher.cc',
        'test/chromedriver/chrome_launcher.h',
        'test/chromedriver/command.h',
        'test/chromedriver/commands.cc',
        'test/chromedriver/commands.h',
        'test/chromedriver/element_commands.cc',
        'test/chromedriver/element_commands.h',
        'test/chromedriver/element_util.cc',
        'test/chromedriver/element_util.h',
        'test/chromedriver/key_converter.cc',
        'test/chromedriver/key_converter.h',
        'test/chromedriver/keycode_text_conversion.h',
        'test/chromedriver/keycode_text_conversion_mac.mm',
        'test/chromedriver/keycode_text_conversion_win.cc',
        'test/chromedriver/keycode_text_conversion_x.cc',
        'test/chromedriver/logging.cc',
        'test/chromedriver/logging.h',
        'test/chromedriver/session.cc',
        'test/chromedriver/session.h',
        'test/chromedriver/session_commands.cc',
        'test/chromedriver/session_commands.h',
        'test/chromedriver/session_thread_map.h',
        'test/chromedriver/util.cc',
        'test/chromedriver/util.h',
        'test/chromedriver/window_commands.cc',
        'test/chromedriver/window_commands.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'chromedriver2_server_lib',
      'type': 'static_library',
      'dependencies': [
        'chromedriver2_lib',
        '../base/base.gyp:base',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/chromedriver/server/http_handler.cc',
        'test/chromedriver/server/http_handler.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'chromedriver2_server',
      'type': 'executable',
      'dependencies': [
        'chromedriver2_server_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/chromedriver/server/chromedriver_server.cc',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      'target_name': 'chromedriver2_unittests',
      'type': 'executable',
      'dependencies': [
        'chromedriver2_lib',
        'chromedriver2_server_lib',
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../testing/gtest.gyp:gtest',
        '../ui/ui.gyp:ui',
      ],
      'sources': [
        'test/chromedriver/capabilities_unittest.cc',
        'test/chromedriver/chrome/chrome_finder_unittest.cc',
        'test/chromedriver/chrome/console_logger_unittest.cc',
        'test/chromedriver/chrome/device_manager_unittest.cc',
        'test/chromedriver/chrome/devtools_client_impl_unittest.cc',
        'test/chromedriver/chrome/devtools_http_client_unittest.cc',
        'test/chromedriver/chrome/dom_tracker_unittest.cc',
        'test/chromedriver/chrome/frame_tracker_unittest.cc',
        'test/chromedriver/chrome/geolocation_override_manager_unittest.cc',
        'test/chromedriver/chrome/javascript_dialog_manager_unittest.cc',
        'test/chromedriver/chrome/navigation_tracker_unittest.cc',
        'test/chromedriver/chrome/performance_logger_unittest.cc',
        'test/chromedriver/chrome/status_unittest.cc',
        'test/chromedriver/chrome/stub_chrome.cc',
        'test/chromedriver/chrome/stub_chrome.h',
        'test/chromedriver/chrome/stub_devtools_client.cc',
        'test/chromedriver/chrome/stub_devtools_client.h',
        'test/chromedriver/chrome/stub_web_view.cc',
        'test/chromedriver/chrome/stub_web_view.h',
        'test/chromedriver/chrome/web_view_impl_unittest.cc',
        'test/chromedriver/chrome_launcher_unittest.cc',
        'test/chromedriver/commands_unittest.cc',
        'test/chromedriver/logging_unittest.cc',
        'test/chromedriver/server/http_handler_unittest.cc',
        'test/chromedriver/session_commands_unittest.cc',
        'test/chromedriver/session_unittest.cc',
        'test/chromedriver/util_unittest.cc',
      ],
      'conditions': [
        # See http://crbug.com/162998#c4 for why this is needed.
        ['OS=="linux" and linux_use_tcmalloc==1', {
          'dependencies': [
            '../base/allocator/allocator.gyp:allocator',
          ],
        }],
      ],
    },
    # ChromeDriver2 tests that aren't run on the main buildbot. Available
    # as an optional test type on trybots.
    {
      'target_name': 'chromedriver2_tests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:run_all_unittests',
        '../net/net.gyp:http_server',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../testing/gtest.gyp:gtest',
        '../url/url.gyp:url_lib',
        'chromedriver2_lib',
      ],
      'include_dirs': [
        '..,'
      ],
      'sources': [
        'test/chromedriver/key_converter_unittest.cc',
        'test/chromedriver/keycode_text_conversion_unittest.cc',
        'test/chromedriver/net/net_util_unittest.cc',
        'test/chromedriver/net/sync_websocket_impl_unittest.cc',
        'test/chromedriver/net/test_http_server.cc',
        'test/chromedriver/net/test_http_server.h',
        'test/chromedriver/net/websocket_unittest.cc',
        'test/chromedriver/test_util.cc',
        'test/chromedriver/test_util.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4267, ],
    },
    {
      # Executable that runs each browser test in a new process.
      'target_name': 'browser_tests',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
      'dependencies': [
        'browser',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common/extensions/api/api.gyp:api',
        'renderer',
        'test_support_common',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../components/components.gyp:autofill_content_risk_proto',
        '../components/components.gyp:autofill_content_test_support',
        '../device/bluetooth/bluetooth.gyp:device_bluetooth_mocks',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_api',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/safe_browsing/safe_browsing.gyp:safe_browsing',
        '../ui/web_dialogs/web_dialogs.gyp:web_dialogs_test_support',
        '../ui/ui.gyp:webui_test_support',
        '../v8/tools/gyp/v8.gyp:v8',
        # Runtime dependencies
        '../ppapi/ppapi_internal.gyp:ppapi_tests',
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        '../apps/app_restore_service_browsertest.cc',
        '../apps/load_and_launch_browsertest.cc',
        '../remoting/test/remote_desktop_browsertest.cc',
        'app/breakpad_mac_stubs.mm',
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'browser/accessibility/accessibility_extension_apitest.cc',
        'browser/accessibility/browser_accessibility_state_browsertest.cc',
        'browser/app_controller_mac_browsertest.mm',
        'browser/autocomplete/autocomplete_browsertest.cc',
        'browser/autofill/autofill_browsertest.cc',
        'browser/autofill/autofill_driver_impl_browsertest.cc',
        'browser/autofill/form_structure_browsertest.cc',
        'browser/autofill/risk/fingerprint_browsertest.cc',
        'browser/browser_encoding_browsertest.cc',
        'browser/browsing_data/browsing_data_database_helper_browsertest.cc',
        'browser/browsing_data/browsing_data_helper_browsertest.h',
        'browser/browsing_data/browsing_data_indexed_db_helper_browsertest.cc',
        'browser/browsing_data/browsing_data_local_storage_helper_browsertest.cc',
        'browser/browsing_data/browsing_data_remover_browsertest.cc',
        'browser/captive_portal/captive_portal_browsertest.cc',
        'browser/chrome_content_browser_client_browsertest.cc',
        'browser/chrome_main_browsertest.cc',
        'browser/chrome_plugin_browsertest.cc',
        'browser/chrome_security_exploit_browsertest.cc',
        'browser/chrome_switches_browsertest.cc',
        'browser/chromeos/accessibility/accessibility_manager_browsertest.cc',
        'browser/chromeos/accessibility/magnification_manager_browsertest.cc',
        'browser/chromeos/app_mode/kiosk_app_manager_browsertest.cc',
        'browser/chromeos/app_mode/kiosk_app_update_service_browsertest.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.cc',
        'browser/chromeos/cros/cros_in_process_browser_test.h',
        'browser/chromeos/drive/drive_integration_service_browsertest.cc',
        'browser/chromeos/drive/test_util.cc',
        'browser/chromeos/drive/test_util.h',
        'browser/chromeos/extensions/echo_private_apitest.cc',
        'browser/chromeos/extensions/file_manager/desktop_notifications_browsertest.cc',
        'browser/chromeos/extensions/file_manager/drive_test_util.cc',
        'browser/chromeos/extensions/file_manager/drive_test_util.h',
        'browser/chromeos/extensions/file_manager/event_router_browsertest.cc',
        'browser/chromeos/extensions/file_manager/external_filesystem_apitest.cc',
        'browser/chromeos/extensions/file_manager/file_browser_handler_api_test.cc',
        'browser/chromeos/extensions/file_manager/file_browser_private_apitest.cc',
        'browser/chromeos/extensions/file_manager/file_manager_browsertest.cc',
        'browser/chromeos/extensions/info_private_apitest.cc',
        'browser/chromeos/extensions/input_method_apitest_chromeos.cc',
        'browser/chromeos/extensions/wallpaper_private_apitest.cc',
        'browser/chromeos/input_method/input_method_engine_ibus_browserttests.cc',
        'browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.cc',
        'browser/chromeos/kiosk_mode/mock_kiosk_mode_settings.h',
        'browser/chromeos/login/crash_restore_browsertest.cc',
        'browser/chromeos/login/enrollment/enrollment_screen_browsertest.cc',
        'browser/chromeos/login/enrollment/mock_enrollment_screen.cc',
        'browser/chromeos/login/enrollment/mock_enrollment_screen.h',
        'browser/chromeos/login/existing_user_controller_browsertest.cc',
        'browser/chromeos/login/kiosk_browsertest.cc',
        'browser/chromeos/login/login_utils_browsertest.cc',
        'browser/chromeos/login/login_manager_test.cc',
        'browser/chromeos/login/login_manager_test.h',
        'browser/chromeos/login/mock_authenticator.cc',
        'browser/chromeos/login/mock_authenticator.h',
        'browser/chromeos/login/session_login_browsertest.cc',
        'browser/chromeos/login/screen_locker_tester.cc',
        'browser/chromeos/login/screen_locker_tester.h',
        'browser/chromeos/login/screens/mock_error_screen.cc',
        'browser/chromeos/login/screens/mock_error_screen.h',
        'browser/chromeos/login/screens/mock_eula_screen.cc',
        'browser/chromeos/login/screens/mock_eula_screen.h',
        'browser/chromeos/login/screens/mock_network_screen.cc',
        'browser/chromeos/login/screens/mock_network_screen.h',
        'browser/chromeos/login/screens/mock_screen_observer.cc',
        'browser/chromeos/login/screens/mock_screen_observer.h',
        'browser/chromeos/login/screens/mock_update_screen.cc',
        'browser/chromeos/login/screens/mock_update_screen.h',
        'browser/chromeos/login/screens/network_screen_browsertest.cc',
        'browser/chromeos/login/screens/update_screen_browsertest.cc',
        'browser/chromeos/login/test_login_utils.cc',
        'browser/chromeos/login/test_login_utils.h',
        'browser/chromeos/login/user_adding_screen_browsertest.cc',
        'browser/chromeos/login/user_image_manager_browsertest.cc',
        'browser/chromeos/login/wizard_controller_browsertest.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.cc',
        'browser/chromeos/login/wizard_in_process_browser_test.h',
        'browser/chromeos/media/media_player_browsertest.cc',
        'browser/chromeos/memory/oom_priority_manager_browsertest.cc',
        'browser/chromeos/policy/device_local_account_browsertest.cc',
        'browser/chromeos/policy/device_policy_cros_browser_test.cc',
        'browser/chromeos/policy/device_policy_cros_browser_test.h',
        'browser/chromeos/policy/device_status_collector_browsertest.cc',
        'browser/chromeos/policy/login_screen_default_policy_browsertest.cc',
        'browser/chromeos/policy/policy_cert_verifier_browsertest.cc',
        'browser/chromeos/policy/power_policy_browsertest.cc',
        'browser/chromeos/policy/variations_service_policy_browsertest.cc',
        'browser/chromeos/power/peripheral_battery_observer_browsertest.cc',
        'browser/chromeos/profiles/profile_helper_browsertest.cc',
        'browser/chromeos/screensaver/screensaver_controller_browsertest.cc',
        'browser/chromeos/system/tray_accessibility_browsertest.cc',
        'browser/chromeos/ui/idle_logout_dialog_view_browsertest.cc',
        'browser/collected_cookies_browsertest.cc',
        'browser/content_settings/content_settings_browsertest.cc',
        'browser/crash_recovery_browsertest.cc',
        'browser/custom_handlers/protocol_handler_registry_browsertest.cc',
        'browser/devtools/devtools_sanity_browsertest.cc',
        'browser/do_not_track_browsertest.cc',
        'browser/download/download_browsertest.cc',
        'browser/download/download_danger_prompt_browsertest.cc',
        'browser/download/save_page_browsertest.cc',
        'browser/drive/fake_drive_service.cc',
        'browser/drive/fake_drive_service.h',
        'browser/errorpage_browsertest.cc',
        'browser/extensions/active_tab_apitest.cc',
        'browser/extensions/activity_log/activity_log_browsertest.cc',
        'browser/extensions/ad_view_browsertest.cc',
        'browser/extensions/alert_apitest.cc',
        'browser/extensions/all_urls_apitest.cc',
        'browser/extensions/api/activity_log_private/activity_log_private_apitest.cc',
        'browser/extensions/api/app_window/app_window_apitest.cc',
        'browser/extensions/api/audio/audio_apitest.cc',
        'browser/extensions/api/autotest_private/autotest_private_apitest.cc',
        'browser/extensions/api/bluetooth/bluetooth_apitest.cc',
        'browser/extensions/api/bookmark_manager_private/bookmark_manager_private_apitest.cc',
        'browser/extensions/api/bookmarks/bookmark_apitest.cc',
        'browser/extensions/api/browsing_data/browsing_data_test.cc',
        'browser/extensions/api/cloud_print_private/cloud_print_private_apitest.cc',
        'browser/extensions/api/command_line_private/command_line_private_apitest.cc',
        'browser/extensions/api/commands/command_service_browsertest.cc',
        'browser/extensions/api/content_settings/content_settings_apitest.cc',
        'browser/extensions/api/context_menus/context_menu_apitest.cc',
        'browser/extensions/api/cookies/cookies_apitest.cc',
        'browser/extensions/api/debugger/debugger_apitest.cc',
        'browser/extensions/api/debugger/debugger_extension_apitest.cc',
        'browser/extensions/api/declarative/declarative_apitest.cc',
        'browser/extensions/api/declarative_content/declarative_content_apitest.cc',
        'browser/extensions/api/developer_private/developer_private_apitest.cc',
        'browser/extensions/api/dial/dial_apitest.cc',
        'browser/extensions/api/dns/dns_apitest.cc',
        'browser/extensions/api/dns/mock_host_resolver_creator.cc',
        'browser/extensions/api/dns/mock_host_resolver_creator.h',
        'browser/extensions/api/downloads/downloads_api_browsertest.cc',
        'browser/extensions/api/extension_action/browser_action_apitest.cc',
        'browser/extensions/api/extension_action/page_action_apitest.cc',
        'browser/extensions/api/extension_action/page_as_browser_action_apitest.cc',
        'browser/extensions/api/extension_action/script_badge_apitest.cc',
        'browser/extensions/api/file_system/file_system_apitest.cc',
        'browser/extensions/api/font_settings/font_settings_apitest.cc',
        'browser/extensions/api/history/history_apitest.cc',
        'browser/extensions/api/i18n/i18n_apitest.cc',
        'browser/extensions/api/identity/experimental_identity_apitest.cc',
        'browser/extensions/api/identity/identity_apitest.cc',
        'browser/extensions/api/idle/idle_apitest.cc',
        'browser/extensions/api/idltest/idltest_apitest.cc',
        'browser/extensions/api/input_ime/input_ime_apitest_chromeos.cc',
        'browser/extensions/api/management/management_api_browsertest.cc',
        'browser/extensions/api/management/management_apitest.cc',
        'browser/extensions/api/management/management_browsertest.cc',
        'browser/extensions/api/media_galleries/media_galleries_apitest.cc',
        'browser/extensions/api/media_galleries_private/media_galleries_eject_apitest.cc',
        'browser/extensions/api/media_galleries_private/media_galleries_private_apitest.cc',
        'browser/extensions/api/media_galleries_private/media_galleries_watch_apitest.cc',
        'browser/extensions/api/messaging/native_messaging_apitest.cc',
        'browser/extensions/api/metrics_private/metrics_apitest.cc',
        'browser/extensions/api/module/module_apitest.cc',
        'browser/extensions/api/music_manager_private/music_manager_private_browsertest.cc',
        'browser/extensions/api/networking_private/networking_private_apitest.cc',
        'browser/extensions/api/notifications/notifications_apitest.cc',
        'browser/extensions/api/omnibox/omnibox_api_browsertest.cc',
        'browser/extensions/api/page_capture/page_capture_apitest.cc',
        'browser/extensions/api/permissions/permissions_apitest.cc',
        'browser/extensions/api/preference/preference_apitest.cc',
        'browser/extensions/api/processes/processes_apitest.cc',
        'browser/extensions/api/proxy/proxy_apitest.cc',
        'browser/extensions/api/push_messaging/push_messaging_apitest.cc',
        'browser/extensions/api/push_messaging/push_messaging_canary_test.cc',
        'browser/extensions/api/push_messaging/sync_setup_helper.cc',
        'browser/extensions/api/rtc_private/rtc_private_apitest.cc',
        'browser/extensions/api/runtime/runtime_apitest.cc',
        'browser/extensions/api/serial/serial_apitest.cc',
        'browser/extensions/api/session_restore/session_restore_apitest.cc',
        'browser/extensions/api/socket/socket_apitest.cc',
        'browser/extensions/api/storage/settings_apitest.cc',
        'browser/extensions/api/streams_private/streams_private_apitest.cc',
        'browser/extensions/api/sync_file_system/sync_file_system_apitest.cc',
        'browser/extensions/api/system_indicator/system_indicator_apitest.cc',
        'browser/extensions/api/system_cpu/system_cpu_apitest.cc',
        'browser/extensions/api/system_display/system_display_apitest.cc',
        'browser/extensions/api/system_memory/system_memory_apitest.cc',
        'browser/extensions/api/system_private/system_private_apitest.cc',
        'browser/extensions/api/system_storage/storage_api_test_util.cc',
        'browser/extensions/api/system_storage/storage_api_test_util.h',
        'browser/extensions/api/system_storage/system_storage_apitest.cc',
        'browser/extensions/api/system_storage/system_storage_eject_apitest.cc',
        'browser/extensions/api/tab_capture/tab_capture_apitest.cc',
        'browser/extensions/api/tabs/tabs_test.cc',
        'browser/extensions/api/terminal/terminal_private_apitest.cc',
        'browser/extensions/api/test/apitest_apitest.cc',
        'browser/extensions/api/top_sites/top_sites_apitest.cc',
        'browser/extensions/api/usb/usb_apitest.cc',
        'browser/extensions/api/usb/usb_manual_apitest.cc',
        'browser/extensions/api/web_navigation/web_navigation_apitest.cc',
        'browser/extensions/api/web_request/web_request_apitest.cc',
        'browser/extensions/api/webstore_private/webstore_private_apitest.cc',
        'browser/extensions/app_background_page_apitest.cc',
        'browser/extensions/app_process_apitest.cc',
        'browser/extensions/background_app_browsertest.cc',
        'browser/extensions/background_page_apitest.cc',
        'browser/extensions/background_scripts_apitest.cc',
        'browser/extensions/chrome_app_api_browsertest.cc',
        'browser/extensions/content_script_apitest.cc',
        'browser/extensions/content_security_policy_apitest.cc',
        'browser/extensions/convert_web_app_browsertest.cc',
        'browser/extensions/crazy_extension_browsertest.cc',
        'browser/extensions/cross_origin_xhr_apitest.cc',
        'browser/extensions/crx_installer_browsertest.cc',
        'browser/extensions/docs/examples/apps/calculator_browsertest.cc',
        'browser/extensions/events_apitest.cc',
        'browser/extensions/execute_script_apitest.cc',
        'browser/extensions/extension_apitest.cc',
        'browser/extensions/extension_apitest.h',
        'browser/extensions/extension_bindings_apitest.cc',
        'browser/extensions/extension_blacklist_browsertest.cc',
        'browser/extensions/extension_browsertest.cc',
        'browser/extensions/extension_browsertest.h',
        'browser/extensions/extension_context_menu_browsertest.cc',
        'browser/extensions/extension_disabled_ui_browsertest.cc',
        'browser/extensions/extension_dom_clipboard_apitest.cc',
        'browser/extensions/extension_fileapi_apitest.cc',
        'browser/extensions/extension_functional_browsertest.cc',
        'browser/extensions/extension_function_test_utils.cc',
        'browser/extensions/extension_function_test_utils.h',
        'browser/extensions/extension_geolocation_apitest.cc',
        'browser/extensions/extension_get_views_apitest.cc',
        'browser/extensions/extension_icon_source_apitest.cc',
        'browser/extensions/extension_incognito_apitest.cc',
        'browser/extensions/extension_install_ui_browsertest.cc',
        'browser/extensions/extension_javascript_url_apitest.cc',
        'browser/extensions/extension_loading_browsertest.cc',
        'browser/extensions/extension_messages_apitest.cc',
        'browser/extensions/extension_override_apitest.cc',
        'browser/extensions/extension_resource_request_policy_apitest.cc',
        'browser/extensions/extension_startup_browsertest.cc',
        'browser/extensions/extension_storage_apitest.cc',
        'browser/extensions/extension_tabs_apitest.cc',
        'browser/extensions/extension_test_message_listener.cc',
        'browser/extensions/extension_test_message_listener.h',
        'browser/extensions/extension_toolbar_model_browsertest.cc',
        'browser/extensions/extension_url_rewrite_browsertest.cc',
        'browser/extensions/extension_websocket_apitest.cc',
        'browser/extensions/gpu_browsertest.cc',
        'browser/extensions/isolated_app_browsertest.cc',
        'browser/extensions/lazy_background_page_apitest.cc',
        'browser/extensions/lazy_background_page_test_util.h',
        'browser/extensions/mutation_observers_apitest.cc',
        'browser/extensions/options_page_apitest.cc',
        'browser/extensions/page_action_browsertest.cc',
        'browser/extensions/platform_app_browsertest.cc',
        'browser/extensions/platform_app_browsertest_util.cc',
        'browser/extensions/platform_app_browsertest_util.h',
        'browser/extensions/plugin_apitest.cc',
        'browser/extensions/process_management_browsertest.cc',
        'browser/extensions/requirements_checker_browsertest.cc',
        'browser/extensions/sandboxed_pages_apitest.cc',
        'browser/extensions/shared_module_apitest.cc',
        'browser/extensions/startup_helper_browsertest.cc',
        'browser/extensions/stubs_apitest.cc',
        'browser/extensions/subscribe_page_action_browsertest.cc',
        'browser/extensions/test_extension_dir.cc',
        'browser/extensions/test_extension_dir.h',
        'browser/extensions/window_controls_browsertest.cc',
        'browser/extensions/web_contents_browsertest.cc',
        'browser/extensions/web_view_browsertest.cc',
        'browser/extensions/webstore_startup_installer_browsertest.cc',
        'browser/extensions/window_open_apitest.cc',
        'browser/external_extension_browsertest.cc',
        'browser/fast_shutdown_browsertest.cc',
        'browser/first_run/first_run_browsertest.cc',
        'browser/first_run/try_chrome_dialog_view_browsertest.cc',
        'browser/geolocation/access_token_store_browsertest.cc',
        'browser/geolocation/geolocation_browsertest.cc',
        'browser/history/history_browsertest.cc',
        'browser/history/multipart_browsertest.cc',
        'browser/history/redirect_browsertest.cc',
        'browser/iframe_browsertest.cc',
        'browser/importer/firefox_importer_browsertest.cc',
        'browser/importer/ie_importer_browsertest_win.cc',
        'browser/importer/importer_unittest_utils.cc',
        'browser/importer/importer_unittest_utils.h',
        'browser/infobars/infobar_extension_apitest.cc',
        'browser/infobars/infobars_browsertest.cc',
        'browser/loadtimes_extension_bindings_browsertest.cc',
        'browser/locale_tests_browsertest.cc',
        'browser/logging_chrome_browsertest.cc',
        'browser/managed_mode/managed_mode_browsertest.cc',
        'browser/managed_mode/managed_mode_resource_throttle_browsertest.cc',
        'browser/managed_mode/managed_user_service_browsertest.cc',
        'browser/media/chrome_media_stream_infobar_browsertest.cc',
        'browser/media/chrome_webrtc_browsertest.cc',
        'browser/media/chrome_webrtc_audio_quality_browsertest.cc',
        'browser/media/chrome_webrtc_video_quality_browsertest.cc',
        'browser/media/webrtc_browsertest_base.cc',
        'browser/media/webrtc_browsertest_base.h',
        'browser/media/webrtc_browsertest_common.cc',
        'browser/media_galleries/fileapi/itunes_finder_win_browsertest.cc',
        'browser/media_galleries/fileapi/media_file_validator_browsertest.cc',
        'browser/media_galleries/media_galleries_dialog_controller_mock.cc',
        'browser/media_galleries/media_galleries_dialog_controller_mock.h',
        'browser/metrics/metrics_service_browsertest.cc',
        'browser/net/cookie_policy_browsertest.cc',
        'browser/net/dns_probe_browsertest.cc',
        'browser/net/ftp_browsertest.cc',
        'browser/net/load_timing_browsertest.cc',
        'browser/net/predictor_browsertest.cc',
        'browser/net/proxy_browsertest.cc',
        'browser/net/websocket_browsertest.cc',
        'browser/notifications/message_center_notifications_browsertest.cc',
        'browser/notifications/sync_notifier/chrome_notifier_delegate_browsertest.cc',
        'browser/notifications/sync_notifier/notification_bitmap_fetcher_browsertest.cc',
        'browser/notifications/sync_notifier/sync_notifier_test_utils.cc',
        'browser/notifications/sync_notifier/sync_notifier_test_utils.h',
        'browser/page_cycler/page_cycler_browsertest.cc',
        'browser/password_manager/password_manager_browsertest.cc',
        'browser/performance_monitor/performance_monitor_browsertest.cc',
        'browser/policy/cloud/cloud_policy_browsertest.cc',
        'browser/policy/cloud/cloud_policy_manager_browsertest.cc',
        'browser/policy/cloud/component_cloud_policy_browsertest.cc',
        'browser/policy/cloud/device_management_service_browsertest.cc',
        'browser/policy/cloud/test_request_interceptor.cc',
        'browser/policy/cloud/test_request_interceptor.h',
        'browser/policy/mock_policy_service.cc',
        'browser/policy/mock_policy_service.h',
        'browser/policy/policy_browsertest.cc',
        'browser/policy/policy_prefs_browsertest.cc',
        'browser/policy/test_utils.cc',
        'browser/policy/test_utils.h',
        'browser/prefs/pref_functional_browsertest.cc',
        'browser/prefs/pref_service_browsertest.cc',
        'browser/prefs/synced_pref_change_registrar_browsertest.cc',
        'browser/prerender/prefetch_browsertest.cc',
        'browser/prerender/prerender_browsertest.cc',
        'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
        'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
        'browser/printing/print_preview_dialog_controller_browsertest.cc',
        'browser/printing/printing_layout_browsertest.cc',
        'browser/process_singleton_browsertest.cc',
        'browser/profiles/avatar_menu_model_browsertest.cc',
        'browser/profiles/profile_browsertest.cc',
        'browser/profiles/profile_manager_browsertest.cc',
        'browser/profile_resetter/profile_resetter_browsertest.cc',
        'browser/referrer_policy_browsertest.cc',
        'browser/renderer_host/render_process_host_chrome_browsertest.cc',
        'browser/renderer_host/web_cache_manager_browsertest.cc',
        'browser/repost_form_warning_browsertest.cc',
        'browser/rlz/rlz_extension_apitest.cc',
        'browser/safe_browsing/local_safebrowsing_test_server.cc',
        'browser/safe_browsing/safe_browsing_blocking_page_test.cc',
        'browser/safe_browsing/safe_browsing_service_browsertest.cc',
        'browser/safe_browsing/safe_browsing_test.cc',
        'browser/service/service_process_control_browsertest.cc',
        'browser/sessions/better_session_restore_browsertest.cc',
        'browser/sessions/persistent_tab_restore_service_browsertest.cc',
        'browser/sessions/session_restore_browsertest.cc',
        'browser/sessions/tab_restore_browsertest.cc',
        'browser/signin/signin_browsertest.cc',
        'browser/speech/extension_api/tts_extension_apitest.cc',
        'browser/speech/speech_recognition_bubble_browsertest.cc',
        'browser/spellchecker/spellcheck_service_browsertest.cc',
        'browser/ssl/ssl_browser_tests.cc',
        'browser/ssl/ssl_client_certificate_selector_test.cc',
        'browser/ssl/ssl_client_certificate_selector_test.h',
        'browser/sync_file_system/mock_local_change_processor.cc',
        'browser/sync_file_system/mock_local_change_processor.h',
        'browser/sync_file_system/mock_remote_file_sync_service.cc',
        'browser/sync_file_system/mock_remote_file_sync_service.h',
        'browser/tab_contents/render_view_context_menu_browsertest.cc',
        'browser/tab_contents/render_view_context_menu_browsertest_util.cc',
        'browser/tab_contents/render_view_context_menu_browsertest_util.h',
        'browser/tab_contents/render_view_context_menu_test_util.cc',
        'browser/tab_contents/render_view_context_menu_test_util.h',
        'browser/tab_contents/spellchecker_submenu_observer_browsertest.cc',
        'browser/tab_contents/spelling_menu_observer_browsertest.cc',
        'browser/tab_contents/view_source_browsertest.cc',
        'browser/task_manager/task_manager_browsertest.cc',
        'browser/task_manager/task_manager_browsertest_util.cc',
        'browser/task_manager/task_manager_browsertest_util.h',
        'browser/task_manager/task_manager_notification_browsertest.cc',
        'browser/themes/theme_service_browsertest.cc',
        'browser/translate/translate_browsertest.cc',
        'browser/translate/translate_manager_browsertest.cc',
        'browser/ui/app_list/app_list_controller_browsertest.cc',
        'browser/ui/app_list/app_list_service_mac_browsertest.mm',
        'browser/ui/app_list/search/webstore_provider_browsertest.cc',
        'browser/ui/ash/caps_lock_delegate_chromeos_browsertest.cc',
        'browser/ui/ash/chrome_shell_delegate_browsertest.cc',
        'browser/ui/ash/launcher/chrome_launcher_controller_browsertest.cc',
        'browser/ui/ash/launcher/chrome_launcher_controller_per_app_browsertest.cc',
        'browser/ui/ash/launcher/launcher_favicon_loader_browsertest.cc',
        'browser/ui/ash/shelf_browsertest.cc',
        'browser/ui/ash/volume_controller_browsertest_chromeos.cc',
        'browser/ui/autofill/autofill_dialog_controller_browsertest.cc',
        'browser/ui/autofill/autofill_popup_controller_browsertest.cc',
        'browser/ui/blocked_content/popup_blocker_browsertest.cc',
        'browser/ui/bookmarks/bookmark_browsertest.cc',
        'browser/ui/browser_browsertest.cc',
        'browser/ui/browser_close_browsertest.cc',
        'browser/ui/browser_command_controller_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.cc',
        'browser/ui/browser_navigator_browsertest.h',
        'browser/ui/browser_navigator_browsertest_chromeos.cc',
        'browser/ui/cocoa/applescript/browsercrapplication+applescript_test.mm',
        'browser/ui/cocoa/applescript/window_applescript_test.mm',
        'browser/ui/cocoa//autofill/autofill_dialog_cocoa_browsertest.mm',
        'browser/ui/cocoa/browser_window_cocoa_browsertest.mm',
        'browser/ui/cocoa/browser_window_controller_browsertest.mm',
        'browser/ui/cocoa/certificate_viewer_mac_browsertest.mm',
        'browser/ui/cocoa/constrained_window/constrained_window_mac_browsertest.mm',
        'browser/ui/cocoa/content_settings/collected_cookies_mac_browsertest.mm',
        'browser/ui/cocoa/content_settings/content_setting_bubble_cocoa_unittest.mm',
        'browser/ui/cocoa/dev_tools_controller_browsertest.mm',
        'browser/ui/cocoa/extensions/extension_action_context_menu_browsertest.mm',
        'browser/ui/cocoa/extensions/extension_install_dialog_controller_browsertest.mm',
        'browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h',
        'browser/ui/cocoa/extensions/extension_install_prompt_test_utils.mm',
        'browser/ui/cocoa/extensions/media_galleries_dialog_cocoa_browsertest.mm',
        'browser/ui/cocoa/find_bar/find_bar_browsertest.mm',
        'browser/ui/cocoa/location_bar/zoom_decoration_browsertest.mm',
        'browser/ui/cocoa/omnibox/omnibox_view_mac_browsertest.mm',
        'browser/ui/cocoa/one_click_signin_bubble_controller_browsertest.mm',
        'browser/ui/cocoa/one_click_signin_dialog_controller_browsertest.mm',
        'browser/ui/cocoa/profile_signin_confirmation_view_controller_browsertest.mm',
        'browser/ui/cocoa/ssl_client_certificate_selector_cocoa_browsertest.mm',
        'browser/ui/cocoa/view_id_util_browsertest.mm',
        'browser/ui/find_bar/find_bar_host_browsertest.cc',
        'browser/ui/fullscreen/fullscreen_controller_browsertest.cc',
        'browser/ui/global_error/global_error_service_browsertest.cc',
        'browser/ui/gtk/bubble/bubble_gtk_browsertest.cc',
        'browser/ui/gtk/confirm_bubble_gtk_browsertest.cc',
        'browser/ui/gtk/location_bar_view_gtk_browsertest.cc',
        'browser/ui/gtk/one_click_signin_bubble_gtk_browsertest.cc',
        'browser/ui/gtk/view_id_util_browsertest.cc',
        'browser/ui/login/login_prompt_browsertest.cc',
        'browser/ui/panels/panel_extension_browsertest.cc',
        'browser/ui/prefs/prefs_tab_helper_browsertest.cc',
        'browser/ui/startup/startup_browser_creator_browsertest.cc',
        'browser/ui/sync/one_click_signin_bubble_links_delegate_browsertest.cc',
        'browser/ui/sync/profile_signin_confirmation_helper_browsertest.cc',
        'browser/ui/tab_modal_confirm_dialog_browsertest.cc',
        'browser/ui/tab_modal_confirm_dialog_browsertest.h',
        'browser/ui/toolbar/test_toolbar_model.cc',
        'browser/ui/toolbar/test_toolbar_model.h',
        'browser/ui/views/app_list/app_list_controller_win_browsertest.cc',
        'browser/ui/views/avatar_menu_button_browsertest.cc',
        'browser/ui/views/browser_actions_container_browsertest.cc',
        'browser/ui/views/frame/app_non_client_frame_view_ash_browsertest.cc',
        'browser/ui/views/frame/browser_non_client_frame_view_ash_browsertest.cc',
        'browser/ui/views/frame/immersive_mode_controller_ash_browsertest.cc',
        'browser/ui/views/location_bar/zoom_bubble_view_browsertest.cc',
        'browser/ui/views/select_file_dialog_extension_browsertest.cc',
        'browser/ui/views/toolbar_view_browsertest.cc',
        'browser/ui/views/web_dialog_view_browsertest.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.cc',
        'browser/ui/webui/bidi_checker_web_ui_test.h',
        'browser/ui/webui/bookmarks_ui_browsertest.cc',
        'browser/ui/webui/chrome_url_data_manager_browsertest.cc',
        'browser/ui/webui/constrained_web_dialog_ui_browsertest.cc',
        'browser/ui/webui/downloads_dom_handler_browsertest.cc',
        'browser/ui/webui/extensions/chromeos/kiosk_apps_browsertest.js',
        'browser/ui/webui/extensions/extension_settings_browsertest.js',
        'browser/ui/webui/help/help_browsertest.js',
        'browser/ui/webui/identity_internals_ui_browsertest.cc',
        'browser/ui/webui/identity_internals_ui_browsertest.h',
        'browser/ui/webui/identity_internals_ui_browsertest.js',
        'browser/ui/webui/inspect_ui_browsertest.cc',
        'browser/ui/webui/net_internals/net_internals_ui_browsertest.cc',
        'browser/ui/webui/net_internals/net_internals_ui_browsertest.h',
        'browser/ui/webui/ntp/most_visited_browsertest.cc',
        'browser/ui/webui/ntp/new_tab_page_sync_handler_browsertest.cc',
        'browser/ui/webui/ntp/new_tab_ui_browsertest.cc',
        'browser/ui/webui/options/autofill_options_browsertest.js',
        'browser/ui/webui/options/browser_options_browsertest.js',
        'browser/ui/webui/options/certificate_manager_browsertest.cc',
        'browser/ui/webui/options/certificate_manager_browsertest.js',
        'browser/ui/webui/options/chromeos/accounts_options_browsertest.js',
        'browser/ui/webui/options/chromeos/bluetooth_options_browsertest.js',
        'browser/ui/webui/options/chromeos/guest_mode_options_ui_browsertest.cc',
        'browser/ui/webui/options/content_options_browsertest.js',
        'browser/ui/webui/options/content_settings_exception_area_browsertest.js',
        'browser/ui/webui/options/cookies_view_browsertest.js',
        'browser/ui/webui/options/edit_dictionary_browsertest.js',
        'browser/ui/webui/options/font_settings_browsertest.js',
        'browser/ui/webui/options/language_options_browsertest.js',
        'browser/ui/webui/options/language_options_dictionary_download_browsertest.js',
        'browser/ui/webui/options/manage_profile_browsertest.js',
        'browser/ui/webui/options/options_browsertest.cc',
        'browser/ui/webui/options/options_browsertest.js',
        'browser/ui/webui/options/options_ui_browsertest.cc',
        'browser/ui/webui/options/options_ui_browsertest.h',
        'browser/ui/webui/options/password_manager_browsertest.js',
        'browser/ui/webui/options/profile_settings_reset_browsertest.js',
        'browser/ui/webui/options/preferences_browsertest.cc',
        'browser/ui/webui/options/preferences_browsertest.h',
        'browser/ui/webui/options/search_engine_manager_browsertest.js',
        'browser/ui/webui/options/settings_app_browsertest.js',
        'browser/ui/webui/options/settings_format_browsertest.js',
        'browser/ui/webui/policy_ui_browsertest.cc',
        'browser/ui/webui/print_preview/print_preview_ui_browsertest.cc',
        'browser/ui/webui/sync_setup_browsertest.js',
        'browser/ui/webui/web_ui_test_handler.cc',
        'browser/ui/webui/web_ui_test_handler.h',
        'browser/user_data_dir_extractor_win_browsertest.cc',
        'browser/unload_browsertest.cc',
        'common/mac/mock_launchd.cc',
        'common/mac/mock_launchd.h',
        'common/time_format_browsertest.cc',
        'renderer/autofill/autofill_renderer_browsertest.cc',
        'renderer/autofill/form_autocomplete_browsertest.cc',
        'renderer/autofill/form_autofill_browsertest.cc',
        'renderer/autofill/page_click_tracker_browsertest.cc',
        'renderer/autofill/password_autofill_agent_browsertest.cc',
        'renderer/autofill/password_generation_manager_browsertest.cc',
        'renderer/content_settings_observer_browsertest.cc',
        'renderer/printing/print_web_view_helper_browsertest.cc',
        'renderer/safe_browsing/malware_dom_details_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_browsertest.cc',
        'renderer/safe_browsing/phishing_classifier_delegate_browsertest.cc',
        'renderer/safe_browsing/phishing_dom_feature_extractor_browsertest.cc',
        'renderer/translate/translate_helper_browsertest.cc',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/chrome_test_launcher.cc',
        'test/base/web_ui_browsertest.cc',
        'test/base/web_ui_browsertest.h',
        'test/data/chromeos/oobe_webui_browsertest.js',
        'test/base/in_process_browser_test_browsertest.cc',
        'test/base/tracing_browsertest.cc',
        'test/base/test_chrome_web_ui_controller_factory.cc',
        'test/base/test_chrome_web_ui_controller_factory.h',
        'test/base/test_chrome_web_ui_controller_factory_browsertest.cc',
        'test/data/webui/accessibility_audit_browsertest.js',
        'test/data/webui/assertions.js',
        'test/data/webui/async_gen.cc',
        'test/data/webui/async_gen.h',
        'test/data/webui/async_gen.js',
        'test/data/webui/certificate_viewer_dialog_test.js',
        'test/data/webui/certificate_viewer_ui_test-inl.h',
        'test/data/webui/chrome_send_browsertest.cc',
        'test/data/webui/chrome_send_browsertest.h',
        'test/data/webui/chrome_send_browsertest.js',
        'test/data/webui/history_browsertest.js',
        'test/data/webui/history_ui_browsertest.cc',
        'test/data/webui/history_ui_browsertest.h',
        'test/data/webui/mock4js_browsertest.js',
        'test/data/webui/net_internals/bandwidth_view.js',
        'test/data/webui/net_internals/dns_view.js',
        'test/data/webui/net_internals/events_view.js',
        'test/data/webui/net_internals/hsts_view.js',
        'test/data/webui/net_internals/http_pipeline_view.js',
        'test/data/webui/net_internals/log_util.js',
        'test/data/webui/net_internals/log_view_painter.js',
        'test/data/webui/net_internals/main.js',
        'test/data/webui/net_internals/net_internals_test.js',
        'test/data/webui/net_internals/prerender_view.js',
        'test/data/webui/net_internals/test_view.js',
        'test/data/webui/net_internals/timeline_view.js',
        'test/data/webui/ntp4.js',
        'test/data/webui/ntp4_browsertest.cc',
        'test/data/webui/ntp4_browsertest.h',
        'test/data/webui/print_preview.cc',
        'test/data/webui/print_preview.h',
        'test/data/webui/print_preview.js',
        'test/data/webui/sandboxstatus_browsertest.js',
        'test/data/webui/webui_resource_browsertest.cc',
        'test/gpu/gpu_feature_browsertest.cc',
        'test/gpu/webgl_infobar_browsertest.cc',
        'test/ppapi/ppapi_browsertest.cc',
        'test/security_tests/sandbox_browsertest_linux.cc',
        'test/security_tests/sandbox_browsertest_win.cc',
        # TODO(craig): Rename this and run from base_unittests when the test
        # is safe to run there. See http://crbug.com/78722 for details.
        '../base/files/file_path_watcher_browsertest.cc',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(accessibility_audit_js)',
            '<(test_api_js)',
            '<(js2gtest)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['enable_one_click_signin==0', {
          'sources!': [
            'browser/ui/gtk/one_click_signin_bubble_gtk_browsertest.cc',
            'browser/ui/sync/one_click_signin_bubble_links_delegate_browsertest.cc',
          ]
        }],
        ['disable_nacl==0', {
          'sources':[
            'browser/extensions/extension_nacl_browsertest.cc',
            'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
          ],
          'dependencies': [
            # Runtime dependency.
            '../ppapi/native_client/src/trusted/plugin/plugin.gyp:ppGoogleNaClPluginChrome',
          ],
          'conditions': [
            ['disable_nacl_untrusted==0', {
              'sources': [
                'test/nacl/nacl_browsertest.cc',
                'test/nacl/nacl_browsertest_uma.cc',
                'test/nacl/nacl_browsertest_util.cc',
                'test/nacl/nacl_browsertest_util.h',
              ],
              'dependencies': [
                'test/data/extensions/api_test/api_test_data.gyp:socket_ppapi',
                'test/data/nacl/nacl_test_data.gyp:*',
                '../ppapi/native_client/native_client.gyp:nacl_irt',
                '../ppapi/ppapi_untrusted.gyp:ppapi_nacl_tests',
              ],
            }],
            ['OS=="win" or OS=="linux"', {
              'sources': [
                'browser/nacl_host/test/nacl_gdb_browsertest.cc',
              ],
              'dependencies': [
                'browser/nacl_host/test/mock_nacl_gdb.gyp:mock_nacl_gdb',
              ],
            }],
            ['OS=="win"', {
              # TODO(halyavin) NaCl on Windows can't open debug stub socket
              # in browser process as needed by this test.
              # See http://crbug.com/157312.
              'sources!': [
                'browser/nacl_host/test/gdb_debug_stub_browsertest.cc',
              ],
              'dependencies': [
                'chrome.gyp:chrome_nacl_win64',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../native_client/src/trusted/service_runtime/linux/nacl_bootstrap.gyp:nacl_helper_bootstrap',
                'nacl_helper',
              ],
            }],
            ['chromeos==0', {
              'sources!': [
                'test/data/chromeos/oobe_webui_browsertest.js',
              ],
            }],
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:ash_test_support',
          ],
        }],
        ['chromeos==0', {
          'sources/': [
            ['exclude', '^browser/chromeos'],
            ['exclude', '^browser/ui/webui/options/chromeos/'],
          ],
          'sources!': [
            'browser/extensions/api/rtc_private/rtc_private_apitest.cc',
            'browser/extensions/api/terminal/terminal_private_apitest.cc',
            'browser/ui/ash/caps_lock_delegate_chromeos_browsertest.cc',
            'test/data/webui/certificate_viewer_dialog_test.js',
            'test/data/webui/certificate_viewer_ui_test-inl.h',
          ],
        }, { # chromeos==1
          'sources!': [
            '../apps/load_and_launch_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
            'browser/printing/cloud_print/test/cloud_print_proxy_process_browsertest.cc',
            'browser/service/service_process_control_browsertest.cc',
            'browser/signin/signin_browsertest.cc',
            # chromeos does not use cross-platform panels
            'browser/ui/panels/panel_extension_browsertest.cc',
          ],
          'dependencies': [
            '../dbus/dbus.gyp:dbus_test_support',
            '../build/linux/system.gyp:dbus',
          ],
        }],
        ['file_manager_extension==0', {
          'sources!': [
            'browser/ui/views/select_file_dialog_extension_browsertest.cc',
          ],
        }],
        ['configuration_policy==0', {
          'sources/': [
            ['exclude', '^browser/policy/'],
          ],
          'sources!': [
            'browser/ui/webui/policy_ui_browsertest.cc',
            'browser/ui/webui/options/preferences_browsertest.cc',
          ],
        }],
        ['input_speech==0', {
          'sources/': [
            ['exclude', '^browser/speech/'],
            ['exclude', '^../content/browser/speech/'],
          ],
        }],
        ['safe_browsing==1', {
          'defines': [
            'FULL_SAFE_BROWSING',
          ],
        }],
        # TODO(sgurun) enable tests.
        ['safe_browsing==2', {
          'sources/': [
            ['exclude', '^browser/safe_browsing/'],
            ['exclude', '^renderer/safe_browsing/'],
          ],
        }],
        ['safe_browsing==0', {
          'sources/': [
            ['exclude', '^browser/safe_browsing/'],
            ['exclude', '^renderer/safe_browsing/'],
          ],
        }],
        ['enable_captive_portal_detection!=1', {
          'sources/': [
            ['exclude', '^browser/captive_portal/'],
          ],
        }],
        ['internal_pdf', {
          'dependencies': [
            '../pdf/pdf.gyp:pdf',
          ],
          'sources': [
            'browser/ui/pdf/pdf_browsertest.cc',
          ],
        }],
        ['OS!="linux" or toolkit_views==1', {
          'sources!': [
            'browser/ui/gtk/view_id_util_browsertest.cc',
          ],
        }],
        ['enable_rlz==0', {
          'sources!': [
            'browser/rlz/rlz_extension_apitest.cc',
          ],
        }],
        ['enable_webrtc==0', {
          'sources!': [
            'browser/media/chrome_webrtc_browsertest.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'app_host',
            'browser_tests_exe_pdb_workaround',
            'chrome_version_resources',
            'security_tests',  # run time dependency
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'sources!': [
            # use_aura currently sets use_ash on Windows. So take these tests out
            # for win aura builds.
            # TODO: enable these for win_ash browser tests.
            'browser/chromeos/system/tray_accessibility_browsertest.cc',
            'browser/ui/ash/caps_lock_delegate_chromeos_browsertest.cc',
            'browser/ui/ash/chrome_shell_delegate_browsertest.cc',
            'browser/ui/ash/launcher/chrome_launcher_controller_per_app_browsertest.cc',
            'browser/ui/ash/launcher/chrome_launcher_controller_browsertest.cc',
            'browser/ui/ash/launcher/launcher_favicon_loader_browsertest.cc',
            'browser/ui/ash/shelf_browsertest.cc',
            'browser/ui/views/frame/app_non_client_frame_view_ash_browsertest.cc',
            'browser/ui/views/frame/browser_non_client_frame_view_ash_browsertest.cc',
            'browser/ui/views/frame/immersive_mode_controller_ash_browsertest.cc',
          ],
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
            # TODO(port): http://crbug.com/45770
            'browser/printing/printing_layout_browsertest.cc',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../third_party/GTM',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome',
            '../components/components.gyp:breakpad_stubs',
          ],
          'sources': [
            'browser/renderer_host/chrome_render_widget_host_view_mac_delegate_browsertest.cc',
            'browser/spellchecker/spellcheck_message_filter_mac_browsertest.cc',
          ],
          'sources!': [
            # TODO(groby): This test depends on hunspell and we cannot run it on
            # Mac, which does not use hunspell by default.
            'browser/spellchecker/spellcheck_service_browsertest.cc',
            # TODO(rouslan): This test depends on the custom dictionary UI,
            # which is disabled on Mac.
            'browser/ui/webui/options/edit_dictionary_browsertest.js',
            # TODO(rouslan): This test depends on hunspell and we cannot run it
            # on Mac, which does use hunspell by default.
            'browser/ui/webui/options/language_options_dictionary_download_browsertest.js',
            # ProcessSingletonMac doesn't do anything.
            'browser/process_singleton_browsertest.cc',
            # This test depends on GetCommandLineForRelaunch, which is not
            # available on Mac.
            'browser/printing/cloud_print/test/cloud_print_policy_browsertest.cc',
          ],
        }],
        ['OS=="mac" or OS=="win"', {
          'sources': [
            'browser/media_galleries/fileapi/itunes_data_provider_browsertest.cc',
          ],
        }],
        ['os_posix == 0 or chromeos == 1', {
          'sources!': [
            'common/time_format_browsertest.cc',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
          'sources!': [
            # TODO(estade): port to views.
            'browser/ui/webui/constrained_web_dialog_ui_browsertest.cc',
          ],
        }, { # else: toolkit_views == 0
          'sources/': [
            ['exclude', '^../ui/views/'],
            ['exclude', '^browser/ui/views/'],
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          # npapi test plugin doesn't build on android or ios
          'dependencies': [
            # build time dependency.
            '../content/content.gyp:copy_npapi_test_plugin',
            '../v8/tools/gyp/v8.gyp:v8_shell#host',
          ],
        }],
        ['enable_app_list==0', {
          'sources/': [
            ['exclude', '^browser/ui/app_list/'],
          ],
        }],
        ['enable_plugins==1', {
          'dependencies': [
            # Runtime dependency.
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
            '../media/media.gyp:clearkeycdmadapter',
          ],
        }],
        ['enable_printing!=1', {
          'sources/': [
            ['exclude', '^browser/extensions/api/cloud_print_private/cloud_print_private_apitest.cc'],
            ['exclude', '^browser/printing/cloud_print/test/.*'],
            ['exclude', '^browser/printing/print_dialog_cloud_interative_uitest.cc'],
            ['exclude', '^browser/printing/printing_layout_browsertest.cc'],
            ['exclude', '^browser/printing/print_preview_dialog_controller_browsertest.cc'],
            ['exclude', '^browser/ui/webui/print_preview/print_preview_ui_browsertest.cc'],
            ['exclude', '^renderer/printing/print_web_view_helper_browsertest.cc'],
            ['exclude', '^test/data/webui/print_preview.cc'],
            ['exclude', '^test/data/webui/print_preview.h'],
            ['exclude', '^test/data/webui/print_preview.js'],
          ],
        }],
      ],  # conditions
    },  # target browser_tests
    {
      # Executable that runs each perf browser test in a new process.
      'target_name': 'performance_browser_tests',
      'type': 'executable',
      'msvs_cygwin_shell': 0,
      'msvs_cygwin_dirs': ['<(DEPTH)/third_party/cygwin'],
      'dependencies': [
        'browser',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'renderer',
        'test_support_common',
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/cld/cld.gyp:cld',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../v8/tools/gyp/v8.gyp:v8',
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        'app/breakpad_mac_stubs.mm',
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'test/base/chrome_render_view_test.cc',
        'test/base/chrome_render_view_test.h',
        'test/base/chrome_test_launcher.cc',
        'test/perf/browser_perf_test.cc',
        'test/perf/browser_perf_test.h',
        'test/perf/rendering/throughput_tests.cc',
      ],
      'rules': [
        {
          'rule_name': 'js2webui',
          'extension': 'js',
          'msvs_external_rule': 1,
          'inputs': [
            '<(gypv8sh)',
            '<(PRODUCT_DIR)/v8_shell<(EXECUTABLE_SUFFIX)',
            '<(mock_js)',
            '<(accessibility_audit_js)',
            '<(test_api_js)',
            '<(js2gtest)',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT)-gen.cc',
            '<(PRODUCT_DIR)/test_data/chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
          ],
          'process_outputs_as_sources': 1,
          'action': [
            'python',
            '<@(_inputs)',
            'webui',
            '<(RULE_INPUT_PATH)',
            'chrome/<(RULE_INPUT_DIRNAME)/<(RULE_INPUT_ROOT).js',
            '<@(_outputs)',
          ],
        },
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug_Base': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          }
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_command_ids.h',
            'app/chrome_dll.rc',
            'app/chrome_dll_resource.h',
            'app/chrome_version.rc.version',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
            '../third_party/GTM',
          ],
          # TODO(mark): We really want this for all non-static library
          # targets, but when we tried to pull it up to the common.gypi
          # level, it broke other things like the ui, startup, and
          # page_cycler tests. *shrug*
          'xcode_settings': {
            'OTHER_LDFLAGS': [
              '-Wl,-ObjC',
            ],
          },
          # Other platforms only need
          # chrome_resources.gyp:{packed_extra_resources,packed_resources},
          # and can build this target standalone much faster.
          'dependencies': [
            'chrome',
            '../components/components.gyp:breakpad_stubs',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
      ],  # conditions
    },  # target performance_browser_tests
    {
      # To run the tests from page_load_test.cc on Linux, we need to:
      #
      #   a) Run with CHROME_HEADLESS=1 to generate crash dumps.
      #   b) Strip the binary if it's a debug build. (binary may be over 2GB)
      'target_name': 'reliability_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:theme_resources',
        'test_support_common',
        'test_support_ui',
        'test_support_ui_runner',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'test/reliability/page_load_test.cc',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        },],
        ['OS=="win" and win_use_allocator_shim==1', {
          'dependencies': [
            '<(allocator_target)',
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                },
              },
            },
          },
        },],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        },],
        ['OS=="linux" and enable_webrtc==1', {
          'dependencies': [
            # TODO(tommi): Figure out why the 32bit lucid builder fails to
            # find this dependency for this target (other builders pick it up
            # correctly).  crbug.com/231068.
            '../third_party/libjingle/libjingle.gyp:libpeerconnection',
          ],
        }],
      ],
    },
    {
      'target_name': 'performance_ui_tests',
      'type': 'executable',
      'dependencies': [
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'debugger',
        'test_support_common',
        'test_support_ui',
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
      ],
      'sources': [
        # TODO(darin): Move other UIPerfTests here.
        'test/perf/dom_checker_uitest.cc',
        'test/perf/feature_startup_test.cc',
        'test/perf/frame_rate/frame_rate_tests.cc',
        'test/perf/generate_profile.cc',
        'test/perf/generate_profile.h',
        'test/perf/indexeddb_uitest.cc',
        'test/perf/memory_test.cc',
        'test/perf/perf_ui_test_suite.cc',
        'test/perf/run_all_perfuitests.cc',
        'test/perf/shutdown_test.cc',
        'test/perf/startup_test.cc',
        'test/perf/tab_switching_test.cc',
      ],
      'conditions': [
        ['OS=="win" and buildtype=="Official"', {
          'configurations': {
            'Release': {
              'msvs_settings': {
                'VCCLCompilerTool': {
                  'WholeProgramOptimization': 'false',
                },
              },
            },
          },
        }],
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
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
        ['OS=="mac"', {
          'sources': [
            'test/perf/mach_ports_test.cc',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
          ],
        }],
        ['toolkit_uses_gtk == 1', {
          'dependencies': [
            '../build/linux/system.gyp:gtk',
          ],
        }],
        ['os_posix == 1 and OS != "mac" and OS != "android"', {
          'conditions': [
            ['linux_use_tcmalloc==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:allocator',
              ],
            }],
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_integration_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome',
        'chrome_resources.gyp:chrome_resources',
        'chrome_resources.gyp:chrome_strings',
        'chrome_resources.gyp:packed_extra_resources',
        'chrome_resources.gyp:packed_resources',
        'common',
        'common/extensions/api/api.gyp:api',
        'renderer',
        'test_support_common',
        '../net/net.gyp:net',
        '../printing/printing.gyp:printing',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_testserver',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/leveldatabase/leveldatabase.gyp:leveldatabase',
        '../third_party/npapi/npapi.gyp:npapi',
        '../third_party/WebKit/public/blink.gyp:blink',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      # TODO(phajdan.jr): Only temporary, to make transition easier.
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'test/base/chrome_test_launcher.cc',
        'test/data/resource.rc',
        'browser/sync/test/integration/apps_helper.cc',
        'browser/sync/test/integration/apps_helper.h',
        'browser/sync/test/integration/autofill_helper.cc',
        'browser/sync/test/integration/autofill_helper.h',
        'browser/sync/test/integration/bookmarks_helper.cc',
        'browser/sync/test/integration/bookmarks_helper.h',
        'browser/sync/test/integration/cross_platform_sync_test.cc',
        'browser/sync/test/integration/dictionary_helper.cc',
        'browser/sync/test/integration/dictionary_helper.h',
        'browser/sync/test/integration/dictionary_load_observer.cc',
        'browser/sync/test/integration/dictionary_load_observer.h',
        'browser/sync/test/integration/enable_disable_test.cc',
        'browser/sync/test/integration/extension_settings_helper.cc',
        'browser/sync/test/integration/extension_settings_helper.h',
        'browser/sync/test/integration/extensions_helper.cc',
        'browser/sync/test/integration/extensions_helper.h',
        'browser/sync/test/integration/migration_errors_test.cc',
        'browser/sync/test/integration/multiple_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/multiple_client_dictionary_sync_test.cc',
        'browser/sync/test/integration/multiple_client_passwords_sync_test.cc',
        'browser/sync/test/integration/multiple_client_preferences_sync_test.cc',
        'browser/sync/test/integration/multiple_client_sessions_sync_test.cc',
        'browser/sync/test/integration/multiple_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/passwords_helper.cc',
        'browser/sync/test/integration/passwords_helper.h',
        'browser/sync/test/integration/preferences_helper.cc',
        'browser/sync/test/integration/preferences_helper.h',
        'browser/sync/test/integration/search_engines_helper.cc',
        'browser/sync/test/integration/search_engines_helper.h',
        'browser/sync/test/integration/sessions_helper.cc',
        'browser/sync/test/integration/sessions_helper.h',
        'browser/sync/test/integration/single_client_apps_sync_test.cc',
        'browser/sync/test/integration/single_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/single_client_dictionary_sync_test.cc',
        'browser/sync/test/integration/single_client_extensions_sync_test.cc',
        'browser/sync/test/integration/single_client_managed_user_settings_sync_test.cc',
        'browser/sync/test/integration/single_client_passwords_sync_test.cc',
        'browser/sync/test/integration/single_client_preferences_sync_test.cc',
        'browser/sync/test/integration/single_client_search_engines_sync_test.cc',
        'browser/sync/test/integration/single_client_sessions_sync_test.cc',
        'browser/sync/test/integration/single_client_themes_sync_test.cc',
        'browser/sync/test/integration/single_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/sync_app_helper.cc',
        'browser/sync/test/integration/sync_app_helper.h',
        'browser/sync/test/integration/sync_datatype_helper.cc',
        'browser/sync/test/integration/sync_datatype_helper.h',
        'browser/sync/test/integration/sync_errors_test.cc',
        'browser/sync/test/integration/sync_extension_helper.cc',
        'browser/sync/test/integration/sync_extension_helper.h',
        'browser/sync/test/integration/sync_test.cc',
        'browser/sync/test/integration/sync_test.h',
        'browser/sync/test/integration/themes_helper.cc',
        'browser/sync/test/integration/themes_helper.h',
        'browser/sync/test/integration/two_client_apps_sync_test.cc',
        'browser/sync/test/integration/two_client_autofill_sync_test.cc',
        'browser/sync/test/integration/two_client_bookmarks_sync_test.cc',
        'browser/sync/test/integration/two_client_dictionary_sync_test.cc',
        'browser/sync/test/integration/two_client_extension_settings_and_app_settings_sync_test.cc',
        'browser/sync/test/integration/two_client_extensions_sync_test.cc',
        'browser/sync/test/integration/two_client_passwords_sync_test.cc',
        'browser/sync/test/integration/two_client_preferences_sync_test.cc',
        'browser/sync/test/integration/two_client_search_engines_sync_test.cc',
        'browser/sync/test/integration/two_client_sessions_sync_test.cc',
        'browser/sync/test/integration/two_client_themes_sync_test.cc',
        'browser/sync/test/integration/two_client_typed_urls_sync_test.cc',
        'browser/sync/test/integration/typed_urls_helper.cc',
        'browser/sync/test/integration/typed_urls_helper.h',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
           ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          # The sync_integration_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          'sources!': [
            'browser/sync/test/integration/dictionary_helper.cc',
            'browser/sync/test/integration/dictionary_helper.h',
            'browser/sync/test/integration/dictionary_load_observer.cc',
            'browser/sync/test/integration/dictionary_load_observer.h',
            'browser/sync/test/integration/multiple_client_dictionary_sync_test.cc',
            'browser/sync/test/integration/single_client_dictionary_sync_test.cc',
            'browser/sync/test/integration/two_client_dictionary_sync_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
            '<(SHARED_INTERMEDIATE_DIR)/content/content_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/net/net_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources/ui_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/webkit/blink_resources.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      'target_name': 'sync_performance_tests',
      'type': 'executable',
      'dependencies': [
        'browser',
        'chrome',
        'common/extensions/api/api.gyp:api',
        'test_support_common',
        '../skia/skia.gyp:skia',
        '../sync/sync.gyp:sync',
        '../sync/sync.gyp:test_support_sync_testserver',
        '../testing/gmock.gyp:gmock',
        '../testing/gtest.gyp:gtest',
      ],
      'include_dirs': [
        '..',
        '<(INTERMEDIATE_DIR)',
        '<(protoc_out_dir)',
      ],
      'defines': [
        'HAS_OUT_OF_PROC_TEST_RUNNER',
      ],
      'sources': [
        'app/chrome_command_ids.h',
        'app/chrome_dll.rc',
        'app/chrome_dll_resource.h',
        'app/chrome_version.rc.version',
        'browser/sync/test/integration/autofill_helper.cc',
        'browser/sync/test/integration/autofill_helper.h',
        'browser/sync/test/integration/bookmarks_helper.cc',
        'browser/sync/test/integration/bookmarks_helper.h',
        'browser/sync/test/integration/dictionary_helper.cc',
        'browser/sync/test/integration/dictionary_helper.h',
        'browser/sync/test/integration/dictionary_load_observer.cc',
        'browser/sync/test/integration/dictionary_load_observer.h',
        'browser/sync/test/integration/extensions_helper.cc',
        'browser/sync/test/integration/extensions_helper.h',
        'browser/sync/test/integration/passwords_helper.cc',
        'browser/sync/test/integration/passwords_helper.h',
        'browser/sync/test/integration/performance/autofill_sync_perf_test.cc',
        'browser/sync/test/integration/performance/bookmarks_sync_perf_test.cc',
        'browser/sync/test/integration/performance/dictionary_sync_perf_test.cc',
        'browser/sync/test/integration/performance/extensions_sync_perf_test.cc',
        'browser/sync/test/integration/performance/sync_timing_helper.cc',
        'browser/sync/test/integration/performance/sync_timing_helper.h',
        'browser/sync/test/integration/performance/passwords_sync_perf_test.cc',
        'browser/sync/test/integration/performance/sessions_sync_perf_test.cc',
        'browser/sync/test/integration/performance/typed_urls_sync_perf_test.cc',
        'browser/sync/test/integration/sessions_helper.cc',
        'browser/sync/test/integration/sessions_helper.h',
        'browser/sync/test/integration/sync_datatype_helper.cc',
        'browser/sync/test/integration/sync_datatype_helper.h',
        'browser/sync/test/integration/sync_extension_helper.cc',
        'browser/sync/test/integration/sync_extension_helper.h',
        'browser/sync/test/integration/sync_test.cc',
        'browser/sync/test/integration/sync_test.h',
        'browser/sync/test/integration/typed_urls_helper.cc',
        'browser/sync/test/integration/typed_urls_helper.h',
        'test/base/chrome_test_launcher.cc',
        'test/data/resource.rc',
      ],
      'conditions': [
        ['toolkit_uses_gtk == 1', {
           'dependencies': [
             '../build/linux/system.gyp:gtk',
           ],
        }],
        ['toolkit_uses_gtk == 1 or chromeos==1 or (OS=="linux" and use_aura==1)', {
          'dependencies': [
            '../build/linux/system.gyp:ssl',
          ],
        }],
        ['OS=="mac"', {
          # The sync_performance_tests do not run on mac without this flag.
          # Search for comments about "xcode_settings" elsewhere in this file.
          'xcode_settings': {'OTHER_LDFLAGS': ['-Wl,-ObjC']},
          # Dictionary sync is disabled on Mac.
          'sources!': [
            'browser/sync/test/integration/dictionary_helper.cc',
            'browser/sync/test/integration/dictionary_helper.h',
            'browser/sync/test/integration/dictionary_load_observer.cc',
            'browser/sync/test/integration/dictionary_load_observer.h',
            'browser/sync/test/integration/performance/dictionary_sync_perf_test.cc',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            '<(SHARED_INTERMEDIATE_DIR)/chrome/browser_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/chrome_unscaled_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/common_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome/extensions_api_resources.rc',
            '<(SHARED_INTERMEDIATE_DIR)/chrome_version/other_version.rc',
          ],
          'include_dirs': [
            '<(DEPTH)/third_party/wtl/include',
          ],
          'dependencies': [
            'chrome_version_resources',
          ],
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '<(allocator_target)',
              ],
            }],
          ],
          'configurations': {
            'Debug': {
              'msvs_settings': {
                'VCLinkerTool': {
                  'LinkIncremental': '<(msvs_debug_link_nonincremental)',
                },
              },
            },
          },
        }, { # else: OS != "win"
          'sources!': [
            'app/chrome_dll.rc',
            'app/chrome_version.rc.version',
            'test/data/resource.rc',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/views.gyp:views',
          ],
        }],
      ],
    },
    {
      # Executable that contains all the tests to be run on the GPU bots.
      'target_name': 'gpu_tests',
      'type': 'executable',
      'dependencies': [
        # Runtime dependencies
        '../third_party/mesa/mesa.gyp:osmesa',
      ],
      'includes': [
        'test/gpu/test_support_gpu.gypi'
      ],
      'sources': [
        'test/gpu/gpu_feature_browsertest.cc',
      ],
    },
  ],
  'conditions': [
    ['OS=="mac"', {
      'targets': [
        {
          # This is the mac equivalent of the security_tests target below. It
          # generates a framework bundle which bundles tests to be run in a
          # renderer process. The test code is built as a framework so it can be
          # run in the context of a renderer without shipping the code to end
          # users.
          'target_name': 'renderer_sandbox_tests',
          'type': 'shared_library',
          'product_name': 'Renderer Sandbox Tests',
          'mac_bundle': 1,
          'xcode_settings': {
            'INFOPLIST_FILE': 'test/security_tests/sandbox_tests_mac-Info.plist',
          },
          'sources': [
            'test/security_tests/renderer_sandbox_tests_mac.mm',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
            ],
          },
        },  # target renderer_sandbox_tests
        {
          # Tests for Mac app launcher.
          'target_name': 'app_mode_app_tests',
          'type': 'executable',
          'product_name': 'app_mode_app_tests',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../chrome/common_constants.gyp:common_constants',
            '../testing/gtest.gyp:gtest',
            'chrome.gyp:chrome',  # run time dependency
            'app_mode_app_support',
          ],
          'sources': [
            'common/mac/app_mode_chrome_locator_unittest.mm',
            'test/base/app_mode_app_tests.cc',
          ],
          'include_dirs': [
            '..',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreFoundation.framework',
              '$(SDKROOT)/System/Library/Frameworks/Foundation.framework',
            ],
          },
        },  # target app_mode_app_tests
      ],
    }],
    ['OS!="mac"', {
      'targets': [
        {
          'target_name': 'perf_tests',
          'type': 'executable',
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)',  # Needed by key_systems.cc.
          ],
          'dependencies': [
            'browser',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'common',
            'renderer',
            '../content/content.gyp:content_gpu',
            '../content/content.gyp:test_support_content',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../base/base.gyp:test_support_perf',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../webkit/support/webkit_support.gyp:glue',
            '../third_party/widevine/cdm/widevine_cdm.gyp:widevine_cdm_version_h',
          ],
          'sources': [
            'test/perf/perftests.cc',
            'test/perf/url_parse_perftest.cc',
            '../content/browser/net/sqlite_persistent_cookie_store_perftest.cc',

            # TODO(boliu): Move this to a separate components_perftest target
            # under components/.
            '../components/visitedlink/test/visitedlink_perftest.cc',
          ],
          'conditions': [
            ['use_x11==1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['OS=="win"', {
              'configurations': {
                'Debug_Base': {
                  'msvs_settings': {
                    'VCLinkerTool': {
                      'LinkIncremental': '<(msvs_large_module_debug_link_mode)',
                    },
                  },
                },
              },
              'conditions': [
                ['win_use_allocator_shim==1', {
                  'dependencies': [
                    '<(allocator_target)',
                  ],
                }],
              ],
            }],
            ['toolkit_views==1', {
              'dependencies': [
                '../ui/views/views.gyp:views',
              ],
            }],
            ['os_posix == 1 and OS != "mac" and OS != "android"', {
              'conditions': [
                ['linux_use_tcmalloc==1', {
                  'dependencies': [
                    '../base/allocator/allocator.gyp:allocator',
                  ],
                }],
              ],
            }],
            ['OS=="linux" and enable_webrtc==1', {
              'dependencies': [
                # TODO(tommi): Figure out why the 32bit lucid builder fails to
                # find this dependency for this target (other builders pick it up
                # correctly).  crbug.com/231068.
                '../third_party/libjingle/libjingle.gyp:libpeerconnection',
              ],
            }],
          ],
        },
      ],
    },],  # OS!="mac"
    ['OS=="win"', {
      'targets': [
        {
          'target_name': 'security_tests',
          'type': 'shared_library',
          'include_dirs': [
            '..',
          ],
          'sources': [
            'test/security_tests/ipc_security_tests.cc',
            'test/security_tests/ipc_security_tests.h',
            'test/security_tests/security_tests.cc',
            '../sandbox/win/tests/validation_tests/commands.cc',
            '../sandbox/win/tests/validation_tests/commands.h',
          ],
        },
      ]},  # 'targets'
    ],  # OS=="win"
    # If you change this condition, make sure you also change it in all.gyp
    # for the chromium_builder_qa target.
    ['enable_automation==1 and (OS=="mac" or ((OS=="win" or os_posix==1) and target_arch==python_arch))', {
      'targets': [
        {
          # Documentation: http://dev.chromium.org/developers/testing/pyauto
          'target_name': 'pyautolib',
          'type': 'loadable_module',
          'product_prefix': '_',
          'dependencies': [
            'chrome',
            'chrome_resources.gyp:chrome_resources',
            'chrome_resources.gyp:chrome_strings',
            'chrome_resources.gyp:theme_resources',
            'debugger',
            'test_support_common',
            '../skia/skia.gyp:skia',
            '../sync/sync.gyp:sync',
            '../testing/gtest.gyp:gtest',
          ],
          'export_dependent_settings': [
            'test_support_common',
          ],
          'include_dirs': [
            '..',
          ],
          'cflags': [
             '-Wno-uninitialized',
             '-Wno-self-assign',  # to keep clang happy for generated code.
          ],
          'sources': [
            'test/automation/proxy_launcher.cc',
            'test/automation/proxy_launcher.h',
            'test/pyautolib/pyautolib.cc',
            'test/pyautolib/pyautolib.h',
            'test/ui/ui_test.cc',
            'test/ui/ui_test.h',
            'test/ui/ui_test_suite.cc',
            'test/ui/ui_test_suite.h',
            '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
            '<@(pyautolib_sources)',
          ],
          'xcode_settings': {
            # Link with python2.6. Using -L/usr/lib and -lpython2.6 does not
            # work with the -isysroot argument passed in. Even if it did,
            # the linker shouldn't use any other lib not in the 10.5 sdk.
            'OTHER_LDFLAGS': [
              '/usr/lib/libpython2.6.dylib'
            ],
          },
          'msvs_disabled_warnings': [4211],
          'conditions': [
            # Disable the type profiler. _POSIX_C_SOURCE and _XOPEN_SOURCE
            # conflict between <Python.h> and <typeinfo>.
            ['OS=="linux" and clang_type_profiler==1', {
              'cflags_cc!': [
                '-fintercept-allocation-functions',
              ],
            }],
            ['os_posix == 1 and OS!="mac"', {
              'include_dirs': [
                '..',
                '<(sysroot)/usr/include/python<(python_ver)',
              ],
              'link_settings': {
                'libraries': [
                  '-lpython<(python_ver)',
                ],
              },
            }],
            ['toolkit_uses_gtk == 1', {
              'dependencies': [
                '../build/linux/system.gyp:gtk',
              ],
            }],
            ['OS=="mac"', {
              'include_dirs': [
                '..',
                '/usr/include/python2.6',
              ],
            }],
            ['OS=="win"', {
              'product_extension': 'pyd',
              'include_dirs': [
                '..',
                '../third_party/python_26/include',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  'AdditionalLibraryDirectories': [
                    '<(DEPTH)/third_party/python_26/libs',
                  ],
                  'AdditionalDependencies': [
                    'python26.lib',
                  ],
                },
              }
            }],
            ['clang == 1', {
              'xcode_settings': {
                'WARNING_CFLAGS': [
                  # swig creates code with self assignments.
                  '-Wno-self-assign',
                ],
              },
              'cflags': [
                '-Wno-self-assign',
              ],
            }],
            ['asan==1', {
              'cflags!': [ '-fsanitize=address' ],
              'xcode_settings': { 'OTHER_CFLAGS!': [ '-fsanitize=address' ] },
            }],
          ],
          'actions': [
            {
              'variables' : {
                'swig_args': [ '-I..',
                               '-python',
                               '-c++',
                               '-threads',
                               '-outdir',
                               '<(PRODUCT_DIR)',
                               '-o',
                               '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                ],
                'conditions': [
                  ['chromeos==1', {
                    'swig_args': [
                      '-DOS_CHROMEOS',
                    ]
                  }],
                ],
              },
              'action_name': 'pyautolib_swig',
              'inputs': [
                'test/pyautolib/argc_argv.i',
                'test/pyautolib/pyautolib.i',
                '<@(pyautolib_sources)',
              ],
              'outputs': [
                '<(INTERMEDIATE_DIR)/pyautolib_wrap.cc',
                '<(PRODUCT_DIR)/pyautolib.py',
              ],
              'action': [ 'python',
                          '../tools/swig/swig.py',
                          '<@(swig_args)',
                          'test/pyautolib/pyautolib.i',
              ],
              'message': 'Generating swig wrappers for pyautolib.',
              'msvs_cygwin_shell': 1,
            },
          ],  # actions
        },  # target 'pyautolib'
        {
          # Required for WebRTC PyAuto tests.
          'target_name': 'webrtc_test_tools',
          'type': 'none',
          'dependencies': [
            'pyautolib',
            '../third_party/libjingle/libjingle.gyp:peerconnection_server',
            '../third_party/webrtc/tools/tools.gyp:frame_analyzer',
            '../third_party/webrtc/tools/tools.gyp:rgba_to_i420_converter',
          ],
        },  # target 'webrtc_test_tools'
      ]  # targets
    }],
    # To enable the coverage targets, do
    #    GYP_DEFINES='coverage=1' gclient sync
    # To match the coverage buildbot more closely, do this:
    #    GYP_DEFINES='coverage=1 enable_svg=0 fastbuild=1' gclient sync
    # (and, on MacOS, be sure to switch your SDK from "Base SDK" to "Mac OS X
    # 10.6")
    # (but on Windows, don't set the fastbuild=1 because it removes the PDB
    # generation which is necessary for code coverage.)
    ['coverage!=0',
      { 'targets': [
        {
          ### Coverage BUILD AND RUN.
          ### Not named coverage_build_and_run for historical reasons.
          'target_name': 'coverage',
          'dependencies': [ 'coverage_build', 'coverage_run' ],
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              'message': 'Coverage is now complete.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage-build-and-run.stamp' ],
              'action_name': 'coverage',
              # Wish gyp had some basic builtin commands (e.g. 'touch').
              'action': [ 'python', '-c',
                          'import os; ' \
                          'open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage-build-and-run.stamp\'' \
                          ', \'w\').close()' ],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage BUILD.  Compile only; does not run the bundles.
        ### Intended as the build phase for our coverage bots.
        ###
        ### Builds unit test bundles needed for coverage.
        ### Outputs this list of bundles into coverage_bundles.py.
        ###
        ### If you want to both build and run coverage from your IDE,
        ### use the 'coverage' target.
        {
          'target_name': 'coverage_build',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'dependencies': [
            'automated_ui_tests',
            '../base/base.gyp:base_unittests',
            # browser_tests's use of subprocesses chokes gcov on 10.6?
            # Disabling for now (enabled on linux/windows below).
            # 'browser_tests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../content/content.gyp:content_unittests',
            'unit_tests',
            '../sync/sync.gyp:sync_unit_tests',
          ],  # 'dependencies'
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                # Courgette has not been ported from Windows.
                # Note build/win/chrome_win.croc uniquely has the
                # courgette source directory in an include path.
                '../courgette/courgette.gyp:courgette_unittests',
                'browser_tests',
                ]}],
            ['OS=="linux"', {
              'dependencies': [
                # Win bot needs to be turned into an interactive bot.
                'interactive_ui_tests',
                # Disabled from running in coverage_posix.py.
                # We need to build this during compile step, so enabling here.
                '../content/content.gyp:content_browsertests',
                '../courgette/courgette.gyp:courgette_unittests',
                '../crypto/crypto.gyp:crypto_unittests',
                '../jingle/jingle.gyp:jingle_unittests',
                '../net/net.gyp:net_perftests',
                '../third_party/WebKit/public/all.gyp:all_blink',
                '../url/url.gyp:url_unittests',
                'browser_tests',
                'chromedriver_unittests',
                'gpu_tests',
                'performance_ui_tests',
                'pyautolib',
                'reliability_tests',
                'sync_integration_tests',
              ]}],
            ['OS=="mac"', {
              'dependencies': [
              # Placeholder; empty for now.
              ]}],
          ],  # 'conditions'
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Compiling coverage bundles.',
              # MSVS must have an input file and an output file.
              #
              # TODO(jrg):
              # Technically I want inputs to be the list of
              # executables created in <@(_dependencies) but use of
              # that variable lists the dep by dep name, not their
              # output executable name.
              # Is there a better way to force this action to run, always?
              #
              # If a test bundle is added to this coverage_build target it
              # necessarily means this file (chrome_tests.gypi) is changed,
              # so the action is run (coverage_bundles.py is generated).
              # Exceptions to that rule are theoretically possible
              # (e.g. re-gyp with a GYP_DEFINES set).
              # Else it's the same list of bundles as last time.  They are
              # built (since on the deps list) but the action may not run.
              # For now, things work, but it's less than ideal.
              'inputs': [ 'chrome_tests.gypi' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'action_name': 'coverage_build',
              'action': [ 'python', '-c',
                          'import os; '
                          'f = open(' \
                          '\'<(PRODUCT_DIR)\' + os.path.sep + ' \
                          '\'coverage_bundles.py\'' \
                          ', \'w\'); ' \
                          'deplist = \'' \
                          '<@(_dependencies)' \
                          '\'.split(\' \'); ' \
                          'f.write(str(deplist)); ' \
                          'f.close()'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
        ### Coverage RUN.  Does not compile the bundles.  Mirrors the
        ### run_coverage_bundles buildbot phase.  If you update this
        ### command update the mirror in
        ### $BUILDBOT/scripts/master/factory/chromium_commands.py.
        ### If you want both build and run, use the 'coverage' target.
        {
          'target_name': 'coverage_run',
          # do NOT place this in the 'all' list; most won't want it.
          # In gyp, booleans are 0/1 not True/False.
          'suppress_wildcard': 1,
          'type': 'none',
          'actions': [
            {
              # 'message' for Linux/scons in particular.  Scons
              # requires the 'coverage' target be run from within
              # src/chrome.
              'message': 'Running the coverage script.  NOT building anything.',
              # MSVS must have an input file and an output file.
              'inputs': [ '<(PRODUCT_DIR)/coverage_bundles.py' ],
              'outputs': [ '<(PRODUCT_DIR)/coverage.info' ],
              'action_name': 'coverage_run',
              'action': [ 'python',
                          '../tools/code_coverage/coverage_posix.py',
                          '--directory',
                          '<(PRODUCT_DIR)',
                          '--src_root',
                          '..',
                          '--bundles',
                          '<(PRODUCT_DIR)/coverage_bundles.py'],
              # Use outputs of this action as inputs for the main target build.
              # Seems as a misnomer but makes this happy on Linux (scons).
              'process_outputs_as_sources': 1,
            },
          ],  # 'actions'
        },
      ]
    }],  # 'coverage!=0'
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'chromium_testshell_test_apk',
          'type': 'none',
          'dependencies': [
            'chrome_java',
            'chromium_testshell_java',
            '../base/base.gyp:base',
            '../base/base.gyp:base_java_test_support',
            '../content/content.gyp:content_java_test_support',
            '../sync/sync.gyp:sync_javatests',
            '../tools/android/forwarder/forwarder.gyp:forwarder',
          ],
          'variables': {
            'apk_name': 'ChromiumTestShellTest',
            'java_in_dir': 'android/testshell/javatests',
            'resource_dir': 'android/testshell/res',
            'additional_src_dirs': ['android/javatests/src'],
            'is_test_apk': 1,
          },
          'includes': [ '../build/java_apk.gypi' ],
        },
        {
          'target_name': 'chromium_testshell_uiautomator_tests_java',
          'type': 'none',
          'variables': {
            'java_in_dir': 'android/uiautomator_tests',
          },
          'dependencies': [
            '../base/base.gyp:base_java_test_support',
            '../third_party/android_tools/android_tools.gyp:uiautomator_jar',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'chromium_testshell_uiautomator_tests',
          'type': 'none',
          'dependencies': [
            'chromium_testshell_uiautomator_tests_java',
          ],
          'includes': [ '../build/uiautomator_test.gypi' ],
        },
      ],
    }],
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'browser_tests_run',
          'type': 'none',
          'dependencies': [
            'browser_tests',
            'chrome',
            '../third_party/WebKit/public/blink_test_plugin.gyp:blink_test_plugin',
          ],
          'includes': [
            '../build/isolate.gypi',
            'browser_tests.isolate',
          ],
          'sources': [
            'browser_tests.isolate',
          ],
        },
        {
          'target_name': 'interactive_ui_tests_run',
          'type': 'none',
          'dependencies': [
            'interactive_ui_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
            'interactive_ui_tests.isolate',
          ],
          'sources': [
            'interactive_ui_tests.isolate',
          ],
        },
        {
          'target_name': 'sync_integration_tests_run',
          'type': 'none',
          'dependencies': [
            'sync_integration_tests',
          ],
          'conditions': [
            ['use_x11 == 1', {
              'dependencies': [
                '../tools/xdisplaycheck/xdisplaycheck.gyp:xdisplaycheck',
              ],
            }],
          ],
          'includes': [
            '../build/isolate.gypi',
            'sync_integration_tests.isolate',
          ],
          'sources': [
            'sync_integration_tests.isolate',
          ],
        },
      ],
    }],
    ['OS=="win"', {
      'targets' : [
        {
          # This target is only depended upon in Windows.
          'target_name': 'automated_ui_tests_exe_pdb_workaround',
          'type': 'static_library',
          'sources': [ 'empty_pdb_workaround.cc' ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              # This *in the compile phase* must match the pdb name that's
              # output by the final link. See empty_pdb_workaround.cc for
              # more details.
              'DebugInformationFormat': '3',
              'ProgramDataBaseFileName': '<(PRODUCT_DIR)/automated_ui_tests.exe.pdb',
            },
          },
        },
        {
          # This target is only depended upon in Windows.
          'target_name': 'browser_tests_exe_pdb_workaround',
          'type': 'static_library',
          'sources': [ 'empty_pdb_workaround.cc' ],
          'msvs_settings': {
            'VCCLCompilerTool': {
              # This *in the compile phase* must match the pdb name that's
              # output by the final link. See empty_pdb_workaround.cc for
              # more details.
              'DebugInformationFormat': '3',
              'ProgramDataBaseFileName': '<(PRODUCT_DIR)/browser_tests.exe.pdb',
            },
          },
        },
      ],
    }],
    [ 'enable_mdns == 1', {
      'targets': [{
          'target_name': 'service_discovery_sniffer',
          'type': 'executable',
          'dependencies': [
            '../net/net.gyp:net',
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            'utility',
          ],
          'sources': [
            'tools/service_discovery_sniffer/service_discovery_sniffer.h',
            'tools/service_discovery_sniffer/service_discovery_sniffer.cc',
          ],
        }]
    }],
  ],  # 'conditions'
}
