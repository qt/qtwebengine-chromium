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
  'abseil_revision': '5e42a36a85a252d8cdee6c39661d2bfd9883fd5c',
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
  'build_revision': '8a46ede8ea86151737d9c284f82d238b3c701fa7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling buildtools
  # and whatever else without interference from each other.
  'buildtools_revision': '5e7f45b2c8d0acf21ee52a32bb6ec83d4620eb59',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling catapult
  # and whatever else without interference from each other.
  'catapult_revision': '9fd976b2695ea3eee2904c3743b1f412464010f1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang format
  # and whatever else without interference from each other.
  'clang_format_revision': '08cce2b81a4d9801ebeaa8cdb43b241475a4d521',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling clang
  # and whatever else without interference from each other.
  'clang_revision': '3e520fe28bbeed4a87f5e575272933d359915989',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling code_coverage
  # and whatever else without interference from each other.
  'code_coverage_revision': '74d04576e893e08d9d16c99866c7d6696713d554',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling cpu_features
  # and whatever else without interference from each other.
  'cpu_features_revision': 'd3b2440fcfc25fe8e6d0d4a85f06d68e98312f5b',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling depot_tools
  # and whatever else without interference from each other.
  'depot_tools_revision': 'ed3a6fdd519a74da32fb73e1d2ce9c405812cf1a',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling dragonbox
  # and whatever else without interference from each other.
  'dragonbox_revision': 'beeeef91cf6fef89a4d4ba5e95d47ca64ccb3a44',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling fast_float
  # and whatever else without interference from each other.
  'fast_float_revision': '05087a303dad9c98768b33c829d398223a649bc6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling fp16
  # and whatever else without interference from each other.
  'fp16_revision': '3d2de1816307bac63c16a297e8c4dc501b4076df',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling freetype
  # and whatever else without interference from each other.
  'freetype_revision': '6d9fc45fc4bca8aef0b8f65592520673638c3334',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling GN CIPD package version
  # and whatever else without interference from each other.
  'gn_version': 'git_revision:1740f5c25bcac5a650ee3d1c1ec22bfa25fcd756',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling goldctl CIPD package version
  # and whatever else without interference from each other.
  'goldctl_version': 'git_revision:ac4d1516c500f636488b69e0b11a9e126abe28b9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling gtest
  # and whatever else without interference from each other.
  'gtest_revision': '4fe3307fb2d9f86d19777c7eb0e4809e9694dde7',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling HarfBuzz
  # and whatever else without interference from each other.
  'harfbuzz_revision': 'e6741e2205309752839da60ff075b7fa2e7cddd3',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling highway
  # and whatever else without interference from each other.
  'highway_revision': '2607d3b5b0113992fe84d3848859eae13b3b52c1',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling icu
  # and whatever else without interference from each other.
  'icu_revision': 'ff7995a708a10ab44db101358083c7f74752da9f',
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
  'jpeg_turbo_revision': 'd1f5f2393e0d51f840207342ae86e55a86443288',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++
  # and whatever else without interference from each other.
  # If you change this, also update the libc++ revision in
  # //buildtools/deps_revisions.gni.
  'libcxx_revision': 'be1c391acca009d8d80535ce924e3d285451cdfa',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libc++abi
  # and whatever else without interference from each other.
  'libcxxabi_revision': '8f11bb1d4438d0239d0dfc1bd9456a9f31629dda',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libpng
  # and whatever else without interference from each other.
  'libpng_revision': '8a5b4ace0c2a8a47d73037610b98096f55f3d18f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling libunwind
  # and whatever else without interference from each other.
  'libunwind_revision': '71192be150bbe04d87bb5298512d464e38d2f654',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling llvm-libc
  # and whatever else without interference from each other.
  'llvm_libc_revision': 'ada0360457d5a183962831bb3e1f8a059aea5da2',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling markupsafe
  # and whatever else without interference from each other.
  'markupsafe_revision': '4256084ae14175d38a3ff7d739dca83ae49ccec6',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling nasm_source
  # and whatever else without interference from each other.
  'nasm_source_revision': '358842b6b7dd69b2ed635bef17f941e030a05e5f',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling Ninja CIPD package version
  # and whatever else without interference from each other.
  'ninja_version': 'version:3@1.12.1.chromium.4',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling partition_allocator
  # and whatever else without interference from each other.
  'partition_allocator_revision': 'fafdd4c9f559c6d0cfdf2ed3170ce370b59bfdbf',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling pdfium_tests
  # and whatever else without interference from each other.
  'pdfium_tests_revision': 'a0cdeeeac46f1b2272094ee498cd59a30ce1c073',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling result_adapter_revision
  # and whatever else without interference from each other.
  'result_adapter_revision': 'git_revision:5fb3ca203842fd691cab615453f8e5a14302a1d8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling rust
  # and whatever else without interference from each other.
  'rust_revision': 'dbc491d22ee3fe201b3c3c4b1e52beac75732f33',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling simdutf
  # and whatever else without interference from each other.
  'simdutf_revision': 'f7356eed293f8208c40b3c1b344a50bd70971983',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling siso
  # and whatever else without interference from each other.
  'siso_version': 'git_revision:8629cde6728d25cde0f6fc1e6ae6b5886e0ebe41',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling skia
  # and whatever else without interference from each other.
  'skia_revision': 'd10ea850c21d01d0a715d03cb3b5d6dd676a953a',
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
  'tools_memory_revision': '66b0df34a721e0f7fe09d1606a92b5e4d909c4a9',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tools_rust
  # and whatever else without interference from each other.
  'tools_rust_revision': '4442fd3860a2e9ddd753850c917b65ade6d8ad0e',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling tools_win_revision
  # and whatever else without interference from each other.
  'tools_win_revision': 'd16e6b55b2bd699735919d8a13a55ff284086603',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling v8
  # and whatever else without interference from each other.
  'v8_revision': '8e10f842e4b72f181433a3a9c0710f6f2247dcf8',
  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling zlib
  # and whatever else without interference from each other.
  'zlib_revision': '5c1dfd53066bf58d3d28197f715717dd88762443',
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
        'object_name': 'Linux_x64/clang-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '6aa45b7398e915919ce07a5e1e15e8710327957f61c1d97d18e77e5f167e9d14',
        'size_bytes': 69605340,
        'generation': 1777639339258203,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '0f6db77d3bd03bd39add2f13a19072236ae00feddc45dc51dfe976d58dd843de',
        'size_bytes': 14615020,
        'generation': 1777639339339127,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'dbbb792a7893c3477fe2a9e378b6a09ed93dbff41f707941b7c84939ba74649c',
        'size_bytes': 14786588,
        'generation': 1777639339349447,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '1e1f962a845d5246e6b541e25ef6bae50b381ec64243f3cc2aee0b8c7d718a8e',
        'size_bytes': 2336008,
        'generation': 1777639339569555,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'f3701a93e5ad4eae4d3aa0c853dd71a4e776d8f9cec5db3e0c800e1843d5a9a4',
        'size_bytes': 5793432,
        'generation': 1777639339446398,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'd25493bb0c74e7aad55135b0aceed9dcca22d6477043cb005aa3ccce7d708b83',
        'size_bytes': 55341104,
        'generation': 1777639341141424,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '2f1677e29b78c584450ce6f5d72273f9584ac43d51bc9ca91619b045e93c2c12',
        'size_bytes': 1016564,
        'generation': 1777639349729752,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'f8a6449263f115c9f69371cf415899d186f11790dce0b6022a7b3a583b0716f5',
        'size_bytes': 14567332,
        'generation': 1777639341376334,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'dbd23694bd418555f8a65871f4b862830db1045f785e003822c4124f6da99484',
        'size_bytes': 15995436,
        'generation': 1777639341422572,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'c24cbc8695bb7ed7f5c9709b6ff2925d57c635b27375c374f23860dec9d633ac',
        'size_bytes': 2379552,
        'generation': 1777639341530758,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '98ac084036e387b7da7729fdb4f0dde9724897b52642f2b1f20782968765c24f',
        'size_bytes': 5722328,
        'generation': 1777639341526564,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'f6fd465945dae58eff22819cdc62ccae2271dd3cf624a2fa68d87f634f32dcb7',
        'size_bytes': 46106304,
        'generation': 1777639351295488,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '26d400934fee65cd9af87d0d7c23c3ec086adac93459759d44ced9b7519694a2',
        'size_bytes': 12619892,
        'generation': 1777639351445994,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'cd4aed6e6e89a9616c76de4faee90f335dea9fc69b45b0d00fb9b9eddb9f81b4',
        'size_bytes': 12980784,
        'generation': 1777639351661710,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'b2ff8a6e42fa14d4bb58a03a64962be7230a47dc2f8eeb83ad1d46cddc4d70da',
        'size_bytes': 1992948,
        'generation': 1777639351899002,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'd1e42c8009c5983bbb37867bba8522db876bd32bb76c6e5560bf5134a02f7aaf',
        'size_bytes': 5444568,
        'generation': 1777639351710887,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '7fa1aa9bf477f565687a2f516bf75ff6bfdd13da9b36bf521a75ac8ae2761d6a',
        'size_bytes': 50211688,
        'generation': 1777639362668478,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '36f00ca3c9ae9bbf226dbe5625b42e06ddf9cfe81586e89c6cd96fa1ed99cc7b',
        'size_bytes': 14723216,
        'generation': 1777639362810257,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'f25fb199c927f86083f5b6d2641daac60aa81de9ea1204f38e9301c2889a0dec',
        'size_bytes': 2611460,
        'generation': 1777639371151452,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '0ef91424212a6b4cb6da4fe6a97ffca044b9469a246ae5a2d3d58dd8e02f2e98',
        'size_bytes': 15101300,
        'generation': 1777639362859425,
        'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': 'ab1fe0acdb40306cadde73474437a98483a914f1bfad95d8de4d5d0c3dae2d18',
        'size_bytes': 2500728,
        'generation': 1777639362926842,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-23-init-10931-g20b6ec66-8.tar.xz',
        'sha256sum': '9f3bf71579786784762963c50cdc908ad07213a8b8c7f81edca209c095b130a9',
        'size_bytes': 5868192,
        'generation': 1777639362899299,
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
        'object_name': 'Linux_x64/rust-toolchain-4c4205163abcbd08948b3efab796c543ba1ea687-2-llvmorg-23-init-10931-g20b6ec66.tar.xz',
        'sha256sum': 'a96863c5b811af23cbe3f20fcfc82939e637be2bd79f05a117f1762c3bb35fe5',
        'size_bytes': 274625900,
        'generation': 1776704596417466,
        'condition': 'host_os == "linux" and non_git_source',
      },
      {
        'object_name': 'Mac/rust-toolchain-4c4205163abcbd08948b3efab796c543ba1ea687-2-llvmorg-23-init-10931-g20b6ec66.tar.xz',
        'sha256sum': 'cfb1cfa17fe96540ae732b2ed8cef39792786f7d84c317f3aca2004f2495c3fa',
        'size_bytes': 262623268,
        'generation': 1776704598519695,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/rust-toolchain-4c4205163abcbd08948b3efab796c543ba1ea687-2-llvmorg-23-init-10931-g20b6ec66.tar.xz',
        'sha256sum': 'f7b34f50331e3d22b9b79b8217b3645dd67700ee801bec801da09bcffb583f9c',
        'size_bytes': 245410044,
        'generation': 1776704600641512,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/rust-toolchain-4c4205163abcbd08948b3efab796c543ba1ea687-2-llvmorg-23-init-10931-g20b6ec66.tar.xz',
        'sha256sum': 'bd99ed04453ccef4916211c6db0a7afd1185b69371e1921ebd94f9bec78af73e',
        'size_bytes': 413531112,
        'generation': 1776704602737316,
        'condition': 'host_os == "win"',
      },
    ],
  },

  'third_party/simdutf': {
    'url': Var('chromium_git') + '/chromium/src/third_party/simdutf@' +
        Var('simdutf_revision'),
    'condition': 'checkout_v8',
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
