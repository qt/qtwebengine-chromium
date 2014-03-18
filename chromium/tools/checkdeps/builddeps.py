#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Traverses the source tree, parses all found DEPS files, and constructs
a dependency rule table to be used by subclasses.

The format of the deps file:

First you have the normal module-level deps. These are the ones used by
gclient. An example would be:

  deps = {
    "base":"http://foo.bar/trunk/base"
  }

DEPS files not in the top-level of a module won't need this. Then you
have any additional include rules. You can add (using "+") or subtract
(using "-") from the previously specified rules (including
module-level deps). You can also specify a path that is allowed for
now but that we intend to remove, using "!"; this is treated the same
as "+" when check_deps is run by our bots, but a presubmit step will
show a warning if you add a new include of a file that is only allowed
by "!".

Note that for .java files, there is currently no difference between
"+" and "!", even in the presubmit step.

  include_rules = [
    # Code should be able to use base (it's specified in the module-level
    # deps above), but nothing in "base/evil" because it's evil.
    "-base/evil",

    # But this one subdirectory of evil is OK.
    "+base/evil/not",

    # And it can include files from this other directory even though there is
    # no deps rule for it.
    "+tools/crime_fighter",

    # This dependency is allowed for now but work is ongoing to remove it,
    # so you shouldn't add further dependencies on it.
    "!base/evil/ok_for_now.h",
  ]

If you have certain include rules that should only be applied for some
files within this directory and subdirectories, you can write a
section named specific_include_rules that is a hash map of regular
expressions to the list of rules that should apply to files matching
them.  Note that such rules will always be applied before the rules
from 'include_rules' have been applied, but the order in which rules
associated with different regular expressions is applied is arbitrary.

  specific_include_rules = {
    ".*_(unit|browser|api)test\.cc": [
      "+libraries/testsupport",
    ],
  }

DEPS files may be placed anywhere in the tree. Each one applies to all
subdirectories, where there may be more DEPS files that provide additions or
subtractions for their own sub-trees.

There is an implicit rule for the current directory (where the DEPS file lives)
and all of its subdirectories. This prevents you from having to explicitly
allow the current directory everywhere.  This implicit rule is applied first,
so you can modify or remove it using the normal include rules.

The rules are processed in order. This means you can explicitly allow a higher
directory and then take away permissions from sub-parts, or the reverse.

