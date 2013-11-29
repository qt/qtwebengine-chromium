# Copyright (C) 2010 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#    * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#    * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#    * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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

import webkitpy.thirdparty.unittest2 as unittest

from webkitpy.common.system.outputcapture import OutputCapture
from webkitpy.common.checkout.baselineoptimizer import BaselineOptimizer
from webkitpy.common.net.buildbot.buildbot_mock import MockBuilder
from webkitpy.common.net.layouttestresults import LayoutTestResults
from webkitpy.common.system.executive_mock import MockExecutive2
from webkitpy.thirdparty.mock import Mock
from webkitpy.tool.commands.rebaseline import *
from webkitpy.tool.mocktool import MockTool, MockOptions


class _BaseTestCase(unittest.TestCase):
    MOCK_WEB_RESULT = 'MOCK Web result, convert 404 to None=True'
    WEB_PREFIX = 'http://example.com/f/builders/WebKit Mac10.7/results/layout-test-results'

    command_constructor = None

    def setUp(self):
        self.tool = MockTool()
        self.command = self.command_constructor()  # lint warns that command_constructor might not be set, but this is intentional; pylint: disable=E1102
        self.command.bind_to_tool(self.tool)
        self.lion_port = self.tool.port_factory.get_from_builder_name("WebKit Mac10.7")
        self.lion_expectations_path = self.lion_port.path_to_generic_test_expectations_file()

        # FIXME: we should override builders._exact_matches here to point to a set
        # of test ports and restore the value in tearDown(), and that way the
        # individual tests wouldn't have to worry about it.

    def _expand(self, path):
        if self.tool.filesystem.isabs(path):
            return path
        return self.tool.filesystem.join(self.lion_port.layout_tests_dir(), path)

    def _read(self, path):
        return self.tool.filesystem.read_text_file(self._expand(path))

    def _write(self, path, contents):
        self.tool.filesystem.write_text_file(self._expand(path), contents)

    def _zero_out_test_expectations(self):
        for port_name in self.tool.port_factory.all_port_names():
            port = self.tool.port_factory.get(port_name)
            for path in port.expectations_files():
                self._write(path, '')
        self.tool.filesystem.written_files = {}

    def _setup_mock_builder_data(self):
        data = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "userscripts": {
            "first-test.html": {
                "expected": "PASS",
                "actual": "IMAGE+TEXT"
            },
            "second-test.html": {
                "expected": "FAIL",
                "actual": "IMAGE+TEXT"
            }
        }
    }
});""")
        for builder in ['MOCK builder', 'MOCK builder (Debug)', 'WebKit Mac10.7']:
            self.command._builder_data[builder] = data


class TestCopyExistingBaselinesInternal(_BaseTestCase):
    command_constructor = CopyExistingBaselinesInternal  # AKA webkit-patch rebaseline-test-internal

    def setUp(self):
        super(TestCopyExistingBaselinesInternal, self).setUp()

    def test_copying_overwritten_baseline(self):
        self.tool.executive = MockExecutive2()

        # FIXME: it's confusing that this is the test- port, and not the regular lion port. Really all of the tests should be using the test ports.
        port = self.tool.port_factory.get('test-mac-snowleopard')
        self._write(port._filesystem.join(port.layout_tests_dir(), 'platform/test-mac-snowleopard/failures/expected/image-expected.txt'), 'original snowleopard result')

        old_exact_matches = builders._exact_matches
        oc = OutputCapture()
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK SnowLeopard": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier"])},
            }

            options = MockOptions(builder="MOCK SnowLeopard", suffixes="txt", verbose=True, test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()
            builders._exact_matches = old_exact_matches

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-mac-leopard/failures/expected/image-expected.txt')), 'original snowleopard result')
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": []}\n')

    def test_copying_overwritten_baseline_to_multiple_locations(self):
        self.tool.executive = MockExecutive2()

        # FIXME: it's confusing that this is the test- port, and not the regular win port. Really all of the tests should be using the test ports.
        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt'), 'original win7 result')

        old_exact_matches = builders._exact_matches
        oc = OutputCapture()
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK Linux": {"port_name": "test-linux-x86_64", "specifiers": set(["mock-specifier"])},
                "MOCK Win7": {"port_name": "test-win-win7", "specifiers": set(["mock-specifier"])},
            }

            options = MockOptions(builder="MOCK Win7", suffixes="txt", verbose=True, test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()
            builders._exact_matches = old_exact_matches

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-linux-x86_64/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/mac-leopard/userscripts/another-test-expected.txt')))
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": []}\n')

    def test_no_copy_existing_baseline(self):
        self.tool.executive = MockExecutive2()

        # FIXME: it's confusing that this is the test- port, and not the regular win port. Really all of the tests should be using the test ports.
        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt'), 'original win7 result')

        old_exact_matches = builders._exact_matches
        oc = OutputCapture()
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK Linux": {"port_name": "test-linux-x86_64", "specifiers": set(["mock-specifier"])},
                "MOCK Win7": {"port_name": "test-win-win7", "specifiers": set(["mock-specifier"])},
            }

            options = MockOptions(builder="MOCK Win7", suffixes="txt", verbose=True, test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()
            builders._exact_matches = old_exact_matches

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-linux-x86_64/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')), 'original win7 result')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/mac-leopard/userscripts/another-test-expected.txt')))
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": []}\n')


class TestRebaselineTest(_BaseTestCase):
    command_constructor = RebaselineTest  # AKA webkit-patch rebaseline-test-internal

    def setUp(self):
        super(TestRebaselineTest, self).setUp()
        self.options = MockOptions(builder="WebKit Mac10.7", test="userscripts/another-test.html", suffixes="txt", results_directory=None)

    def test_baseline_directory(self):
        command = self.command
        self.assertMultiLineEqual(command._baseline_directory("WebKit Mac10.7"), "/mock-checkout/LayoutTests/platform/mac-lion")
        self.assertMultiLineEqual(command._baseline_directory("WebKit Mac10.6"), "/mock-checkout/LayoutTests/platform/mac-snowleopard")

    def test_rebaseline_updates_expectations_file_noop(self):
        self._zero_out_test_expectations()
        self._write(self.lion_expectations_path, """Bug(B) [ Mac Linux XP Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]
Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]
""")
        self._write("fast/dom/Window/window-postmessage-clone-really-deep-array.html", "Dummy test contents")
        self._write("fast/css/large-list-of-rules-crash.html", "Dummy test contents")
        self._write("userscripts/another-test.html", "Dummy test contents")

        self.options.suffixes = "png,wav,txt"
        self.command._rebaseline_test_and_update_expectations(self.options)

        self.assertItemsEqual(self.tool.web.urls_fetched,
            [self.WEB_PREFIX + '/userscripts/another-test-actual.png',
             self.WEB_PREFIX + '/userscripts/another-test-actual.wav',
             self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])
        new_expectations = self._read(self.lion_expectations_path)
        self.assertMultiLineEqual(new_expectations, """Bug(B) [ Mac Linux XP Debug ] fast/dom/Window/window-postmessage-clone-really-deep-array.html [ Pass ]
