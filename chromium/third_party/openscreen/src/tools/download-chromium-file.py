#!/usr/bin/env python3
# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script is used to download a file from the Chromium repository.

It's equivalent to using curl to download the file, and is intended to be ran
as a gclient hook.
"""

import argparse
import curlish
import os
import sys

URL_TEMPLATE = 'https://raw.githubusercontent.com/chromium/chromium/{}/{}'


def get_url(revision, path):
    return URL_TEMPLATE.format(revision or "main", path)


def main():
    parser = argparse.ArgumentParser(
        description='Download a file from the Chromium repository')
    parser.add_argument('--output',
                        required=True,
                        help='path to file to create/overwrite')
    parser.add_argument('--revision',
                        required=True,
                        help='revision to download')
    parser.add_argument('--path',
                        required=True,
                        help='path within the Chromium repository')
    args = parser.parse_args()

    os.makedirs(os.path.dirname(os.path.abspath(args.output)), exist_ok=True)

    return 0 if curlish.curlish(get_url(args.revision, args.path),
                                args.output) else 1


if __name__ == '__main__':
    sys.exit(main())
