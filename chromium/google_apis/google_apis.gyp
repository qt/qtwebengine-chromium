# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,  # Use higher warning level.
  },
  'includes': [
    '../build/win_precompile.gypi',
  ],
  'targets': [
    {
      'target_name': 'google_apis',
      'type': 'static_library',
      'includes': [
        'determine_use_official_keys.gypi',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../crypto/crypto.gyp:crypto',
        '../net/net.gyp:net',
        '../third_party/libxml/libxml.gyp:libxml',
      ],
      'conditions': [
        ['google_api_key!=""', {
          'defines': ['GOOGLE_API_KEY="<(google_api_key)"'],
        }],
        ['google_default_client_id!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_ID="<(google_default_client_id)"',
          ]
        }],
        ['google_default_client_secret!=""', {
          'defines': [
            'GOOGLE_DEFAULT_CLIENT_SECRET="<(google_default_client_secret)"',
          ]
        }],
        [ 'OS == "android"', {
            'dependencies': [
              '../third_party/openssl/openssl.gyp:openssl',
            ],
            'sources/': [
              ['exclude', 'cup/client_update_protocol_nss\.cc$'],
            ],
        }],
        [ 'use_openssl==1', {
            'sources!': [
              'cup/client_update_protocol_nss.cc',
            ],
          }, {
            'sources!': [
              'cup/client_update_protocol_openssl.cc',
            ],
        },],
      ],
      'sources': [
        'cup/client_update_protocol.cc',
        'cup/client_update_protocol.h',
        'cup/client_update_protocol_nss.cc',
        'cup/client_update_protocol_openssl.cc',
        'drive/auth_service.cc',
        'drive/auth_service.h',
        'drive/auth_service_interface.h',
        'drive/auth_service_observer.h',
        'drive/base_requests.cc',
        'drive/base_requests.h',
        'drive/drive_api_parser.cc',
        'drive/drive_api_parser.h',
        'drive/drive_api_requests.cc',
        'drive/drive_api_requests.h',
        'drive/drive_api_url_generator.cc',
        'drive/drive_api_url_generator.h',
        'drive/drive_common_callbacks.h',
        'drive/drive_entry_kinds.h',
        'drive/gdata_contacts_requests.cc',
        'drive/gdata_contacts_requests.h',
        'drive/gdata_errorcode.cc',
        'drive/gdata_errorcode.h',
        'drive/gdata_wapi_requests.cc',
        'drive/gdata_wapi_requests.h',
        'drive/gdata_wapi_parser.cc',
        'drive/gdata_wapi_parser.h',
        'drive/gdata_wapi_url_generator.cc',
        'drive/gdata_wapi_url_generator.h',
        'drive/request_sender.cc',
        'drive/request_sender.h',
        'drive/request_util.cc',
        'drive/request_util.h',
        'drive/task_util.cc',
        'drive/task_util.h',
        'drive/time_util.cc',
        'drive/time_util.h',
        'gaia/gaia_auth_consumer.cc',
        'gaia/gaia_auth_consumer.h',
        'gaia/gaia_auth_fetcher.cc',
        'gaia/gaia_auth_fetcher.h',
        'gaia/gaia_auth_util.cc',
        'gaia/gaia_auth_util.h',
        'gaia/gaia_constants.cc',
        'gaia/gaia_constants.h',
        'gaia/gaia_oauth_client.cc',
        'gaia/gaia_oauth_client.h',
        'gaia/gaia_switches.cc',
        'gaia/gaia_switches.h',
        'gaia/gaia_urls.cc',
        'gaia/gaia_urls.h',
        'gaia/google_service_auth_error.cc',
        'gaia/google_service_auth_error.h',
        'gaia/oauth_request_signer.cc',
        'gaia/oauth_request_signer.h',
        'gaia/oauth2_access_token_consumer.h',
        'gaia/oauth2_access_token_fetcher.cc',
        'gaia/oauth2_access_token_fetcher.h',
        'gaia/oauth2_api_call_flow.cc',
        'gaia/oauth2_api_call_flow.h',
        'gaia/oauth2_mint_token_flow.cc',
        'gaia/oauth2_mint_token_flow.h',
        'gaia/oauth2_token_service.cc',
        'gaia/oauth2_token_service.h',
        'google_api_keys.cc',
        'google_api_keys.h',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [4267, ],
    },
    {
      'target_name': 'google_apis_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:run_all_unittests',
        '../testing/gtest.gyp:gtest',
        'google_apis',
      ],
      'include_dirs': [
        '..',
      ],
      'sources': [
        'google_api_keys_unittest.cc',
      ],
    },
    {
      'target_name': 'google_apis_test_support',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
      ],
      'export_dependent_settings': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../net/net.gyp:net',
        '../net/net.gyp:net_test_support',
      ],
      'sources': [
        'gaia/fake_gaia.cc',
        'gaia/fake_gaia.h',
      ],
    },
  ],
}
