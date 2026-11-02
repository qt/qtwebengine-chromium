use_relative_paths = True

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'android_ndk_version',
  'build_with_chromium',
  'checkout_android',
  'checkout_skia',
]

vars = {
  # Variable that can be used to support multiple build scenarios, like having
  # Chromium specific targets in a client project's GN file or sync dependencies
  # conditionally etc.
  # Standalone PDFium, by definition, does not build with Chromium.
  'build_with_chromium': False,

  # By default, we should check out everything needed to run on the main
  # pdfium waterfalls. This var can be also be set to 'small', in order to skip
  # things are not strictly needed to build pdfium for development purposes,
  # by adding the following line to the .gclient file inside a solutions entry:
  #      "custom_vars": { "checkout_configuration": "small" },
  # Similarly, this var can be set to 'minimal' to also skip the Skia and V8
  # checkouts for the smallest possible checkout, where some features will not
  # work.
  'checkout_configuration': 'default',

  # By default, don't check out android. Will be overridden by gclient
  # variables.
  # TODO(crbug.com/875037): Remove this once the bug in gclient is fixed.
  'checkout_android': False,

  # Fetch the prebuilt binaries for llvm-cov and llvm-profdata. Needed to
  # process the raw profiles produced by instrumented targets (built with
  # the gn arg 'use_clang_coverage').
  'checkout_clang_coverage_tools': 'False',

  # Fetch clang-tidy into the same bin/ directory as the clang binary.
  'checkout_clang_tidy': 'False',

  # Fetch clangd into the same bin/ directory as the clang binary.
  'checkout_clangd': 'False',

  # Pull in Android native toolchain dependencies, so we can build ARC++
  # support libraries.
  'checkout_android_native_support': 'checkout_android',

  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration != "small" and checkout_configuration != "minimal"',

  # Fetch the rust toolchain.
  #
  # Use a custom_vars section to enable it:
  # "custom_vars": {
  #   "checkout_rust": True,
  # }
  'checkout_rust': False,

  'checkout_skia': 'checkout_configuration != "minimal"',

  'checkout_testing_corpus': 'checkout_configuration != "small" and checkout_configuration != "minimal"',

  'checkout_v8': 'checkout_configuration != "minimal"',

  # condition to allowlist deps for non-git-source processing.
  'non_git_source': 'True',

  # Fetch configuration files required for the 'use_remoteexec' gn arg
  'download_remoteexec_cfg': False,
  # RBE instance to use for running remote builds
  'rbe_instance': Str('projects/rbe-chrome-untrusted/instances/default_instance'),
  # RBE project to download rewrapper config files for. Only needed if
  # different from the project used in 'rbe_instance'
  'rewrapper_cfg_project': Str(''),
  # reclient CIPD package
  'reclient_package': 'infra/rbe/client/',
  # reclient CIPD package version
  'reclient_version': 're_client_version:0.185.0.db415f21-gomaip',

  'chromium_git': 'https://chromium.googlesource.com',
  'pdfium_git': 'https://pdfium.googlesource.com',
  'skia_git': 'https://skia.googlesource.com',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling abseil
  # and whatever else without interference from each other.
  'abseil_revision': 'd8e483edd8b44da1845874ee84b42489589bb90f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling android_ndk
  # and whatever else without interference from each other.
  'android_ndk_version': Str('2@30.0.14608247'),
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling brotli
  # and whatever else without interference from each other.
  'brotli_revision': '803ac71664ad62af1de39ff55e63ebbe07006fc8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling build
  # and whatever else without interference from each other.
  'build_revision': '73c0fa98c5cf963c60ea685c57826fa7ba6253d8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling buildtools
  # and whatever else without interference from each other.
  'buildtools_revision': '0d39be5a3f129cf1f35e7812108a2184e2193315',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '2cdd339993500e67d43bcb139ad6132dfc5f4855',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang format
  # and whatever else without interference from each other.
  'clang_format_revision': '6eddfb5ec5f92127a531eda66c568d3a11e7ec11',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang
  # and whatever else without interference from each other.
  'clang_revision': '66934666a7c7151a24f4ed95e7e94ef8664de057',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling code_coverage
  # and whatever else without interference from each other.
  'code_coverage_revision': '88b5286372e2cb7295f962fe23efa58bf0ff59f8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling cpu_features
  # and whatever else without interference from each other.
  'cpu_features_revision': '81d13c49649f0714dd41fb56bb246398b6584085',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling depot_tools
  # and whatever else without interference from each other.
  'depot_tools_revision': '1709e20d5ca2afe97bfa726168719c3b77eb9883',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling dragonbox
  # and whatever else without interference from each other.
  'dragonbox_revision': 'beeeef91cf6fef89a4d4ba5e95d47ca64ccb3a44',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling fast_float
  # and whatever else without interference from each other.
  'fast_float_revision': '34164f547b7df3f5d794ff67e9f885c36819ebfc',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling fp16
  # and whatever else without interference from each other.
  'fp16_revision': '782eea126dc5c755827be751a099eb01826175cf',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '12f5eb32aedb3e27d47c22b0447dc4363703ed94',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling GN CIPD package version
  # and whatever else without interference from each other.
  'gn_version': 'git_revision:1d86777e7f2562a86ecea77d1809ac4f82bb5bfe',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling goldctl CIPD package version
  # and whatever else without interference from each other.
  'goldctl_version': 'git_revision:39511f7e42627cd90b53273667b9aca539f50bd3',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling gtest
  # and whatever else without interference from each other.
  'gtest_revision': '4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling HarfBuzz
  # and whatever else without interference from each other.
  'harfbuzz_revision': '511df88b82e697cd2a0f1f0635787aa0b18bddbb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling highway
  # and whatever else without interference from each other.
  'highway_revision': '2607d3b5b0113992fe84d3848859eae13b3b52c1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling icu
  # and whatever else without interference from each other.
  'icu_revision': 'd578f2e8b7bd5938e21cfb6bf15c079e0aa5b738',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling instrumented_lib
  # and whatever else without interference from each other.
  'instrumented_lib_revision': 'e8cb570a9a2ee9128e2214c73417ad2a3c47780b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling jinja2
  # and whatever else without interference from each other.
  'jinja2_revision': 'c3027d884967773057bf74b957e3fea87e5df4d7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling jpeg_turbo
  # and whatever else without interference from each other.
  'jpeg_turbo_revision': '640f254ad0fa03f6b1f29f89b7dd9366f2f6e533',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++
  # and whatever else without interference from each other.
  # If you change this, also update the libc++ revision in
  # //buildtools/deps_revisions.gni.
  'libcxx_revision': '5abc7f839700f0f17338434e1c1c6a8c87c00c11',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++abi
  # and whatever else without interference from each other.
  'libcxxabi_revision': '8f11bb1d4438d0239d0dfc1bd9456a9f31629dda',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libpng
  # and whatever else without interference from each other.
  'libpng_revision': '6d5341764ef4e38cbc07d35514a8aa73de50de8a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libunwind
  # and whatever else without interference from each other.
  'libunwind_revision': '1b601b7f07cd1511b4ba52bfe0625387f51f72a7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling llvm-libc
  # and whatever else without interference from each other.
  'llvm_libc_revision': '9a821f4e58bbb7f6701f9951d087f8750f523647',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling markupsafe
  # and whatever else without interference from each other.
  'markupsafe_revision': '4256084ae14175d38a3ff7d739dca83ae49ccec6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling nasm_source
  # and whatever else without interference from each other.
  'nasm_source_revision': '525a09a813be0f75b646ee93fc2a31c27b87d722',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Ninja CIPD package version
  # and whatever else without interference from each other.
  'ninja_version': 'version:3@1.12.1.chromium.4',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling partition_allocator
  # and whatever else without interference from each other.
  'partition_allocator_revision': '2723da2d1b2cc49feb9848a6e44fdf9681b8c123',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling pdfium_tests
  # and whatever else without interference from each other.
  'pdfium_tests_revision': 'cfac443cd7d0c5961815f36a63b5989b0a3fdf86',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling result_adapter_revision
  # and whatever else without interference from each other.
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling rust
  # and whatever else without interference from each other.
  'rust_revision': '9bcbc576725c2fe6cf9c3b4c9db8aefbf9d99553',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling simdutf
  # and whatever else without interference from each other.
  'simdutf_revision': 'f7356eed293f8208c40b3c1b344a50bd70971983',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling siso
  # and whatever else without interference from each other.
  'siso_version': 'git_revision:24b80aff540b202a88417f9f93b0dc86229eca7a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling skia
  # and whatever else without interference from each other.
  'skia_revision': '3ba832f8688f6215c4591941355f9e6b1070a012',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling test_fonts
  # and whatever else without interference from each other.
  'test_fonts_revision': '7f51783942943e965cd56facf786544ccfc07713',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling testing_rust
  # and whatever else without interference from each other.
  'testing_rust_revision': '6712dc59f4a6c5626f391057cded3842700a17eb',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tools_memory
  # and whatever else without interference from each other.
  'tools_memory_revision': 'a7e928b8bb8d79aa2feb809c1bd4752eecc68802',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tools_rust
  # and whatever else without interference from each other.
  'tools_rust_revision': '1607679eb7fa1df8841b90b911b11315cd4e31dd',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tools_win_revision
  # and whatever else without interference from each other.
  'tools_win_revision': '45843c2c1e993427751e2a07f904db069dc26ad6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling v8
  # and whatever else without interference from each other.
  'v8_revision': '7ed96d92b72bea98ff3a84513dfaf73c40681227',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling zlib
  # and whatever else without interference from each other.
  'zlib_revision': '8b3aa8a1cd7585c0c4c67351481227b046a662a0',
}

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, and the new host is not in Chromium's DEPS
# file's allowed_hosts list, contact Chrome infrastructure team.
allowed_hosts = [
  'chromium.googlesource.com',
  'pdfium.googlesource.com',
  'skia.googlesource.com',

   # TODO(337061377): Move into a separate allowed gcs bucket list.
  'chromium-browser-clang',
]

