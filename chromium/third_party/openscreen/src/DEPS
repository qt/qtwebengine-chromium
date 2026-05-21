# This file is used to manage the dependencies of the Open Screen repo. It is
# used by gclient to determine what version of each dependency to check out.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.

use_relative_paths = True
git_dependencies = 'SYNC'

gclient_gn_args_file = 'build/config/gclient_args.gni'
gclient_gn_args = [
  'build_with_chromium',
]

vars = {
  'boringssl_git': 'https://boringssl.googlesource.com',
  'chromium_git': 'https://chromium.googlesource.com',
  'quiche_git': 'https://quiche.googlesource.com',

  # NOTE: we should only reference GitHub directly for dependencies toggled
  # with the "not build_with_chromium" condition.
  'github': 'https://github.com',

  # NOTE: Strangely enough, this will be overridden by any _parent_ DEPS, so
  # in Chromium it will correctly be True.
  'build_with_chromium': False,

  # Needed to download additional clang binaries for processing coverage data
  # (from binaries with GN arg `use_coverage=true`).
  #
  # TODO(issuetracker.google.com/155195126): Change this to False and update
  # buildbot to call tools/download-clang-update-script.py instead.
  'checkout_clang_coverage_tools': True,

 # Fetch clang-tidy into the same bin/ directory as our clang binary.
  'checkout_clang_tidy': False,

  # Fetch clangd into the same bin/ directory as our clang binary.
  'checkout_clangd': False,

  # Fetch instrumented libraries for using MSAN builds.
  'checkout_configuration': 'default',
  'checkout_instrumented_libraries': 'checkout_linux and checkout_configuration == "default"',

  # GN CIPD package version.
  'gn_version': 'git_revision:c5a0003bcc2ac3f8d128aaffd700def6068e9a76',
  'clang_format_revision': '37f6e68a107df43b7d7e044fd36a13cbae3413f2',

  # Chrome version to pull clang update.py script from. This is necessary
  # because this script does experience breaking changes, such as removing
  # command line arguments, that need to be handled intentionally by a roll.
  'chrome_version': '4a1c93eb7da3e438ea5cb677c783379a282ed75d',

  # 'magic' text to tell depot_tools that git submodules should be accepted
  # but parity with DEPS file is expected.
  'SUBMODULE_MIGRATION': 'True',

  # condition to allowlist deps to be synced in Cider. Allowlisting is needed
  # because not all deps are compatible with Cider. Once we migrate everything
  # to be compatible we can get rid of this allowlisting mecahnism and remove
  # this condition. Tracking bug for removing this condition: b/349365433
  'non_git_source': 'True',

  # This can be overridden, e.g. with custom_vars, to build clang from HEAD
  # instead of downloading the prebuilt pinned revision.
  'llvm_force_head_revision': False,
}

