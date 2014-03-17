#!/usr/bin/env python
# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import tempfile
import unittest

import common_includes
from common_includes import *
import push_to_trunk
from push_to_trunk import *
import auto_roll


TEST_CONFIG = {
  BRANCHNAME: "test-prepare-push",
  TRUNKBRANCH: "test-trunk-push",
  PERSISTFILE_BASENAME: "/tmp/test-v8-push-to-trunk-tempfile",
  TEMP_BRANCH: "test-prepare-push-temporary-branch-created-by-script",
  DOT_GIT_LOCATION: None,
  VERSION_FILE: None,
  CHANGELOG_FILE: None,
  CHANGELOG_ENTRY_FILE: "/tmp/test-v8-push-to-trunk-tempfile-changelog-entry",
  PATCH_FILE: "/tmp/test-v8-push-to-trunk-tempfile-patch",
  COMMITMSG_FILE: "/tmp/test-v8-push-to-trunk-tempfile-commitmsg",
  CHROMIUM: "/tmp/test-v8-push-to-trunk-tempfile-chromium",
  DEPS_FILE: "/tmp/test-v8-push-to-trunk-tempfile-chromium/DEPS",
}


class ToplevelTest(unittest.TestCase):
  def testMakeComment(self):
    self.assertEquals("#   Line 1\n#   Line 2\n#",
                      MakeComment("    Line 1\n    Line 2\n"))
    self.assertEquals("#Line 1\n#Line 2",
                      MakeComment("Line 1\n Line 2"))

  def testStripComments(self):
    self.assertEquals("    Line 1\n    Line 3\n",
        StripComments("    Line 1\n#   Line 2\n    Line 3\n#\n"))
    self.assertEquals("\nLine 2 ### Test\n #",
        StripComments("###\n# \n\n#  Line 1\nLine 2 ### Test\n #"))

  def testMakeChangeLogBodySimple(self):
    commits = [
          ["Title text 1",
           "Title text 1\n\nBUG=\n",
           "author1@chromium.org"],
          ["Title text 2.",
           "Title text 2\n\nBUG=1234\n",
           "author2@chromium.org"],
        ]
    self.assertEquals("        Title text 1.\n"
                      "        (author1@chromium.org)\n\n"
                      "        Title text 2 (Chromium issue 1234).\n"
                      "        (author2@chromium.org)\n\n",
                      MakeChangeLogBody(commits))

  def testMakeChangeLogBodyEmpty(self):
    self.assertEquals("", MakeChangeLogBody([]))

  def testMakeChangeLogBodyAutoFormat(self):
    commits = [
          ["Title text 1!",
           "Title text 1\nLOG=y\nBUG=\n",
           "author1@chromium.org"],
          ["Title text 2",
           "Title text 2\n\nBUG=1234\n",
           "author2@chromium.org"],
          ["Title text 3",
           "Title text 3\n\nBUG=1234\nLOG = Yes\n",
           "author3@chromium.org"],
          ["Title text 3",
           "Title text 4\n\nBUG=1234\nLOG=\n",
           "author4@chromium.org"],
        ]
    self.assertEquals("        Title text 1.\n\n"
                      "        Title text 3 (Chromium issue 1234).\n\n",
                      MakeChangeLogBody(commits, True))

  def testMakeChangeLogBugReferenceEmpty(self):
    self.assertEquals("", MakeChangeLogBugReference(""))
    self.assertEquals("", MakeChangeLogBugReference("LOG="))
    self.assertEquals("", MakeChangeLogBugReference(" BUG ="))
    self.assertEquals("", MakeChangeLogBugReference("BUG=none\t"))

  def testMakeChangeLogBugReferenceSimple(self):
    self.assertEquals("(issue 987654)",
                      MakeChangeLogBugReference("BUG = v8:987654"))
    self.assertEquals("(Chromium issue 987654)",
                      MakeChangeLogBugReference("BUG=987654 "))

  def testMakeChangeLogBugReferenceFromBody(self):
    self.assertEquals("(Chromium issue 1234567)",
                      MakeChangeLogBugReference("Title\n\nTBR=\nBUG=\n"
                                                " BUG=\tchromium:1234567\t\n"
                                                "R=somebody\n"))

  def testMakeChangeLogBugReferenceMultiple(self):
    # All issues should be sorted and grouped. Multiple references to the same
    # issue should be filtered.
    self.assertEquals("(issues 123, 234, Chromium issue 345)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=v8:234\n"
                                                "  BUG\t= 345, \tv8:234,\n"
                                                "BUG=v8:123\n"
                                                "R=somebody\n"))
    self.assertEquals("(Chromium issues 123, 234)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=234,,chromium:123 \n"
                                                "R=somebody\n"))
    self.assertEquals("(Chromium issues 123, 234)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=chromium:234, , 123\n"
                                                "R=somebody\n"))
    self.assertEquals("(issues 345, 456)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "\t\tBUG=v8:345,v8:456\n"
                                                "R=somebody\n"))
    self.assertEquals("(issue 123, Chromium issues 345, 456)",
                      MakeChangeLogBugReference("Title\n\n"
                                                "BUG=chromium:456\n"
                                                "BUG = none\n"
                                                "R=somebody\n"
                                                "BUG=456,v8:123, 345"))

  # TODO(machenbach): These test don't make much sense when the formatting is
  # done later.
  def testMakeChangeLogBugReferenceLong(self):
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(issues 234, 1234567890, 1234567"
                      "8901234567890, Chromium issues 12345678,"
                      " 123456789)",
                      MakeChangeLogBugReference("BUG=v8:234\n"
                                                "BUG=v8:1234567890\n"
                                                "BUG=v8:12345678901234567890\n"
                                                "BUG=123456789\n"
                                                "BUG=12345678\n"))
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(issues 234, 1234567890, 1234567"
                      "8901234567890, Chromium issues"
                      " 123456789, 1234567890)",
                      MakeChangeLogBugReference("BUG=v8:234\n"
                                                "BUG=v8:12345678901234567890\n"
                                                "BUG=v8:1234567890\n"
                                                "BUG=123456789\n"
                                                "BUG=1234567890\n"))
    # -----------------00--------10--------20--------30--------
    self.assertEquals("(Chromium issues 234, 1234567890"
                      ", 12345678901234567, "
                      "1234567890123456789)",
                      MakeChangeLogBugReference("BUG=234\n"
                                                "BUG=12345678901234567\n"
                                                "BUG=1234567890123456789\n"
                                                "BUG=1234567890\n"))


