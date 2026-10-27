#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unittests for code_explorer.py"""

# pylint: disable=protected-access

import argparse
import contextlib
import io
import json
import os
import pathlib
import subprocess
import sys
import unittest
from unittest import mock

import code_explorer


class TestHandleViewCl(unittest.TestCase):

    def setUp(self):
        self.mock_run_patcher = mock.patch('subprocess.run')
        self.mock_run = self.mock_run_patcher.start()
        self.addCleanup(self.mock_run_patcher.stop)

    def test_success(self):
        args = argparse.Namespace(revision='my-rev',
                                  cwd=pathlib.Path('/my/cwd'))
        code_explorer._handle_view_cl(args)

        self.mock_run.assert_called_once_with(
            ['git', 'show', 'my-rev'],
            cwd=pathlib.Path('/my/cwd'),
            check=True,
        )


class TestHandleViewFile(unittest.TestCase):

    def setUp(self):
        self.mock_run_patcher = mock.patch('subprocess.run')
        self.mock_run = self.mock_run_patcher.start()
        self.addCleanup(self.mock_run_patcher.stop)

    def test_success(self):
        args = argparse.Namespace(
            revision='my-rev',
            path=pathlib.Path('my/file.py'),
            cwd=pathlib.Path('/my/cwd'),
        )
        code_explorer._handle_view_file(args)

        self.mock_run.assert_called_once_with(
            ['git', 'cat-file', 'blob', 'my-rev:my/file.py'],
            cwd=pathlib.Path('/my/cwd'),
            check=True,
        )

    def test_file_not_found(self):
        self.mock_run.side_effect = subprocess.CalledProcessError(
            1, 'git cat-file')
        args = argparse.Namespace(
            revision='my-rev',
            path=pathlib.Path('my/file.py'),
            cwd=pathlib.Path('/my/cwd'),
        )
        f = io.StringIO()
        with contextlib.redirect_stdout(f):
            code_explorer._handle_view_file(args)

        self.mock_run.assert_called_once_with(
            ['git', 'cat-file', 'blob', 'my-rev:my/file.py'],
            cwd=pathlib.Path('/my/cwd'),
            check=True,
        )
        self.assertEqual(
            f.getvalue(),
            'File my/file.py does not appear to exist at revision my-rev\n')


class TestHandleListDir(unittest.TestCase):

    def setUp(self):
        self.mock_run_patcher = mock.patch('subprocess.run')
        self.mock_run = self.mock_run_patcher.start()
        self.addCleanup(self.mock_run_patcher.stop)

    def test_success(self):
        mock_proc = mock.Mock()
        mock_proc.stdout = (
            'blob:my/dir/file1.py\ntree:my/dir/subdir\nblob:my/dir/file2.py\n')
        self.mock_run.return_value = mock_proc

        args = argparse.Namespace(
            revision='my-rev',
            path=pathlib.Path('my/dir'),
            cwd=pathlib.Path('/my/cwd'),
        )
        f = io.StringIO()
        with contextlib.redirect_stdout(f):
            code_explorer._handle_list_dir(args)

        # Path should have trailing slash added
        expected_path = f'my/dir{os.sep}'
        self.mock_run.assert_called_once_with(
            [
                'git',
                'ls-tree',
                '--format=%(objecttype):%(path)',
                'my-rev',
                expected_path,
            ],
            cwd=pathlib.Path('/my/cwd'),
            capture_output=True,
            text=True,
            check=True,
        )

        expected_results = {
            'files': ['my/dir/file1.py', 'my/dir/file2.py'],
            'directories': ['my/dir/subdir'],
        }
        self.assertEqual(json.loads(f.getvalue()), expected_results)

    def test_success_with_trailing_slash(self):
        mock_proc = mock.Mock()
        mock_proc.stdout = 'blob:my/dir/file1.py\n'
        self.mock_run.return_value = mock_proc

        args = argparse.Namespace(
            revision='my-rev',
            path=pathlib.Path(f'my/dir{os.sep}'),
            cwd=pathlib.Path('/my/cwd'),
        )
        f = io.StringIO()
        with contextlib.redirect_stdout(f):
            code_explorer._handle_list_dir(args)

        expected_path = f'my/dir{os.sep}'
        self.mock_run.assert_called_once_with(
            [
                'git',
                'ls-tree',
                '--format=%(objecttype):%(path)',
                'my-rev',
                expected_path,
            ],
            cwd=pathlib.Path('/my/cwd'),
            capture_output=True,
            text=True,
            check=True,
        )
        expected_results = {
            'files': ['my/dir/file1.py'],
            'directories': [],
        }
        self.assertEqual(json.loads(f.getvalue()), expected_results)

    def test_empty_directory(self):
        mock_proc = mock.Mock()
        mock_proc.stdout = ''
        self.mock_run.return_value = mock_proc

        args = argparse.Namespace(
            revision='my-rev',
            path=pathlib.Path('my/dir'),
            cwd=pathlib.Path('/my/cwd'),
        )
        f = io.StringIO()
        with contextlib.redirect_stdout(f):
            code_explorer._handle_list_dir(args)

        expected_results = {
            'files': [],
            'directories': [],
        }
        self.assertEqual(json.loads(f.getvalue()), expected_results)


