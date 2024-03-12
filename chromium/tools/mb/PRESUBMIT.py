# Copyright 2015 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


PRESUBMIT_VERSION = '2.0.0'


_IGNORE_FREEZE_FOOTER = 'Ignore-Freeze'

# The time module's handling of timezones is abysmal, so the boundaries are
# precomputed in UNIX time
_FREEZE_START = 1702627200  # 2023/12/15 00:00 -0800
_FREEZE_END = 1704182400  # 2024/01/02 00:00 -0800


def CheckFreeze(input_api, output_api):
  if _FREEZE_START <= input_api.time.time() < _FREEZE_END:
    footers = input_api.change.GitFootersFromDescription()
    if _IGNORE_FREEZE_FOOTER not in footers:

      def convert(t):
        ts = input_api.time.localtime(t)
        return input_api.time.strftime('%Y/%m/%d %H:%M %z', ts)

      # Don't report errors when on the presubmit --all bot or when testing
      # with presubmit --files.
      if input_api.no_diffs:
        report_type = output_api.PresubmitPromptWarning
      else:
        report_type = output_api.PresubmitError
      return [
          report_type('There is a prod freeze in effect from {} until {},'
                      ' files in //tools/mb cannot be modified'.format(
                          convert(_FREEZE_START), convert(_FREEZE_END)))
      ]

  return []


def CheckTests(input_api, output_api):
  return input_api.RunTests(
      input_api.canned_checks.GetUnitTestsInDirectory(input_api, output_api,
                                                      '.',
                                                      [r'.+_(unit)?test\.py$']))


def CheckPylint(input_api, output_api):
  return input_api.canned_checks.RunPylint(
      input_api,
      output_api,
      version='2.7',
      # pylint complains about Checkfreeze not being defined, its probably
      # finding a different PRESUBMIT.py. Note that this warning only
      # appears if the number of Pylint jobs is greater than one.
      files_to_skip=['PRESUBMIT_test.py'],
      # Disabling this warning because this pattern involving ToSrcRelPath
      # seems intrinsic to how mb_unittest.py is implemented.
      disabled_warnings=[
          'attribute-defined-outside-init',
      ],
  )


def CheckMbValidate(input_api, output_api):
  cmd = [input_api.python3_executable, 'mb.py', 'validate']
  kwargs = {'cwd': input_api.PresubmitLocalPath()}
  return input_api.RunTests([
      input_api.Command(name='mb_validate',
                        cmd=cmd,
                        kwargs=kwargs,
                        message=output_api.PresubmitError),
  ])