class SimpleMock(object):
  def __init__(self, name):
    self._name = name
    self._recipe = []
    self._index = -1

  def Expect(self, recipe):
    self._recipe = recipe

  def Call(self, *args):
    self._index += 1
    try:
      expected_call = self._recipe[self._index]
    except IndexError:
      raise Exception("Calling %s %s" % (self._name, " ".join(args)))

    # Pack expectations without arguments into a list.
    if not isinstance(expected_call, list):
      expected_call = [expected_call]

    # The number of arguments in the expectation must match the actual
    # arguments.
    if len(args) > len(expected_call):
      raise Exception("When calling %s with arguments, the expectations "
                      "must consist of at least as many arguments.")

    # Compare expected and actual arguments.
    for (expected_arg, actual_arg) in zip(expected_call, args):
      if expected_arg != actual_arg:
        raise Exception("Expected: %s - Actual: %s"
                        % (expected_arg, actual_arg))

    # The expectation list contains a mandatory return value and an optional
    # callback for checking the context at the time of the call.
    if len(expected_call) == len(args) + 2:
      expected_call[len(args) + 1]()
    return_value = expected_call[len(args)]

    # If the return value is an exception, raise it instead of returning.
    if isinstance(return_value, Exception):
      raise return_value
    return return_value

  def AssertFinished(self):
    if self._index < len(self._recipe) -1:
      raise Exception("Called %s too seldom: %d vs. %d"
                      % (self._name, self._index, len(self._recipe)))


