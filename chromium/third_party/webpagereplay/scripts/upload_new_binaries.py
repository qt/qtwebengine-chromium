#!/usr/bin/env vpython3
# Copyright 2025 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

_MIN_GO_VERSION = [1, 21]
_REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_WPR_GO_DIR = os.path.join(_REPO_DIR, 'src')
_SUPPORTED_PLATFORMS = (('win', 'x86'), ('mac', 'arm64'), ('mac', 'x86_64'),
                        ('linux', 'x86_64'), ('win', 'AMD64'),
                        ('linux', 'armv7l'), ('linux', 'aarch64'))


def upload_dependency(dependency, dep_local_path, os_name, os_arch):
    with open(dep_local_path, 'rb') as file:
        hash = hashlib.sha1(file.read()).hexdigest()
    subprocess.check_call([
        'gsutil.py', 'cp', dep_local_path,
        f'gs://chromium-telemetry/binary_dependencies/{dependency}_{hash}'
    ])
    json_path = os.path.join(_REPO_DIR, 'scripts', 'binary_dependencies.json')
    with open(json_path) as file:
        deps_data = json.load(file)
    deps_data[dependency][f'{os_name}_{os_arch}']['cloud_storage_hash'] = hash
    with open(json_path, 'w') as file:
        json.dump(deps_data, file, indent=2)
        file.write('\n')


def check_go_version():
    try:
        out = subprocess.check_output(['go', 'version']).decode()
    except subprocess.CalledProcessError:
        out = 'no go binary found'
    match = re.findall(r'go(\d+).(\d+)', out)
    assert len(match) > 0, ('Unable to parse go version from "%s"' % out)
    version = [int(match[0][0]), int(match[0][1])]
    assert (version[0] > _MIN_GO_VERSION[0]
            or (version[0] == _MIN_GO_VERSION[0]
                and version[1] >= _MIN_GO_VERSION[1])), (
                    'Require go version %s or higher. Found: %s' %
                    (_MIN_GO_VERSION, version))


# GOARCH in the build command expects values that differ from the keys in
# binary_dependencies.json. Changing the keys to match GOARCH would require
# touching all consumers of the JSON.
def compute_go_arch(os_arch):
    # go build command recognizes 'amd64' but not 'x86_64', so we switch x86_64
    # to amd64 string here.
    # The two names can be used interchangbly, see:
    # https://wiki.debian.org/DebianAMD64Faq?action=recall&rev=65
    if os_arch == 'x86_64' or os_arch == 'AMD64':
        return 'amd64'

    if os_arch == 'x86':
        return '386'

    if os_arch == 'armv7l':
        return 'arm'

    if os_arch == 'aarch64':
        return 'arm64'

    if os_arch == 'mips':
        return 'mipsle'

    return os_arch


# GOOS in the build command expects values that differ from the keys in
# binary_dependencies.json. Changing the keys to match GOOS would require
# touching all consumers of the JSON.
def compute_go_os(os_name):
    # go build command recognizes 'darwin' but not 'mac'.
    if os_name == 'mac':
        return 'darwin'

    if os_name == 'win':
        return 'windows'

    return os_name


def build_and_upload_go_binary(binary_name, os_name, os_arch):
    if (os_name, os_arch) not in _SUPPORTED_PLATFORMS:
        raise NotImplementedError('OS = %s, ARCH = %s is not supported' %
                                  (os_name, os_arch))

    check_go_version()

    print('Build %s binary for OS %s, ARCH: %s' %
          (binary_name, os_name, os_arch))
    try:
        # We want to build wpr go binaries from the local source. We do this by
        # making a temporary GOPATH that symlinks to our local directory.
        go_path_dir = tempfile.mkdtemp()
        repo_dir = os.path.join(go_path_dir, 'src/go.chromium.org')
        os.makedirs(repo_dir)
        os.symlink(_REPO_DIR, os.path.join(repo_dir, 'webpagereplay'))

        env = os.environ.copy()
        env['GOPATH'] = go_path_dir
        env['GOOS'] = compute_go_os(os_name)
        env['GOARCH'] = compute_go_arch(os_arch)
        env['CGO_ENABLED'] = '0'

        print('GOPATH=%s' % go_path_dir)
        print('CWD=%s' % _WPR_GO_DIR)

        get_cmd = ['go', 'get', '-d', './...']
        print('Running get command: %s' % ' '.join(get_cmd))
        subprocess.check_call(get_cmd, env=env, cwd=_WPR_GO_DIR)

        # Build in `go_path_dir`, so the binaries are deleted by the end.
        binary_file = (os.path.join(go_path_dir, '%s.exe' %
                                    binary_name) if os_name == 'win' else
                       os.path.join(go_path_dir, binary_name))
        build_cmd = [
            'go', 'build', '-v', '-trimpath', '-o', binary_file,
            '%s.go' % binary_name
        ]
        print('Running build command: %s' % ' '.join(build_cmd))
        subprocess.check_call(build_cmd, env=env, cwd=_WPR_GO_DIR)

        clean_cmd = ['go', 'clean', '-modcache']
        print('Running clean command: %s' % ' '.join(clean_cmd))
        subprocess.check_call(clean_cmd, env=env, cwd=_WPR_GO_DIR)

    except:
        if go_path_dir:
            shutil.rmtree(go_path_dir)
        raise

    print('Upload %s binary dependency for OS %s, ARCH: %s' %
          (binary_name, os_name, os_arch))
    upload_dependency('%s_go' % binary_name,
                      binary_file,
                      os_name=os_name,
                      os_arch=os_arch)
    shutil.rmtree(go_path_dir)




def main():
    for os_name, os_arch in _SUPPORTED_PLATFORMS:
        # wpr is the wpr binary for recording and replaying network traffic to
        # allow for consistent and hermetic tests.
        build_and_upload_go_binary('wpr', os_name, os_arch)
        # httparchive is the wpr binary for interrogating and editing a wpr
        # archive that was previously recorded.
        build_and_upload_go_binary('httparchive', os_name, os_arch)


if __name__ == '__main__':
    sys.exit(main())
