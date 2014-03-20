# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'variables': {
    'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
    'about_credits_file': '<(SHARED_INTERMEDIATE_DIR)/about_credits.html',
    'additional_modules_list_file': '<(SHARED_INTERMEDIATE_DIR)/chrome/browser/internal/additional_modules_list.txt',
    'repack_locales_cmd': ['python', 'tools/build/repack_locales.py'],
  },
  'targets': [
    {
      'target_name': 'chrome_extra_resources',
      'type': 'none',
      # These resources end up in resources.pak because they are resources
      # used by internal pages.  Putting them in a separate pak file makes
      # it easier for us to reference them internally.
      'actions': [
        {
          'action_name': 'memory_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/memory_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'net_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/net_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'signin_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/signin_internals_resources.grd',
            },
          'includes': ['../build/grit_action.gypi' ],
        },
        {
          'action_name': 'sync_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/sync_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'translate_internals_resources',
          'variables': {
            'grit_grd_file': 'browser/resources/translate_internals_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            '../components/components.gyp:dom_distiller_resources',
            '../content/browser/devtools/devtools_resources.gyp:devtools_resources',
            '../content/browser/tracing/tracing_resources.gyp:tracing_resources',
          ],
          'actions': [
            {
              'action_name': 'component_extension_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/component_extension_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
            {
              'action_name': 'options_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/options_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
            {
              'action_name': 'quota_internals_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/quota_internals_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
            {
              'action_name': 'devtools_discovery_page_resources',
              'variables': {
                'grit_grd_file':
                   'browser/devtools/frontend/devtools_discovery_page_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ]
            },
            {
              'action_name': 'sync_file_system_internals_resources',
              'variables': {
                'grit_grd_file': 'browser/resources/sync_file_system_internals_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            },
          ],
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)/resources/extension/demo',
              'files': [
                'browser/resources/extension_resource/demo/library.js',
              ],
            },
          ],
        }],
        ['chromeos==1 and disable_nacl==0 and disable_nacl_untrusted==0', {
          'dependencies': [
            '../chrome/third_party/chromevox/chromevox.gyp:chromevox_resources',
          ],
        }],
      ],
    },
    {
      'target_name': 'chrome_internal_resources_gen',
      'type': 'none',
      'conditions': [
        ['branding=="Chrome"', {
          'actions': [
            {
              'action_name': 'transform_additional_modules_list',
              'variables': {
                'additional_modules_input_path':
                  'browser/internal/resources/additional_modules_list.input',
                'additional_modules_py_path':
                  'browser/internal/transform_additional_modules_list.py',
              },
              'inputs': [
                '<(additional_modules_input_path)',
              ],
              'outputs': [
                '<(additional_modules_list_file)',
              ],
              'action': [
                'python',
                '<(additional_modules_py_path)',
                '<(additional_modules_input_path)',
                '<@(_outputs)',
              ],
              'message': 'Transforming additional modules list.',
            }
          ],
        }],
      ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_resources',
      'type': 'none',
      'dependencies': [
        'about_credits',
        'chrome_internal_resources_gen',
      ],
      'actions': [
        # Data resources.
        {
          'action_name': 'browser_resources',
          'variables': {
            'grit_grd_file': 'browser/browser_resources.grd',
            'grit_additional_defines': [
              '-E', 'about_credits_file=<(about_credits_file)',
              '-E', 'additional_modules_list_file=<(additional_modules_list_file)',
            ],
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'common_resources',
          'variables': {
            'grit_grd_file': 'common/common_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'renderer_resources',
          'variables': {
            'grit_grd_file': 'renderer/resources/renderer_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'conditions': [
        ['enable_extensions==1', {
          'actions': [
            {
              'action_name': 'extensions_api_resources',
              'variables': {
                'grit_grd_file': 'common/extensions_api_resources.grd',
              },
              'includes': [ '../build/grit_action.gypi' ],
            }
          ],
        }],
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      # TODO(mark): It would be better if each static library that needed
      # to run grit would list its own .grd files, but unfortunately some
      # of the static libraries currently have circular dependencies among
      # generated headers.
      'target_name': 'chrome_strings',
      'type': 'none',
      'actions': [
        # Localizable resources.
        {
          'action_name': 'locale_settings',
          'variables': {
            'grit_grd_file': 'app/resources/locale_settings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'chromium_strings.grd',
          'variables': {
            'grit_grd_file': 'app/chromium_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'generated_resources',
          'variables': {
            'grit_grd_file': 'app/generated_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
        {
          'action_name': 'google_chrome_strings',
          'variables': {
            'grit_grd_file': 'app/google_chrome_strings.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'platform_locale_settings',
      'type': 'none',
      'variables': {
        'conditions': [
          ['OS=="win"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_win.grd',
          },],
          ['OS=="linux"', {
            'conditions': [
              ['chromeos==1', {
                'conditions': [
                  ['branding=="Chrome"', {
                    'platform_locale_settings_grd':
                        'app/resources/locale_settings_google_chromeos.grd',
                  }, {  # branding!=Chrome
                    'platform_locale_settings_grd':
                        'app/resources/locale_settings_chromiumos.grd',
                  }],
                ]
              }, {  # chromeos==0
                'platform_locale_settings_grd':
                    'app/resources/locale_settings_linux.grd',
              }],
            ],
          },],
          ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "linux"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_linux.grd',
          },],
          ['OS == "mac" or OS == "ios"', {
            'platform_locale_settings_grd':
                'app/resources/locale_settings_mac.grd',
          }],
        ],  # conditions
      },  # variables
      'actions': [
        {
          'action_name': 'platform_locale_settings',
          'variables': {
            'grit_grd_file': '<(platform_locale_settings_grd)',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources_gen',
      'type': 'none',
      'actions': [
        {
          'action_name': 'theme_resources',
          'variables': {
            'grit_grd_file': 'app/theme/theme_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'theme_resources',
      'type': 'none',
      'dependencies': [
        'chrome_unscaled_resources',
        'theme_resources_gen',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
      ],
    },
    {
      'target_name': 'packed_extra_resources',
      'type': 'none',
      'variables': {
        'repack_path': '../tools/grit/grit/format/repack.py',
      },
      'dependencies': [
        'chrome_extra_resources',
        'packed_resources',
      ],
      'actions': [
        {
          'includes': ['chrome_repack_resources.gypi']
        },
      ],
      'conditions': [
        ['OS != "mac" and OS != "ios"', {
          # We'll install the resource files to the product directory.  The Mac
          # copies the results over as bundle resources in its own special way.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/resources.pak'
              ],
            },
          ],
        }],
      ],
    },
    {
      'target_name': 'packed_resources',
      'type': 'none',
      'variables': {
        'repack_path': '../tools/grit/grit/format/repack.py',
      },
      'dependencies': [
        # MSVS needs the dependencies explictly named, Make is able to
        # derive the dependencies from the output files.
        'chrome_resources',
        'chrome_strings',
        'platform_locale_settings',
        'theme_resources',
        '<(DEPTH)/components/component_strings.gyp:component_strings',
        '<(DEPTH)/net/net.gyp:net_resources',
        '<(DEPTH)/ui/base/strings/ui_strings.gyp:ui_strings',
        '<(DEPTH)/ui/resources/ui_resources.gyp:ui_resources',
      ],
      'actions': [
        {
          'includes': ['chrome_repack_chrome.gypi']
        },
        {
          'includes': ['chrome_repack_locales.gypi']
        },
        {
          'includes': ['chrome_repack_pseudo_locales.gypi']
        },
        {
          'includes': ['chrome_repack_chrome_100_percent.gypi']
        },
        {
          'includes': ['chrome_repack_chrome_200_percent.gypi']
        },
        {
          'includes': ['chrome_repack_chrome_touch_100_percent.gypi']
        },
      ],
      'conditions': [
        ['OS != "ios"', {
          'dependencies': [
            # TODO(zork): Protect this with if use_aura==1
            '<(DEPTH)/ash/ash_strings.gyp:ash_strings',
            '<(DEPTH)/content/content_resources.gyp:content_resources',
            '<(DEPTH)/device/bluetooth/bluetooth_strings.gyp:device_bluetooth_strings',
            '<(DEPTH)/webkit/webkit_resources.gyp:webkit_resources',
            '<(DEPTH)/webkit/webkit_resources.gyp:webkit_strings',
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
             '<(DEPTH)/ash/ash.gyp:ash_resources',
          ],
        }],
        ['OS != "mac" and OS != "ios"', {
          # Copy pak files to the product directory. These files will be picked
          # up by the following installer scripts:
          #   - Windows: chrome/installer/mini_installer/chrome.release
          #   - Linux: chrome/installer/linux/internal/common/installer.include
          # Ensure that the above scripts are updated when adding or removing
          # pak files.
          # Copying files to the product directory is not needed on the Mac
          # since the framework build phase will copy them into the framework
          # bundle directly.
          'copies': [
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/chrome.pak'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)',
              'files': [
                '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_100_percent.pak'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(locales))'
              ],
            },
            {
              'destination': '<(PRODUCT_DIR)/pseudo_locales',
              'files': [
                '<!@pymod_do_main(repack_locales -o -p <(OS) -g <(grit_out_dir) -s <(SHARED_INTERMEDIATE_DIR) -x <(SHARED_INTERMEDIATE_DIR) <(pseudo_locales))'
              ],
            },
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'copies': [
                {
                  # This location is for the Windows and Linux builds. For
                  # Windows, the chrome.release file ensures that these files
                  # are copied into the installer. Note that we have a separate
                  # section in chrome_dll.gyp to copy these files for Mac, as it
                  # needs to be dropped inside the framework.
                  'destination': '<(PRODUCT_DIR)/default_apps',
                  'files': ['<@(default_apps_list)']
                },
              ],
            }],
            ['enable_hidpi == 1 and OS!="win"', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_200_percent.pak',
                  ],
                },
              ],
            }],
            ['enable_touch_ui==1', {
              'copies': [
                {
                  'destination': '<(PRODUCT_DIR)',
                  'files': [
                    '<(SHARED_INTERMEDIATE_DIR)/repack/chrome_touch_100_percent.pak',
                  ],
                },
              ],
            }],
          ], # conditions
        }], # end OS != "mac" and OS != "ios"
      ], # conditions
    },
    {
      'target_name': 'chrome_unscaled_resources',
      'type': 'none',
      'variables': {
        'grit_out_dir': '<(SHARED_INTERMEDIATE_DIR)/chrome',
      },
      'actions': [
        {
          'action_name': 'chrome_unscaled_resources',
          'variables': {
            'grit_grd_file': 'app/theme/chrome_unscaled_resources.grd',
          },
          'includes': [ '../build/grit_action.gypi' ],
        },
      ],
      'includes': [ '../build/grit_target.gypi' ],
    },
    {
      'target_name': 'about_credits',
      'type': 'none',
      'actions': [
        {
          'variables': {
            'generator_path': '../tools/licenses.py',
          },
          'action_name': 'generate_about_credits',
          'inputs': [
            # TODO(phajdan.jr): make licenses.py print inputs too.
            '<(generator_path)',
          ],
          'outputs': [
            '<(about_credits_file)',
          ],
          'hard_dependency': 1,
          'action': ['python',
                     '<(generator_path)',
                     'credits',
                     '<(about_credits_file)',
          ],
          'message': 'Generating about:credits',
        },
      ],
    },
  ], # targets
}
