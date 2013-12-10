#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import unittest

import PRESUBMIT


class MockInputApi(object):
  def __init__(self):
    self.re = re
    self.os_path = os.path
    self.files = []
    self.is_committing = False

  def AffectedFiles(self):
    return self.files


class MockOutputApi(object):
  class PresubmitResult(object):
    def __init__(self, message, items=None, long_text=''):
      self.message = message
      self.items = items
      self.long_text = long_text

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'error'

  class PresubmitPromptWarning(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'warning'

  class PresubmitNotifyResult(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'notify'

  class PresubmitPromptOrNotify(PresubmitResult):
    def __init__(self, message, items, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'promptOrNotify'


class MockFile(object):
  def __init__(self, local_path, new_contents):
    self._local_path = local_path
    self._new_contents = new_contents
    self._changed_contents = [(i + 1, l) for i, l in enumerate(new_contents)]

  def ChangedContents(self):
    return self._changed_contents

  def NewContents(self):
    return self._new_contents

  def LocalPath(self):
    return self._local_path


class MockChange(object):
  def __init__(self, changed_files):
    self._changed_files = changed_files

  def LocalPaths(self):
    return self._changed_files


class IncludeOrderTest(unittest.TestCase):
  def testSystemHeaderOrder(self):
    scope = [(1, '#include <csystem.h>'),
             (2, '#include <cppsystem>'),
             (3, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(0, len(warnings))

  def testSystemHeaderOrderMismatch1(self):
    scope = [(10, '#include <cppsystem>'),
             (20, '#include <csystem.h>'),
             (30, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(1, len(warnings))
    self.assertTrue('20' in warnings[0])

  def testSystemHeaderOrderMismatch2(self):
    scope = [(10, '#include <cppsystem>'),
             (20, '#include "acustom.h"'),
             (30, '#include <csystem.h>')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(1, len(warnings))
    self.assertTrue('30' in warnings[0])

  def testSystemHeaderOrderMismatch3(self):
    scope = [(10, '#include "acustom.h"'),
             (20, '#include <csystem.h>'),
             (30, '#include <cppsystem>')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(2, len(warnings))
    self.assertTrue('20' in warnings[0])
    self.assertTrue('30' in warnings[1])

  def testAlphabeticalOrderMismatch(self):
    scope = [(10, '#include <csystem.h>'),
             (15, '#include <bsystem.h>'),
             (20, '#include <cppsystem>'),
             (25, '#include <bppsystem>'),
             (30, '#include "bcustom.h"'),
             (35, '#include "acustom.h"')]
    all_linenums = [linenum for (linenum, _) in scope]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', all_linenums)
    self.assertEqual(3, len(warnings))
    self.assertTrue('15' in warnings[0])
    self.assertTrue('25' in warnings[1])
    self.assertTrue('35' in warnings[2])

  def testSpecialFirstInclude1(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude2(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/other/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude3(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo_platform.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testSpecialFirstInclude4(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/path/bar.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo_platform.cc', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(1, len(warnings))
    self.assertTrue('2' in warnings[0])

  def testSpecialFirstInclude5(self):
    mock_input_api = MockInputApi()
    contents = ['#include "some/other/path/foo.h"',
                '#include "a/header.h"']
    mock_file = MockFile('some/path/foo-suffix.h', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testOrderAlreadyWrong(self):
    scope = [(1, '#include "b.h"'),
             (2, '#include "a.h"'),
             (3, '#include "c.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [3])
    self.assertEqual(0, len(warnings))

  def testConflictAdded1(self):
    scope = [(1, '#include "a.h"'),
             (2, '#include "c.h"'),
             (3, '#include "b.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [2])
    self.assertEqual(1, len(warnings))
    self.assertTrue('3' in warnings[0])

  def testConflictAdded2(self):
    scope = [(1, '#include "c.h"'),
             (2, '#include "b.h"'),
             (3, '#include "d.h"')]
    mock_input_api = MockInputApi()
    warnings = PRESUBMIT._CheckIncludeOrderForScope(scope, mock_input_api,
                                                    '', [2])
    self.assertEqual(1, len(warnings))
    self.assertTrue('2' in warnings[0])

  def testIfElifElseEndif(self):
    mock_input_api = MockInputApi()
    contents = ['#include "e.h"',
                '#define foo',
                '#include "f.h"',
                '#undef foo',
                '#include "e.h"',
                '#if foo',
                '#include "d.h"',
                '#elif bar',
                '#include "c.h"',
                '#else',
                '#include "b.h"',
                '#endif',
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testExcludedIncludes(self):
    # #include <sys/...>'s can appear in any order.
    mock_input_api = MockInputApi()
    contents = ['#include <sys/b.h>',
                '#include <sys/a.h>']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include <atlbase.h>',
                '#include <aaa.h>']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include "build/build_config.h"',
                '#include "aaa.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

  def testCheckOnlyCFiles(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    contents = ['#include <b.h>',
                '#include <a.h>']
    mock_file_cc = MockFile('something.cc', contents)
    mock_file_h = MockFile('something.h', contents)
    mock_file_other = MockFile('something.py', contents)
    mock_input_api.files = [mock_file_cc, mock_file_h, mock_file_other]
    warnings = PRESUBMIT._CheckIncludeOrder(mock_input_api, mock_output_api)
    self.assertEqual(1, len(warnings))
    self.assertEqual(2, len(warnings[0].items))
    self.assertEqual('promptOrNotify', warnings[0].type)

  def testUncheckableIncludes(self):
    mock_input_api = MockInputApi()
    contents = ['#include <windows.h>',
                '#include "b.h"'
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include "gpu/command_buffer/gles_autogen.h"',
                '#include "b.h"'
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include "gl_mock_autogen.h"',
                '#include "b.h"'
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))

    contents = ['#include "ipc/some_macros.h"',
                '#include "b.h"'
                '#include "a.h"']
    mock_file = MockFile('', contents)
    warnings = PRESUBMIT._CheckIncludeOrderInFile(
        mock_input_api, mock_file, range(1, len(contents) + 1))
    self.assertEqual(0, len(warnings))


class VersionControlConflictsTest(unittest.TestCase):
  def testTypicalConflict(self):
    lines = ['<<<<<<< HEAD',
             '  base::ScopedTempDir temp_dir_;',
             '=======',
             '  ScopedTempDir temp_dir_;',
             '>>>>>>> master']
    errors = PRESUBMIT._CheckForVersionControlConflictsInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(3, len(errors))
    self.assertTrue('1' in errors[0])
    self.assertTrue('3' in errors[1])
    self.assertTrue('5' in errors[2])


class BadExtensionsTest(unittest.TestCase):
  def testBadRejFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('some/path/foo.cc', ''),
      MockFile('some/path/foo.cc.rej', ''),
      MockFile('some/path2/bar.h.rej', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(2, len(results[0].items))
    self.assertTrue('foo.cc.rej' in results[0].items[0])
    self.assertTrue('bar.h.rej' in results[0].items[1])

  def testBadOrigFile(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h.orig', ''),
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]

    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(1, len(results))
    self.assertEqual(1, len(results[0].items))
    self.assertTrue('qux.h.orig' in results[0].items[0])

  def testGoodFiles(self):
    mock_input_api = MockInputApi()
    mock_input_api.files = [
      MockFile('other/path/qux.h', ''),
      MockFile('other/path/qux.cc', ''),
    ]
    results = PRESUBMIT._CheckPatchFiles(mock_input_api, MockOutputApi())
    self.assertEqual(0, len(results))

  def testOnlyOwnersFiles(self):
    mock_change = MockChange([
      'some/path/OWNERS',
      'A\Windows\Path\OWNERS',
    ])
    results = PRESUBMIT.GetPreferredTrySlaves(None, mock_change)
    self.assertEqual(0, len(results))


class InvalidOSMacroNamesTest(unittest.TestCase):
  def testInvalidOSMacroNames(self):
    lines = ['#if defined(OS_WINDOWS)',
             ' #elif defined(OS_WINDOW)',
             ' # if defined(OS_MACOSX) || defined(OS_CHROME)',
             '# else  // defined(OS_MAC)',
             '#endif  // defined(OS_MACOS)']
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(len(lines), len(errors))
    self.assertTrue(':1 OS_WINDOWS' in errors[0])
    self.assertTrue('(did you mean OS_WIN?)' in errors[0])

  def testValidOSMacroNames(self):
    lines = ['#if defined(%s)' % m for m in PRESUBMIT._VALID_OS_MACROS]
    errors = PRESUBMIT._CheckForInvalidOSMacrosInFile(
        MockInputApi(), MockFile('some/path/foo_platform.cc', lines))
    self.assertEqual(0, len(errors))


class CheckAddedDepsHaveTetsApprovalsTest(unittest.TestCase):
  def testDepsFilesToCheck(self):
    changed_lines = [
      '"+breakpad",',
      '"+chrome/installer",',
      '"+chrome/plugin/chrome_content_plugin_client.h",',
      '"+chrome/utility/chrome_content_utility_client.h",',
      '"+chromeos/chromeos_paths.h",',
      '"+components/breakpad",',
      '"+components/nacl/common",',
      '"+content/public/browser/render_process_host.h",',
      '"+grit",  # For generated headers',
      '"+grit/generated_resources.h",',
      '"+grit/",',
      '"+policy",  # For generated headers and source',
      '"+sandbox",',
      '"+tools/memory_watcher",',
      '"+third_party/lss/linux_syscall_support.h",',
    ]
    files_to_check = PRESUBMIT._DepsFilesToCheck(re, changed_lines)
    expected = set([
      'breakpad/DEPS',
      'chrome/installer/DEPS',
      'chrome/plugin/DEPS',
      'chrome/utility/DEPS',
      'chromeos/DEPS',
      'components/breakpad/DEPS',
      'components/nacl/common/DEPS',
      'content/public/browser/DEPS',
      'policy/DEPS',
      'sandbox/DEPS',
      'tools/memory_watcher/DEPS',
      'third_party/lss/DEPS',
    ])
    self.assertEqual(expected, files_to_check);


if __name__ == '__main__':
  unittest.main()
