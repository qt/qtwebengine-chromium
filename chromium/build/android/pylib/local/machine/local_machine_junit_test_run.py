# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import multiprocessing
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
import zipfile

from concurrent.futures import ThreadPoolExecutor
from six.moves import range  # pylint: disable=redefined-builtin
from devil.utils import cmd_helper
from py_utils import tempfile_ext
from pylib import constants
from pylib.base import base_test_result
from pylib.base import test_run
from pylib.constants import host_paths
from pylib.results import json_results


# These Test classes are used for running tests and are excluded in the test
# runner. See:
# https://android.googlesource.com/platform/frameworks/testing/+/android-support-test/runner/src/main/java/android/support/test/internal/runner/TestRequestBuilder.java
# base/test/android/javatests/src/org/chromium/base/test/BaseChromiumAndroidJUnitRunner.java # pylint: disable=line-too-long
_EXCLUDED_CLASSES_PREFIXES = ('android', 'junit', 'org/bouncycastle/util',
                              'org/hamcrest', 'org/junit', 'org/mockito')

# Suites we shouldn't shard, usually because they don't contain enough test
# cases.
_EXCLUDED_SUITES = {
    'password_check_junit_tests',
    'touch_to_fill_junit_tests',
}

_FAILURE_TYPES = (
    base_test_result.ResultType.FAIL,
    base_test_result.ResultType.CRASH,
    base_test_result.ResultType.TIMEOUT,
)

# Running the largest test suite with a single shard takes about 22 minutes.
_SHARD_TIMEOUT = 30 * 60

# RegExp to detect logcat lines, e.g., 'I/AssetManager: not found'.
_LOGCAT_RE = re.compile(r'(:?\d+\| )?[A-Z]/[\w\d_-]+:')