deps = {
  'base/allocator/partition_allocator':
    Var('chromium_git') +
        '/chromium/src/base/allocator/partition_allocator.git@' +
        Var('partition_allocator_revision'),

  'build':
    Var('chromium_git') + '/chromium/src/build.git@' + Var('build_revision'),

  'buildtools':
    Var('chromium_git') + '/chromium/src/buildtools.git@' +
        Var('buildtools_revision'),

  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },

  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },

  'buildtools/reclient': {
    'packages': [
      {
        'package': Var('reclient_package') + '${{platform}}',
        'version': Var('reclient_version'),
      }
    ],
    'dep_type': 'cipd',
  },

  'buildtools/win': {
    'packages': [
      {
        'package': 'gn/gn/windows-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },

  'testing/corpus': {
    'url': Var('pdfium_git') + '/pdfium_tests@' + Var('pdfium_tests_revision'),
    'condition': 'checkout_testing_corpus',
  },

  'testing/scripts/rust': {
    'url': Var('chromium_git') + '/chromium/src/testing/scripts/rust.git@' +
        Var('testing_rust_revision'),
    'condition': 'checkout_rust',
  },

  'third_party/abseil-cpp':
    Var('chromium_git') + '/chromium/src/third_party/abseil-cpp.git@' +
        Var('abseil_revision'),

  'third_party/android_toolchain/ndk': {
    'packages': [
      {
        'package': 'chromium/third_party/android_toolchain/android_toolchain',
        'version': 'version:' + Var('android_ndk_version'),
      },
    ],
    'condition': 'checkout_android_native_support',
    'dep_type': 'cipd',
  },

  'third_party/brotli':
    Var('chromium_git') + '/chromium/src/third_party/brotli.git@' +
        Var('brotli_revision'),

  'third_party/catapult': {
    'url': Var('chromium_git') + '/catapult.git@' + Var('catapult_revision'),
    'condition': 'checkout_android',
  },

  'third_party/clang-format/script':
    Var('chromium_git') +
        '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git@' +
        Var('clang_format_revision'),

  'third_party/cpu_features/src': {
    'url': Var('chromium_git') +
        '/external/github.com/google/cpu_features.git@' +
        Var('cpu_features_revision'),
    'condition': 'checkout_android',
  },

  'third_party/depot_tools':
    Var('chromium_git') + '/chromium/tools/depot_tools.git@' +
        Var('depot_tools_revision'),

  'third_party/dragonbox/src': {
    'url': Var('chromium_git') + '/external/github.com/jk-jeon/dragonbox.git@' +
        Var('dragonbox_revision'),
    'condition': 'checkout_v8',
  },

  'third_party/fast_float/src':
    Var('chromium_git') + '/external/github.com/fastfloat/fast_float.git@' +
        Var('fast_float_revision'),

  'third_party/fp16/src':
    Var('chromium_git') + '/external/github.com/Maratyszcza/FP16.git@' +
        Var('fp16_revision'),

  'third_party/freetype/src':
    Var('chromium_git') + '/chromium/src/third_party/freetype2.git@' +
        Var('freetype_revision'),

  'third_party/googletest/src':
    Var('chromium_git') + '/external/github.com/google/googletest.git@' +
        Var('gtest_revision'),

  'third_party/harfbuzz/src':
    Var('chromium_git') + '/external/github.com/harfbuzz/harfbuzz.git@' +
        Var('harfbuzz_revision'),

  'third_party/highway/src': {
    'url': Var('chromium_git') + '/external/github.com/google/highway.git@' +
        Var('highway_revision'),
    'condition': 'checkout_v8',
  },

  'third_party/icu':
    Var('chromium_git') + '/chromium/deps/icu.git@' + Var('icu_revision'),

  'third_party/instrumented_libs':
    Var('chromium_git') +
        '/chromium/third_party/instrumented_libraries.git@' +
        Var('instrumented_lib_revision'),

  'third_party/jinja2':
    Var('chromium_git') + '/chromium/src/third_party/jinja2.git@' +
        Var('jinja2_revision'),

  'third_party/libc++/src':
    Var('chromium_git') +
        '/external/github.com/llvm/llvm-project/libcxx.git@' +
        Var('libcxx_revision'),

  'third_party/libc++abi/src':
    Var('chromium_git') +
        '/external/github.com/llvm/llvm-project/libcxxabi.git@' +
        Var('libcxxabi_revision'),

  'third_party/libunwind/src': {
    'url': Var('chromium_git') +
        '/external/github.com/llvm/llvm-project/libunwind.git@' +
        Var('libunwind_revision'),
    'condition': 'checkout_android',
  },

  'third_party/libjpeg_turbo':
    Var('chromium_git') + '/chromium/deps/libjpeg_turbo.git@' +
        Var('jpeg_turbo_revision'),

  'third_party/libpng':
    Var('chromium_git') + '/chromium/src/third_party/libpng.git@' +
        Var('libpng_revision'),

  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'e22e06c05fe1657f48f988b15804b8226e691addb00abba5b984a5c99ac98c42',
        'size_bytes': 59093892,
        'generation': 1781627382446731,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '96da04c2fff4e580ac81b840405edfa292108d0b593927a10750c0a1d8599c0a',
        'size_bytes': 14808648,
        'generation': 1781627382496605,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '1e6b6918bb270659ef517a3e8f80af8f371bc79a9d942241978db8faea22f152',
        'size_bytes': 15001372,
        'generation': 1781627382490030,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'bb63e33c20329c344a9dfa958f3742b316c0b9dd602647190fb0037d8d53a7e6',
        'size_bytes': 2332356,
        'generation': 1781627382888550,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'e43b98bce71d58bbe1b456f7b62997c4d017cf8362a0367592428ac5a7512f41',
        'size_bytes': 5875704,
        'generation': 1781627382574869,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '84b3934db2fb3c2e657d4a783a83ca6d2facaf598991490ae8ab712fcb03224b',
        'size_bytes': 56083264,
        'generation': 1781627385112611,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '5079e2ac3f0fa76e8b0218bf99a65a5439e8dcf6a750f886a355a66df34c69c1',
        'size_bytes': 1010816,
        'generation': 1781627394919334,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '1ca3dc160992d8c27765adaffcc8115aed34cdf7645a5354820e8c8b16b75dcd',
        'size_bytes': 14790996,
        'generation': 1781627384977799,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '4920275e6050dffbb81ba0210a627a61a20faa896aa50f5ecc2d765522790526',
        'size_bytes': 16720488,
        'generation': 1781627385229783,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '07e8bf3354e0c086e420ab8f1f376e48c8b6207788cf30b56d1b0ad1fd4b0f12',
        'size_bytes': 2372720,
        'generation': 1781627385661382,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '9b286eecf04996dfa82cd1434bceb19612fa0754e4390649ba8dcd846dca9c1e',
        'size_bytes': 5797028,
        'generation': 1781627385345493,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'e96c5e31ddc2a6e841bcb0f7278ae82b3a581799904276e6f7673213c9748c27',
        'size_bytes': 47058068,
        'generation': 1781627397208650,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '27eff49b49d393c903246d5e17ba620f62f493a8a39cb6dbdd2e3f5f06c592a2',
        'size_bytes': 12835944,
        'generation': 1781627397575694,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '9519e51006f45a797a351b616d92bff319793ab976eeb59df60901d6181f3c18',
        'size_bytes': 13205948,
        'generation': 1781627397237653,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '62084a5523a1c3b140166c2ed5a21bc110ceb65503f2759272a9f1feb00b503e',
        'size_bytes': 2000216,
        'generation': 1781627397451117,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'b6c6c0b1b4147ba4d51603713d3ed21f88ee2f506ff07fb432b3444f1d323295',
        'size_bytes': 5548680,
        'generation': 1781627397409118,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '9e3894b94d0d5e3d5904a559f590f12bd53aa5d1d9b6a902de2acb957825de46',
        'size_bytes': 51306328,
        'generation': 1781627409675428,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '8fa52c3d1707e73d0689aaa3c8d3789f13e85526bef029d942baf44ed980b442',
        'size_bytes': 14867152,
        'generation': 1781627409620988,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': 'f925c30e1f63291662b2ad91103592208ecc4354f669c98a07e35360d2561a13',
        'size_bytes': 2623388,
        'generation': 1781627419134531,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '54042464918d992a19efed3a5f2a170e6fc2e4551884b4870650e3a3a6a03b41',
        'size_bytes': 15262064,
        'generation': 1781627409708803,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '7c4fbafcf2741a036d1f4058fc31b52cb5b0951e617561fc3d51b7fbe0a3044b',
        'size_bytes': 2496336,
        'generation': 1781627409916932,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-23-init-19482-g53d18800-1.tar.xz',
        'sha256sum': '7ea19d03f21ef59d2e511af8d9cccff20ea72deb54923557173bb46a64244fa7',
        'size_bytes': 5934284,
        'generation': 1781627409787893,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  'third_party/llvm-libc/src':
    Var('chromium_git') + '/external/github.com/llvm/llvm-project/libc.git@' +
        Var('llvm_libc_revision'),

  'third_party/markupsafe':
    Var('chromium_git') + '/chromium/src/third_party/markupsafe.git@' +
        Var('markupsafe_revision'),

  'third_party/nasm':
    Var('chromium_git') + '/chromium/deps/nasm.git@' +
        Var('nasm_source_revision'),

  'third_party/ninja': {
    'packages': [
      {
        # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': Var('ninja_version'),
      }
    ],
    'dep_type': 'cipd',
  },

  'third_party/rust': {
    'url': Var('chromium_git') + '/chromium/src/third_party/rust@' +
        Var('rust_revision'),
    'condition': 'checkout_rust',
  },

  'third_party/rust-toolchain': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        'object_name': 'Linux_x64/rust-toolchain-b998449636a48e2c4a362809085b600a0174e1f2-2-llvmorg-23-init-19482-g53d18800.tar.xz',
        'sha256sum': '88db954bdbaea527bebf832758d6ff8193eb214877b4eef362e97701d64c0b54',
        'size_bytes': 274790504,
        'generation': 1782418940816837,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-b998449636a48e2c4a362809085b600a0174e1f2-2-llvmorg-23-init-19482-g53d18800.tar.xz',
        'sha256sum': '728aaa01b049cd5252915db4ed8a7492955b3d5d3939990f18e6b085cf8ad4ad',
        'size_bytes': 262355184,
        'generation': 1782418942567956,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-b998449636a48e2c4a362809085b600a0174e1f2-2-llvmorg-23-init-19482-g53d18800.tar.xz',
        'sha256sum': '00a11ecce7cebc2ce777518e6e51a150d980dd5f41392200b65f543767536b56',
        'size_bytes': 246777680,
        'generation': 1782418944301166,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-b998449636a48e2c4a362809085b600a0174e1f2-2-llvmorg-23-init-19482-g53d18800.tar.xz',
        'sha256sum': '482c5a8946869aae595adc7361b619e8af62dc002cd8f3a00b27fe59db4d74f3',
        'size_bytes': 415939796,
        'generation': 1782418946203516,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'third_party/simdutf': {
    'url': Var('chromium_git') + '/chromium/src/third_party/simdutf@' +
        Var('simdutf_revision'),
  },

  'third_party/siso/cipd': {
    'packages': [
      {
        'package': 'build/siso/${{platform}}',
        'version': Var('siso_version'),
      }
    ],
    'dep_type': 'cipd',
  },

  'third_party/skia': {
    'url': Var('skia_git') + '/skia.git@' + Var('skia_revision'),
    'condition': 'checkout_skia',
  },

  'third_party/test_fonts':
    Var('chromium_git') + '/chromium/src/third_party/test_fonts.git@' +
        Var('test_fonts_revision'),

  'third_party/zlib':
    Var('chromium_git') + '/chromium/src/third_party/zlib.git@' +
        Var('zlib_revision'),

  'tools/clang':
    Var('chromium_git') + '/chromium/src/tools/clang@' + Var('clang_revision'),

  'tools/code_coverage':
    Var('chromium_git') + '/chromium/src/tools/code_coverage.git@' +
        Var('code_coverage_revision'),

  'tools/memory':
    Var('chromium_git') + '/chromium/src/tools/memory@' +
        Var('tools_memory_revision'),

  'tools/resultdb': {
    'packages': [
      {
        'package': 'infra/tools/result_adapter/${{platform}}',
        'version': Var('result_adapter_revision'),
      },
    ],
    'dep_type': 'cipd',
  },

  'tools/rust': {
    'url': Var('chromium_git') + '/chromium/src/tools/rust@' +
        Var('tools_rust_revision'),
    'condition': 'checkout_rust',
  },

  'tools/skia_goldctl/linux': {
    'packages': [
      {
        'package': 'skia/tools/goldctl/linux-amd64',
        'version': Var('goldctl_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_linux',
  },

  'tools/skia_goldctl/mac_amd64': {
    'packages': [
      {
        'package': 'skia/tools/goldctl/mac-amd64',
        'version': Var('goldctl_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },

  'tools/skia_goldctl/mac_arm64': {
    'packages': [
      {
        'package': 'skia/tools/goldctl/mac-arm64',
        'version': Var('goldctl_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_mac',
  },

  'tools/skia_goldctl/win': {
    'packages': [
      {
        'package': 'skia/tools/goldctl/windows-amd64',
        'version': Var('goldctl_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'checkout_win',
  },

  'tools/win': {
    'url': Var('chromium_git') + '/chromium/src/tools/win.git@' +
        Var('tools_win_revision'),
    'condition': 'checkout_win',
  },

  'v8': {
    'url': Var('chromium_git') + '/v8/v8.git@' + Var('v8_revision'),
    'condition': 'checkout_v8',
  },

}

recursedeps = [
  'build',
  'buildtools',
  'third_party/instrumented_libs',
]

include_rules = [
  # Basic stuff that everyone can use.
  # Note: public is not here because core cannot depend on public.
  '+build/build_config.h',
  '+constants',
  '+testing',

  # Abseil is allowed by default, but some features are banned. See Chromium's
  # //styleguide/c++/c++-features.md.
  '+third_party/abseil-cpp',
  '-third_party/abseil-cpp/absl/algorithm/container.h',
  '-third_party/abseil-cpp/absl/base/attributes.h',
  '-third_party/abseil-cpp/absl/base/no_destructor.h',
  '-third_party/abseil-cpp/absl/base/nullability.h',
  '-third_party/abseil-cpp/absl/container/btree_map.h',
  '-third_party/abseil-cpp/absl/container/btree_set.h',
  '-third_party/abseil-cpp/absl/flags',
  '-third_party/abseil-cpp/absl/functional/any_invocable.h',
  '-third_party/abseil-cpp/absl/functional/bind_front.h',
  '-third_party/abseil-cpp/absl/functional/function_ref.h',
  '-third_party/abseil-cpp/absl/hash',
  '-third_party/abseil-cpp/absl/log',
  '-third_party/abseil-cpp/absl/random',
  '-third_party/abseil-cpp/absl/status/statusor.h',
  '-third_party/abseil-cpp/absl/strings',
  '+third_party/abseil-cpp/absl/strings/ascii.h',
  '+third_party/abseil-cpp/absl/strings/cord.h',
  '+third_party/abseil-cpp/absl/strings/str_format.h',
  '-third_party/abseil-cpp/absl/synchronization',
  '-third_party/abseil-cpp/absl/time',
  '-third_party/abseil-cpp/absl/types/any.h',
  '-third_party/abseil-cpp/absl/types/optional.h',
  '-third_party/abseil-cpp/absl/types/span.h',
  '-third_party/abseil-cpp/absl/types/variant.h',
  '-third_party/abseil-cpp/absl/utility/utility.h',
]

specific_include_rules = {
  # Allow embedder tests to use public APIs.
  '(.*embeddertest\.cpp)': [
    '+public',
  ]
}

hooks = [
  {
    # Ensure that the DEPS'd "depot_tools" has its self-update capability
    # disabled.
    'name': 'disable_depot_tools_selfupdate',
    'pattern': '.',
    'action': [ 'python3',
                'third_party/depot_tools/update_depot_tools_toggle.py',
                '--disable',
    ],
  },
  {
    # Case-insensitivity for the Win SDK. Must run before win_toolchain below.
    'name': 'ciopfs_linux',
    'pattern': '.',
    'condition': 'checkout_win and host_os == "linux"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang/ciopfs',
                '-s', 'build/ciopfs.sha1',
    ]
  },
  {
    # Update the Windows toolchain if necessary.  Must run before 'clang' below.
    'name': 'win_toolchain',
    'pattern': '.',
    'condition': 'checkout_win',
    'action': ['python3', 'build/vs_toolchain.py', 'update', '--force'],
  },
  {
    # Update the Mac toolchain if necessary.
    'name': 'mac_toolchain',
    'pattern': '.',
    'condition': 'checkout_mac',
    'action': ['python3', 'build/mac_toolchain.py'],
  },
  # Pull dsymutil binaries using checked-in hashes.
  {
    'name': 'dsymutil_mac_arm64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.arm64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'dsymutil_mac_x64',
    'pattern': '.',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--bucket', 'chromium-browser-clang',
                '-s', 'tools/clang/dsymutil/bin/dsymutil.x64.sha1',
                '-o', 'tools/clang/dsymutil/bin/dsymutil',
    ],
  },
  {
    'name': 'test_fonts',
    'pattern': '.',
    'action': [ 'python3',
                'third_party/depot_tools/download_from_google_storage.py',
                '--no_resume',
                '--extract',
                '--bucket', 'chromium-fonts',
                '-s', 'third_party/test_fonts/test_fonts.tar.gz.sha1',
    ],
  },
  {
    # Update LASTCHANGE.
    'name': 'lastchange',
    'pattern': '.',
    'action': ['python3', 'build/util/lastchange.py',
               '-o', 'build/util/LASTCHANGE'],
  },
  # Configure remote exec cfg files
  {
    # Use luci_auth if on windows and using chrome-untrusted project
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and host_os == "win"',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--use_luci_auth_credshelper',
               '--quiet',
               ],
  },  {
    'name': 'download_and_configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'download_remoteexec_cfg and not host_os == "win"',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--quiet',
               ],
  },
  {
    'name': 'configure_reclient_cfgs',
    'pattern': '.',
    'condition': 'not download_remoteexec_cfg',
    'action': ['python3',
               'buildtools/reclient_cfgs/configure_reclient_cfgs.py',
               '--rbe_instance',
               Var('rbe_instance'),
               '--reproxy_cfg_template',
               'reproxy.cfg.template',
               '--rewrapper_cfg_project',
               Var('rewrapper_cfg_project'),
               '--skip_remoteexec_cfg_fetch',
               '--quiet',
               ],
  },
  # Configure Siso for developer builds.
  {
    'name': 'configure_siso',
    'pattern': '.',
    'action': ['python3',
               'build/config/siso/configure_siso.py',
               '--rbe_instance',
               Var('rbe_instance'),
               ],
  },
]
