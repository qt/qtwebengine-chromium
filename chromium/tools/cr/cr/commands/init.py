# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the init command."""

import os

import cr

# The set of variables to store in the per output configuration.
OUT_CONFIG_VARS = [
    'CR_VERSION',
    cr.Platform.SELECTOR, cr.BuildType.SELECTOR, cr.Arch.SELECTOR,
    'CR_OUT_BASE', 'CR_OUT_FULL',
]


class InitCommand(cr.Command):
  """The implementation of the init command.

  The init command builds or updates an output directory.
  It then uses the Prepare and Select commands to get that directory
  ready to use.
  """

  def __init__(self):
    super(InitCommand, self).__init__()
    self.requires_build_dir = False
    self.help = 'Create and configure an output directory'
    self.description = ("""
        If the .cr directory is not present, build it and add
        the specified configuration.
        If the file already exists, update the configuration with any
        additional settings.
        """)
    self._settings = []

  def AddArguments(self, subparsers):
    """Overridden from cr.Command."""
    parser = super(InitCommand, self).AddArguments(subparsers)
    cr.Platform.AddArguments(parser)
    cr.BuildType.AddArguments(parser)
    cr.Arch.AddArguments(parser)
    cr.SelectCommand.AddPrepareArguments(parser)
    parser.add_argument(
        '-s', '--set', dest='_settings', metavar='settings',
        action='append',
        help='Configuration overrides.'
    )
    return parser

  def EarlyArgProcessing(self, context):
    base_settings = getattr(context.args, '_settings', None)
    if base_settings:
      self._settings.extend(base_settings)
    # Do not call super early processing, we do not want to apply
    # the output arg...
    out = cr.base.client.GetOutArgument(context)
    if out:
      # Output directory is fully specified
      # We need to deduce other settings from it's name
      base, buildtype = os.path.split(out)
      if not (base and buildtype):
        print 'Specified output directory must be two levels'
        exit(1)
      if not cr.BuildType.FindPlugin(buildtype):
        print 'Specified build type', buildtype, 'is not valid'
        print 'Must be one of', ','.join(p.name for p in cr.BuildType.Plugins())
        exit(1)
      if context.args.CR_BUILDTYPE and context.args.CR_BUILDTYPE != buildtype:
        print 'If --type and --out are both specified, they must match'
        print 'Got', context.args.CR_BUILDTYPE, 'and', buildtype
        exit(1)
      platform = context.args.CR_PLATFORM
      if not platform:
        # Try to guess platform based on output name
        platforms = [p.name for p in cr.Platform.AllPlugins()]
        matches = [p for p in platforms if p in base]
        if len(matches) != 1:
          print 'Platform is not set, and could not be guessed from', base
          print 'Should be one of', ','.join(platforms)
          if len(matches) > 1:
            print 'Matched all of', ','.join(matches)
          exit(1)
        platform = matches[0]
      context.derived.Set(
          CR_OUT_FULL=out,
          CR_OUT_BASE=base,
          CR_PLATFORM=platform,
          CR_BUILDTYPE=buildtype,
      )
    if not 'CR_OUT_BASE' in context:
      context.derived['CR_OUT_BASE'] = 'out_{CR_PLATFORM}'
    if not 'CR_OUT_FULL' in context:
      context.derived['CR_OUT_FULL'] = os.path.join(
          '{CR_OUT_BASE}', '{CR_BUILDTYPE}')

  def Run(self, context):
    """Overridden from cr.Command."""
    src_path = context.Get('CR_SRC')
    if not os.path.isdir(src_path):
      print context.Substitute('Path {CR_SRC} is not a valid client')
      exit(1)

    # Ensure we have an output directory override ready to fill in
    # This will only be missing if we are creating a brand new output
    # directory
    build_package = cr.auto.build

    # Collect the old version (and float convert)
    old_version = context.Find('CR_VERSION')
    try:
      old_version = float(old_version)
    except (ValueError, TypeError):
      old_version = 0.0
    is_new = not hasattr(build_package, 'config')
    if is_new:

      class FakeModule(object):
        OVERRIDES = cr.Config('OVERRIDES')

        def __init__(self):
          self.__name__ = 'config'

      old_version = None
      config = FakeModule()
      setattr(build_package, 'config', config)
      cr.plugin.ChainModuleConfigs(config)

    # Force override the version
    build_package.config.OVERRIDES.Set(CR_VERSION=cr.base.client.VERSION)
    # Add all the variables that we always want to have
    for name in OUT_CONFIG_VARS:
      value = context.Find(name)
      build_package.config.OVERRIDES[name] = value
    # Apply the settings from the command line
    for setting in self._settings:
      name, separator, value = setting.partition('=')
      name = name.strip()
      if not separator:
        value = True
      else:
        value = cr.Config.ParseValue(value.strip())
      build_package.config.OVERRIDES[name] = value

    # Run all the output directory init hooks
    for hook in InitHook.Plugins():
      hook.Run(context, old_version, build_package.config)
    # Redo activations, they might have changed
    cr.plugin.Activate(context)

    # Write out the new configuration, and select it as the default
    cr.base.client.WriteConfig(context, context.Get('CR_BUILD_DIR'),
                               build_package.config.OVERRIDES.exported)
    # Prepare the platform in here, using the updated config
    cr.Platform.Prepare(context)
    cr.SelectCommand.Select(context)


class InitHook(cr.Plugin, cr.Plugin.Type):
  """Base class for output directory initialization hooks.

  Implementations used to fix from old version to new ones live in the
  cr.fixups package.
  """

  def Run(self, context, old_version, config):
    """Run the initialization hook.

    This is invoked once per init invocation.
    Args:
      context: The context of the init command.
      old_version: The old version,
          0.0 if the old version was bad or missing,
          None if building a new output direcory.
      config: The mutable config that will be written.
    """
    raise NotImplementedError('Must be overridden.')

