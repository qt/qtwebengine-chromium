# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'targets': [
    {
      'target_name': 'device_bluetooth',
      'type': 'static_library',
      'dependencies': [
        '../../base/base.gyp:base',
        '../../base/third_party/dynamic_annotations/dynamic_annotations.gyp:dynamic_annotations',
        '../../net/net.gyp:net',
        '../../third_party/libxml/libxml.gyp:libxml',
        '../../ui/gfx/gfx.gyp:gfx',
        '../../ui/ui.gyp:ui',
        'bluetooth_strings.gyp:device_bluetooth_strings',
      ],
      'sources': [
        'bluetooth_adapter.cc',
        'bluetooth_adapter.h',
        'bluetooth_adapter_chromeos.cc',
        'bluetooth_adapter_chromeos.h',
        'bluetooth_adapter_factory.cc',
        'bluetooth_adapter_factory.h',
        'bluetooth_adapter_mac.h',
        'bluetooth_adapter_mac.mm',
        'bluetooth_adapter_win.cc',
        'bluetooth_adapter_win.h',
        'bluetooth_device.cc',
        'bluetooth_device.h',
        'bluetooth_device_chromeos.cc',
        'bluetooth_device_chromeos.h',
        'bluetooth_device_mac.h',
        'bluetooth_device_mac.mm',
        'bluetooth_device_win.cc',
        'bluetooth_device_win.h',
        'bluetooth_init_win.cc',
        'bluetooth_init_win.h',
        'bluetooth_out_of_band_pairing_data.h',
        'bluetooth_profile.cc',
        'bluetooth_profile.h',
        'bluetooth_profile_chromeos.cc',
        'bluetooth_profile_chromeos.h',
        'bluetooth_profile_mac.h',
        'bluetooth_profile_mac.mm',
        'bluetooth_profile_win.cc',
        'bluetooth_profile_win.h',
        'bluetooth_service_record.cc',
        'bluetooth_service_record.h',
        'bluetooth_service_record_mac.h',
        'bluetooth_service_record_mac.mm',
        'bluetooth_service_record_win.cc',
        'bluetooth_service_record_win.h',
        'bluetooth_socket.h',
        'bluetooth_socket_chromeos.cc',
        'bluetooth_socket_chromeos.h',
        'bluetooth_socket_mac.h',
        'bluetooth_socket_mac.mm',
        'bluetooth_socket_win.cc',
        'bluetooth_socket_win.h',
        'bluetooth_task_manager_win.cc',
        'bluetooth_task_manager_win.h',
        'bluetooth_utils.cc',
        'bluetooth_utils.h',
      ],
      'conditions': [
        ['chromeos==1', {
          'dependencies': [
            '../../build/linux/system.gyp:dbus',
            '../../chromeos/chromeos.gyp:chromeos',
            '../../dbus/dbus.gyp:dbus',
          ]
        }],
        ['OS=="win"', {
          'all_dependent_settings': {
            'msvs_settings': {
              'VCLinkerTool': {
                'DelayLoadDLLs': [
                  # Despite MSDN stating that Bthprops.dll contains the
                  # symbols declared by bthprops.lib, they actually reside here:
                  'Bthprops.cpl',
                ],
              },
            },
          },
        }],
        ['OS=="mac"', {
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/IOBluetooth.framework',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'device_bluetooth_mocks',
      'type': 'static_library',
      'dependencies': [
        '../../testing/gmock.gyp:gmock',
        'device_bluetooth',
      ],
      'include_dirs': [
        '../../',
      ],
      'sources': [
        'test/mock_bluetooth_adapter.cc',
        'test/mock_bluetooth_adapter.h',
        'test/mock_bluetooth_device.cc',
        'test/mock_bluetooth_device.h',
        'test/mock_bluetooth_profile.cc',
        'test/mock_bluetooth_profile.h',
        'test/mock_bluetooth_socket.cc',
        'test/mock_bluetooth_socket.h',
      ],
    },
  ],
}
