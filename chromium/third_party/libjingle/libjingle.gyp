# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../build/win_precompile.gypi',
  ],
  'variables': {
    'enabled_libjingle_device_manager%': 0,
    'libjingle_additional_deps%': [],
    'libjingle_peerconnection_additional_deps%': [],
    'libjingle_source%': "source",
    'libpeer_target_type%': 'static_library',
    'libpeer_allocator_shim%': 0,
  },
  'target_defaults': {
    'defines': [
      'EXPAT_RELATIVE_PATH',
      'FEATURE_ENABLE_SSL',
      'GTEST_RELATIVE_PATH',
      'HAVE_SRTP',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
      'JSONCPP_RELATIVE_PATH',
      'LOGGING_INSIDE_LIBJINGLE',
      'NO_MAIN_THREAD_WRAPPING',
      'NO_SOUND_SYSTEM',
      'SRTP_RELATIVE_PATH',
      'USE_WEBRTC_DEV_BRANCH',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
          # _DEBUG and remove this define. See below as well.
          '_DEBUG',
        ],
      }
    },
    'include_dirs': [
      './overrides',
      './<(libjingle_source)',
      '../../testing/gtest/include',
      '../../third_party',
      '../../third_party/libyuv/include',
      '../../third_party/usrsctp',
      '../../third_party/webrtc',
    ],
    'dependencies': [
      '<(DEPTH)/base/base.gyp:base',
      '<(DEPTH)/net/net.gyp:net',
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'export_dependent_settings': [
      '<(DEPTH)/third_party/expat/expat.gyp:expat',
    ],
    'direct_dependent_settings': {
      'include_dirs': [
        './overrides',
        './<(libjingle_source)',
        '../../testing/gtest/include',
        '../../third_party',
        '../../third_party/webrtc',
      ],
      'defines': [
        'FEATURE_ENABLE_SSL',
        'FEATURE_ENABLE_VOICEMAIL',
        'EXPAT_RELATIVE_PATH',
        'GTEST_RELATIVE_PATH',
        'JSONCPP_RELATIVE_PATH',
        'NO_MAIN_THREAD_WRAPPING',
        'NO_SOUND_SYSTEM',
      ],
      'conditions': [
        ['OS=="win"', {
          'link_settings': {
            'libraries': [
              '-lsecur32.lib',
              '-lcrypt32.lib',
              '-liphlpapi.lib',
            ],
          },
        }],
        ['OS=="win"', {
          'include_dirs': [
            '../third_party/platformsdk_win7/files/Include',
          ],
          'defines': [
            '_CRT_SECURE_NO_WARNINGS',  # Suppres warnings about _vsnprinf
          ],
          # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
          'msvs_disabled_warnings': [ 4267 ],
        }],
        ['OS=="linux"', {
          'defines': [
            'LINUX',
          ],
        }],
        ['OS=="mac"', {
          'defines': [
            'OSX',
          ],
        }],
        ['OS=="android"', {
          'defines': [
            'ANDROID',
          ],
        }],
        ['os_posix==1', {
          'defines': [
            'POSIX',
          ],
        }],
        ['os_bsd==1', {
          'defines': [
            'BSD',
          ],
        }],
        ['OS=="openbsd"', {
          'defines': [
            'OPENBSD',
          ],
        }],
        ['OS=="freebsd"', {
          'defines': [
            'FREEBSD',
          ],
        }],
        ['chromeos==1', {
          'defines': [
            'CHROMEOS',
          ],
        }],
      ],
    },
    'all_dependent_settings': {
      'configurations': {
        'Debug': {
          'defines': [
            # TODO(sergeyu): Fix libjingle to use NDEBUG instead of
            # _DEBUG and remove this define. See above as well.
            '_DEBUG',
          ],
        }
      },
    },
    'conditions': [
      ['"<(libpeer_target_type)"=="static_library"', {
        'defines': [ 'LIBPEERCONNECTION_LIB=1' ],
      }],
      ['use_openssl==1', {
        'defines': [
          'SSL_USE_OPENSSL',
          'HAVE_OPENSSL_SSL_H',
        ],
        'dependencies': [
          '../../third_party/openssl/openssl.gyp:openssl',
        ],
      }, {
        'defines': [
          'SSL_USE_NSS',
          'HAVE_NSS_SSL_H',
          'SSL_USE_NSS_RNG',
        ],
        'conditions': [
          ['os_posix == 1 and OS != "mac" and OS != "ios" and OS != "android"', {
            'dependencies': [
              '<(DEPTH)/build/linux/system.gyp:ssl',
            ],
          }],
          ['OS == "mac" or OS == "ios" or OS == "win"', {
            'dependencies': [
              '<(DEPTH)/net/third_party/nss/ssl.gyp:libssl',
              '<(DEPTH)/third_party/nss/nss.gyp:nspr',
              '<(DEPTH)/third_party/nss/nss.gyp:nss',
            ],
          }],
        ],
      }],
      ['OS=="win"', {
        'include_dirs': [
          '../third_party/platformsdk_win7/files/Include',
        ],
        'conditions' : [
          ['target_arch == "ia32"', {
            'defines': [
              '_USE_32BIT_TIME_T',
            ],
          }],
        ],
      }],
      ['clang == 1', {
        'xcode_settings': {
          'WARNING_CFLAGS!': [
            # Don't warn about string->bool used in asserts.
            '-Wstring-conversion',
          ],
        },
        'cflags!': [
          '-Wstring-conversion',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'IOS',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'POSIX',
        ],
      }],
      ['os_bsd==1', {
        'defines': [
          'BSD',
        ],
      }],
      ['OS=="openbsd"', {
        'defines': [
          'OPENBSD',
        ],
      }],
      ['OS=="freebsd"', {
        'defines': [
          'FREEBSD',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'libjingle',
      'type': 'static_library',
      'sources': [
        'overrides/talk/base/basictypes.h',
        'overrides/talk/base/constructormagic.h',

        # Overrides logging.h/.cc because libjingle logging should be done to
        # the same place as the chromium logging.
        'overrides/talk/base/logging.cc',
        'overrides/talk/base/logging.h',

        '<(libjingle_source)/talk/base/asyncfile.cc',
        '<(libjingle_source)/talk/base/asyncfile.h',
        '<(libjingle_source)/talk/base/asynchttprequest.cc',
        '<(libjingle_source)/talk/base/asynchttprequest.h',
        '<(libjingle_source)/talk/base/asyncpacketsocket.h',
        '<(libjingle_source)/talk/base/asyncsocket.cc',
        '<(libjingle_source)/talk/base/asyncsocket.h',
        '<(libjingle_source)/talk/base/asynctcpsocket.cc',
        '<(libjingle_source)/talk/base/asynctcpsocket.h',
        '<(libjingle_source)/talk/base/asyncudpsocket.cc',
        '<(libjingle_source)/talk/base/asyncudpsocket.h',
        '<(libjingle_source)/talk/base/autodetectproxy.cc',
        '<(libjingle_source)/talk/base/autodetectproxy.h',
        '<(libjingle_source)/talk/base/base64.cc',
        '<(libjingle_source)/talk/base/base64.h',
        '<(libjingle_source)/talk/base/basicdefs.h',
        '<(libjingle_source)/talk/base/bytebuffer.cc',
        '<(libjingle_source)/talk/base/bytebuffer.h',
        '<(libjingle_source)/talk/base/byteorder.h',
        '<(libjingle_source)/talk/base/checks.cc',
        '<(libjingle_source)/talk/base/checks.h',
        '<(libjingle_source)/talk/base/common.cc',
        '<(libjingle_source)/talk/base/common.h',
        '<(libjingle_source)/talk/base/cpumonitor.cc',
        '<(libjingle_source)/talk/base/cpumonitor.h',
        '<(libjingle_source)/talk/base/crc32.cc',
        '<(libjingle_source)/talk/base/crc32.h',
        '<(libjingle_source)/talk/base/criticalsection.h',
        '<(libjingle_source)/talk/base/cryptstring.h',
        '<(libjingle_source)/talk/base/diskcache.cc',
        '<(libjingle_source)/talk/base/diskcache.h',
        '<(libjingle_source)/talk/base/dscp.h',
        '<(libjingle_source)/talk/base/event.cc',
        '<(libjingle_source)/talk/base/event.h',
        '<(libjingle_source)/talk/base/fileutils.cc',
        '<(libjingle_source)/talk/base/fileutils.h',
        '<(libjingle_source)/talk/base/firewallsocketserver.cc',
        '<(libjingle_source)/talk/base/firewallsocketserver.h',
        '<(libjingle_source)/talk/base/flags.cc',
        '<(libjingle_source)/talk/base/flags.h',
        '<(libjingle_source)/talk/base/helpers.cc',
        '<(libjingle_source)/talk/base/helpers.h',
        '<(libjingle_source)/talk/base/httpbase.cc',
        '<(libjingle_source)/talk/base/httpbase.h',
        '<(libjingle_source)/talk/base/httpclient.cc',
        '<(libjingle_source)/talk/base/httpclient.h',
        '<(libjingle_source)/talk/base/httpcommon-inl.h',
        '<(libjingle_source)/talk/base/httpcommon.cc',
        '<(libjingle_source)/talk/base/httpcommon.h',
        '<(libjingle_source)/talk/base/httprequest.cc',
        '<(libjingle_source)/talk/base/httprequest.h',
        '<(libjingle_source)/talk/base/ipaddress.cc',
        '<(libjingle_source)/talk/base/ipaddress.h',
        '<(libjingle_source)/talk/base/json.cc',
        '<(libjingle_source)/talk/base/json.h',
        '<(libjingle_source)/talk/base/linked_ptr.h',
        '<(libjingle_source)/talk/base/md5.cc',
        '<(libjingle_source)/talk/base/md5.h',
        '<(libjingle_source)/talk/base/md5digest.h',
        '<(libjingle_source)/talk/base/messagedigest.cc',
        '<(libjingle_source)/talk/base/messagedigest.h',
        '<(libjingle_source)/talk/base/messagehandler.cc',
        '<(libjingle_source)/talk/base/messagehandler.h',
        '<(libjingle_source)/talk/base/messagequeue.cc',
        '<(libjingle_source)/talk/base/messagequeue.h',
        '<(libjingle_source)/talk/base/nethelpers.cc',
        '<(libjingle_source)/talk/base/nethelpers.h',
        '<(libjingle_source)/talk/base/network.cc',
        '<(libjingle_source)/talk/base/network.h',
        '<(libjingle_source)/talk/base/nssidentity.cc',
        '<(libjingle_source)/talk/base/nssidentity.h',
        '<(libjingle_source)/talk/base/nssstreamadapter.cc',
        '<(libjingle_source)/talk/base/nssstreamadapter.h',
        '<(libjingle_source)/talk/base/nullsocketserver.h',
        '<(libjingle_source)/talk/base/pathutils.cc',
        '<(libjingle_source)/talk/base/pathutils.h',
        '<(libjingle_source)/talk/base/physicalsocketserver.cc',
        '<(libjingle_source)/talk/base/physicalsocketserver.h',
        '<(libjingle_source)/talk/base/proxydetect.cc',
        '<(libjingle_source)/talk/base/proxydetect.h',
        '<(libjingle_source)/talk/base/proxyinfo.cc',
        '<(libjingle_source)/talk/base/proxyinfo.h',
        '<(libjingle_source)/talk/base/ratelimiter.cc',
        '<(libjingle_source)/talk/base/ratelimiter.h',
        '<(libjingle_source)/talk/base/ratetracker.cc',
        '<(libjingle_source)/talk/base/ratetracker.h',
        '<(libjingle_source)/talk/base/scoped_ptr.h',
        '<(libjingle_source)/talk/base/sec_buffer.h',
        '<(libjingle_source)/talk/base/sha1.cc',
        '<(libjingle_source)/talk/base/sha1.h',
        '<(libjingle_source)/talk/base/sha1digest.h',
        '<(libjingle_source)/talk/base/signalthread.cc',
        '<(libjingle_source)/talk/base/signalthread.h',
        '<(libjingle_source)/talk/base/sigslot.h',
        '<(libjingle_source)/talk/base/sigslotrepeater.h',
        '<(libjingle_source)/talk/base/socket.h',
        '<(libjingle_source)/talk/base/socketadapters.cc',
        '<(libjingle_source)/talk/base/socketadapters.h',
        '<(libjingle_source)/talk/base/socketaddress.cc',
        '<(libjingle_source)/talk/base/socketaddress.h',
        '<(libjingle_source)/talk/base/socketaddresspair.cc',
        '<(libjingle_source)/talk/base/socketaddresspair.h',
        '<(libjingle_source)/talk/base/socketfactory.h',
        '<(libjingle_source)/talk/base/socketpool.cc',
        '<(libjingle_source)/talk/base/socketpool.h',
        '<(libjingle_source)/talk/base/socketserver.h',
        '<(libjingle_source)/talk/base/socketstream.cc',
        '<(libjingle_source)/talk/base/socketstream.h',
        '<(libjingle_source)/talk/base/ssladapter.cc',
        '<(libjingle_source)/talk/base/ssladapter.h',
        '<(libjingle_source)/talk/base/sslidentity.cc',
        '<(libjingle_source)/talk/base/sslidentity.h',
        '<(libjingle_source)/talk/base/sslsocketfactory.cc',
        '<(libjingle_source)/talk/base/sslsocketfactory.h',
        '<(libjingle_source)/talk/base/sslstreamadapter.cc',
        '<(libjingle_source)/talk/base/sslstreamadapter.h',
        '<(libjingle_source)/talk/base/sslstreamadapterhelper.cc',
        '<(libjingle_source)/talk/base/sslstreamadapterhelper.h',
        '<(libjingle_source)/talk/base/stream.cc',
        '<(libjingle_source)/talk/base/stream.h',
        '<(libjingle_source)/talk/base/stringencode.cc',
        '<(libjingle_source)/talk/base/stringencode.h',
        '<(libjingle_source)/talk/base/stringutils.cc',
        '<(libjingle_source)/talk/base/stringutils.h',
        '<(libjingle_source)/talk/base/systeminfo.cc',
        '<(libjingle_source)/talk/base/systeminfo.h',
        '<(libjingle_source)/talk/base/task.cc',
        '<(libjingle_source)/talk/base/task.h',
        '<(libjingle_source)/talk/base/taskparent.cc',
        '<(libjingle_source)/talk/base/taskparent.h',
        '<(libjingle_source)/talk/base/taskrunner.cc',
        '<(libjingle_source)/talk/base/taskrunner.h',
        '<(libjingle_source)/talk/base/thread.cc',
        '<(libjingle_source)/talk/base/thread.h',
        '<(libjingle_source)/talk/base/timeutils.cc',
        '<(libjingle_source)/talk/base/timeutils.h',
        '<(libjingle_source)/talk/base/timing.cc',
        '<(libjingle_source)/talk/base/timing.h',
        '<(libjingle_source)/talk/base/urlencode.cc',
        '<(libjingle_source)/talk/base/urlencode.h',
        '<(libjingle_source)/talk/base/worker.cc',
        '<(libjingle_source)/talk/base/worker.h',
        '<(libjingle_source)/talk/p2p/base/asyncstuntcpsocket.cc',
        '<(libjingle_source)/talk/p2p/base/asyncstuntcpsocket.h',
        '<(libjingle_source)/talk/p2p/base/basicpacketsocketfactory.cc',
        '<(libjingle_source)/talk/p2p/base/basicpacketsocketfactory.h',
        '<(libjingle_source)/talk/p2p/base/candidate.h',
        '<(libjingle_source)/talk/p2p/base/common.h',
        '<(libjingle_source)/talk/p2p/base/dtlstransport.h',
        '<(libjingle_source)/talk/p2p/base/dtlstransportchannel.cc',
        '<(libjingle_source)/talk/p2p/base/dtlstransportchannel.h',
        '<(libjingle_source)/talk/p2p/base/p2ptransport.cc',
        '<(libjingle_source)/talk/p2p/base/p2ptransport.h',
        '<(libjingle_source)/talk/p2p/base/p2ptransportchannel.cc',
        '<(libjingle_source)/talk/p2p/base/p2ptransportchannel.h',
        '<(libjingle_source)/talk/p2p/base/parsing.cc',
        '<(libjingle_source)/talk/p2p/base/parsing.h',
        '<(libjingle_source)/talk/p2p/base/port.cc',
        '<(libjingle_source)/talk/p2p/base/port.h',
        '<(libjingle_source)/talk/p2p/base/portallocator.cc',
        '<(libjingle_source)/talk/p2p/base/portallocator.h',
        '<(libjingle_source)/talk/p2p/base/portallocatorsessionproxy.cc',
        '<(libjingle_source)/talk/p2p/base/portallocatorsessionproxy.h',
        '<(libjingle_source)/talk/p2p/base/portproxy.cc',
        '<(libjingle_source)/talk/p2p/base/portproxy.h',
        '<(libjingle_source)/talk/p2p/base/pseudotcp.cc',
        '<(libjingle_source)/talk/p2p/base/pseudotcp.h',
        '<(libjingle_source)/talk/p2p/base/rawtransport.cc',
        '<(libjingle_source)/talk/p2p/base/rawtransport.h',
        '<(libjingle_source)/talk/p2p/base/rawtransportchannel.cc',
        '<(libjingle_source)/talk/p2p/base/rawtransportchannel.h',
        '<(libjingle_source)/talk/p2p/base/relayport.cc',
        '<(libjingle_source)/talk/p2p/base/relayport.h',
        '<(libjingle_source)/talk/p2p/base/session.cc',
        '<(libjingle_source)/talk/p2p/base/session.h',
        '<(libjingle_source)/talk/p2p/base/sessionclient.h',
        '<(libjingle_source)/talk/p2p/base/sessiondescription.cc',
        '<(libjingle_source)/talk/p2p/base/sessiondescription.h',
        '<(libjingle_source)/talk/p2p/base/sessionid.h',
        '<(libjingle_source)/talk/p2p/base/sessionmanager.cc',
        '<(libjingle_source)/talk/p2p/base/sessionmanager.h',
        '<(libjingle_source)/talk/p2p/base/sessionmessages.cc',
        '<(libjingle_source)/talk/p2p/base/sessionmessages.h',
        '<(libjingle_source)/talk/p2p/base/stun.cc',
        '<(libjingle_source)/talk/p2p/base/stun.h',
        '<(libjingle_source)/talk/p2p/base/stunport.cc',
        '<(libjingle_source)/talk/p2p/base/stunport.h',
        '<(libjingle_source)/talk/p2p/base/stunrequest.cc',
        '<(libjingle_source)/talk/p2p/base/stunrequest.h',
        '<(libjingle_source)/talk/p2p/base/tcpport.cc',
        '<(libjingle_source)/talk/p2p/base/tcpport.h',
        '<(libjingle_source)/talk/p2p/base/transport.cc',
        '<(libjingle_source)/talk/p2p/base/transport.h',
        '<(libjingle_source)/talk/p2p/base/transportchannel.cc',
        '<(libjingle_source)/talk/p2p/base/transportchannel.h',
        '<(libjingle_source)/talk/p2p/base/transportchannelimpl.h',
        '<(libjingle_source)/talk/p2p/base/transportchannelproxy.cc',
        '<(libjingle_source)/talk/p2p/base/transportchannelproxy.h',
        '<(libjingle_source)/talk/p2p/base/transportdescription.cc',
        '<(libjingle_source)/talk/p2p/base/transportdescription.h',
        '<(libjingle_source)/talk/p2p/base/transportdescriptionfactory.cc',
        '<(libjingle_source)/talk/p2p/base/transportdescriptionfactory.h',
        '<(libjingle_source)/talk/p2p/base/turnport.cc',
        '<(libjingle_source)/talk/p2p/base/turnport.h',
        '<(libjingle_source)/talk/p2p/client/basicportallocator.cc',
        '<(libjingle_source)/talk/p2p/client/basicportallocator.h',
        '<(libjingle_source)/talk/p2p/client/httpportallocator.cc',
        '<(libjingle_source)/talk/p2p/client/httpportallocator.h',
        '<(libjingle_source)/talk/p2p/client/sessionmanagertask.h',
        '<(libjingle_source)/talk/p2p/client/sessionsendtask.h',
        '<(libjingle_source)/talk/p2p/client/socketmonitor.cc',
        '<(libjingle_source)/talk/p2p/client/socketmonitor.h',
        '<(libjingle_source)/talk/xmllite/qname.cc',
        '<(libjingle_source)/talk/xmllite/qname.h',
        '<(libjingle_source)/talk/xmllite/xmlbuilder.cc',
        '<(libjingle_source)/talk/xmllite/xmlbuilder.h',
        '<(libjingle_source)/talk/xmllite/xmlconstants.cc',
        '<(libjingle_source)/talk/xmllite/xmlconstants.h',
        '<(libjingle_source)/talk/xmllite/xmlelement.cc',
        '<(libjingle_source)/talk/xmllite/xmlelement.h',
        '<(libjingle_source)/talk/xmllite/xmlnsstack.cc',
        '<(libjingle_source)/talk/xmllite/xmlnsstack.h',
        '<(libjingle_source)/talk/xmllite/xmlparser.cc',
        '<(libjingle_source)/talk/xmllite/xmlparser.h',
        '<(libjingle_source)/talk/xmllite/xmlprinter.cc',
        '<(libjingle_source)/talk/xmllite/xmlprinter.h',
        '<(libjingle_source)/talk/xmpp/asyncsocket.h',
        '<(libjingle_source)/talk/xmpp/constants.cc',
        '<(libjingle_source)/talk/xmpp/constants.h',
        '<(libjingle_source)/talk/xmpp/jid.cc',
        '<(libjingle_source)/talk/xmpp/jid.h',
        '<(libjingle_source)/talk/xmpp/plainsaslhandler.h',
        '<(libjingle_source)/talk/xmpp/prexmppauth.h',
        '<(libjingle_source)/talk/xmpp/saslcookiemechanism.h',
        '<(libjingle_source)/talk/xmpp/saslhandler.h',
        '<(libjingle_source)/talk/xmpp/saslmechanism.cc',
        '<(libjingle_source)/talk/xmpp/saslmechanism.h',
        '<(libjingle_source)/talk/xmpp/saslplainmechanism.h',
        '<(libjingle_source)/talk/xmpp/xmppclient.cc',
        '<(libjingle_source)/talk/xmpp/xmppclient.h',
        '<(libjingle_source)/talk/xmpp/xmppclientsettings.h',
        '<(libjingle_source)/talk/xmpp/xmppengine.h',
        '<(libjingle_source)/talk/xmpp/xmppengineimpl.cc',
        '<(libjingle_source)/talk/xmpp/xmppengineimpl.h',
        '<(libjingle_source)/talk/xmpp/xmppengineimpl_iq.cc',
        '<(libjingle_source)/talk/xmpp/xmpplogintask.cc',
        '<(libjingle_source)/talk/xmpp/xmpplogintask.h',
        '<(libjingle_source)/talk/xmpp/xmppstanzaparser.cc',
        '<(libjingle_source)/talk/xmpp/xmppstanzaparser.h',
        '<(libjingle_source)/talk/xmpp/xmpptask.cc',
        '<(libjingle_source)/talk/xmpp/xmpptask.h',
      ],
      'dependencies': [
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
        'libjingle_p2p_constants',
        '<@(libjingle_additional_deps)',
      ],
      'export_dependent_settings': [
        '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
      ],
      'conditions': [
        ['OS=="win"', {
          'sources': [
            'overrides/talk/base/win32socketinit.cc',
            '<(libjingle_source)/talk/base/schanneladapter.cc',
            '<(libjingle_source)/talk/base/schanneladapter.h',
            '<(libjingle_source)/talk/base/win32.cc',
            '<(libjingle_source)/talk/base/win32.h',
            '<(libjingle_source)/talk/base/win32filesystem.cc',
            '<(libjingle_source)/talk/base/win32filesystem.h',
            '<(libjingle_source)/talk/base/win32window.h',
            '<(libjingle_source)/talk/base/win32window.cc',
            '<(libjingle_source)/talk/base/win32securityerrors.cc',
            '<(libjingle_source)/talk/base/winfirewall.cc',
            '<(libjingle_source)/talk/base/winfirewall.h',
            '<(libjingle_source)/talk/base/winping.cc',
            '<(libjingle_source)/talk/base/winping.h',
          ],
          # Suppress warnings about WIN32_LEAN_AND_MEAN.
          'msvs_disabled_warnings': [ 4005, 4267 ],
        }],
        ['os_posix == 1', {
          'sources': [
            '<(libjingle_source)/talk/base/unixfilesystem.cc',
            '<(libjingle_source)/talk/base/unixfilesystem.h',
          ],
        }],
        ['OS=="linux"', {
          'sources': [
            '<(libjingle_source)/talk/base/latebindingsymboltable.cc',
            '<(libjingle_source)/talk/base/latebindingsymboltable.h',
            '<(libjingle_source)/talk/base/linux.cc',
            '<(libjingle_source)/talk/base/linux.h',
          ],
        }],
        ['OS=="mac" or OS=="ios"', {
          'sources': [
            '<(libjingle_source)/talk/base/macconversion.cc',
            '<(libjingle_source)/talk/base/macconversion.h',
            '<(libjingle_source)/talk/base/maccocoathreadhelper.h',
            '<(libjingle_source)/talk/base/maccocoathreadhelper.mm',
            '<(libjingle_source)/talk/base/macutils.cc',
            '<(libjingle_source)/talk/base/macutils.h',
            '<(libjingle_source)/talk/base/scoped_autorelease_pool.h',
            '<(libjingle_source)/talk/base/scoped_autorelease_pool.mm',
          ],
        }],
        ['OS=="android"', {
          'sources': [
            '<(libjingle_source)/talk/base/ifaddrs-android.cc',
            '<(libjingle_source)/talk/base/ifaddrs-android.h',
            '<(libjingle_source)/talk/base/linux.cc',
            '<(libjingle_source)/talk/base/linux.h',
          ],
          'sources!': [
            # These depend on jsoncpp which we don't load because we probably
            # don't actually need this code at all.
            '<(libjingle_source)/talk/base/json.cc',
            '<(libjingle_source)/talk/base/json.h',
          ],
          'dependencies!': [
            '<(DEPTH)/third_party/jsoncpp/jsoncpp.gyp:jsoncpp',
          ],
        }],
        ['use_openssl==1', {
          'sources': [
            '<(libjingle_source)/talk/base/openssladapter.cc',
            '<(libjingle_source)/talk/base/openssldigest.cc',
            '<(libjingle_source)/talk/base/opensslidentity.cc',
            '<(libjingle_source)/talk/base/opensslstreamadapter.cc',
          ],
        }],
      ],
    },  # target libjingle
    # This has to be is a separate project due to a bug in MSVS 2008 and the
    # current toolset on android.  The problem is that we have two files named
    # "constants.cc" and MSVS/android doesn't handle this properly.
    # GYP currently has guards to catch this, so if you want to remove it,
    # run GYP and if GYP has removed the validation check, then we can assume
    # that the toolchains have been fixed (we currently use VS2010 and later,
    # so VS2008 isn't a concern anymore).
    {
      'target_name': 'libjingle_p2p_constants',
      'type': 'static_library',
      'sources': [
        '<(libjingle_source)/talk/p2p/base/constants.cc',
        '<(libjingle_source)/talk/p2p/base/constants.h',
      ],
    },  # target libjingle_p2p_constants
    {
      'target_name': 'peerconnection_server',
      'type': 'executable',
      'sources': [
        '<(libjingle_source)/talk/examples/peerconnection/server/data_socket.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/data_socket.h',
        '<(libjingle_source)/talk/examples/peerconnection/server/main.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/peer_channel.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/peer_channel.h',
        '<(libjingle_source)/talk/examples/peerconnection/server/utils.cc',
        '<(libjingle_source)/talk/examples/peerconnection/server/utils.h',
      ],
      'include_dirs': [
        '<(libjingle_source)',
      ],
      'dependencies': [
        'libjingle',
      ],
      # TODO(jschuh): crbug.com/167187 fix size_t to int truncations.
      'msvs_disabled_warnings': [ 4309, ],
    }, # target peerconnection_server
  ],
  'conditions': [
    ['enable_webrtc==1', {
      'targets': [
        {
          'target_name': 'libjingle_webrtc_common',
          'type': 'static_library',
          'all_dependent_settings': {
            'conditions': [
              ['"<(libpeer_target_type)"=="static_library"', {
                'defines': [ 'LIBPEERCONNECTION_LIB=1' ],
              }],
            ],
          },
          'sources': [
            'overrides/talk/media/webrtc/webrtcexport.h',

            '<(libjingle_source)/talk/app/webrtc/audiotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/audiotrack.h',
            '<(libjingle_source)/talk/app/webrtc/audiotrackrenderer.cc',
            '<(libjingle_source)/talk/app/webrtc/audiotrackrenderer.h',
            '<(libjingle_source)/talk/app/webrtc/datachannel.cc',
            '<(libjingle_source)/talk/app/webrtc/datachannel.h',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.cc',
            '<(libjingle_source)/talk/app/webrtc/dtmfsender.h',
            '<(libjingle_source)/talk/app/webrtc/jsep.h',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepicecandidate.h',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.cc',
            '<(libjingle_source)/talk/app/webrtc/jsepsessiondescription.h',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.cc',
            '<(libjingle_source)/talk/app/webrtc/localaudiosource.h',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.cc',
            '<(libjingle_source)/talk/app/webrtc/mediaconstraintsinterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediastream.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastream.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamhandler.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreaminterface.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamprovider.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamproxy.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamsignaling.cc',
            '<(libjingle_source)/talk/app/webrtc/mediastreamsignaling.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrack.h',
            '<(libjingle_source)/talk/app/webrtc/mediastreamtrackproxy.h',
            '<(libjingle_source)/talk/app/webrtc/notifier.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnection.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/peerconnectionfactory.h',
            '<(libjingle_source)/talk/app/webrtc/peerconnectioninterface.h',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/portallocatorfactory.h',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.cc',
            '<(libjingle_source)/talk/app/webrtc/remotevideocapturer.h',
            '<(libjingle_source)/talk/app/webrtc/statscollector.cc',
            '<(libjingle_source)/talk/app/webrtc/statscollector.h',
            '<(libjingle_source)/talk/app/webrtc/statstypes.h',
            '<(libjingle_source)/talk/app/webrtc/streamcollection.h',
            '<(libjingle_source)/talk/app/webrtc/videosource.cc',
            '<(libjingle_source)/talk/app/webrtc/videosource.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceinterface.h',
            '<(libjingle_source)/talk/app/webrtc/videosourceproxy.h',
            '<(libjingle_source)/talk/app/webrtc/videotrack.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrack.h',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.cc',
            '<(libjingle_source)/talk/app/webrtc/videotrackrenderers.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsdp.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsession.h',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.cc',
            '<(libjingle_source)/talk/app/webrtc/webrtcsessiondescriptionfactory.h',
            '<(libjingle_source)/talk/media/base/audiorenderer.h',
            '<(libjingle_source)/talk/media/base/capturemanager.cc',
            '<(libjingle_source)/talk/media/base/capturemanager.h',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.cc',
            '<(libjingle_source)/talk/media/base/capturerenderadapter.h',
            '<(libjingle_source)/talk/media/base/codec.cc',
            '<(libjingle_source)/talk/media/base/codec.h',
            '<(libjingle_source)/talk/media/base/constants.cc',
            '<(libjingle_source)/talk/media/base/constants.h',
            '<(libjingle_source)/talk/media/base/cryptoparams.h',
            '<(libjingle_source)/talk/media/base/filemediaengine.cc',
            '<(libjingle_source)/talk/media/base/filemediaengine.h',
            '<(libjingle_source)/talk/media/base/hybriddataengine.h',
            '<(libjingle_source)/talk/media/base/mediachannel.h',
            '<(libjingle_source)/talk/media/base/mediaengine.cc',
            '<(libjingle_source)/talk/media/base/mediaengine.h',
            '<(libjingle_source)/talk/media/base/rtpdataengine.cc',
            '<(libjingle_source)/talk/media/base/rtpdataengine.h',
            '<(libjingle_source)/talk/media/base/rtpdump.cc',
            '<(libjingle_source)/talk/media/base/rtpdump.h',
            '<(libjingle_source)/talk/media/base/rtputils.cc',
            '<(libjingle_source)/talk/media/base/rtputils.h',
            '<(libjingle_source)/talk/media/base/streamparams.cc',
            '<(libjingle_source)/talk/media/base/streamparams.h',
            '<(libjingle_source)/talk/media/base/videoadapter.cc',
            '<(libjingle_source)/talk/media/base/videoadapter.h',
            '<(libjingle_source)/talk/media/base/videocapturer.cc',
            '<(libjingle_source)/talk/media/base/videocapturer.h',
            '<(libjingle_source)/talk/media/base/videocommon.cc',
            '<(libjingle_source)/talk/media/base/videocommon.h',
            '<(libjingle_source)/talk/media/base/videoframe.cc',
            '<(libjingle_source)/talk/media/base/videoframe.h',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
            '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.cc',
            '<(libjingle_source)/talk/media/devices/filevideocapturer.h',
            '<(libjingle_source)/talk/media/webrtc/webrtccommon.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcpassthroughrender.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcpassthroughrender.h',
            '<(libjingle_source)/talk/media/webrtc/webrtctexturevideoframe.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtctexturevideoframe.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideocapturer.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideocapturer.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoframe.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvie.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoe.h',
            '<(libjingle_source)/talk/session/media/audiomonitor.cc',
            '<(libjingle_source)/talk/session/media/audiomonitor.h',
            '<(libjingle_source)/talk/session/media/call.cc',
            '<(libjingle_source)/talk/session/media/call.h',
            '<(libjingle_source)/talk/session/media/channel.cc',
            '<(libjingle_source)/talk/session/media/channel.h',
            '<(libjingle_source)/talk/session/media/channelmanager.cc',
            '<(libjingle_source)/talk/session/media/channelmanager.h',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.cc',
            '<(libjingle_source)/talk/session/media/currentspeakermonitor.h',
            '<(libjingle_source)/talk/session/media/mediamessages.cc',
            '<(libjingle_source)/talk/session/media/mediamessages.h',
            '<(libjingle_source)/talk/session/media/mediamonitor.cc',
            '<(libjingle_source)/talk/session/media/mediamonitor.h',
            '<(libjingle_source)/talk/session/media/mediasession.cc',
            '<(libjingle_source)/talk/session/media/mediasession.h',
            '<(libjingle_source)/talk/session/media/mediasessionclient.cc',
            '<(libjingle_source)/talk/session/media/mediasessionclient.h',
            '<(libjingle_source)/talk/session/media/mediasink.h',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.cc',
            '<(libjingle_source)/talk/session/media/rtcpmuxfilter.h',
            '<(libjingle_source)/talk/session/media/soundclip.cc',
            '<(libjingle_source)/talk/session/media/soundclip.h',
            '<(libjingle_source)/talk/session/media/srtpfilter.cc',
            '<(libjingle_source)/talk/session/media/srtpfilter.h',
            '<(libjingle_source)/talk/session/media/ssrcmuxfilter.cc',
            '<(libjingle_source)/talk/session/media/ssrcmuxfilter.h',
            '<(libjingle_source)/talk/session/media/typingmonitor.cc',
            '<(libjingle_source)/talk/session/media/typingmonitor.h',
            '<(libjingle_source)/talk/session/media/voicechannel.h',
            '<(libjingle_source)/talk/session/tunnel/pseudotcpchannel.cc',
            '<(libjingle_source)/talk/session/tunnel/pseudotcpchannel.h',
            '<(libjingle_source)/talk/session/tunnel/tunnelsessionclient.cc',
            '<(libjingle_source)/talk/session/tunnel/tunnelsessionclient.h',
          ],
          'conditions': [
            ['libpeer_allocator_shim==1 and '
             'libpeer_target_type!="static_library" and OS!="mac"', {
              'sources': [
                'overrides/allocator_shim/allocator_stub.cc',
                'overrides/allocator_shim/allocator_stub.h',
              ],
            }],
            # TODO(mallinath) - Enable SCTP for Android and iOS platforms.
            ['OS!="android" and OS!="ios"', {
              'defines': [
                'HAVE_SCTP',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.cc',
                '<(libjingle_source)/talk/media/sctp/sctpdataengine.h',
              ],
              'dependencies': [
                '<(DEPTH)/third_party/usrsctp/usrsctp.gyp:usrsctplib',
              ],
            }],
            ['enabled_libjingle_device_manager==1', {
              'sources!': [
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.cc',
                '<(libjingle_source)/talk/media/devices/dummydevicemanager.h',
              ],
              'sources': [
                '<(libjingle_source)/talk/media/devices/devicemanager.cc',
                '<(libjingle_source)/talk/media/devices/devicemanager.h',
                '<(libjingle_source)/talk/sound/nullsoundsystem.cc',
                '<(libjingle_source)/talk/sound/nullsoundsystem.h',
                '<(libjingle_source)/talk/sound/nullsoundsystemfactory.cc',
                '<(libjingle_source)/talk/sound/nullsoundsystemfactory.h',
                '<(libjingle_source)/talk/sound/platformsoundsystem.cc',
                '<(libjingle_source)/talk/sound/platformsoundsystem.h',
                '<(libjingle_source)/talk/sound/platformsoundsystemfactory.cc',
                '<(libjingle_source)/talk/sound/platformsoundsystemfactory.h',
                '<(libjingle_source)/talk/sound/soundsysteminterface.cc',
                '<(libjingle_source)/talk/sound/soundsysteminterface.h',
                '<(libjingle_source)/talk/sound/soundsystemproxy.cc',
                '<(libjingle_source)/talk/sound/soundsystemproxy.h',
              ],
              'conditions': [
                ['OS=="win"', {
                  'sources': [
                    '<(libjingle_source)/talk/base/win32window.cc',
                    '<(libjingle_source)/talk/base/win32window.h',
                    '<(libjingle_source)/talk/base/win32windowpicker.cc',
                    '<(libjingle_source)/talk/base/win32windowpicker.h',
                    '<(libjingle_source)/talk/media/devices/win32deviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/win32devicemanager.h',
                  ],
                }],
                ['OS=="linux"', {
                  'sources': [
                    '<(libjingle_source)/talk/base/linuxwindowpicker.cc',
                    '<(libjingle_source)/talk/base/linuxwindowpicker.h',
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.cc',
                    '<(libjingle_source)/talk/media/devices/libudevsymboltable.h',
                    '<(libjingle_source)/talk/media/devices/linuxdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/linuxdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/v4llookup.cc',
                    '<(libjingle_source)/talk/media/devices/v4llookup.h',
                    '<(libjingle_source)/talk/sound/alsasoundsystem.cc',
                    '<(libjingle_source)/talk/sound/alsasoundsystem.h',
                    '<(libjingle_source)/talk/sound/alsasymboltable.cc',
                    '<(libjingle_source)/talk/sound/alsasymboltable.h',
                    '<(libjingle_source)/talk/sound/linuxsoundsystem.cc',
                    '<(libjingle_source)/talk/sound/linuxsoundsystem.h',
                    '<(libjingle_source)/talk/sound/pulseaudiosoundsystem.cc',
                    '<(libjingle_source)/talk/sound/pulseaudiosoundsystem.h',
                    '<(libjingle_source)/talk/sound/pulseaudiosymboltable.cc',
                    '<(libjingle_source)/talk/sound/pulseaudiosymboltable.h',
                  ],
                }],
                ['OS=="mac"', {
                  'sources': [
                    '<(libjingle_source)/talk/media/devices/macdeviceinfo.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.cc',
                    '<(libjingle_source)/talk/media/devices/macdevicemanager.h',
                    '<(libjingle_source)/talk/media/devices/macdevicemanagermm.mm',
                  ],
                  'xcode_settings': {
                    'WARNING_CFLAGS': [
                      # Suppres warnings about using deprecated functions in
                      # macdevicemanager.cc.
                      '-Wno-deprecated-declarations',
                    ],
                  },
                }],
              ],
            }],
          ],
          'dependencies': [
            '<(DEPTH)/third_party/libsrtp/libsrtp.gyp:libsrtp',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:media_file',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_capture_module',
            '<(DEPTH)/third_party/webrtc/modules/modules.gyp:video_render_module',
            'libjingle',
          ],
        },  # target libjingle_webrtc_common
        {
          'target_name': 'libjingle_webrtc',
          'type': 'static_library',
          'sources': [
            'overrides/init_webrtc.cc',
            'overrides/init_webrtc.h',
          ],
          'dependencies': [
            'libjingle_webrtc_common',
          ],
        },
        {
          'target_name': 'libpeerconnection',
          'type': '<(libpeer_target_type)',
          'sources': [
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvideoengine.h',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.cc',
            '<(libjingle_source)/talk/media/webrtc/webrtcvoiceengine.h',
          ],
          'dependencies': [
            '<(DEPTH)/third_party/webrtc/system_wrappers/source/system_wrappers.gyp:system_wrappers',
            '<(DEPTH)/third_party/webrtc/video_engine/video_engine.gyp:video_engine_core',
            '<(DEPTH)/third_party/webrtc/voice_engine/voice_engine.gyp:voice_engine',
            '<@(libjingle_peerconnection_additional_deps)',
            'libjingle_webrtc_common',
          ],
          'conditions': [
            ['libpeer_target_type!="static_library"', {
              'sources': [
                'overrides/initialize_module.cc',
              ],
              'conditions': [
                ['OS!="mac" and OS!="android"', {
                  'sources': [
                    'overrides/allocator_shim/allocator_proxy.cc',
                  ],
                }],
              ],
            }],
            ['"<(libpeer_target_type)"!="static_library"', {
              # Used to control symbol export/import.
              'defines': [ 'LIBPEERCONNECTION_IMPLEMENTATION=1' ],
            }],
            ['OS=="win" and "<(libpeer_target_type)"!="static_library"', {
              'link_settings': {
                'libraries': [
                  '-lsecur32.lib',
                  '-lcrypt32.lib',
                  '-liphlpapi.lib',
                ],
              },
            }],
            ['OS!="win" and "<(libpeer_target_type)"!="static_library"', {
              'cflags': [
                # For compatibility with how we export symbols from this
                # target on Windows.  This also prevents the linker from
                # picking up symbols from this target that should be linked
                # in from other libjingle libs.
                '-fvisibility=hidden',
              ],
            }],
            ['OS=="mac" and libpeer_target_type!="static_library"', {
              'product_name': 'libpeerconnection',
            }],
            ['OS=="android" and "<(libpeer_target_type)"=="static_library"', {
              'standalone_static_library': 1,
            }],
            ['OS=="linux" and libpeer_target_type!="static_library"', {
              # The installer and various tools depend on finding the .so
              # in this directory and not lib.target as will otherwise be
              # the case with make builds.
              'product_dir': '<(PRODUCT_DIR)/lib',
            }],
          ],
        },  # target libpeerconnection
      ],
    }],
  ],
}
