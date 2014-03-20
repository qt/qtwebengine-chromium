# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""A module for the select command."""

import cr

# The set of variables SELECT writes into the client plugin to control the
# active output directory.
SELECT_OUT_VARS = ['CR_OUT_FULL']


class SelectCommand(cr.Command):
  """The implementation of the select command.

  The select command is used to set the default output directory used by all
  other commands. It does this by writing out a plugin into the client root
  that sets the active output path.
  """

  def __init__(self):
    super(SelectCommand, self).__init__()
    self.help = 'Select an output directory'
    self.description = ("""
        This makes the specified output directory the default for all future
        operations. It also invokes prepare on that directory.
        """)

  def AddArguments(self, subparsers):
    parser = super(SelectCommand, self).AddArguments(subparsers)
    self.AddPrepareArguments(parser)
    return parser

  @classmethod
  def AddPrepareArguments(cls, parser):
    parser.add_argument(
        '--no-prepare', dest='_no_prepare',
        action='store_true', default=False,
        help='Don\'t prepare the output directory.'
    )

  def Run(self, context):
    self.Select(context)

  @classmethod
  def Select(cls, context):
    """Performs the select.

    This is also called by the init command to auto select the new output
    directory.
    Args:
      context: The cr Context to select in.
    """
    cr.base.client.WriteConfig(
        context, context.Get('CR_CLIENT_PATH'), dict(
            CR_OUT_FULL=context.Get('CR_OUT_FULL')))
    cr.base.client.PrintInfo(context)
    # Now we run the post select actions
    if not getattr(context.args, '_no_prepare', None):
      cr.PrepareCommand.Prepare(context)
