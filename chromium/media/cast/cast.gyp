# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'include_tests%': 1,
  },
  'targets': [
    {
      'target_name': 'cast_config',
      'type': 'static_library',
      'include_dirs': [
        '<(DEPTH)/',
      ],
      'sources': [
        'cast_config.cc',
        'cast_config.h',
        'cast_thread.cc',
        'cast_thread.h',
      ], # source
    },
    {
      'target_name': 'cast_sender',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_sender.gyp:cast_sender_impl',
      ],
    },
    {
      'target_name': 'cast_receiver',
      'type': 'static_library',
      'dependencies': [
        'cast_config',
        'cast_receiver.gyp:cast_receiver_impl',
      ],
    },
  ],  # targets,
  'conditions': [
    ['include_tests==1', {
      'targets': [
        {
          'target_name': 'cast_unittest',
          'type': '<(gtest_target_type)',
          'dependencies': [
            'cast_sender',
            'cast_receiver',
            'rtcp/rtcp.gyp:cast_rtcp_test',
            '<(DEPTH)/base/base.gyp:run_all_unittests',
            '<(DEPTH)/net/net.gyp:net',
            '<(DEPTH)/testing/gmock.gyp:gmock',
            '<(DEPTH)/testing/gtest.gyp:gtest',
          ],
          'include_dirs': [
            '<(DEPTH)/',
            '<(DEPTH)/third_party/',
            '<(DEPTH)/third_party/webrtc/',
          ],
          'sources': [
            'audio_receiver/audio_decoder_unittest.cc',
            'audio_receiver/audio_receiver_unittest.cc',
            'audio_sender/audio_encoder_unittest.cc',
            'audio_sender/audio_sender_unittest.cc',
            'congestion_control/congestion_control_unittest.cc',
            'framer/cast_message_builder_unittest.cc',
            'framer/frame_buffer_unittest.cc',
            'framer/framer_unittest.cc',
            'pacing/paced_sender_unittest.cc',
            'rtcp/rtcp_receiver_unittest.cc',
            'rtcp/rtcp_sender_unittest.cc',
            'rtcp/rtcp_unittest.cc',
            'rtp_receiver/receiver_stats_unittest.cc',
            'rtp_receiver/rtp_parser/test/rtp_packet_builder.cc',
            'rtp_receiver/rtp_parser/rtp_parser_unittest.cc',
            'rtp_sender/packet_storage/packet_storage_unittest.cc',
            'rtp_sender/rtp_packetizer/rtp_packetizer_unittest.cc',
            'rtp_sender/rtp_packetizer/test/rtp_header_parser.cc',
            'rtp_sender/rtp_packetizer/test/rtp_header_parser.h',
            'test/fake_task_runner.cc',
            'video_receiver/video_decoder_unittest.cc',
            'video_receiver/video_receiver_unittest.cc',
            'video_sender/video_encoder_unittest.cc',
            'video_sender/video_sender_unittest.cc',
          ], # source
        },
      ],  # targets
    }], # include_tests
  ],
}