Bug(A) [ Debug ] : fast/css/large-list-of-rules-crash.html [ Failure ]
""")

    def test_rebaseline_test(self):
        self.command._rebaseline_test("WebKit Linux", "userscripts/another-test.html", "txt", self.WEB_PREFIX)
        self.assertItemsEqual(self.tool.web.urls_fetched, [self.WEB_PREFIX + '/userscripts/another-test-actual.txt'])

    def test_rebaseline_test_with_results_directory(self):
        self._write("userscripts/another-test.html", "test data")
        self._write(self.lion_expectations_path, "Bug(x) [ Mac ] userscripts/another-test.html [ ImageOnlyFailure ]\nbug(z) [ Linux ] userscripts/another-test.html [ ImageOnlyFailure ]\n")
        self.options.results_directory = '/tmp'
        self.command._rebaseline_test_and_update_expectations(self.options)
        self.assertItemsEqual(self.tool.web.urls_fetched, ['file:///tmp/userscripts/another-test-actual.txt'])

    def test_rebaseline_reftest(self):
        self._write("userscripts/another-test.html", "test data")
        self._write("userscripts/another-test-expected.html", "generic result")
        OutputCapture().assert_outputs(self, self.command._rebaseline_test_and_update_expectations, args=[self.options],
            expected_logs="Cannot rebaseline reftest: userscripts/another-test.html\n")
        self.assertDictEqual(self.command._scm_changes, {'add': [], 'remove-lines': []})

    def test_rebaseline_test_and_print_scm_changes(self):
        self.command._print_scm_changes = True
        self.command._scm_changes = {'add': [], 'delete': []}
        self.tool._scm.exists = lambda x: False

        self.command._rebaseline_test("WebKit Linux", "userscripts/another-test.html", "txt", None)

        self.assertDictEqual(self.command._scm_changes, {'add': ['/mock-checkout/LayoutTests/platform/linux/userscripts/another-test-expected.txt'], 'delete': []})

    def test_rebaseline_test_internal_with_port_that_lacks_buildbot(self):
        self.tool.executive = MockExecutive2()

        # FIXME: it's confusing that this is the test- port, and not the regular win port. Really all of the tests should be using the test ports.
        port = self.tool.port_factory.get('test-win-win7')
        self._write(port._filesystem.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt'), 'original win7 result')

        old_exact_matches = builders._exact_matches
        oc = OutputCapture()
        try:
            builders._exact_matches = {
                "MOCK XP": {"port_name": "test-win-xp"},
                "MOCK Win7": {"port_name": "test-win-win7"},
            }

            options = MockOptions(optimize=True, builder="MOCK Win7", suffixes="txt",
                verbose=True, test="failures/expected/image.html", results_directory=None)

            oc.capture_output()
            self.command.execute(options, [], self.tool)
        finally:
            out, _, _ = oc.restore_output()
            builders._exact_matches = old_exact_matches

        self.assertMultiLineEqual(self._read(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-win-win7/failures/expected/image-expected.txt')), 'MOCK Web result, convert 404 to None=True')
        self.assertFalse(self.tool.filesystem.exists(self.tool.filesystem.join(port.layout_tests_dir(), 'platform/test-win-xp/failures/expected/image-expected.txt')))
        self.assertMultiLineEqual(out, '{"add": [], "remove-lines": [{"test": "failures/expected/image.html", "builder": "MOCK Win7"}]}\n')


class TestAbstractParallelRebaselineCommand(_BaseTestCase):
    command_constructor = AbstractParallelRebaselineCommand

    def test_builders_to_fetch_from(self):
        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK XP": {"port_name": "test-win-xp"},
                "MOCK Win7": {"port_name": "test-win-win7"},
                "MOCK Win7 (dbg)(1)": {"port_name": "test-win-win7"},
                "MOCK Win7 (dbg)(2)": {"port_name": "test-win-win7"},
            }

            builders_to_fetch = self.command._builders_to_fetch_from(["MOCK XP", "MOCK Win7 (dbg)(1)", "MOCK Win7 (dbg)(2)", "MOCK Win7"])
            self.assertEqual(builders_to_fetch, ["MOCK XP", "MOCK Win7"])
        finally:
            builders._exact_matches = old_exact_matches


class TestRebaselineJson(_BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJson, self).setUp()
        self.tool.executive = MockExecutive2()
        self.old_exact_matches = builders._exact_matches
        builders._exact_matches = {
            "MOCK builder": {"port_name": "test-mac-snowleopard"},
            "MOCK builder (Debug)": {"port_name": "test-mac-snowleopard"},
        }

    def tearDown(self):
        builders._exact_matches = self.old_exact_matches
        super(TestRebaselineJson, self).tearDown()

    def test_rebaseline_all(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=True, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options,  {"userscripts/first-test.html": {"MOCK builder": ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(self.tool.executive.calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose']],
             ['echo', '--verbose', 'optimize-baselines', '--suffixes', 'txt,png', 'userscripts/first-test.html']])

    def test_rebaseline_debug(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=True, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options,  {"userscripts/first-test.html": {"MOCK builder (Debug)": ["txt", "png"]}})

        # Note that we have one run_in_parallel() call followed by a run_command()
        self.assertEqual(self.tool.executive.calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder (Debug)', '--test', 'userscripts/first-test.html', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder (Debug)', '--test', 'userscripts/first-test.html', '--verbose']],
             ['echo', '--verbose', 'optimize-baselines', '--suffixes', 'txt,png', 'userscripts/first-test.html']])

    def test_no_optimize(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=False, verbose=True, results_directory=None)
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options,  {"userscripts/first-test.html": {"MOCK builder (Debug)": ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(self.tool.executive.calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder (Debug)', '--test', 'userscripts/first-test.html', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder (Debug)', '--test', 'userscripts/first-test.html', '--verbose']]])

    def test_results_directory(self):
        self._setup_mock_builder_data()

        options = MockOptions(optimize=False, verbose=True, results_directory='/tmp')
        self._write("userscripts/first-test.html", "Dummy test contents")
        self.command._rebaseline(options,  {"userscripts/first-test.html": {"MOCK builder": ["txt", "png"]}})

        # Note that we have only one run_in_parallel() call
        self.assertEqual(self.tool.executive.calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--results-directory', '/tmp', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--results-directory', '/tmp', '--verbose']]])

class TestRebaselineJsonUpdatesExpectationsFiles(_BaseTestCase):
    command_constructor = RebaselineJson

    def setUp(self):
        super(TestRebaselineJsonUpdatesExpectationsFiles, self).setUp()
        self.tool.executive = MockExecutive2()

        def mock_run_command(args,
                             cwd=None,
                             input=None,
                             error_handler=None,
                             return_exit_code=False,
                             return_stderr=True,
                             decode_output=False,
                             env=None):
            return '{"add": [], "remove-lines": [{"test": "userscripts/first-test.html", "builder": "WebKit Mac10.7"}]}\n'
        self.tool.executive.run_command = mock_run_command

    def test_rebaseline_updates_expectations_file(self):
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.lion_expectations_path, "Bug(x) [ Mac ] userscripts/first-test.html [ ImageOnlyFailure ]\nbug(z) [ Linux ] userscripts/first-test.html [ ImageOnlyFailure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options,  {"userscripts/first-test.html": {"WebKit Mac10.7": ["txt", "png"]}})

        new_expectations = self._read(self.lion_expectations_path)
        self.assertMultiLineEqual(new_expectations, "Bug(x) [ MountainLion SnowLeopard ] userscripts/first-test.html [ ImageOnlyFailure ]\nbug(z) [ Linux ] userscripts/first-test.html [ ImageOnlyFailure ]\n")

    def test_rebaseline_updates_expectations_file_all_platforms(self):
        options = MockOptions(optimize=False, verbose=True, results_directory=None)

        self._write(self.lion_expectations_path, "Bug(x) userscripts/first-test.html [ ImageOnlyFailure ]\n")
        self._write("userscripts/first-test.html", "Dummy test contents")
        self._setup_mock_builder_data()

        self.command._rebaseline(options,  {"userscripts/first-test.html": {"WebKit Mac10.7": ["txt", "png"]}})

        new_expectations = self._read(self.lion_expectations_path)
        self.assertMultiLineEqual(new_expectations, "Bug(x) [ Linux MountainLion SnowLeopard Win ] userscripts/first-test.html [ ImageOnlyFailure ]\n")


class TestRebaseline(_BaseTestCase):
    # This command shares most of its logic with RebaselineJson, so these tests just test what is different.

    command_constructor = Rebaseline  # AKA webkit-patch rebaseline

    def test_rebaseline(self):
        self.command._builders_to_pull_from = lambda: [MockBuilder('MOCK builder')]

        self._write("userscripts/first-test.html", "test data")

        self._zero_out_test_expectations()
        self._setup_mock_builder_data()

        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
            }
            self.command.execute(MockOptions(results_directory=False, optimize=False, builders=None, suffixes="txt,png", verbose=True), ['userscripts/first-test.html'], self.tool)
        finally:
            builders._exact_matches = old_exact_matches

        calls = filter(lambda x: x != ['qmake', '-v'] and x[0] != 'perl', self.tool.executive.calls)
        self.assertEqual(calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose']]])

    def test_rebaseline_directory(self):
        self.command._builders_to_pull_from = lambda: [MockBuilder('MOCK builder')]

        self._write("userscripts/first-test.html", "test data")
        self._write("userscripts/second-test.html", "test data")

        self._setup_mock_builder_data()

        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK builder": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
            }
            self.command.execute(MockOptions(results_directory=False, optimize=False, builders=None, suffixes="txt,png", verbose=True), ['userscripts'], self.tool)
        finally:
            builders._exact_matches = old_exact_matches

        calls = filter(lambda x: x != ['qmake', '-v'] and x[0] != 'perl', self.tool.executive.calls)
        self.assertEqual(calls,
            [[['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose'],
              ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/second-test.html', '--verbose']],
             [['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/first-test.html', '--verbose'],
              ['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK builder', '--test', 'userscripts/second-test.html', '--verbose']]])


class TestRebaselineExpectations(_BaseTestCase):
    command_constructor = RebaselineExpectations

    def setUp(self):
        super(TestRebaselineExpectations, self).setUp()
        self.options = MockOptions(optimize=False, builders=None, suffixes=['txt'], verbose=False, platform=None, results_directory=None)

    def test_rebaseline_expectations(self):
        self._zero_out_test_expectations()

        self.tool.executive = MockExecutive2()

        def builder_data():
            self.command._builder_data['MOCK SnowLeopard'] = self.command._builder_data['MOCK Leopard'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "userscripts": {
            "another-test.html": {
                "expected": "PASS",
                "actual": "PASS TEXT"
            },
            "images.svg": {
                "expected": "FAIL",
                "actual": "IMAGE+TEXT"
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self._write("userscripts/another-test.html", "Dummy test contents")
        self._write("userscripts/images.svg", "Dummy test contents")
        self.command._tests_to_rebaseline = lambda port: {
            'userscripts/another-test.html': set(['txt']),
            'userscripts/images.svg': set(['png']),
            'userscripts/not-actually-failing.html': set(['txt', 'png', 'wav']),
        }

        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK SnowLeopard": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier"])},
            }
            self.command.execute(self.options, [], self.tool)
        finally:
            builders._exact_matches = old_exact_matches

        # FIXME: change this to use the test- ports.
        calls = filter(lambda x: x != ['qmake', '-v'], self.tool.executive.calls)
        self.assertEqual(self.tool.executive.calls, [
            [
                ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'userscripts/another-test.html'],
                ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'userscripts/another-test.html'],
                ['echo', 'copy-existing-baselines-internal', '--suffixes', 'png', '--builder', 'MOCK Leopard', '--test', 'userscripts/images.svg'],
                ['echo', 'copy-existing-baselines-internal', '--suffixes', 'png', '--builder', 'MOCK SnowLeopard', '--test', 'userscripts/images.svg']
            ],
            [
                ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'userscripts/another-test.html'],
                ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'userscripts/another-test.html'],
                ['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'MOCK Leopard', '--test', 'userscripts/images.svg'],
                ['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'MOCK SnowLeopard', '--test', 'userscripts/images.svg']
            ]
        ])

    def test_rebaseline_expectations_noop(self):
        self._zero_out_test_expectations()

        oc = OutputCapture()
        try:
            oc.capture_output()
            self.command.execute(self.options, [], self.tool)
        finally:
            _, _, logs = oc.restore_output()
            self.assertEqual(self.tool.filesystem.written_files, {})
            self.assertEqual(logs, 'Did not find any tests marked Rebaseline.\n')

    def disabled_test_overrides_are_included_correctly(self):
        # This tests that the any tests marked as REBASELINE in the overrides are found, but
        # that the overrides do not get written into the main file.
        self._zero_out_test_expectations()

        self._write(self.lion_expectations_path, '')
        self.lion_port.expectations_dict = lambda: {
            self.lion_expectations_path: '',
            'overrides': ('Bug(x) userscripts/another-test.html [ Failure Rebaseline ]\n'
                          'Bug(y) userscripts/test.html [ Crash ]\n')}
        self._write('/userscripts/another-test.html', '')

        self.assertDictEqual(self.command._tests_to_rebaseline(self.lion_port), {'userscripts/another-test.html': set(['png', 'txt', 'wav'])})
        self.assertEqual(self._read(self.lion_expectations_path), '')

    def test_rebaseline_without_other_expectations(self):
        self._write("userscripts/another-test.html", "Dummy test contents")
        self._write(self.lion_expectations_path, "Bug(x) userscripts/another-test.html [ Rebaseline ]\n")
        self.assertDictEqual(self.command._tests_to_rebaseline(self.lion_port), {'userscripts/another-test.html': ('png', 'wav', 'txt')})


class _FakeOptimizer(BaselineOptimizer):
    def read_results_by_directory(self, baseline_name):
        if baseline_name.endswith('txt'):
            return {'LayoutTests/passes/text.html': '123456'}
        return {}


class TestAnalyzeBaselines(_BaseTestCase):
    command_constructor = AnalyzeBaselines

    def setUp(self):
        super(TestAnalyzeBaselines, self).setUp()
        self.port = self.tool.port_factory.get('test')
        self.tool.port_factory.get = (lambda port_name=None, options=None: self.port)
        self.lines = []
        self.command._optimizer_class = _FakeOptimizer
        self.command._write = (lambda msg: self.lines.append(msg))  # pylint bug warning about unnecessary lambda? pylint: disable=W0108

    def test_default(self):
        self.command.execute(MockOptions(suffixes='txt', missing=False, platform=None), ['passes/text.html'], self.tool)
        self.assertEqual(self.lines,
            ['passes/text-expected.txt:',
             '  (generic): 123456'])

    def test_missing_baselines(self):
        self.command.execute(MockOptions(suffixes='png,txt', missing=True, platform=None), ['passes/text.html'], self.tool)
        self.assertEqual(self.lines,
            ['passes/text-expected.png: (no baselines found)',
             'passes/text-expected.txt:',
             '  (generic): 123456'])


class TestAutoRebaseline(_BaseTestCase):
    command_constructor = AutoRebaseline

    def _write_test_file(self, port, path, contents):
        abs_path = self.tool.filesystem.join(port.layout_tests_dir(), path)
        self.tool.filesystem.write_text_file(abs_path, contents)

    def setUp(self):
        super(TestAutoRebaseline, self).setUp()
        self.command.latest_revision_processed_on_all_bots = lambda: 9000

    def test_tests_to_rebaseline(self):
        def blame(path):
            return """