class LocalMachineJunitTestRun(test_run.TestRun):
  # override
  def TestPackage(self):
    return self._test_instance.suite

  # override
  def SetUp(self):
    pass

  def _GetFilterArgs(self, shard_test_filter=None):
    ret = []
    if shard_test_filter:
      ret += ['-gtest-filter', ':'.join(shard_test_filter)]

    for test_filter in self._test_instance.test_filters:
      ret += ['-gtest-filter', test_filter]

    if self._test_instance.package_filter:
      ret += ['-package-filter', self._test_instance.package_filter]
    if self._test_instance.runner_filter:
      ret += ['-runner-filter', self._test_instance.runner_filter]

    return ret

  def _CreateJarArgsList(self, json_result_file_paths, grouped_tests, shards):
    # Creates a list of jar_args. The important thing is each jar_args list
    # has a different json_results file for writing test results to and that
    # each list of jar_args has its own test to run as specified in the
    # -gtest-filter.
    jar_args_list = [['-json-results-file', result_file]
                     for result_file in json_result_file_paths]
    for index, jar_arg in enumerate(jar_args_list):
      shard_test_filter = grouped_tests[index] if shards > 1 else None
      jar_arg += self._GetFilterArgs(shard_test_filter)

    return jar_args_list

  def _CreateJvmArgsList(self, for_listing=False):
    # Creates a list of jvm_args (robolectric, code coverage, etc...)
    jvm_args = [
        '-Drobolectric.dependency.dir=%s' %
        self._test_instance.robolectric_runtime_deps_dir,
        '-Ddir.source.root=%s' % constants.DIR_SOURCE_ROOT,
        # Use locally available sdk jars from 'robolectric.dependency.dir'
        '-Drobolectric.offline=true',
        '-Drobolectric.resourcesMode=binary',
        '-Drobolectric.logging=stdout',
        '-Djava.library.path=%s' % self._test_instance.native_libs_dir,
    ]
    if self._test_instance.debug_socket and not for_listing:
      jvm_args += [
          '-Dchromium.jdwp_active=true',
          ('-agentlib:jdwp=transport=dt_socket'
           ',server=y,suspend=y,address=%s' % self._test_instance.debug_socket)
      ]

    if self._test_instance.coverage_dir and not for_listing:
      if not os.path.exists(self._test_instance.coverage_dir):
        os.makedirs(self._test_instance.coverage_dir)
      elif not os.path.isdir(self._test_instance.coverage_dir):
        raise Exception('--coverage-dir takes a directory, not file path.')
      # Jacoco supports concurrent processes using the same output file:
      # https://github.com/jacoco/jacoco/blob/6cd3f0bd8e348f8fba7bffec5225407151f1cc91/org.jacoco.agent.rt/src/org/jacoco/agent/rt/internal/output/FileOutput.java#L67
      # So no need to vary the output based on shard number.
      jacoco_coverage_file = os.path.join(self._test_instance.coverage_dir,
                                          '%s.exec' % self._test_instance.suite)
      if self._test_instance.coverage_on_the_fly:
        jacoco_agent_path = os.path.join(host_paths.DIR_SOURCE_ROOT,
                                         'third_party', 'jacoco', 'lib',
                                         'jacocoagent.jar')

        # inclnolocationclasses is false to prevent no class def found error.
        jacoco_args = '-javaagent:{}=destfile={},inclnolocationclasses=false'
        jvm_args.append(
            jacoco_args.format(jacoco_agent_path, jacoco_coverage_file))
      else:
        jvm_args.append('-Djacoco-agent.destfile=%s' % jacoco_coverage_file)

    return jvm_args

  @property
  def _wrapper_path(self):
    return os.path.join(constants.GetOutDirectory(), 'bin', 'helper',
                        self._test_instance.suite)

  #override
  def GetTestsForListing(self):
    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      cmd = [self._wrapper_path, '--list-tests'] + self._GetFilterArgs()
      jvm_args = self._CreateJvmArgsList(for_listing=True)
      if jvm_args:
        cmd += ['--jvm-args', '"%s"' % ' '.join(jvm_args)]
      AddPropertiesJar([cmd], temp_dir, self._test_instance.resource_apk)
      lines = subprocess.check_output(cmd, encoding='utf8').splitlines()

    PREFIX = '#TEST# '
    prefix_len = len(PREFIX)
    # Filter log messages other than test names (Robolectric logs to stdout).
    return sorted(l[prefix_len:] for l in lines if l.startswith(PREFIX))

  # override
  def RunTests(self, results, raw_logs_fh=None):
    # This avoids searching through the classparth jars for tests classes,
    # which takes about 1-2 seconds.
    if (self._test_instance.shards == 1
        # TODO(crbug.com/1383650): remove this
        or self._test_instance.has_literal_filters or
        self._test_instance.suite in _EXCLUDED_SUITES):
      test_classes = []
      shards = 1
    else:
      test_classes = _GetTestClasses(self._wrapper_path)
      shards = ChooseNumOfShards(test_classes, self._test_instance.shards)

    grouped_tests = GroupTestsForShard(shards, test_classes)
    shard_list = list(range(shards))
    shard_filter = self._test_instance.shard_filter
    if shard_filter:
      shard_list = [x for x in shard_list if x in shard_filter]

    if not shard_list:
      results_list = [
          base_test_result.BaseTestResult('Invalid shard filter',
                                          base_test_result.ResultType.UNKNOWN)
      ]
      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)
      return

    if shard_filter:
      logging.warning('Running test shards: %s',
                      ', '.join(str(x) for x in shard_list))
    else:
      logging.warning('Running tests on %d shard(s).', shards)

    with tempfile_ext.NamedTemporaryDirectory() as temp_dir:
      cmd_list = [[self._wrapper_path] for _ in shard_list]
      json_result_file_paths = [
          os.path.join(temp_dir, 'results%d.json' % i) for i in shard_list
      ]
      active_groups = [
          g for i, g in enumerate(grouped_tests) if i in shard_list
      ]
      jar_args_list = self._CreateJarArgsList(json_result_file_paths,
                                              active_groups, shards)
      if jar_args_list:
        for cmd, jar_args in zip(cmd_list, jar_args_list):
          cmd += ['--jar-args', '"%s"' % ' '.join(jar_args)]

      jvm_args = self._CreateJvmArgsList()
      if jvm_args:
        for cmd in cmd_list:
          cmd.extend(['--jvm-args', '"%s"' % ' '.join(jvm_args)])

      AddPropertiesJar(cmd_list, temp_dir, self._test_instance.resource_apk)

      show_logcat = logging.getLogger().isEnabledFor(logging.INFO)
      num_omitted_lines = 0
      for line in _RunCommandsAndSerializeOutput(cmd_list, shard_list):
        if raw_logs_fh:
          raw_logs_fh.write(line)
        if show_logcat or not _LOGCAT_RE.match(line):
          sys.stdout.write(line)
        else:
          num_omitted_lines += 1

      if num_omitted_lines > 0:
        logging.critical('%d log lines omitted.', num_omitted_lines)
      sys.stdout.flush()
      if raw_logs_fh:
        raw_logs_fh.flush()

      results_list = []
      failed_shards = []
      try:
        for i, json_file_path in enumerate(json_result_file_paths):
          with open(json_file_path, 'r') as f:
            parsed_results = json_results.ParseResultsFromJson(
                json.loads(f.read()))
            results_list += parsed_results
            if any(r for r in parsed_results if r.GetType() in _FAILURE_TYPES):
              failed_shards.append(shard_list[i])
      except IOError:
        # In the case of a failure in the JUnit or Robolectric test runner
        # the output json file may never be written.
        results_list = [
            base_test_result.BaseTestResult('Test Runner Failure',
                                            base_test_result.ResultType.UNKNOWN)
        ]

      if shards > 1 and failed_shards:
        for i in failed_shards:
          filt = ':'.join(grouped_tests[i])
          print(f'Test filter for failed shard {i}: --test-filter "{filt}"')

        print(
            f'{len(failed_shards)} shards had failing tests. To re-run only '
            f'these shards, use the above filter flags, or use: '
            f'--shards {shards} --shard-filter',
            ','.join(str(x) for x in failed_shards))

      test_run_results = base_test_result.TestRunResults()
      test_run_results.AddResults(results_list)
      results.append(test_run_results)

  # override
  def TearDown(self):
    pass