class TestHandleSearchFiles(unittest.TestCase):

    def setUp(self):
        self.mock_run_patcher = mock.patch('subprocess.run')
        self.mock_run = self.mock_run_patcher.start()
        self.addCleanup(self.mock_run_patcher.stop)

    def test_success(self):
        args = argparse.Namespace(
            query='my-query',
            revision='my-rev',
            cwd=pathlib.Path('/my/cwd'),
        )
        code_explorer._handle_search_files(args)

        self.mock_run.assert_called_once_with(
            ['git', 'grep', '--fixed-strings', '-e', 'my-query', 'my-rev'],
            cwd=pathlib.Path('/my/cwd'),
            check=True,
        )

    def test_no_matches(self):
        self.mock_run.side_effect = subprocess.CalledProcessError(
            1, 'git grep')
        args = argparse.Namespace(
            query='my-query',
            revision='my-rev',
            cwd=pathlib.Path('/my/cwd'),
        )
        # Should not raise exception
        code_explorer._handle_search_files(args)

        self.mock_run.assert_called_once_with(
            ['git', 'grep', '--fixed-strings', '-e', 'my-query', 'my-rev'],
            cwd=pathlib.Path('/my/cwd'),
            check=True,
        )

    def test_other_error(self):
        self.mock_run.side_effect = subprocess.CalledProcessError(
            128, 'git grep')
        args = argparse.Namespace(
            query='my-query',
            revision='my-rev',
            cwd=pathlib.Path('/my/cwd'),
        )
        with self.assertRaises(subprocess.CalledProcessError):
            code_explorer._handle_search_files(args)


class TestParseArgs(unittest.TestCase):

    def test_view_cl_required_args(self):
        with mock.patch.object(
                sys, 'argv',
            ['code_explorer.py', 'view_cl', '--revision', 'my-rev']):
            args = code_explorer._parse_args()
            self.assertEqual(args.handler, code_explorer._handle_view_cl)
            self.assertEqual(args.revision, 'my-rev')
            self.assertEqual(args.cwd, pathlib.Path.cwd())

    def test_view_cl_all_args(self):
        with mock.patch.object(
                sys,
                'argv',
            [
                'code_explorer.py',
                'view_cl',
                '--revision',
                'my-rev',
                '--cwd',
                '/my/cwd',
            ],
        ):
            args = code_explorer._parse_args()
            self.assertEqual(args.revision, 'my-rev')
            self.assertEqual(args.cwd, pathlib.Path('/my/cwd'))

    def test_view_file_required_args(self):
        with mock.patch.object(
                sys,
                'argv',
            [
                'code_explorer.py',
                'view_file',
                '--revision',
                'my-rev',
                '--path',
                'my/file.py',
            ],
        ):
            args = code_explorer._parse_args()
            self.assertEqual(args.handler, code_explorer._handle_view_file)
            self.assertEqual(args.revision, 'my-rev')
            self.assertEqual(args.path, pathlib.Path('my/file.py'))

    def test_list_dir_required_args(self):
        with mock.patch.object(
                sys,
                'argv',
            [
                'code_explorer.py',
                'list_dir',
                '--revision',
                'my-rev',
                '--path',
                'my/dir',
            ],
        ):
            args = code_explorer._parse_args()
            self.assertEqual(args.handler, code_explorer._handle_list_dir)
            self.assertEqual(args.revision, 'my-rev')
            self.assertEqual(args.path, pathlib.Path('my/dir'))

    def test_search_files_required_args(self):
        with mock.patch.object(
                sys,
                'argv',
            [
                'code_explorer.py',
                'search_files',
                '--revision',
                'my-rev',
                '--query',
                'my-query',
            ],
        ):
            args = code_explorer._parse_args()
            self.assertEqual(args.handler, code_explorer._handle_search_files)
            self.assertEqual(args.revision, 'my-rev')
            self.assertEqual(args.query, 'my-query')

    def test_missing_revision(self):
        # argparse will write to stderr and exit
        with mock.patch.object(sys, 'argv',
                               ['code_explorer.py', 'view_cl']), mock.patch(
                                   'sys.stderr', new_callable=io.StringIO):
            with self.assertRaises(SystemExit):
                code_explorer._parse_args()


if __name__ == '__main__':
    unittest.main()