deps = {
  # A mirror of the corresponding folder in Chromium maintained here:
  # https://chromium.googlesource.com/chromium/src/buildtools/+/refs/heads/main
  #
  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'buildtools': {
    'url': Var('chromium_git') + '/chromium/src/buildtools' +
      '@' + 'eca5f0685c48ed59ff06077cb18cee00934249dd',
  },

  # A mirror of the corresponding folder in Chromium maintained here:
  # https://chromium.googlesource.com/chromium/src/build/+/refs/heads/main
  'build': {
    'url': Var('chromium_git') + '/chromium/src/build' +
      '@' + 'c53d22a398b881e70e53a972e285a925337a2494',
    'condition': 'not build_with_chromium',
  },

  'third_party/clang-format/script': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/clang/tools/clang-format.git' +
      '@' + Var('clang_format_revision'),
    'condition': 'not build_with_chromium',
  },
  'buildtools/linux64': {
    'packages': [
      {
        'package': 'gn/gn/linux-amd64',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux" and not build_with_chromium',
  },
  'buildtools/mac': {
    'packages': [
      {
        'package': 'gn/gn/mac-${{arch}}',
        'version': Var('gn_version'),
      }
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac" and not build_with_chromium',
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

  'third_party/ninja': {
    'packages': [
      # https://chrome-infra-packages.appspot.com/p/infra/3pp/tools/ninja
      {
        'package': 'infra/3pp/tools/ninja/${{platform}}',
        'version': 'version:2@1.12.1.chromium.4',
      }
    ],
    'dep_type': 'cipd',
    'condition': 'not build_with_chromium',
  },

  'third_party/libprotobuf-mutator/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/libprotobuf-mutator.git' +
      '@' + 'a304ec48dcf15d942607032151f7e9ee504b5dcf',
    'condition': 'not build_with_chromium',
  },

  'third_party/zlib/src': {
    'url': Var('github') +
      '/madler/zlib.git' +
      '@' + '51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf', # version 1.3.1
    'condition': 'not build_with_chromium',
  },

  'third_party/jsoncpp/src': {
    'url': Var('chromium_git') +
      '/external/github.com/open-source-parsers/jsoncpp.git' +
      '@' + '9af09c4a4abe5928d1f7a6e7ec1c73a565bb362e',
    'condition': 'not build_with_chromium',
  },

  # googletest now recommends "living at head," which is a bit of a crapshoot
  # because regressions land upstream frequently.  This is a known good revision.
  'third_party/googletest/src': {
    'url': Var('chromium_git') +
      '/external/github.com/google/googletest.git' +
      '@' + 'eb2d85edd0bff7a712b6aff147cd9f789f0d7d0b',  # 2025-08-28
    'condition': 'not build_with_chromium',
  },

  # Make sure to also update ./third_party/boringssl/README.chromium's
  # `Revision:` field when updating this dependency.
  'third_party/boringssl/src': {
    'url' : Var('boringssl_git') + '/boringssl.git' +
      '@' + '26e8a8acb91a0cfbd2f95bf7245e2eb87d533a2f',
    'condition': 'not build_with_chromium',
  },

  # To roll forward, typically it is best to match Chrome's version by using
  # quiche_revision from chromium/src/DEPS. Coordination with the QUICHE
  # maintainers may be needed for some breaking changes.
  'third_party/quiche/src': {
    'url': Var('quiche_git') + '/quiche.git' +
      '@' + 'd54ca111b1085b9fea302560b41e371df349061c',  # 2025-12-19
    'condition': 'not build_with_chromium',
  },

  'third_party/instrumented_libs': {
    'url': Var('chromium_git') + '/chromium/third_party/instrumented_libraries.git' +
      '@' + '69015643b3f68dbd438c010439c59adc52cac808',
    'condition': 'not build_with_chromium',
  },

  'third_party/tinycbor/src':
    Var('chromium_git') + '/external/github.com/intel/tinycbor.git' +
    '@' +  'd393c16f3eb30d0c47e6f9d92db62272f0ec4dc7',  # Version 0.6.0

  # Abseil recommends living at head; we take a revision from one of the LTS
  # tags.  Chromium has forked abseil for reasons and it seems to be rolled
  # frequently, but LTS should generally be safe for interop with Chromium code.
  'third_party/abseil/src': {
    'url': Var('chromium_git') +
      '/external/github.com/abseil/abseil-cpp.git' + '@' +
      '987c57f325f7fa8472fa84e1f885f7534d391b0d',  # 2025-11-11
    'condition': 'not build_with_chromium',
  },

  'third_party/libfuzzer/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/compiler-rt/lib/fuzzer.git' +
      '@' + 'bea408a6e01f0f7e6c82a43121fe3af4506c932e',
    'condition': 'not build_with_chromium',
  },

  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'third_party/libc++/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxx.git' + '@' + '07572e7b169225ef3a999584cba9d9004631ae66',
    'condition': 'not build_with_chromium',
  },

  # IMPORTANT: Read the instructions at docs/roll_deps.md
  'third_party/libc++abi/src': {
    'url': Var('chromium_git') +
    '/external/github.com/llvm/llvm-project/libcxxabi.git' + '@' + '83a852080747b9a362e8f9e361366b7a601f302c',
    'condition': 'not build_with_chromium',
  },

  'third_party/llvm-build/Release+Asserts': {
    'dep_type': 'gcs',
    'bucket': 'chromium-browser-clang',
    'objects': [
      {
        # The Android libclang_rt.builtins libraries are currently only included in the Linux clang package.
        'object_name': 'Linux_x64/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '1ef7b1d60fb433100c27b4552b44577ab86ef5394531d1fbebc237db64a893fd',
        'size_bytes': 56552908,
        'generation': 1762971374100697,
        'condition': '(host_os == "linux" or checkout_android) and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clang-tidy-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '505f0fa190dc3266f36f7908f46d4e2514b7b5edab02a25dbd721fb7f28dffd8',
        'size_bytes': 14268616,
        'generation': 1762971374302563,
        'condition': 'host_os == "linux" and checkout_clang_tidy and non_git_source',
      },
      {
        'object_name': 'Linux_x64/clangd-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'cc0fe5e6f78a6d70234aa5fc9010761e63f283b8ad24e8194529c4677f723fdd',
        'size_bytes': 14443332,
        'generation': 1762971374370609,
        'condition': 'host_os == "linux" and checkout_clangd and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvm-code-coverage-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'd5e60668fe312a345637b6c7918715ea54bb7078aa1bed1115dc382f955979d6',
        'size_bytes': 2304960,
        'generation': 1762971374620627,
        'condition': 'host_os == "linux" and checkout_clang_coverage_tools and non_git_source',
      },
      {
        'object_name': 'Linux_x64/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'ec1d88867045b8348659f7a8f677d12aa91d7d61a68603a82bad1926bf57c3b0',
        'size_bytes': 5723188,
        'generation': 1762971374436694,
        'condition': '((checkout_linux or checkout_mac or checkout_android) and host_os == "linux") and non_git_source',
      },
      {
        'object_name': 'Mac/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'f266b79576d4fc0075e9380b68b8879ec2bc9617c973e7bdea694ec006f43636',
        'size_bytes': 54056416,
        'generation': 1762971376161293,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac/clang-mac-runtime-library-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '6f2d61383a3c0ab28286e5a57b7e755eb14726bb9a73a7737b685488eae18b90',
        'size_bytes': 1010052,
        'generation': 1762971385382392,
        'condition': 'checkout_mac and not host_os == "mac"',
      },
      {
        'object_name': 'Mac/clang-tidy-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '8e5157522a2557e14d8a456a1c227ebc522f383738f498c1451af3a98f361f99',
        'size_bytes': 14299120,
        'generation': 1762971376313425,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac/clangd-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '399d8930899c2f9bfb9bbcf841b9ca237d876e818a54b04256c790e6a2cb14c2',
        'size_bytes': 15832668,
        'generation': 1762971376411558,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clangd',
      },
      {
        'object_name': 'Mac/llvm-code-coverage-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '5b4d56a772e6128c98e7f880de5a052869334f39b59b81fee7079d56cd6bcfd4',
        'size_bytes': 2338512,
        'generation': 1762971376592644,
        'condition': 'host_os == "mac" and host_cpu == "x64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '9a282bf252e0c7ac88152844f347428e02970aa22941fb583439ce72134f0161',
        'size_bytes': 5607404,
        'generation': 1762971376526568,
        'condition': 'host_os == "mac" and host_cpu == "x64"',
      },
      {
        'object_name': 'Mac_arm64/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'a7b7caf53f4e722234e85aecfdbb3eeb94608c37394672bebd074d6b2f300362',
        'size_bytes': 45184380,
        'generation': 1762971386895625,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Mac_arm64/clang-tidy-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'bb3750fb501048c7ec2d145e69236b87bfe016bd3b81251c0e12f220c00d5875',
        'size_bytes': 12313940,
        'generation': 1762971387031271,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_tidy',
      },
      {
        'object_name': 'Mac_arm64/clangd-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '5b585c910a8eb3f251a1efc76bc27fd63bcb4ebe99671f434f5d7fbfe76604c3',
        'size_bytes': 12690748,
        'generation': 1762971387200930,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clangd',
      },
      {
        'object_name': 'Mac_arm64/llvm-code-coverage-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '044fec98aa72c1f4aebdc454a2bcc8d19735357e9f255d6fc01aae25c1369d41',
        'size_bytes': 1970340,
        'generation': 1762971387351744,
        'condition': 'host_os == "mac" and host_cpu == "arm64" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Mac_arm64/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'c5ee70e78ae5aa7a0d9b613ea5a8e21629438f12acb50bca0f7e18fae6abfe0a',
        'size_bytes': 5353832,
        'generation': 1762971387217357,
        'condition': 'host_os == "mac" and host_cpu == "arm64"',
      },
      {
        'object_name': 'Win/clang-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '483b9b2809c3f53b9640e77d83ca6ab3017a0974979d242198abf23d99639e62',
        'size_bytes': 48337640,
        'generation': 1762971401378315,
        'condition': 'host_os == "win"',
      },
      {
        'object_name': 'Win/clang-tidy-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'd40723233f6d59b1ba64cd7600d4da7b67a7433d81c32be3806ff1c47c9794aa',
        'size_bytes': 14255432,
        'generation': 1762971401522927,
        'condition': 'host_os == "win" and checkout_clang_tidy',
      },
      {
        'object_name': 'Win/clang-win-runtime-library-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': 'd8b3310760c3a8f5dac4801583f7872601f4ba312742b0bf530f043ce6b6f36f',
        'size_bytes': 2520664,
        'generation': 1762971410370409,
        'condition': 'checkout_win and not host_os == "win"',
      },
      {
        'object_name': 'Win/clangd-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '563b9f1a82980634657b89bb61ae5c6d386c8199acc01b84fb57cdcd0a53e1d1',
        'size_bytes': 14641972,
        'generation': 1762971401646458,
       'condition': 'host_os == "win" and checkout_clangd',
      },
      {
        'object_name': 'Win/llvm-code-coverage-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '7caa4ecfbc320bf993640dcaa58882433a0adcc266adf26798f45d28d6d73af8',
        'size_bytes': 2385732,
        'generation': 1762971401865919,
        'condition': 'host_os == "win" and checkout_clang_coverage_tools',
      },
      {
        'object_name': 'Win/llvmobjdump-llvmorg-22-init-14273-gea10026b-1.tar.xz',
        'sha256sum': '00c4dab7747534548e2111b3adbdbf9ef561887e18c7d6de4c7e273af799c190',
        'size_bytes': 5742908,
        'generation': 1762971401692156,
        'condition': '(checkout_linux or checkout_mac or checkout_android) and host_os == "win"',
      },
    ]
  },

  'third_party/llvm-libc/src': {
    'url': Var('chromium_git') +
      '/external/github.com/llvm/llvm-project/libc.git' + '@' + '74b25173cba70124bff5da97cc339d90c516c5f6',
    'condition': 'not build_with_chromium',
  },

  'third_party/modp_b64': {
    'url': Var('chromium_git') + '/chromium/src/third_party/modp_b64'
    '@' + '7c1b3276e72757e854b5b642284aa367436a4723',  # 2024-11-18
    'condition': 'not build_with_chromium',
  },

  'third_party/valijson/src': {
    'url': Var('github') + '/tristanpenman/valijson.git' +
      '@' + 'fc9ddf14db683c9443c48ae3a6bf83e0ce3ad37c', # Version 1.0.3
    'condition': 'not build_with_chromium',
  },

  # Googleurl recommends living at head. This is a copy of Chrome's URL parsing
  # library. It is meant to be used by QUICHE.
  #
  # Make sure to also update ./third_party/googleurl/README.chromium's
  # `Revision:` field when updating this dependency.
  'third_party/googleurl/src': {
    'url': Var('quiche_git') + '/googleurl.git' +
      '@' + '94ff147fe0b96b4cca5d6d316b9af6210c0b8051',  #2025-11-11
    'condition': 'not build_with_chromium',
  }
}

