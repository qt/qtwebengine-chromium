#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for formatting and writing DEPS files."""

import re
import sys

# Used by Varify() to automatically convert variable names tagged with this
# prefix into Var('<variable name>').
VARIFY_MARKER_TAG_PREFIX = 'VARIFY_MARKER_TAG_'


class VarImpl(object):
  """Implement the Var function used within the DEPS file."""

  def __init__(self, local_scope):
    self._local_scope = local_scope

  def Lookup(self, var_name):
    """Implements the Var syntax."""
    if var_name in self._local_scope.get('vars', {}):
      return self._local_scope['vars'][var_name]
    raise Exception('Var is not defined: %s' % var_name)


def GetDepsContent(deps_path):
  """Read a DEPS file and return all the sections."""
  deps_file = open(deps_path, 'rU')
  content = deps_file.read()
  local_scope = {}
  var = VarImpl(local_scope)
  global_scope = {
      'Var': var.Lookup,
      'deps': {},
      'deps_os': {},
      'include_rules': [],
      'skip_child_includes': [],
      'hooks': [],
  }
  exec(content, global_scope, local_scope)
  local_scope.setdefault('deps', {})
  local_scope.setdefault('deps_os', {})
  local_scope.setdefault('include_rules', [])
  local_scope.setdefault('skip_child_includes', [])
  local_scope.setdefault('hooks', [])
  local_scope.setdefault('vars', {})

  return (local_scope['deps'], local_scope['deps_os'],
          local_scope['include_rules'], local_scope['skip_child_includes'],
          local_scope['hooks'], local_scope['vars'])


def PrettyDeps(deps, indent=0):
  """Stringify a deps dictionary in a pretty way."""
  pretty = ' ' * indent
  pretty += '{\n'

  indent += 4

  for item in sorted(deps):
    if type(deps[item]) == dict:
      value = PrettyDeps(deps[item], indent)
    else:
      value = ' ' * (indent + 4)
      if deps[item] is None:
        value += str(deps[item])
      else:
        value += '\'%s\'' % str(deps[item])
    pretty += ' ' * indent
    pretty += '\'%s\':\n' % item
    pretty += '%s,\n' % value

  indent -= 4
  pretty += ' ' * indent
  pretty += '}'
  return pretty


def PrettyObj(obj):
  """Stringify an object in a pretty way."""
  pretty = str(obj).replace('{', '{\n    ')
  pretty = pretty.replace('}', '\n}')
  pretty = pretty.replace('[', '[\n    ')
  pretty = pretty.replace(']', '\n]')
  pretty = pretty.replace('\':', '\':\n        ')
  pretty = pretty.replace(', ', ',\n    ')
  return pretty


def Varify(deps):
  """Replace all instances of our git server with a git_url var."""
  deps = deps.replace(
      '\'https://chromium.googlesource.com/chromium/blink.git',
      'Var(\'webkit_url\')')
  deps = deps.replace(
      '\'https://chromium.googlesource.com', 'Var(\'git_url\') + \'')
  deps = deps.replace(
      '\'https://git.chromium.org', 'Var(\'git_url\') + \'')
  deps = deps.replace('VAR_WEBKIT_REV\'', ' + Var(\'webkit_rev\')')

  # Try to replace all instances of form "marker_prefix_<name>'" with
  # "' + Var('<name>')".  If there are no matches, nothing is done.
  deps = re.sub(VARIFY_MARKER_TAG_PREFIX + '_(\w+)\'',
                lambda match: '\' + Var(\'%s\')' % match.group(1), deps)
  return deps


def WriteDeps(deps_file_name, deps_vars, deps, deps_os, include_rules,
              skip_child_includes, hooks):
  """Given all the sections in a DEPS file, write it to disk."""
  new_deps = ('# DO NOT EDIT EXCEPT FOR LOCAL TESTING.\n'
              '# THIS IS A GENERATED FILE.\n',
              '# ALL MANUAL CHANGES WILL BE OVERWRITTEN.\n',
              '# SEE http://code.google.com/p/chromium/wiki/UsingNewGit\n',
              '# FOR HOW TO ROLL DEPS\n'
              'vars = %s\n\n' % PrettyObj(deps_vars),
              'deps = %s\n\n' % Varify(PrettyDeps(deps)),
              'deps_os = %s\n\n' % Varify(PrettyDeps(deps_os)),
              'include_rules = %s\n\n' % PrettyObj(include_rules),
              'skip_child_includes = %s\n\n' % PrettyObj(skip_child_includes),
              'hooks = %s\n' % PrettyObj(hooks))
  new_deps = ''.join(new_deps)
  if deps_file_name:
    deps_file = open(deps_file_name, 'w')
  else:
    deps_file = sys.stdout

  try:
    deps_file.write(new_deps)
  finally:
    if deps_file_name:
      deps_file.close()
