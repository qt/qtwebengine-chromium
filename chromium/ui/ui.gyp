# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'ui',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:base_i18n',
        '../base/base.gyp:base_static',
        '../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/icu/icu.gyp:icui18n',
        '../third_party/icu/icu.gyp:icuuc',
        '../third_party/libpng/libpng.gyp:libpng',
        '../third_party/zlib/zlib.gyp:zlib',
        '../url/url.gyp:url_lib',
        'base/strings/ui_strings.gyp:ui_strings',
        'events/events.gyp:events_base',
        'gfx/gfx.gyp:gfx',
        'resources/ui_resources.gyp:ui_resources',
      ],
      'defines': [
        'UI_IMPLEMENTATION',
      ],
      'export_dependent_settings': [
        '../net/net.gyp:net',
        'gfx/gfx.gyp:gfx',
      ],
      'sources' : [
        'base/accelerators/accelerator.cc',
        'base/accelerators/accelerator.h',
        'base/accelerators/accelerator_manager.cc',
        'base/accelerators/accelerator_manager.h',
        'base/accelerators/menu_label_accelerator_util_linux.cc',
        'base/accelerators/menu_label_accelerator_util_linux.h',
        'base/accelerators/platform_accelerator.h',
        'base/accelerators/platform_accelerator_cocoa.h',
        'base/accelerators/platform_accelerator_cocoa.mm',
        'base/accelerators/platform_accelerator_gtk.cc',
        'base/accelerators/platform_accelerator_gtk.h',
        'base/accessibility/accessibility_types.h',
        'base/accessibility/accessible_text_utils.cc',
        'base/accessibility/accessible_text_utils.h',
        'base/accessibility/accessible_view_state.cc',
        'base/accessibility/accessible_view_state.h',
        'base/android/ui_base_jni_registrar.cc',
        'base/android/ui_base_jni_registrar.h',
        'base/android/view_android.cc',
        'base/android/view_android.h',
        'base/android/window_android.cc',
        'base/android/window_android.h',
        'base/android/window_android_observer.h',
        'base/base_window.cc',
        'base/base_window.h',
        'base/clipboard/clipboard.cc',
        'base/clipboard/clipboard.h',
        'base/clipboard/clipboard_android.cc',
        'base/clipboard/clipboard_android_initialization.h',
        'base/clipboard/clipboard_aura.cc',
        'base/clipboard/clipboard_aurax11.cc',
        'base/clipboard/clipboard_constants.cc',
        'base/clipboard/clipboard_gtk.cc',
        'base/clipboard/clipboard_mac.mm',
        'base/clipboard/clipboard_types.h',
        'base/clipboard/clipboard_util_win.cc',
        'base/clipboard/clipboard_util_win.h',
        'base/clipboard/clipboard_win.cc',
        'base/clipboard/custom_data_helper.cc',
        'base/clipboard/custom_data_helper.h',
        'base/clipboard/custom_data_helper_linux.cc',
        'base/clipboard/custom_data_helper_mac.mm',
        'base/clipboard/scoped_clipboard_writer.cc',
        'base/clipboard/scoped_clipboard_writer.h',
        'base/cocoa/animation_utils.h',
        'base/cocoa/appkit_utils.h',
        'base/cocoa/appkit_utils.mm',
        'base/cocoa/base_view.h',
        'base/cocoa/base_view.mm',
        'base/cocoa/cocoa_event_utils.h',
        'base/cocoa/cocoa_event_utils.mm',
        'base/cocoa/controls/blue_label_button.h',
        'base/cocoa/controls/blue_label_button.mm',
        'base/cocoa/controls/hover_image_menu_button.h',
        'base/cocoa/controls/hover_image_menu_button.mm',
        'base/cocoa/controls/hover_image_menu_button_cell.h',
        'base/cocoa/controls/hover_image_menu_button_cell.mm',
        'base/cocoa/controls/hyperlink_button_cell.h',
        'base/cocoa/controls/hyperlink_button_cell.mm',
        'base/cocoa/find_pasteboard.h',
        'base/cocoa/find_pasteboard.mm',
        'base/cocoa/flipped_view.h',
        'base/cocoa/flipped_view.mm',
        'base/cocoa/focus_tracker.h',
        'base/cocoa/focus_tracker.mm',
        'base/cocoa/focus_window_set.h',
        'base/cocoa/focus_window_set.mm',
        'base/cocoa/fullscreen_window_manager.h',
        'base/cocoa/fullscreen_window_manager.mm',
        'base/cocoa/hover_button.h',
        'base/cocoa/hover_button.mm',
        'base/cocoa/hover_image_button.h',
        'base/cocoa/hover_image_button.mm',
        'base/cocoa/menu_controller.h',
        'base/cocoa/menu_controller.mm',
        'base/cocoa/nib_loading.h',
        'base/cocoa/nib_loading.mm',
        'base/cocoa/nsgraphics_context_additions.h',
        'base/cocoa/nsgraphics_context_additions.mm',
        'base/cocoa/tracking_area.h',
        'base/cocoa/tracking_area.mm',
        'base/cocoa/underlay_opengl_hosting_window.h',
        'base/cocoa/underlay_opengl_hosting_window.mm',
        'base/cocoa/view_description.h',
        'base/cocoa/view_description.mm',
        'base/cocoa/window_size_constants.h',
        'base/cocoa/window_size_constants.mm',
        'base/cursor/cursor.cc',
        'base/cursor/cursor.h',
        'base/cursor/cursor_loader.h',
        'base/cursor/cursor_loader_null.cc',
        'base/cursor/cursor_loader_null.h',
        'base/cursor/cursor_loader_win.cc',
        'base/cursor/cursor_loader_win.h',
        'base/cursor/cursor_loader_x11.cc',
        'base/cursor/cursor_loader_x11.h',
        'base/cursor/cursor_null.cc',
        'base/cursor/cursor_win.cc',
        'base/cursor/cursor_x11.cc',
        'base/cursor/cursors_aura.cc',
        'base/cursor/cursors_aura.h',
        'base/default_theme_provider.cc',
        'base/default_theme_provider.h',
        'base/default_theme_provider_mac.mm',
        'base/device_form_factor_android.cc',
        'base/device_form_factor_desktop.cc',
        'base/device_form_factor_ios.mm',
        'base/device_form_factor.h',
        'base/dragdrop/cocoa_dnd_util.h',
        'base/dragdrop/cocoa_dnd_util.mm',
        'base/dragdrop/drag_drop_types.h',
        'base/dragdrop/drag_drop_types_win.cc',
        'base/dragdrop/drag_source_win.cc',
        'base/dragdrop/drag_source_win.h',
        'base/dragdrop/drag_utils.cc',
        'base/dragdrop/drag_utils.h',
        'base/dragdrop/drag_utils_aura.cc',
        'base/dragdrop/drag_utils_win.cc',
        'base/dragdrop/drop_target_event.cc',
        'base/dragdrop/drop_target_event.h',
        'base/dragdrop/drop_target_win.cc',
        'base/dragdrop/drop_target_win.h',
        'base/dragdrop/gtk_dnd_util.cc',
        'base/dragdrop/gtk_dnd_util.h',
        'base/dragdrop/os_exchange_data.cc',
        'base/dragdrop/os_exchange_data.h',
        'base/dragdrop/os_exchange_data_provider_aura.cc',
        'base/dragdrop/os_exchange_data_provider_aura.h',
        'base/dragdrop/os_exchange_data_provider_aurax11.cc',
        'base/dragdrop/os_exchange_data_provider_aurax11.h',
        'base/dragdrop/os_exchange_data_provider_win.cc',
        'base/dragdrop/os_exchange_data_provider_win.h',
        'base/gtk/event_synthesis_gtk.cc',
        'base/gtk/event_synthesis_gtk.h',
        'base/gtk/focus_store_gtk.cc',
        'base/gtk/focus_store_gtk.h',
        'base/gtk/g_object_destructor_filo.cc',
        'base/gtk/g_object_destructor_filo.h',
        'base/gtk/gtk_expanded_container.cc',
        'base/gtk/gtk_expanded_container.h',
        'base/gtk/gtk_floating_container.cc',
        'base/gtk/gtk_floating_container.h',
        'base/gtk/gtk_hig_constants.h',
        'base/gtk/gtk_screen_util.cc',
        'base/gtk/gtk_screen_util.h',
        'base/gtk/gtk_signal.h',
        'base/gtk/gtk_signal_registrar.cc',
        'base/gtk/gtk_signal_registrar.h',
        'base/gtk/gtk_windowing.cc',
        'base/gtk/gtk_windowing.h',
        'base/gtk/owned_widget_gtk.cc',
        'base/gtk/owned_widget_gtk.h',
        'base/gtk/scoped_region.cc',
        'base/gtk/scoped_region.h',
        'base/hit_test.h',
        'base/l10n/l10n_font_util.cc',
        'base/l10n/l10n_font_util.h',
        'base/l10n/l10n_util.cc',
        'base/l10n/l10n_util.h',
        'base/l10n/l10n_util_android.cc',
        'base/l10n/l10n_util_android.h',
        'base/l10n/l10n_util_collator.h',
        'base/l10n/l10n_util_mac.h',
        'base/l10n/l10n_util_mac.mm',
        'base/l10n/l10n_util_plurals.cc',
        'base/l10n/l10n_util_plurals.h',
        'base/l10n/l10n_util_posix.cc',
        'base/l10n/l10n_util_win.cc',
        'base/l10n/l10n_util_win.h',
        'base/l10n/time_format.cc',
        'base/l10n/time_format.h',
        'base/layout.cc',
        'base/layout.h',
        'base/layout_mac.mm',
        'base/models/button_menu_item_model.cc',
        'base/models/button_menu_item_model.h',
        'base/models/combobox_model.cc',
        'base/models/combobox_model.h',
        'base/models/combobox_model_observer.h',
        'base/models/dialog_model.cc',
        'base/models/dialog_model.h',
        'base/models/list_model.h',
        'base/models/list_model_observer.h',
        'base/models/list_selection_model.cc',
        'base/models/list_selection_model.h',
        'base/models/menu_model.cc',
        'base/models/menu_model.h',
        'base/models/menu_model_delegate.h',
        'base/models/menu_separator_types.h',
        'base/models/simple_menu_model.cc',
        'base/models/simple_menu_model.h',
        'base/models/table_model.cc',
        'base/models/table_model.h',
        'base/models/table_model_observer.h',
        'base/models/tree_model.cc',
        'base/models/tree_model.h',
        'base/models/tree_node_iterator.h',
        'base/models/tree_node_model.h',
        'base/resource/data_pack.cc',
        'base/resource/data_pack.h',
        'base/resource/resource_bundle.cc',
        'base/resource/resource_bundle.h',
        'base/resource/resource_bundle_android.cc',
        'base/resource/resource_bundle_auralinux.cc',
        'base/resource/resource_bundle_gtk.cc',
        'base/resource/resource_bundle_ios.mm',
        'base/resource/resource_bundle_mac.mm',
        'base/resource/resource_bundle_win.cc',
        'base/resource/resource_bundle_win.h',
        'base/resource/resource_data_dll_win.cc',
        'base/resource/resource_data_dll_win.h',
        'base/resource/resource_handle.h',
        'base/text/bytes_formatting.cc',
        'base/text/bytes_formatting.h',
        'base/theme_provider.cc',
        'base/theme_provider.h',
        'base/touch/touch_device.cc',
        'base/touch/touch_device.h',
        'base/touch/touch_device_android.cc',
        'base/touch/touch_device_aurax11.cc',
        'base/touch/touch_device_ozone.cc',
        'base/touch/touch_device_win.cc',
        'base/touch/touch_editing_controller.cc',
        'base/touch/touch_editing_controller.h',
        'base/touch/touch_enabled.cc',
        'base/touch/touch_enabled.h',
        'base/ui_base_exports.cc',
        'base/ui_base_paths.cc',
        'base/ui_base_paths.h',
        'base/ui_base_switches.cc',
        'base/ui_base_switches.h',
        'base/ui_base_switches_util.cc',
        'base/ui_base_switches_util.h',
        'base/ui_base_types.cc',
        'base/ui_base_types.h',
        'base/ui_export.h',
        'base/view_prop.cc',
        'base/view_prop.h',
        'base/webui/jstemplate_builder.cc',
        'base/webui/jstemplate_builder.h',
        'base/webui/web_ui_util.cc',
        'base/webui/web_ui_util.h',
        'base/win/accessibility_ids_win.h',
        'base/win/accessibility_misc_utils.cc',
        'base/win/accessibility_misc_utils.h',
        'base/win/atl_module.h',
        'base/win/dpi_setup.cc',
        'base/win/dpi_setup.h',
        'base/win/foreground_helper.cc',
        'base/win/foreground_helper.h',
        'base/win/hidden_window.cc',
        'base/win/hidden_window.h',
        'base/win/hwnd_subclass.cc',
        'base/win/hwnd_subclass.h',
        'base/win/lock_state.cc',
        'base/win/lock_state.h',
        'base/win/message_box_win.cc',
        'base/win/message_box_win.h',
        'base/win/mouse_wheel_util.cc',
        'base/win/mouse_wheel_util.h',
        'base/win/scoped_ole_initializer.cc',
        'base/win/scoped_ole_initializer.h',
        'base/win/shell.cc',
        'base/win/shell.h',
        'base/win/touch_input.cc',
        'base/win/touch_input.h',
        'base/window_open_disposition.cc',
        'base/window_open_disposition.h',
        'base/work_area_watcher_observer.h',
        'base/x/active_window_watcher_x.cc',
        'base/x/active_window_watcher_x.h',
        'base/x/active_window_watcher_x_observer.h',
        'base/x/root_window_property_watcher_x.cc',
        'base/x/root_window_property_watcher_x.h',
        'base/x/selection_owner.cc',
        'base/x/selection_owner.h',
        'base/x/selection_requestor.cc',
        'base/x/selection_requestor.h',
        'base/x/selection_utils.cc',
        'base/x/selection_utils.h',
        'base/x/work_area_watcher_x.cc',
        'base/x/work_area_watcher_x.h',
        'base/x/x11_util.cc',
        'base/x/x11_util.h',
        'base/x/x11_util_internal.h',
      ],
      'target_conditions': [
        ['OS == "ios"', {
          'sources/': [
            ['include', '^base/l10n/l10n_util_mac\\.mm$'],
          ],
        }],
      ],
      'conditions': [
        ['OS!="ios"', {
          'includes': [
            'base/ime/ime.gypi',
          ],
        }, {  # OS=="ios"
          # iOS only uses a subset of UI.
          'sources/': [
            ['exclude', '\\.(cc|mm)$'],
            ['include', '_ios\\.(cc|mm)$'],
            ['include', '(^|/)ios/'],
            ['include', '^base/l10n/'],
            ['include', '^base/layout'],
            ['include', '^base/resource/'],
            ['include', '^base/ui_base_'],
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/CoreGraphics.framework',
            ],
          },
        }],
        ['toolkit_views==1', {
          'dependencies': [
            'events/events.gyp:events',
          ],
        }],
        ['use_aura==1', {
          'sources/': [
            ['exclude', 'base/work_area_watcher_observer.h'],
            ['exclude', 'base/x/active_window_watcher_x.cc'],
            ['exclude', 'base/x/active_window_watcher_x.h'],
            ['exclude', 'base/x/active_window_watcher_x_observer.h'],
            ['exclude', 'base/x/root_window_property_watcher_x.cc'],
            ['exclude', 'base/x/root_window_property_watcher_x.h'],
            ['exclude', 'base/x/work_area_watcher_x.cc'],
            ['exclude', 'base/x/work_area_watcher_x.h'],
          ],
          'dependencies': [
            'events/events.gyp:events',
          ],
        }, {  # use_aura!=1
          'sources!': [
            'base/cursor/cursor.cc',
            'base/cursor/cursor.h',
            'base/cursor/cursor_loader_x11.cc',
            'base/cursor/cursor_loader_x11.h',
            'base/cursor/cursor_win.cc',
            'base/cursor/cursor_x11.cc',
            'base/x/selection_owner.cc',
            'base/x/selection_owner.h',
            'base/x/selection_requestor.cc',
            'base/x/selection_requestor.h',
            'base/x/selection_utils.cc',
            'base/x/selection_utils.h',
          ]
        }],
        ['use_aura==0 or OS!="linux"', {
          'sources!': [
            'base/resource/resource_bundle_auralinux.cc',
          ],
        }],
        ['use_aura==1 and OS=="win"', {
          'sources/': [
            ['exclude', 'base/dragdrop/drag_utils_aura.cc'],
          ],
        }],
        ['use_glib == 1', {
          'dependencies': [
            # font_gtk.cc uses fontconfig.
            '../build/linux/system.gyp:fontconfig',
            '../build/linux/system.gyp:glib',
          ],
        }],
        ['desktop_linux == 1 or chromeos == 1', {
          'conditions': [
            ['toolkit_views==0 and use_aura==0', {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['exclude', '^base/dragdrop/drag_utils.cc'],
                ['exclude', '^base/dragdrop/drag_utils.h'],
                ['exclude', '^base/dragdrop/os_exchange_data.cc'],
                ['exclude', '^base/dragdrop/os_exchange_data.h'],
              ],
            }, {
              # Note: because of gyp predence rules this has to be defined as
              # 'sources/' rather than 'sources!'.
              'sources/': [
                ['include', '^base/dragdrop/os_exchange_data.cc'],
              ],
            }],
          ],
        }],
        ['use_pango==1', {
          'dependencies': [
            '../build/linux/system.gyp:pangocairo',
          ],
        }],
        ['use_x11==0 or use_clipboard_aurax11==1', {
          'sources!': [
            'base/clipboard/clipboard_aura.cc',
          ],
        }, {
          'sources!': [
            'base/clipboard/clipboard_aurax11.cc',
          ],
        }],
        ['chromeos==1 or (use_aura==1 and OS=="linux" and use_x11==0)', {
          'sources!': [
            'base/dragdrop/os_exchange_data_provider_aurax11.cc',
            'base/touch/touch_device.cc',
          ],
        }, {
          'sources!': [
            'base/dragdrop/os_exchange_data_provider_aura.cc',
            'base/dragdrop/os_exchange_data_provider_aura.h',
            'base/touch/touch_device_aurax11.cc',
          ],
        }],
        ['OS=="win"', {
          'sources!': [
            'base/touch/touch_device.cc',
          ],
          'include_dirs': [
            '../',
            '../third_party/wtl/include',
          ],
          # TODO(jschuh): C4267: http://crbug.com/167187 size_t -> int
          # C4324 is structure was padded due to __declspec(align()), which is
          # uninteresting.
          'msvs_disabled_warnings': [ 4267, 4324 ],
          'msvs_settings': {
            'VCLinkerTool': {
              'DelayLoadDLLs': [
                'd2d1.dll',
                'd3d10_1.dll',
                'dwmapi.dll',
              ],
              'AdditionalDependencies': [
                'd2d1.lib',
                'd3d10_1.lib',
                'dwmapi.lib',
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-limm32.lib',
              '-ld2d1.lib',
              '-ldwmapi.lib',
              '-loleacc.lib',
            ],
          },
        },{  # OS!="win"
          'conditions': [
            ['use_aura==0', {
              'sources!': [
                'base/view_prop.cc',
                'base/view_prop.h',
              ],
            }],
          ],
          'sources!': [
            'base/dragdrop/drag_drop_types.h',
            'base/dragdrop/os_exchange_data.cc',
          ],
          'sources/': [
            ['exclude', '^base/win/'],
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../third_party/mozilla/mozilla.gyp:mozilla',
          ],
          'sources!': [
            'base/dragdrop/drag_utils.cc',
            'base/dragdrop/drag_utils.h',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Accelerate.framework',
              '$(SDKROOT)/System/Library/Frameworks/AudioUnit.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
            ],
          },
        }],
        ['use_x11==1', {
          'all_dependent_settings': {
            'ldflags': [
              '-L<(PRODUCT_DIR)',
            ],
            'link_settings': {
              'libraries': [
                '-lX11',
                '-lXcursor',
                '-lXrender',  # For XRender* function calls in x11_util.cc.
              ],
            },
          },
          'link_settings': {
            'libraries': [
              '-lX11',
              '-lXcursor',
              '-lXrender',  # For XRender* function calls in x11_util.cc.
            ],
          },
          'dependencies': [
            '../build/linux/system.gyp:x11',
            '../build/linux/system.gyp:xext',
            '../build/linux/system.gyp:xfixes',
          ],
        }],
        ['use_ozone==0', {
          'sources!': [
            'base/cursor/cursor_null.cc',
            'base/cursor/cursor_loader_null.cc',
            'base/cursor/cursor_loader_null.h',
          ],
        }],
        ['toolkit_views==0', {
          'sources!': [
            'base/dragdrop/drop_target_event.cc',
            'base/dragdrop/drop_target_event.h',
          ],
        }],
        ['OS=="android"', {
          'sources!': [
            'base/accessibility/accessible_text_utils.cc',
            'base/accessibility/accessible_view_state.cc',
            'base/default_theme_provider.cc',
            'base/dragdrop/drag_utils.cc',
            'base/dragdrop/drag_utils.h',
            'base/l10n/l10n_font_util.cc',
            'base/models/button_menu_item_model.cc',
            'base/models/dialog_model.cc',
            'base/theme_provider.cc',
            'base/touch/touch_device.cc',
            'base/touch/touch_editing_controller.cc',
            'base/ui_base_types.cc',
          ],
          'dependencies': [
            'ui_base_jni_headers',
          ],
          'include_dirs': [
            '<(SHARED_INTERMEDIATE_DIR)/ui',
          ],
          'link_settings': {
            'libraries': [
              '-ljnigraphics',
            ],
          },
        }],
        ['OS=="android" and android_webview_build==0', {
          'dependencies': [
            'android/ui_android.gyp:ui_java',
          ],
        }],
        ['OS=="android" or OS=="ios"', {
          'sources!': [
            'base/device_form_factor_desktop.cc'
          ],
        }],
        ['OS=="linux"', {
          'libraries': [
            '-ldl',
          ],
        }],
        ['use_system_icu==1', {
          # When using the system icu, the icu targets generate shim headers
          # which are included by public headers in the ui target, so we need
          # ui to be a hard dependency for all its users.
          'hard_dependency': 1,
        }],
      ],
    },
    {
      'target_name': 'webui_test_support',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          '<(SHARED_INTERMEDIATE_DIR)/ui/ui_resources',
        ]
      }
    },
  ],
  'conditions': [
    ['OS=="android"' , {
       'targets': [
         {
           'target_name': 'ui_base_jni_headers',
           'type': 'none',
           'direct_dependent_settings': {
             'include_dirs': [
               '<(SHARED_INTERMEDIATE_DIR)/ui',
             ],
           },
           'sources': [
             'android/java/src/org/chromium/ui/base/Clipboard.java',
             'android/java/src/org/chromium/ui/base/LocalizationUtils.java',
             'android/java/src/org/chromium/ui/base/SelectFileDialog.java',
             'android/java/src/org/chromium/ui/base/ViewAndroid.java',
             'android/java/src/org/chromium/ui/base/WindowAndroid.java',
           ],
           'variables': {
             'jni_gen_package': 'ui',
             'jni_generator_ptr_type': 'long',
           },
           'includes': [ '../build/jni_generator.gypi' ],
         },
       ],
    }],
    ['OS=="mac"', {
      'targets': [
        {
          'target_name': 'ui_cocoa_third_party_toolkits',
          'type': '<(component)',
          'sources': [
            # Build the necessary GTM sources
            '../third_party/GTM/AppKit/GTMFadeTruncatingTextFieldCell.h',
            '../third_party/GTM/AppKit/GTMFadeTruncatingTextFieldCell.m',
            '../third_party/GTM/AppKit/GTMIBArray.h',
            '../third_party/GTM/AppKit/GTMIBArray.m',
            '../third_party/GTM/AppKit/GTMKeyValueAnimation.h',
            '../third_party/GTM/AppKit/GTMKeyValueAnimation.m',
            '../third_party/GTM/AppKit/GTMNSAnimation+Duration.h',
            '../third_party/GTM/AppKit/GTMNSAnimation+Duration.m',
            '../third_party/GTM/AppKit/GTMNSBezierPath+CGPath.h',
            '../third_party/GTM/AppKit/GTMNSBezierPath+CGPath.m',
            '../third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.h',
            '../third_party/GTM/AppKit/GTMNSBezierPath+RoundRect.m',
            '../third_party/GTM/AppKit/GTMNSColor+Luminance.m',
            '../third_party/GTM/AppKit/GTMUILocalizer.h',
            '../third_party/GTM/AppKit/GTMUILocalizer.m',
            '../third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h',
            '../third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.m',
            '../third_party/GTM/Foundation/GTMNSNumber+64Bit.h',
            '../third_party/GTM/Foundation/GTMNSNumber+64Bit.m',
            '../third_party/GTM/Foundation/GTMNSObject+KeyValueObserving.h',
            '../third_party/GTM/Foundation/GTMNSObject+KeyValueObserving.m',
          ],
          'include_dirs': [
            '..',
            '../third_party/GTM',
            '../third_party/GTM/AppKit',
            '../third_party/GTM/DebugUtils',
            '../third_party/GTM/Foundation',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/QuartzCore.framework',
            ],
          },
          'conditions': [
            ['component=="shared_library"', {
              # GTM is third-party code, so we don't want to add _EXPORT
              # annotations to it, so build it without -fvisibility=hidden
              # (else the interface class symbols will be hidden in a 64bit
              # build). Only do this in a component build, so that the shipping
              # chrome binary doesn't end up with unnecessarily exported
              # symbols.
              'xcode_settings': {
                'GCC_SYMBOLS_PRIVATE_EXTERN': 'NO',
              },
            }],
          ],
        },
      ],
    }],
  ],
}