hooks = [
  {
    'name': 'clang_update_script',
    'pattern': '.',
    'condition': 'not build_with_chromium',
    'action': [ 'python3', 'tools/download-clang-update-script.py',
                '--revision', Var('chrome_version'),
                '--output', 'tools/clang/scripts/update.py' ],
    # NOTE: This file appears in .gitignore, as it is not a part of the
    # openscreen repo.
  },
]

# This exists to allow Google Cloud Storage blobs in these DEPS to be fetched.
# Do not add any additional recursedeps entries without consulting
# mfoltz@chromium.org!
recursedeps = [
  'build',
  'buildtools',
  'third_party/instrumented_libs',
]

include_rules = [
  '+util',
  '+platform/api',
  '+platform/base',
  '+platform/test',
  '+testing/util',
  '+third_party',

  # Inter-module dependencies must be through public APIs.
  '-discovery',
  '+discovery/common',
  '+discovery/dnssd/public',
  '+discovery/mdns/public',
  '+discovery/public',

  # Don't include abseil from the root so the path can change via include_dirs
  # rules when in Chromium.
  '-third_party/abseil',

  # Abseil allowed headers.
  # IMPORTANT: Do not add new entries; abseil is being removed from the library.
  # See https://issuetracker.google.com/158433927
  '+absl/types/variant.h',

  # Similar to abseil, don't include boringssl using root path.  Instead,
  # explicitly allow 'openssl' where needed.
  '-third_party/boringssl',

  # Test framework includes.
  '-third_party/googletest',
  '+gtest',
  '+gmock',

  # Can use generic Chromium buildflags.
  '+build/build_config.h',
]