Note that all directory separators must be slashes (Unix-style) and not
backslashes. All directories should be relative to the source root and use
only lowercase.
"""

import os
import subprocess
import copy

from rules import Rule, Rules


# Variable name used in the DEPS file to add or subtract include files from
# the module-level deps.
INCLUDE_RULES_VAR_NAME = 'include_rules'

# Variable name used in the DEPS file to add or subtract include files
# from module-level deps specific to files whose basename (last
# component of path) matches a given regular expression.
SPECIFIC_INCLUDE_RULES_VAR_NAME = 'specific_include_rules'

# Optionally present in the DEPS file to list subdirectories which should not
# be checked. This allows us to skip third party code, for example.
SKIP_SUBDIRS_VAR_NAME = 'skip_child_includes'


def NormalizePath(path):
  """Returns a path normalized to how we write DEPS rules and compare paths.
  """
  return path.lower().replace('\\', '/')


class DepsBuilder(object):
  """Parses include_rules from DEPS files.
  """

  def __init__(self,
               base_directory=None,
               verbose=False,
               being_tested=False,
               ignore_temp_rules=False,
               ignore_specific_rules=False):
    """Creates a new DepsBuilder.

    Args:
      base_directory: OS-compatible path to root of checkout, e.g. C:\chr\src.
      verbose: Set to true for debug output.
      being_tested: Set to true to ignore the DEPS file at tools/checkdeps/DEPS.
      ignore_temp_rules: Ignore rules that start with Rule.TEMP_ALLOW ("!").
    """
    self.base_directory = base_directory
    self.verbose = verbose
    self._under_test = being_tested
    self._ignore_temp_rules = ignore_temp_rules
    self._ignore_specific_rules = ignore_specific_rules

    if not base_directory:
      self.base_directory = os.path.abspath(
        os.path.join(os.path.abspath(os.path.dirname(__file__)), '..', '..'))

    self.git_source_directories = set()
    self._AddGitSourceDirectories()

    # Map of normalized directory paths to rules to use for those
    # directories, or None for directories that should be skipped.
    self.directory_rules = {}
    self._ApplyDirectoryRulesAndSkipSubdirs(Rules(), self.base_directory)

  def _ApplyRules(self, existing_rules, includes, specific_includes, cur_dir):
    """Applies the given include rules, returning the new rules.

    Args:
      existing_rules: A set of existing rules that will be combined.
      include: The list of rules from the "include_rules" section of DEPS.
      specific_includes: E.g. {'.*_unittest\.cc': ['+foo', '-blat']} rules
                         from the "specific_include_rules" section of DEPS.
      cur_dir: The current directory, normalized path. We will create an
               implicit rule that allows inclusion from this directory.

    Returns: A new set of rules combining the existing_rules with the other
             arguments.
    """
    rules = copy.deepcopy(existing_rules)

    # First apply the implicit "allow" rule for the current directory.
    if cur_dir.startswith(
          NormalizePath(os.path.normpath(self.base_directory))):
      relative_dir = cur_dir[len(self.base_directory) + 1:]

      source = relative_dir
      if len(source) == 0:
        source = 'top level'  # Make the help string a little more meaningful.
      rules.AddRule('+' + relative_dir,
                    relative_dir,
                    'Default rule for ' + source)
    else:
      raise Exception('Internal error: base directory is not at the beginning' +
                      ' for\n  %s and base dir\n  %s' %
                      (cur_dir, self.base_directory))

    def ApplyOneRule(rule_str, cur_dir, dependee_regexp=None):
      """Deduces a sensible description for the rule being added, and
      adds the rule with its description to |rules|.

      If we are ignoring temporary rules, this function does nothing
      for rules beginning with the Rule.TEMP_ALLOW character.
      """
      if self._ignore_temp_rules and rule_str.startswith(Rule.TEMP_ALLOW):
        return

      rule_block_name = 'include_rules'
      if dependee_regexp:
        rule_block_name = 'specific_include_rules'
      if not relative_dir:
        rule_description = 'the top level %s' % rule_block_name
      else:
        rule_description = relative_dir + "'s %s" % rule_block_name
      rules.AddRule(rule_str, relative_dir, rule_description, dependee_regexp)

    # Apply the additional explicit rules.
    for (_, rule_str) in enumerate(includes):
      ApplyOneRule(rule_str, cur_dir)

    # Finally, apply the specific rules.
    if not self._ignore_specific_rules:
      for regexp, specific_rules in specific_includes.iteritems():
        for rule_str in specific_rules:
          ApplyOneRule(rule_str, cur_dir, regexp)

    return rules

  def _ApplyDirectoryRules(self, existing_rules, dir_name):
    """Combines rules from the existing rules and the new directory.

    Any directory can contain a DEPS file. Toplevel DEPS files can contain
    module dependencies which are used by gclient. We use these, along with
    additional include rules and implicit rules for the given directory, to
    come up with a combined set of rules to apply for the directory.

    Args:
      existing_rules: The rules for the parent directory. We'll add-on to these.
      dir_name: The directory name that the deps file may live in (if
                it exists).  This will also be used to generate the
                implicit rules.  This is a non-normalized path.

    Returns: A tuple containing: (1) the combined set of rules to apply to the
             sub-tree, and (2) a list of all subdirectories that should NOT be
             checked, as specified in the DEPS file (if any).
    """
    norm_dir_name = NormalizePath(dir_name)

    # Check for a .svn directory in this directory or check this directory is
    # contained in git source direcotries. This will tell us if it's a source
    # directory and should be checked.
    if not (os.path.exists(os.path.join(dir_name, ".svn")) or
            (norm_dir_name in self.git_source_directories)):
      return (None, [])

    # Check the DEPS file in this directory.
    if self.verbose:
      print 'Applying rules from', dir_name
    def FromImpl(_unused, _unused2):
      pass  # NOP function so "From" doesn't fail.

    def FileImpl(_unused):
      pass  # NOP function so "File" doesn't fail.

    class _VarImpl:
      def __init__(self, local_scope):
        self._local_scope = local_scope

      def Lookup(self, var_name):
        """Implements the Var syntax."""
        if var_name in self._local_scope.get('vars', {}):
          return self._local_scope['vars'][var_name]
        raise Exception('Var is not defined: %s' % var_name)

    local_scope = {}
    global_scope = {
        'File': FileImpl,
        'From': FromImpl,
        'Var': _VarImpl(local_scope).Lookup,
        }
    deps_file = os.path.join(dir_name, 'DEPS')

    # The second conditional here is to disregard the
    # tools/checkdeps/DEPS file while running tests.  This DEPS file
    # has a skip_child_includes for 'testdata' which is necessary for
    # running production tests, since there are intentional DEPS
    # violations under the testdata directory.  On the other hand when
    # running tests, we absolutely need to verify the contents of that
    # directory to trigger those intended violations and see that they
    # are handled correctly.
    if os.path.isfile(deps_file) and (
        not self._under_test or not os.path.split(dir_name)[1] == 'checkdeps'):
      execfile(deps_file, global_scope, local_scope)
    elif self.verbose:
      print '  No deps file found in', dir_name

    # Even if a DEPS file does not exist we still invoke ApplyRules
    # to apply the implicit "allow" rule for the current directory
    include_rules = local_scope.get(INCLUDE_RULES_VAR_NAME, [])
    specific_include_rules = local_scope.get(SPECIFIC_INCLUDE_RULES_VAR_NAME,
                                             {})
    skip_subdirs = local_scope.get(SKIP_SUBDIRS_VAR_NAME, [])

    return (self._ApplyRules(existing_rules, include_rules,
                             specific_include_rules, norm_dir_name),
            skip_subdirs)

  def _ApplyDirectoryRulesAndSkipSubdirs(self, parent_rules, dir_path):
    """Given |parent_rules| and a subdirectory |dir_path| from the
    directory that owns the |parent_rules|, add |dir_path|'s rules to
    |self.directory_rules|, and add None entries for any of its
    subdirectories that should be skipped.
    """
    directory_rules, excluded_subdirs = self._ApplyDirectoryRules(parent_rules,
                                                                  dir_path)
    self.directory_rules[NormalizePath(dir_path)] = directory_rules
    for subdir in excluded_subdirs:
      self.directory_rules[NormalizePath(
          os.path.normpath(os.path.join(dir_path, subdir)))] = None

  def GetDirectoryRules(self, dir_path):
    """Returns a Rules object to use for the given directory, or None
    if the given directory should be skipped.  This takes care of
    first building rules for parent directories (up to
    self.base_directory) if needed.

    Args:
      dir_path: A real (non-normalized) path to the directory you want
      rules for.
    """
    norm_dir_path = NormalizePath(dir_path)

    if not norm_dir_path.startswith(
        NormalizePath(os.path.normpath(self.base_directory))):
      dir_path = os.path.join(self.base_directory, dir_path)
      norm_dir_path = NormalizePath(dir_path)

    parent_dir = os.path.dirname(dir_path)
    parent_rules = None
    if not norm_dir_path in self.directory_rules:
      parent_rules = self.GetDirectoryRules(parent_dir)

    # We need to check for an entry for our dir_path again, in case we
    # are at a path e.g. A/B/C where A/B/DEPS specifies the C
    # subdirectory to be skipped; in this case, the invocation to
    # GetDirectoryRules(parent_dir) has already filled in an entry for
    # A/B/C.
    if not norm_dir_path in self.directory_rules:
      if not parent_rules:
        # If the parent directory should be skipped, then the current
        # directory should also be skipped.
        self.directory_rules[norm_dir_path] = None
      else:
        self._ApplyDirectoryRulesAndSkipSubdirs(parent_rules, dir_path)
    return self.directory_rules[norm_dir_path]

  def _AddGitSourceDirectories(self):
    """Adds any directories containing sources managed by git to
    self.git_source_directories.
    """
    if not os.path.exists(os.path.join(self.base_directory, '.git')):
      return

    popen_out = os.popen('cd %s && git ls-files --full-name .' %
                         subprocess.list2cmdline([self.base_directory]))
    for line in popen_out.readlines():
      dir_name = os.path.join(self.base_directory, os.path.dirname(line))
      # Add the directory as well as all the parent directories. Use
      # forward slashes and lower case to normalize paths.
      while dir_name != self.base_directory:
        self.git_source_directories.add(NormalizePath(dir_name))
        dir_name = os.path.dirname(dir_name)
    self.git_source_directories.add(NormalizePath(self.base_directory))
