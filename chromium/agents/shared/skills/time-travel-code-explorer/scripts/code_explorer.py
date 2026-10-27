#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script for interacting with historical git revisions."""

import argparse
import json
import os
import pathlib
import subprocess


def _handle_view_cl(args: argparse.Namespace) -> None:
    """Handler for the view_cl command.

    Args:
        args: The parsed command line arguments.
    """
    subprocess.run(['git', 'show', args.revision], cwd=args.cwd, check=True)


def _handle_view_file(args: argparse.Namespace) -> None:
    """Handler for the view_file command.

    Args:
        args: The parsed command line arguments.
    """
    try:
        subprocess.run([
            'git',
            'cat-file',
            'blob',
            f'{args.revision}:{str(args.path)}',
        ],
                       cwd=args.cwd,
                       check=True)
    except subprocess.CalledProcessError:
        print(f'File {args.path} does not appear to exist at revision '
              f'{args.revision}')


def _handle_list_dir(args: argparse.Namespace) -> None:
    """Handler for the list_dir command.

    Args:
        args: The parsed command line arguments.
    """
    # git ls-tree treats a directory as a file if a trailing slash is not
    # provided.
    path = str(args.path)
    if not path.endswith(os.sep):
        path += os.sep

    proc = subprocess.run([
        'git',
        'ls-tree',
        '--format=%(objecttype):%(path)',
        args.revision,
        path,
    ],
                          cwd=args.cwd,
                          capture_output=True,
                          text=True,
                          check=True)
    results = {
        'files': [],
        'directories': [],
    }
    for line in proc.stdout.strip().splitlines():
        obj_type, _, obj_path = line.partition(':')
        if obj_type == 'blob':
            results['files'].append(obj_path)
        elif obj_type == 'tree':
            results['directories'].append(obj_path)
    print(json.dumps(results))


def _handle_search_files(args: argparse.Namespace) -> None:
    """Handler for the search_files command.

    Args:
        args: The parsed command line arguments.
    """
    try:
        subprocess.run([
            'git',
            'grep',
            '--fixed-strings',
            '-e',
            args.query,
            args.revision,
        ],
                       cwd=args.cwd,
                       check=True)
    except subprocess.CalledProcessError as e:
        # Successful run, but no matches found.
        if e.returncode == 1:
            return
        raise


def _parse_args() -> argparse.Namespace:
    """Parse and return command line arguments.

    Returns:
        An argparse.Namespace with the parsed arguments.
    """
    parser = argparse.ArgumentParser(
        description=('Explore a non-HEAD git revision without needing to '
                     'sync the checkout to that revision.'))
    subparsers = parser.add_subparsers()

    shared_argument_parser = argparse.ArgumentParser(add_help=False)
    shared_argument_parser.add_argument('--revision',
                                        required=True,
                                        help='The git revision to explore at')
    shared_argument_parser.add_argument(
        '--cwd',
        type=pathlib.Path,
        default=pathlib.Path.cwd(),
        help=('The working directory to explore from which affects which git '
              'repo is interacted with. Defaults to the current working '
              'directory'))

    view_revision_parser = subparsers.add_parser(
        'view_cl',
        parents=[shared_argument_parser],
        help=('Retrieves the the commit description and full commit content '
              'for a revision'))
    view_revision_parser.set_defaults(handler=_handle_view_cl)

    view_file_parser = subparsers.add_parser(
        'view_file',
        parents=[shared_argument_parser],
        help='Retrieves the full content for a given file for a revision')
    view_file_parser.set_defaults(handler=_handle_view_file)
    view_file_parser.add_argument(
        '--path',
        required=True,
        type=pathlib.Path,
        help=('A path to a file to retrieve the content for. Must be relative '
              'to the repo root'))

    list_dir_parser = subparsers.add_parser(
        'list_dir',
        parents=[shared_argument_parser],
        help=('Lists directory content at a revision. Output is a JSON object '
              'containing lists for the files and directories in the '
              'specified directory'))
    list_dir_parser.set_defaults(handler=_handle_list_dir)
    list_dir_parser.add_argument(
        '--path',
        required=True,
        type=pathlib.Path,
        help='A path to a directory to list the contents of')

    search_files_parser = subparsers.add_parser(
        'search_files',
        parents=[shared_argument_parser],
        help='Search for exact substrings in files at a revision.')
    search_files_parser.set_defaults(handler=_handle_search_files)
    search_files_parser.add_argument('--query',
                                     required=True,
                                     help='The exact string to search for')

    return parser.parse_args()


def main() -> None:
    args = _parse_args()
    args.handler(args)


if __name__ == '__main__':
    main()
