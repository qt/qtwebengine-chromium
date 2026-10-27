# This file is used to manage the dependencies. It is
# used by gclient to determine what version of each dependency to check out, and
# where.
#
# For more information, please refer to the official documentation:
#   https://sites.google.com/a/chromium.org/dev/developers/how-tos/get-the-code
#
# When adding a new dependency, please update the top-level .gitignore file
# to list the dependency's destination directory.
#
# -----------------------------------------------------------------------------
# Rolling deps
# -----------------------------------------------------------------------------
# All repositories in this file are git-based, using Chromium git mirrors where
# necessary (e.g., a git mirror is used when the source project is SVN-based).
# To update the revision that Chromium pulls for a given dependency:
#
#  # Create and switch to a new branch
#  git new-branch depsroll
#  # Run roll-dep (provided by depot_tools) giving the dep's path and optionally
#  # a regex that will match the line in this file that contains the current
#  # revision. The script ALWAYS rolls the dependency to the latest revision
#  # in origin/master. The path for the dep should start with src/.
#  roll-dep src/third_party/foo_package/src foo_package.git
#  # You should now have a modified DEPS file; commit and upload as normal
#  git commit -aspv_he
#  git cl upload
#
# For more on the syntax and semantics of this file, see:
#   https://bit.ly/chromium-gclient-conditionals
#
# which is a bit incomplete but the best documentation we have at the
# moment.

# We expect all git dependencies specified in this file to be in sync with git
# submodules (gitlinks).
git_dependencies = 'SYNC'

use_relative_paths = True

vars = {
  'golang_version': '1.26.2',
  'clang_format_git': 'https://chromium.googlesource.com/external/github.com/llvm/llvm-project/clang/tools/clang-format.git',

  # Three lines of non-changing comments so that
  # the commit queue can handle CLs rolling deps
  # without interference from each other.
  'clang_format_revision': '842084dbd4b0f205c9d014f0e129c139aa5ac517',
}

# Only these hosts are allowed for dependencies in this DEPS file.
# If you need to add a new host, contact chrome infrastructure team.
allowed_hosts = [
  'chromium.googlesource.com',
]

deps = {
  'buildtools/win-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "win"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '565cab9c66d61360c27c7d4df5defe1a78ab56d3',
        'sha256sum': '5557943a174e3b67cdc389c10b0ceea2195f318c5c665dd77a427ed01a094557',
        'size_bytes': 3784704,
        'generation': 1738622386314064,
        'output_file': 'clang-format.exe',
      },
    ],
  },
  'buildtools/mac-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "x64"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '7d46d237f9664f41ef46b10c1392dcb559250f25',
        'sha256sum': '0c3c13febeb0495ef0086509c24605ecae9e3d968ff9669d12514b8a55c7824e',
        'size_bytes': 3204008,
        'generation': 1738622388489334,
        'output_file': 'clang-format',
      },
    ],
  },
  'buildtools/mac_arm64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "mac" and host_cpu == "arm64"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '8503422f469ae56cc74f0ea2c03f2d872f4a2303',
        'sha256sum': 'dabf93691361e8bd1d07466d67584072ece5c24e2b812c16458b8ff801c33e29',
        'size_bytes': 3212560,
        'generation': 1738622390717009,
        'output_file': 'clang-format',
      },
    ],
  },
  'buildtools/linux64-format': {
    'bucket': 'chromium-clang-format',
    'condition': 'host_os == "linux"',
    'dep_type': 'gcs',
    'objects': [
      {
        'object_name': '79a7b4e5336339c17b828de10d80611ff0f85961',
        'sha256sum': '889266a51681d55bd4b9e02c9a104fa6ee22ecdfa7e8253532e5ea47e2e4cb4a',
        'size_bytes': 3899440,
        'generation': 1738622384130717,
        'output_file': 'clang-format',
      },
    ],
  },
  'third_party/golang/linux/{host_cpu}': {
    'packages': [
      {
        'package': 'infra/3pp/tools/go/${{platform}}',
        'version': 'version:3@{golang_version}',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "linux"',
  },
  'third_party/golang/win/{host_cpu}': {
    'packages': [
      {
        'package': 'infra/3pp/tools/go/${{platform}}',
        'version': 'version:3@{golang_version}',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "win"',
  },
  # Mac checks out both architectures because some bots are composed of a modern
  # Arm orchestrator and a x64 test runner.
  'third_party/golang/mac/x64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/go/mac-amd64',
        'version': 'version:3@{golang_version}',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'third_party/golang/mac/arm64': {
    'packages': [
      {
        'package': 'infra/3pp/tools/go/mac-arm64',
        'version': 'version:3@{golang_version}',
      },
    ],
    'dep_type': 'cipd',
    'condition': 'host_os == "mac"',
  },
  'third_party/clang-format/script': '{clang_format_git}@{clang_format_revision}',
}
