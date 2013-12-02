# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
{
  'targets': [
    {
      'target_name': 'android_java',
      'type' : 'none',
      'variables': {
        'jar_path': '<(android_sdk)/android.jar',
        'exclude_from_apk': 1,
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      'target_name': 'android_gcm',
      'type' : 'none',
      'variables': {
        'jar_path': 'sdk/extras/google/gcm/gcm-client/dist/gcm.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
    {
      'target_name': 'uiautomator_jar',
      'type': 'none',
      'variables': {
        'jar_path': '<(android_sdk)/uiautomator.jar',
      },
      'includes': ['../../build/java_prebuilt.gypi'],
    },
  ],
}
