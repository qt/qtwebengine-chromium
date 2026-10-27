#!/usr/bin/env vpython3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Unit tests for roll_protobuf.py."""

import argparse
import os
import sys
import unittest
from unittest.mock import patch, mock_open, MagicMock

import roll_protobuf


class RollProtobufTests(unittest.TestCase):

    def test_parse_current_versions(self):
        mock_content = ('Name: Protocol Buffers\n'
                        'Version: 33.6\n'
                        'Revision: 6e1998413a5bca7c058b85999667893f167434bc\n')
        with patch('os.path.exists', return_value=True):
            with patch('builtins.open', mock_open(read_data=mock_content)):
                current = roll_protobuf._parse_current_versions('dummy_path')
                self.assertIsNotNone(current)
                self.assertEqual(current.version, '33.6')
                self.assertEqual(current.revision,
                                 '6e1998413a5bca7c058b85999667893f167434bc')

    def test_parse_current_versions_missing_file(self):
        with patch('os.path.exists', return_value=False):
            current = roll_protobuf._parse_current_versions('dummy_path')
            self.assertIsNone(current)

    @patch('roll_protobuf._run_cmd')
    def test_fetch_git_revision_success(self, mock_run_cmd):
        mock_result = MagicMock()
        mock_result.stdout = 'abc123def456  refs/tags/v33.1\n'
        mock_run_cmd.return_value = mock_result

        revision = roll_protobuf._fetch_git_revision('33.1')
        self.assertEqual(revision, 'abc123def456')
        mock_run_cmd.assert_called_once()

    @patch('roll_protobuf._run_cmd')
    def test_get_available_versions(self, mock_run_cmd):
        mock_result = MagicMock()
        mock_result.stdout = ('hash1 refs/tags/v33.1\n'
                              'hash2 refs/tags/v33.1.2\n'
                              'hash3 refs/tags/v3.19.4\n'
                              'hash4 refs/tags/v34.0\n')
        mock_run_cmd.return_value = mock_result
        versions = roll_protobuf._get_available_versions()
        self.assertEqual(versions, ['3.19.4', '33.1', '33.1.2', '34.0'])

    @patch('builtins.open',
           new_callable=mock_open,
           read_data='- 0001-some-patch.patch\n')
    @patch('os.path.exists', return_value=True)
    @patch('glob.glob', return_value=['patches/0001-some-patch.patch'])
    def test_validate_patches_success(self, mock_glob, mock_exists, mock_file):
        try:
            roll_protobuf._validate_patches('dummy_dir')
        except SystemExit:
            self.fail('_validate_patches exited unexpectedly.')

    @patch('builtins.open',
           new_callable=mock_open,
           read_data='- 0001-some-patch.patch\n')
    @patch('os.path.exists', return_value=True)
    @patch('glob.glob',
           return_value=[
               'patches/0001-some-patch.patch',
               'patches/0002-missing-in-readme.patch'
           ])
    @patch('sys.exit')
    def test_validate_patches_missing_in_readme(self, mock_exit, mock_glob,
                                                mock_exists, mock_file):
        roll_protobuf._validate_patches('dummy_dir')
        mock_exit.assert_called_once_with(1)

    @patch('builtins.open',
           new_callable=mock_open,
           read_data='- 0001-some-patch.patch\n- 0002-missing-on-disk.patch\n')
    @patch('os.path.exists', return_value=True)
    @patch('glob.glob', return_value=['patches/0001-some-patch.patch'])
    @patch('sys.exit')
    def test_validate_patches_missing_on_disk(self, mock_exit, mock_glob,
                                              mock_exists, mock_file):
        roll_protobuf._validate_patches('dummy_dir')
        mock_exit.assert_called_once_with(1)


if __name__ == '__main__':
    unittest.main()
