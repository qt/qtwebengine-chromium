# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""A CURL-ish method that downloads things without needing CURL installed."""

import os
from urllib.error import HTTPError, URLError
from urllib.request import urlopen

from cli_utils import print_error, print_substep


def curlish(download_url: str, output_path: str) -> bool:
    """Downloads a file from a URL to a local path.

    Args:
        download_url: The URL to download from.
        output_path: The local path to write the downloaded file to.

    Returns:
        True if the download succeeded, False otherwise.
    """
    if not output_path or not download_url:
        print_error('Need both output path and download URL to download, '
                    'exiting.')
        return False

    print_substep(f'Downloading from "{download_url}" to "{output_path}"...')
    try:
        response = urlopen(download_url)
        script_contents = response.read()
    except HTTPError as e:
        print_error(
            f'HTTP Error {e.code}: {e.read().decode("utf-8", "ignore")}')
        return False
    except URLError as e:
        print_error(f'Download failed. Reason: {e.reason}')
        return False

    directory = os.path.dirname(output_path)
    if directory and not os.path.exists(directory):
        os.makedirs(directory)

    with open(output_path, 'wb') as script_file:
        script_file.write(script_contents)

    return True
