#!/usr/bin/env python3
# Copyright (C) 2016 The Qt Company Ltd.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Assembles a GNU-syntax arm64 files with armasm64 via gas-preprocessor."""

import argparse
import os
import subprocess
import sys


def load_env(path):
    with open(path) as f:
        pairs = f.read().split('\0')
    return dict(p.split('=', 1) for p in pairs if '=' in p)

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--env-file', required=True)
    parser.add_argument('--gas-pp', required=True)
    parser.add_argument('-I', dest='includes', action='append', default=[])
    parser.add_argument('-o', dest='output', required=True)
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('source')
    args = parser.parse_args()
    os.makedirs(os.path.dirname(args.output) or '.', exist_ok=True)
    cmd = ['perl', args.gas_pp, '-arch', 'aarch64',
           '-as-type', 'armasm', '--',
           'armasm64.exe', '-nologo', '-ignore', '4509']
    cmd += ['-I' + i for i in args.includes]
    cmd += ['-DHAVE_AV_CONFIG_H', '-c', '-o', args.output, args.source]
    if args.verbose:
        print('[gaspp] %s' % subprocess.list2cmdline(cmd), file=sys.stderr)
    return subprocess.call(cmd, env=load_env(args.env_file))

if __name__ == '__main__':
    sys.exit(main())