def AddPropertiesJar(cmd_list, temp_dir, resource_apk):
  # Create properties file for Robolectric test runners so they can find the
  # binary resources.
  properties_jar_path = os.path.join(temp_dir, 'properties.jar')
  with zipfile.ZipFile(properties_jar_path, 'w') as z:
    z.writestr('com/android/tools/test_config.properties',
               'android_resource_apk=%s\n' % resource_apk)
    props = [
        'application = android.app.Application',
        'sdk = 28',
        ('shadows = org.chromium.testing.local.'
         'CustomShadowApplicationPackageManager'),
    ]
    z.writestr('robolectric.properties', '\n'.join(props))

  for cmd in cmd_list:
    cmd.extend(['--classpath', properties_jar_path])


def ChooseNumOfShards(test_classes, shards=None):
  if shards is None:
    # Local tests of explicit --shard values show that max speed is achieved
    # at cpu_count() / 2.
    # Using -XX:TieredStopAtLevel=1 is required for this result. The flag
    # reduces CPU time by two-thirds, making sharding more effective.
    shards = max(1, multiprocessing.cpu_count() // 2)

    # It can actually take longer to run if you shard too much, especially on
    # smaller suites. Locally media_base_junit_tests takes 4.3 sec with 1 shard,
    # and 6 sec with 2 or more shards.
    min_classes_per_shard = 8
  else:
    min_classes_per_shard = 1

  shards = max(1, min(shards, len(test_classes) // min_classes_per_shard))
  return shards


def GroupTestsForShard(num_of_shards, test_classes):
  """Groups tests that will be ran on each shard.

  Args:
    num_of_shards: number of shards to split tests between.
    test_classes: A list of test_class files in the jar.

  Return:
    Returns a list test lists.
  """
  ret = [[] for _ in range(num_of_shards)]

  # Round robin test distribiution to reduce chance that a sequential group of
  # classes all have an unusually high number of tests.
  for count, test_cls in enumerate(test_classes):
    test_cls = test_cls.replace('.class', '*')
    test_cls = test_cls.replace('/', '.')
    ret[count % num_of_shards].append(test_cls)

  return ret


def _DumpJavaStacks(pid):
  jcmd = os.path.join(constants.JAVA_HOME, 'bin', 'jcmd')
  cmd = [jcmd, str(pid), 'Thread.print']
  result = subprocess.run(cmd,
                          check=False,
                          stdout=subprocess.PIPE,
                          encoding='utf8')
  if result.returncode:
    return 'Failed to dump stacks\n' + result.stdout
  return result.stdout


def _RunCommandsAndSerializeOutput(cmd_list, shard_list):
  """Runs multiple commands in parallel and yields serialized output lines.

  Raises:
    TimeoutError: If timeout is exceeded.
  """
  num_shards = len(shard_list)
  assert num_shards > 0
  temp_files = []
  first_shard = shard_list[0]
  for i, cmd in zip(shard_list, cmd_list):
    # Shard 0 yields results immediately, the rest write to files.
    if i == first_shard:
      temp_files.append(None)  # Placeholder.
    else:
      temp_file = tempfile.TemporaryFile(mode='w+t', encoding='utf-8')
      temp_files.append(temp_file)

  deadline = time.time() + (_SHARD_TIMEOUT / (num_shards // 2 + 1))

  yield '\n'
  yield f'Shard {first_shard} output:\n'

  timeout_dumps = {}

  def run_proc(cmd, idx):
    if idx == 0:
      s_out = subprocess.PIPE
      s_err = subprocess.STDOUT
    else:
      s_out = temp_files[idx]
      s_err = temp_files[idx]

    proc = cmd_helper.Popen(cmd, stdout=s_out, stderr=s_err)
    # Need to return process so that output can be displayed on stdout
    # in real time.
    if idx == first_shard:
      return proc

    try:
      proc.wait(timeout=deadline - time.time())
    except subprocess.TimeoutExpired:
      timeout_dumps[idx] = _DumpJavaStacks(proc.pid)
      proc.kill()

    # Not needed, but keeps pylint happy.
    return None

  with ThreadPoolExecutor(max_workers=num_shards) as pool:
    futures = []
    for i, cmd in enumerate(cmd_list):
      futures.append(pool.submit(run_proc, cmd=cmd, idx=i))

    yield from _StreamFirstShardOutput(futures[0].result(), deadline)

    for i, shard in enumerate(shard_list[1:]):
      # Shouldn't cause timeout as run_proc terminates the process with
      # a proc.wait().
      futures[i + 1].result()
      f = temp_files[i + 1]
      yield '\n'
      yield f'Shard {shard} output:\n'
      f.seek(0)
      for line in f.readlines():
        yield f'{shard:2}| {line}'
      f.close()

  # Output stacks
  if timeout_dumps:
    yield '\n'
    yield ('=' * 80) + '\n'
    yield '\nOne or mord shards timed out.\n'
    yield ('=' * 80) + '\n'
    for i, dump in timeout_dumps.items():
      yield f'Index of timed out shard: {shard_list[i]}\n'
      yield 'Thread dump:\n'
      yield dump
      yield '\n'

    raise cmd_helper.TimeoutError('Junit shards timed out.')


def _StreamFirstShardOutput(shard_proc, deadline):
  # The following will be run from a thread to pump Shard 0 results, allowing
  # live output while allowing timeout.
  shard_queue = queue.Queue()

  def pump_stream_to_queue():
    for line in shard_proc.stdout:
      shard_queue.put(line)
    shard_queue.put(None)

  shard_0_pump = threading.Thread(target=pump_stream_to_queue)
  shard_0_pump.start()
  # Print the first process until timeout or completion.
  while shard_0_pump.is_alive():
    try:
      line = shard_queue.get(timeout=deadline - time.time())
      if line is None:
        break
      yield f'0| {line}'
    except queue.Empty:
      if time.time() > deadline:
        break

  # Output any remaining output from a timed-out first shard.
  shard_0_pump.join()
  while not shard_queue.empty():
    line = shard_queue.get()
    if line:
      yield f'0| {line}'


def _GetTestClasses(file_path):
  test_jar_paths = subprocess.check_output([file_path,
                                            '--print-classpath']).decode()
  test_jar_paths = test_jar_paths.split(':')

  test_classes = []
  for test_jar_path in test_jar_paths:
    # Avoid searching through jars that are for the test runner.
    # TODO(crbug.com/1144077): Use robolectric buildconfig file arg.
    if 'third_party/robolectric/' in test_jar_path:
      continue

    test_classes += _GetTestClassesFromJar(test_jar_path)

  logging.info('Found %d test classes in class_path jars.', len(test_classes))
  return test_classes


def _GetTestClassesFromJar(test_jar_path):
  """Returns a list of test classes from a jar.

  Test files end in Test, this is enforced:
  //tools/android/errorprone_plugin/src/org/chromium/tools/errorprone
  /plugin/TestClassNameCheck.java

  Args:
    test_jar_path: Path to the jar.

  Return:
    Returns a list of test classes that were in the jar.
  """
  class_list = []
  with zipfile.ZipFile(test_jar_path, 'r') as zip_f:
    for test_class in zip_f.namelist():
      if test_class.startswith(_EXCLUDED_CLASSES_PREFIXES):
        continue
      if test_class.endswith('Test.class') and '$' not in test_class:
        class_list.append(test_class)

  return class_list