624c3081c0 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ ImageOnlyFailure ]
624c3081c0 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   13) Bug(foo) path/to/rebaseline-without-bug-number.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/rebaseline-with-modifiers.html [ NeedsRebaseline ]
624c3081c0 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   12) crbug.com/24182 crbug.com/234 path/to/rebaseline-without-modifiers.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/rebaseline-new-revision.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (foo@chromium.org        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (foo@chromium.org        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
                set(['path/to/rebaseline-without-bug-number.html', 'path/to/rebaseline-with-modifiers.html', 'path/to/rebaseline-without-modifiers.html']),
                5678,
                'foobarbaz1@chromium.org',
                set(['24182', '234'])))

    def test_tests_to_rebaseline_over_limit(self):
        def blame(path):
            result = ""
            for i in range(0, self.command.MAX_LINES_TO_REBASELINE + 1):
                result += "624c3081c0 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   13) crbug.com/24182 path/to/rebaseline-%s.html [ NeedsRebaseline ]\n" % i
            return result
        self.tool.scm().blame = blame

        expected_list_of_tests = []
        for i in range(0, self.command.MAX_LINES_TO_REBASELINE):
            expected_list_of_tests.append("path/to/rebaseline-%s.html" % i)

        min_revision = 9000
        self.assertEqual(self.command.tests_to_rebaseline(self.tool, min_revision, print_revisions=False), (
                set(expected_list_of_tests),
                5678,
                'foobarbaz1@chromium.org',
                set(['24182'])))

    def test_commit_message(self):
        author = "foo@chromium.org"
        revision = 1234
        bugs = set()
        self.assertEqual(self.command.commit_message(author, revision, bugs),
            """Auto-rebaseline for r1234

http://src.chromium.org/viewvc/blink?view=revision&revision=1234

TBR=foo@chromium.org
""")

        bugs = set(["234", "345"])
        self.assertEqual(self.command.commit_message(author, revision, bugs),
            """Auto-rebaseline for r1234

http://src.chromium.org/viewvc/blink?view=revision&revision=1234

BUG=234,345
TBR=foo@chromium.org
""")

    def test_no_needs_rebaseline_lines(self):
        def blame(path):
            return """
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ ImageOnlyFailure ]
"""
        self.tool.scm().blame = blame

        self.command.execute(MockOptions(optimize=True, verbose=False, move_overwritten_baselines=False, results_directory=False), [], self.tool)
        self.assertEqual(self.tool.executive.calls, [])

    def test_execute(self):
        def blame(path):
            return """
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ Debug ] path/to/norebaseline.html [ ImageOnlyFailure ]
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-06-14 20:18:46 +0000   11) crbug.com/24182 [ SnowLeopard ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   12) crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
624caaaaaa path/to/TestExpectations                   (foo@chromium.org        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
0000000000 path/to/TestExpectations                   (foo@chromium.org        2013-04-28 04:52:41 +0000   12) crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')
        original_get = self.tool.port_factory.get

        def get_test_port(port_name=None, options=None, **kwargs):
            if not port_name:
                return test_port
            return original_get(port_name, options, **kwargs)
        # Need to make sure all the ports grabbed use the test checkout path instead of the mock checkout path.
        self.tool.port_factory.get = get_test_port

        old_builder_data = self.command.builder_data

        def builder_data():
            old_builder_data()
            # have prototype-chocolate only fail on "MOCK Leopard".
            self.command._builder_data['MOCK SnowLeopard'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "PASS",
                    "actual": "PASS TEXT",
                    "is_unexpected": true
                },
                "prototype-chocolate.html": {
                    "expected": "FAIL",
                    "actual": "PASS"
                },
                "prototype-strawberry.html": {
                    "expected": "PASS",
                    "actual": "IMAGE PASS",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ SnowLeopard ] fast/dom/prototype-strawberry.html [ NeedsRebaseline ]
crbug.com/24182 fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-strawberry.html', "Dummy test contents")
        self._write_test_file(test_port, 'fast/dom/prototype-chocolate.html', "Dummy test contents")

        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK SnowLeopard": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier"])},
            }

            self.command.tree_status = lambda: 'closed'
            self.command.execute(MockOptions(optimize=True, verbose=False, move_overwritten_baselines=False, results_directory=False), [], self.tool)
            self.assertEqual(self.tool.executive.calls, [])

            self.command.tree_status = lambda: 'open'

            self.tool.executive.calls = []
            self.command.execute(MockOptions(optimize=True, verbose=False, move_overwritten_baselines=False, results_directory=False), [], self.tool)
            self.assertEqual(self.tool.executive.calls, [
                [
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-chocolate.html'],
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'png', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-strawberry.html'],
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-taco.html'],
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-taco.html'],
                ],
                [
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'txt,png', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-chocolate.html'],
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'png', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-strawberry.html'],
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-taco.html'],
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-taco.html'],
                ],
                ['echo', 'optimize-baselines', '--suffixes', 'txt,png', 'fast/dom/prototype-chocolate.html'],
                ['echo', 'optimize-baselines', '--suffixes', 'png', 'fast/dom/prototype-strawberry.html'],
                ['echo', 'optimize-baselines', '--suffixes', 'txt', 'fast/dom/prototype-taco.html'],
                ['git', 'pull'],
            ])

            # The mac ports should both be removed since they're the only ones in builders._exact_matches.
            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
crbug.com/24182 [ Debug ] path/to/norebaseline.html [ Rebaseline ]
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
crbug.com/24182 [ Linux Win ] fast/dom/prototype-chocolate.html [ NeedsRebaseline ]
crbug.com/24182 path/to/not-cycled-through-bots.html [ NeedsRebaseline ]
crbug.com/24182 path/to/locally-changed-lined.html [ NeedsRebaseline ]
""")
        finally:
            builders._exact_matches = old_exact_matches

    def test_execute_test_passes_everywhere(self):
        def blame(path):
            return """
6469e754a1 path/to/TestExpectations                   (foobarbaz1@chromium.org 2013-04-28 04:52:41 +0000   13) Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
"""
        self.tool.scm().blame = blame

        test_port = self.tool.port_factory.get('test')
        original_get = self.tool.port_factory.get

        def get_test_port(port_name=None, options=None, **kwargs):
            if not port_name:
                return test_port
            return original_get(port_name, options, **kwargs)
        # Need to make sure all the ports grabbed use the test checkout path instead of the mock checkout path.
        self.tool.port_factory.get = get_test_port

        old_builder_data = self.command.builder_data

        def builder_data():
            self.command._builder_data['MOCK Leopard'] = self.command._builder_data['MOCK SnowLeopard'] = LayoutTestResults.results_from_string("""ADD_RESULTS({
    "tests": {
        "fast": {
            "dom": {
                "prototype-taco.html": {
                    "expected": "PASS",
                    "actual": "PASS TEXT",
                    "is_unexpected": true
                }
            }
        }
    }
});""")
            return self.command._builder_data

        self.command.builder_data = builder_data

        self.tool.filesystem.write_text_file(test_port.path_to_generic_test_expectations_file(), """
Bug(foo) fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")

        self._write_test_file(test_port, 'fast/dom/prototype-taco.html', "Dummy test contents")

        old_exact_matches = builders._exact_matches
        try:
            builders._exact_matches = {
                "MOCK Leopard": {"port_name": "test-mac-leopard", "specifiers": set(["mock-specifier"])},
                "MOCK SnowLeopard": {"port_name": "test-mac-snowleopard", "specifiers": set(["mock-specifier"])},
            }

            self.command.tree_status = lambda: 'open'
            self.command.execute(MockOptions(optimize=True, verbose=False, move_overwritten_baselines=False, results_directory=False), [], self.tool)
            self.assertEqual(self.tool.executive.calls, [
                [
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-taco.html'],
                    ['echo', 'copy-existing-baselines-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-taco.html'],
                ],
                [
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK Leopard', '--test', 'fast/dom/prototype-taco.html'],
                    ['echo', 'rebaseline-test-internal', '--suffixes', 'txt', '--builder', 'MOCK SnowLeopard', '--test', 'fast/dom/prototype-taco.html'],
                ],
                ['echo', 'optimize-baselines', '--suffixes', 'txt', 'fast/dom/prototype-taco.html'],
                ['git', 'pull'],
            ])

            # The mac ports should both be removed since they're the only ones in builders._exact_matches.
            self.assertEqual(self.tool.filesystem.read_text_file(test_port.path_to_generic_test_expectations_file()), """
Bug(foo) [ Linux Win ] fast/dom/prototype-taco.html [ NeedsRebaseline ]
""")
        finally:
            builders._exact_matches = old_exact_matches
