# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'conditions': [
    ['OS == "android"', {
      'targets': [
        {
          'target_name': 'sync_java',
          'type': 'none',
          'variables': {
            'java_in_dir': '../sync/android/java',
          },
          'dependencies': [
            '../base/base.gyp:base_java',
            '../net/net.gyp:net_java',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
            '../third_party/guava/guava.gyp:guava_javalib',
            '../third_party/jsr-305/jsr-305.gyp:jsr_305_javalib',
          ],
          'includes': [ '../build/java.gypi' ],
        },
        {
          'target_name': 'sync_jni_headers',
          'type': 'none',
          'sources': [
            'android/java/src/org/chromium/sync/notifier/InvalidationController.java',
          ],
          'variables': {
            'jni_gen_package': 'sync',
          },
          'includes': [ '../build/jni_generator.gypi' ],
        },
      ],
    }],
  ],
}
