# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'httpcomponents_core_javalib',
      'type' : 'none',
      'variables': {
        'input_jar_file': 'binary-distribution/lib/httpcore-4.2.2.jar',
        'pattern': 'org.apache.**',
      },
      'includes': [ '../jarjar/jarjar.gypi' ],
    },
  ],
}
