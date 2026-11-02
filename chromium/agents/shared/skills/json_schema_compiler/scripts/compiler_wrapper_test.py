#!/usr/bin/env python3
# Copyright 2026 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""compiler_wrapper_test.py - Unit tests for compiler_wrapper.py."""

from pathlib import Path
import unittest
from unittest.mock import MagicMock, patch

import compiler_wrapper
from pyfakefs import fake_filesystem_unittest


class CompilerWrapperTest(fake_filesystem_unittest.TestCase):

    def setUp(self):
        self.setUpPyfakefs()

    def test_find_src_root_found(self):
        # Create the target file in the fake filesystem
        self.fs.create_file(
            "/workspace/src/tools/json_schema_compiler/compiler.py"
        )

        root = compiler_wrapper.find_src_root(
            "/workspace/src/chrome/common/extensions/api"
        )
        self.assertEqual(root, Path("/workspace/src"))

    def test_find_src_root_not_found(self):
        # Do not create compiler.py, so it shouldn't be found
        root = compiler_wrapper.find_src_root(
            "/workspace/src/chrome/common/extensions/api"
        )
        self.assertIsNone(root)

    @patch("subprocess.run")
    def test_main_success(self, mock_run):
        # Create target file and compiler script in fake filesystem
        target_file = "/mock/src/extensions/common/api/test_api.json"
        compiler_path = "/mock/src/tools/json_schema_compiler/compiler.py"
        self.fs.create_file(target_file)
        self.fs.create_file(compiler_path)

        # Mock sys.argv
        sys_argv = [
            "compiler_wrapper.py",
            "-f",
            target_file,
            "-g",
            "cpp",
            "-d",
            "/mock/src/out/gen",
        ]

        # Mock subprocess.run
        mock_process = MagicMock()
        mock_process.returncode = 0
        mock_run.return_value = mock_process

        with patch("sys.argv", sys_argv):
            retval = compiler_wrapper.main()
            self.assertEqual(retval, 0)
            mock_run.assert_called_once()
            args, _ = mock_run.call_args
            cmd = args[0]
            self.assertIn("vpython3", cmd)
            self.assertIn(
                "/mock/src/tools/json_schema_compiler/compiler.py", cmd
            )
            self.assertIn("extensions/common/api/test_api.json", cmd)


if __name__ == "__main__":
    unittest.main()
