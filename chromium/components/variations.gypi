# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      'target_name': 'variations',
      'type': 'static_library',
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../third_party/mt19937ar/mt19937ar.gyp:mt19937ar',
      ],
      'sources': [
        'variations/entropy_provider.cc',
        'variations/entropy_provider.h',
        'variations/proto/variations_seed.proto',
        'variations/proto/study.proto',
        'variations/metrics_util.cc',
        'variations/metrics_util.h',
        'variations/variations_associated_data.cc',
        'variations/variations_associated_data.h',
        'variations/variations_seed_processor.cc',
        'variations/variations_seed_processor.h',
      ],
      'variables': {
        'proto_in_dir': 'variations/proto',
        'proto_out_dir': 'components/variations/proto',
      },
      'includes': [ '../build/protoc.gypi' ]
    },
  ],
}