class ScriptTest(unittest.TestCase):
  def MakeEmptyTempFile(self):
    handle, name = tempfile.mkstemp()
    os.close(handle)
    self._tmp_files.append(name)
    return name

  def MakeTempVersionFile(self):
    name = self.MakeEmptyTempFile()
    with open(name, "w") as f:
      f.write("  // Some line...\n")
      f.write("\n")
      f.write("#define MAJOR_VERSION    3\n")
      f.write("#define MINOR_VERSION    22\n")
      f.write("#define BUILD_NUMBER     5\n")
      f.write("#define PATCH_LEVEL      0\n")
      f.write("  // Some line...\n")
      f.write("#define IS_CANDIDATE_VERSION 0\n")
    return name

  def MakeStep(self, step_class=Step, state=None):
    """Convenience wrapper."""
    return MakeStep(step_class=step_class, number=0, state=state,
                    config=TEST_CONFIG, options=None, side_effect_handler=self)

  def GitMock(self, cmd, args="", pipe=True):
    return self._git_mock.Call(args)

  def LogMock(self, cmd, args=""):
    print "Log: %s %s" % (cmd, args)

  MOCKS = {
    "git": GitMock,
    "vi": LogMock,
  }

  def Command(self, cmd, args="", prefix="", pipe=True):
    return ScriptTest.MOCKS[cmd](self, cmd, args)

  def ReadLine(self):
    return self._rl_mock.Call()

  def ReadURL(self, url):
    return self._url_mock.Call(url)

  def Sleep(self, seconds):
    pass

  def ExpectGit(self, *args):
    """Convenience wrapper."""
    self._git_mock.Expect(*args)

  def ExpectReadline(self, *args):
    """Convenience wrapper."""
    self._rl_mock.Expect(*args)

  def ExpectReadURL(self, *args):
    """Convenience wrapper."""
    self._url_mock.Expect(*args)

  def setUp(self):
    self._git_mock = SimpleMock("git")
    self._rl_mock = SimpleMock("readline")
    self._url_mock = SimpleMock("readurl")
    self._tmp_files = []

  def tearDown(self):
    Command("rm", "-rf %s*" % TEST_CONFIG[PERSISTFILE_BASENAME])

    # Clean up temps. Doesn't work automatically.
    for name in self._tmp_files:
      if os.path.exists(name):
        os.remove(name)

    self._git_mock.AssertFinished()
    self._rl_mock.AssertFinished()
    self._url_mock.AssertFinished()

  def testPersistRestore(self):
    self.MakeStep().Persist("test1", "")
    self.assertEquals("", self.MakeStep().Restore("test1"))
    self.MakeStep().Persist("test2", "AB123")
    self.assertEquals("AB123", self.MakeStep().Restore("test2"))

  def testGitOrig(self):
    self.assertTrue(Command("git", "--version").startswith("git version"))

  def testGitMock(self):
    self.ExpectGit([["--version", "git version 1.2.3"], ["dummy", ""]])
    self.assertEquals("git version 1.2.3", self.MakeStep().Git("--version"))
    self.assertEquals("", self.MakeStep().Git("dummy"))

  def testCommonPrepareDefault(self):
    self.ExpectGit([
      ["status -s -uno", ""],
      ["status -s -b -uno", "## some_branch"],
      ["svn fetch", ""],
      ["branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]],
      ["branch -D %s" % TEST_CONFIG[TEMP_BRANCH], ""],
      ["checkout -b %s" % TEST_CONFIG[TEMP_BRANCH], ""],
      ["branch", ""],
    ])
    self.ExpectReadline(["Y"])
    self.MakeStep().CommonPrepare()
    self.MakeStep().PrepareBranch()
    self.assertEquals("some_branch", self.MakeStep().Restore("current_branch"))

  def testCommonPrepareNoConfirm(self):
    self.ExpectGit([
      ["status -s -uno", ""],
      ["status -s -b -uno", "## some_branch"],
      ["svn fetch", ""],
      ["branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]],
    ])
    self.ExpectReadline(["n"])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self.MakeStep().Restore("current_branch"))

  def testCommonPrepareDeleteBranchFailure(self):
    self.ExpectGit([
      ["status -s -uno", ""],
      ["status -s -b -uno", "## some_branch"],
      ["svn fetch", ""],
      ["branch", "  branch1\n* %s" % TEST_CONFIG[TEMP_BRANCH]],
      ["branch -D %s" % TEST_CONFIG[TEMP_BRANCH], None],
    ])
    self.ExpectReadline(["Y"])
    self.MakeStep().CommonPrepare()
    self.assertRaises(Exception, self.MakeStep().PrepareBranch)
    self.assertEquals("some_branch", self.MakeStep().Restore("current_branch"))

  def testInitialEnvironmentChecks(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    os.environ["EDITOR"] = "vi"
    self.MakeStep().InitialEnvironmentChecks()

  def testReadAndPersistVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeTempVersionFile()
    step = self.MakeStep()
    step.ReadAndPersistVersion()
    self.assertEquals("3", self.MakeStep().Restore("major"))
    self.assertEquals("22", self.MakeStep().Restore("minor"))
    self.assertEquals("5", self.MakeStep().Restore("build"))
    self.assertEquals("0", self.MakeStep().Restore("patch"))
    self.assertEquals("3", step._state["major"])
    self.assertEquals("22", step._state["minor"])
    self.assertEquals("5", step._state["build"])
    self.assertEquals("0", step._state["patch"])

  def testRegex(self):
    self.assertEqual("(issue 321)",
                     re.sub(r"BUG=v8:(.*)$", r"(issue \1)", "BUG=v8:321"))
    self.assertEqual("(Chromium issue 321)",
                     re.sub(r"BUG=(.*)$", r"(Chromium issue \1)", "BUG=321"))

    cl = "  too little\n\ttab\ttab\n         too much\n        trailing  "
    cl = MSub(r"\t", r"        ", cl)
    cl = MSub(r"^ {1,7}([^ ])", r"        \1", cl)
    cl = MSub(r"^ {9,80}([^ ])", r"        \1", cl)
    cl = MSub(r" +$", r"", cl)
    self.assertEqual("        too little\n"
                     "        tab        tab\n"
                     "        too much\n"
                     "        trailing", cl)

    self.assertEqual("//\n#define BUILD_NUMBER  3\n",
                     MSub(r"(?<=#define BUILD_NUMBER)(?P<space>\s+)\d*$",
                          r"\g<space>3",
                          "//\n#define BUILD_NUMBER  321\n"))

  def testPrepareChangeLog(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeTempVersionFile()
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()

    self.ExpectGit([
      ["log 1234..HEAD --format=%H", "rev1\nrev2\nrev3\nrev4"],
      ["log -1 rev1 --format=\"%s\"", "Title text 1"],
      ["log -1 rev1 --format=\"%B\"", "Title\n\nBUG=\nLOG=y\n"],
      ["log -1 rev1 --format=\"%an\"", "author1@chromium.org"],
      ["log -1 rev2 --format=\"%s\"", "Title text 2."],
      ["log -1 rev2 --format=\"%B\"", "Title\n\nBUG=123\nLOG= \n"],
      ["log -1 rev2 --format=\"%an\"", "author2@chromium.org"],
      ["log -1 rev3 --format=\"%s\"", "Title text 3"],
      ["log -1 rev3 --format=\"%B\"", "Title\n\nBUG=321\nLOG=true\n"],
      ["log -1 rev3 --format=\"%an\"", "author3@chromium.org"],
      ["log -1 rev4 --format=\"%s\"", "Title text 4"],
      ["log -1 rev4 --format=\"%B\"",
       ("Title\n\nBUG=456\nLOG=Y\n\n"
        "Review URL: https://codereview.chromium.org/9876543210\n")],
      ["log -1 rev4 --format=\"%an\"", "author4@chromium.org"],
    ])

    # The cl for rev4 on rietveld has an updated LOG flag.
    self.ExpectReadURL([
      ["https://codereview.chromium.org/9876543210/description",
       "Title\n\nBUG=456\nLOG=N\n\n"],
    ])

    self.MakeStep().Persist("last_push", "1234")
    self.MakeStep(PrepareChangeLog).Run()

    actual_cl = FileToText(TEST_CONFIG[CHANGELOG_ENTRY_FILE])

    # TODO(machenbach): Mock out call to date() in order to make a fixed
    # comparison here instead of a regexp match.
    expected_cl = """\\d+\\-\\d+\\-\\d+: Version 3\\.22\\.5

        Title text 1.

        Title text 3 \\(Chromium issue 321\\).

        Performance and stability improvements on all platforms\\.
#
# The change log above is auto-generated\\. Please review if all relevant
# commit messages from the list below are included\\.
# All lines starting with # will be stripped\\.
#
#       Title text 1.
#       \\(author1@chromium\\.org\\)
#
#       Title text 2 \\(Chromium issue 123\\).
#       \\(author2@chromium\\.org\\)
#
#       Title text 3 \\(Chromium issue 321\\).
#       \\(author3@chromium\\.org\\)
#
#       Title text 4 \\(Chromium issue 456\\).
#       \\(author4@chromium\\.org\\)
#
#"""

    self.assertTrue(re.match(expected_cl, actual_cl))
    self.assertEquals("3", self.MakeStep().Restore("major"))
    self.assertEquals("22", self.MakeStep().Restore("minor"))
    self.assertEquals("5", self.MakeStep().Restore("build"))
    self.assertEquals("0", self.MakeStep().Restore("patch"))

  def testEditChangeLog(self):
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    TEST_CONFIG[CHANGELOG_FILE] = self.MakeEmptyTempFile()
    TextToFile("        Original CL", TEST_CONFIG[CHANGELOG_FILE])
    TextToFile("  New  \n\tLines  \n", TEST_CONFIG[CHANGELOG_ENTRY_FILE])
    os.environ["EDITOR"] = "vi"

    self.ExpectReadline([
      "",  # Open editor.
    ])

    self.MakeStep(EditChangeLog).Run()

    self.assertEquals("New\n        Lines\n\n\n        Original CL",
                      FileToText(TEST_CONFIG[CHANGELOG_FILE]))

  def testIncrementVersion(self):
    TEST_CONFIG[VERSION_FILE] = self.MakeTempVersionFile()
    self.MakeStep().Persist("build", "5")

    self.ExpectReadline([
      "Y",  # Increment build number.
    ])

    self.MakeStep(IncrementVersion).Run()

    self.assertEquals("3", self.MakeStep().Restore("new_major"))
    self.assertEquals("22", self.MakeStep().Restore("new_minor"))
    self.assertEquals("6", self.MakeStep().Restore("new_build"))
    self.assertEquals("0", self.MakeStep().Restore("new_patch"))

  def testLastChangeLogEntries(self):
    TEST_CONFIG[CHANGELOG_FILE] = self.MakeEmptyTempFile()
    l = """
        Fixed something.
        (issue 1234)\n"""
    for _ in xrange(10): l = l + l

    cl_chunk = """2013-11-12: Version 3.23.2\n%s
        Performance and stability improvements on all platforms.\n\n\n""" % l

    cl_chunk_full = cl_chunk + cl_chunk + cl_chunk
    TextToFile(cl_chunk_full, TEST_CONFIG[CHANGELOG_FILE])

    cl = GetLastChangeLogEntries(TEST_CONFIG[CHANGELOG_FILE])
    self.assertEquals(cl_chunk, cl)

  def testSquashCommits(self):
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    with open(TEST_CONFIG[CHANGELOG_ENTRY_FILE], "w") as f:
      f.write("1999-11-11: Version 3.22.5\n")
      f.write("\n")
      f.write("        Log text 1.\n")
      f.write("        Chromium issue 12345\n")
      f.write("\n")
      f.write("        Performance and stability improvements on all "
              "platforms.\n")

    self.ExpectGit([
      ["diff svn/trunk hash1", "patch content"],
    ])

    self.MakeStep().Persist("prepare_commit_hash", "hash1")
    self.MakeStep().Persist("date", "1999-11-11")

    self.MakeStep(SquashCommits).Run()

    msg = FileToText(TEST_CONFIG[COMMITMSG_FILE])
    self.assertTrue(re.search(r"Version 3\.22\.5", msg))
    self.assertTrue(re.search(r"Performance and stability", msg))
    self.assertTrue(re.search(r"Log text 1\. Chromium issue 12345", msg))
    self.assertFalse(re.search(r"\d+\-\d+\-\d+", msg))

    patch = FileToText(TEST_CONFIG[ PATCH_FILE])
    self.assertTrue(re.search(r"patch content", patch))

  def _PushToTrunk(self, force=False):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()
    TEST_CONFIG[VERSION_FILE] = self.MakeTempVersionFile()
    TEST_CONFIG[CHANGELOG_ENTRY_FILE] = self.MakeEmptyTempFile()
    TEST_CONFIG[CHANGELOG_FILE] = self.MakeEmptyTempFile()
    if not os.path.exists(TEST_CONFIG[CHROMIUM]):
      os.makedirs(TEST_CONFIG[CHROMIUM])
    TextToFile("1999-04-05: Version 3.22.4", TEST_CONFIG[CHANGELOG_FILE])
    TextToFile("Some line\n   \"v8_revision\": \"123444\",\n  some line",
                 TEST_CONFIG[DEPS_FILE])
    os.environ["EDITOR"] = "vi"

    def CheckPreparePush():
      cl = FileToText(TEST_CONFIG[CHANGELOG_FILE])
      self.assertTrue(re.search(r"Version 3.22.5", cl))
      self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
      self.assertFalse(re.search(r"        \(author1@chromium\.org\)", cl))

      # Make sure all comments got stripped.
      self.assertFalse(re.search(r"^#", cl, flags=re.M))

      version = FileToText(TEST_CONFIG[VERSION_FILE])
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+6", version))

    def CheckUpload():
      cl = FileToText(TEST_CONFIG[CHANGELOG_FILE])

    def CheckSVNCommit():
      commit = FileToText(TEST_CONFIG[COMMITMSG_FILE])
      self.assertTrue(re.search(r"Version 3.22.5", commit))
      self.assertTrue(re.search(r"Log text 1 \(issue 321\).", commit))
      version = FileToText(TEST_CONFIG[VERSION_FILE])
      self.assertTrue(re.search(r"#define MINOR_VERSION\s+22", version))
      self.assertTrue(re.search(r"#define BUILD_NUMBER\s+5", version))
      self.assertFalse(re.search(r"#define BUILD_NUMBER\s+6", version))
      self.assertTrue(re.search(r"#define PATCH_LEVEL\s+0", version))
      self.assertTrue(re.search(r"#define IS_CANDIDATE_VERSION\s+0", version))

    force_flag = " -f" if force else ""
    self.ExpectGit([
      ["status -s -uno", ""],
      ["status -s -b -uno", "## some_branch\n"],
      ["svn fetch", ""],
      ["branch", "  branch1\n* branch2\n"],
      ["checkout -b %s" % TEST_CONFIG[TEMP_BRANCH], ""],
      ["branch", "  branch1\n* branch2\n"],
      ["branch", "  branch1\n* branch2\n"],
      ["checkout -b %s svn/bleeding_edge" % TEST_CONFIG[BRANCHNAME], ""],
      ["log -1 --format=%H ChangeLog", "1234\n"],
      ["log -1 1234", "Last push ouput\n"],
      ["log 1234..HEAD --format=%H", "rev1\n"],
      ["log -1 rev1 --format=\"%s\"", "Log text 1.\n"],
      ["log -1 rev1 --format=\"%B\"", "Text\nLOG=YES\nBUG=v8:321\nText\n"],
      ["log -1 rev1 --format=\"%an\"", "author1@chromium.org\n"],
      [("commit -a -m \"Prepare push to trunk.  "
        "Now working on version 3.22.6.\""),
       " 2 files changed\n",
        CheckPreparePush],
      ["cl upload -r \"reviewer@chromium.org\" --send-mail%s" % force_flag,
       "done\n"],
      ["cl dcommit -f", "Closing issue\n"],
      ["svn fetch", "fetch result\n"],
      ["checkout svn/bleeding_edge", ""],
      [("log -1 --format=%H --grep=\"Prepare push to trunk.  "
        "Now working on version 3.22.6.\""),
       "hash1\n"],
      ["diff svn/trunk hash1", "patch content\n"],
      ["checkout -b %s svn/trunk" % TEST_CONFIG[TRUNKBRANCH], ""],
      ["apply --index --reject  \"%s\"" % TEST_CONFIG[PATCH_FILE], ""],
      ["add \"%s\"" % TEST_CONFIG[VERSION_FILE], ""],
      ["commit -F \"%s\"" % TEST_CONFIG[COMMITMSG_FILE], "", CheckSVNCommit],
      ["svn dcommit 2>&1", "Some output\nCommitted r123456\nSome output\n"],
      ["svn tag 3.22.5 -m \"Tagging version 3.22.5\"", ""],
      ["status -s -uno", ""],
      ["checkout master", ""],
      ["pull", ""],
      ["checkout -b v8-roll-123456", ""],
      [("commit -am \"Update V8 to version 3.22.5.\n\n"
        "TBR=reviewer@chromium.org\""),
       ""],
      ["cl upload --send-mail%s" % force_flag, ""],
      ["checkout -f some_branch", ""],
      ["branch -D %s" % TEST_CONFIG[TEMP_BRANCH], ""],
      ["branch -D %s" % TEST_CONFIG[BRANCHNAME], ""],
      ["branch -D %s" % TEST_CONFIG[TRUNKBRANCH], ""],
    ])
    self.ExpectReadline([
      "Y",  # Confirm last push.
      "",  # Open editor.
      "Y",  # Increment build number.
      "reviewer@chromium.org",  # V8 reviewer.
      "LGTX",  # Enter LGTM for V8 CL (wrong).
      "LGTM",  # Enter LGTM for V8 CL.
      "Y",  # Sanity check.
      "reviewer@chromium.org",  # Chromium reviewer.
    ])
    if force:
      # TODO(machenbach): The lgtm for the prepare push is just temporary.
      # There should be no user input in "force" mode.
      self.ExpectReadline([
        "LGTM",  # Enter LGTM for V8 CL.
      ])

    class Options( object ):
      pass

    options = Options()
    options.s = 0
    options.l = None
    options.f = force
    options.r = "reviewer@chromium.org" if force else None
    options.c = TEST_CONFIG[CHROMIUM]
    RunPushToTrunk(TEST_CONFIG, options, self)

    deps = FileToText(TEST_CONFIG[DEPS_FILE])
    self.assertTrue(re.search("\"v8_revision\": \"123456\"", deps))

    cl = FileToText(TEST_CONFIG[CHANGELOG_FILE])
    self.assertTrue(re.search(r"^\d\d\d\d\-\d+\-\d+: Version 3\.22\.5", cl))
    self.assertTrue(re.search(r"        Log text 1 \(issue 321\).", cl))
    self.assertTrue(re.search(r"1999\-04\-05: Version 3\.22\.4", cl))

    # Note: The version file is on build number 5 again in the end of this test
    # since the git command that merges to the bleeding edge branch is mocked
    # out.

  def testPushToTrunk(self):
    self._PushToTrunk()

  def testPushToTrunkForced(self):
    self._PushToTrunk(force=True)

  def testAutoRoll(self):
    TEST_CONFIG[DOT_GIT_LOCATION] = self.MakeEmptyTempFile()

    # TODO(machenbach): Get rid of the editor check in automatic mode.
    os.environ["EDITOR"] = "vi"

    self.ExpectReadURL([
      ["https://v8-status.appspot.com/lkgr", Exception("Network problem")],
      ["https://v8-status.appspot.com/lkgr", "100"],
    ])

    self.ExpectGit([
      ["status -s -uno", ""],
      ["status -s -b -uno", "## some_branch\n"],
      ["svn fetch", ""],
      ["svn log -1 --oneline", "r101 | Text"],
    ])

    # TODO(machenbach): Make a convenience wrapper for this.
    class Options( object ):
      pass

    options = Options()
    options.s = 0

    auto_roll.RunAutoRoll(TEST_CONFIG, options, self)

    self.assertEquals("100", self.MakeStep().Restore("lkgr"))
    self.assertEquals("101", self.MakeStep().Restore("latest"))


class SystemTest(unittest.TestCase):
  def testReload(self):
    step = MakeStep(step_class=PrepareChangeLog, number=0, state={}, config={},
                    options=None,
                    side_effect_handler=DEFAULT_SIDE_EFFECT_HANDLER)
    body = step.Reload(
"""------------------------------------------------------------------------
r17997 | machenbach@chromium.org | 2013-11-22 11:04:04 +0100 (...) | 6 lines

Prepare push to trunk.  Now working on version 3.23.11.

R=danno@chromium.org

Review URL: https://codereview.chromium.org/83173002

------------------------------------------------------------------------""")
    self.assertEquals(
"""Prepare push to trunk.  Now working on version 3.23.11.

R=danno@chromium.org

Committed: https://code.google.com/p/v8/source/detail?r=17997""", body)
