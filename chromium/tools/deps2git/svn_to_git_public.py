#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""SVN to GIT mapping for the public Chromium repositories."""

import re


GIT_HOST = 'https://chromium.googlesource.com/'

BLINK_TRUNK_RE = re.compile(
    '^https?://src.chromium.org/blink/trunk$')
BLINK_TRUNK_PUBLIC_RE = re.compile(
    '^https?://src.chromium.org/blink/trunk/public$')

# Used by deps2git.ConvertDepsToGit() as overrides for SVN DEPS.  Each entry
# maps a DEPS path to a DEPS variable identifying the Git hash for its
# respective repository.  Variables are automatically transferred from SVN DEPS
# to .DEPS.git and converted into variables by deps_utils.Varify().
DEPS_OVERRIDES = {
  'src/third_party/ffmpeg': 'ffmpeg_hash'
}


def SvnUrlToGitUrl(path, svn_url):
  """Convert a chromium SVN URL to a chromium Git URL."""

  match = re.match(
      '(https?://src.chromium.org/svn|svn://svn.chromium.org/chrome)(/.*)',
      svn_url)
  if match:
    svn_url = match.group(2)

  # A few special cases.
  if re.match('^https?://sctp-refimpl.googlecode.com/svn/' +
              'trunk/KERN/usrsctp/usrsctplib$', svn_url):
    return (path, GIT_HOST + 'external/usrsctplib.git', GIT_HOST)

  if svn_url == '/trunk/deps/page_cycler/acid3':
    return (path, GIT_HOST + 'chromium/deps/acid3.git', GIT_HOST)

  if svn_url == '/trunk/deps/canvas_bench':
    return (path, GIT_HOST + 'chromium/canvas_bench.git', GIT_HOST)

  if svn_url == '/trunk/deps/gpu/software_rendering_list':
    return (path, GIT_HOST + 'chromium/deps/gpu/software_rendering_list.git',
            GIT_HOST)

  if svn_url == '/trunk/tools/third_party/python_26':
    return (path, GIT_HOST + 'chromium/deps/python_26.git', GIT_HOST)

  if svn_url == '/trunk/deps/support':
    return (path, GIT_HOST + 'chromium/support.git', GIT_HOST)

  if svn_url == '/trunk/deps/frame_rate/content':
    return (path, GIT_HOST + 'chromium/frame_rate/content.git', GIT_HOST)

  if svn_url == 'svn://svn.chromium.org/boto':
    return (path, GIT_HOST + 'external/boto.git', GIT_HOST)

  if svn_url == 'svn://svn.chromium.org/gsutil/trunk/src':
    return (path, GIT_HOST + 'external/gsutil/src.git', GIT_HOST)

  if svn_url == 'svn://svn.chromium.org/jsoncpp/trunk/jsoncpp':
    return (path, GIT_HOST + 'external/jsoncpp/jsoncpp.git', GIT_HOST)

  if svn_url == '/trunk/deps/third_party/ffmpeg':
    return (path, GIT_HOST + 'chromium/third_party/ffmpeg.git', GIT_HOST)

  if svn_url == '/trunk/deps/cdm':
    return (path, GIT_HOST + 'chromium/cdm.git', GIT_HOST)

  if re.match('^https?://webrtc.googlecode.com/svn/stable/webrtc$', svn_url):
    return (path, GIT_HOST + 'external/webrtc/stable/webrtc.git', GIT_HOST)

  if re.match('^https?://webrtc.googlecode.com/svn/stable/talk$', svn_url):
    return (path, GIT_HOST + 'external/webrtc/stable/talk.git', GIT_HOST)

  if re.match('^https?://webrtc.googlecode.com/svn/stable/src$', svn_url):
    return (path, GIT_HOST + 'external/webrtc/stable/src.git', GIT_HOST)

  if re.match('^https?://webrtc.googlecode.com/svn/deps/third_party/openmax$',
              svn_url):
    return (path, GIT_HOST + 'external/webrtc/deps/third_party/openmax.git',
            GIT_HOST)

  if svn_url in ('http://selenium.googlecode.com/svn/trunk/py/test',
                 'https://selenium.googlecode.com/svn/trunk/py/test',
                 '/trunk/deps/reference_builds/chrome'):
    # Those can't be git svn cloned. Skipping for now.
    return

  # Projects on sourceforge using trunk
  match = re.match('^https?://svn.code.sf.net/p/(.*)/code/trunk(.*)',
                   svn_url)
  if match:
    repo = '%s%s.git' % (match.group(1), match.group(2))
    return (path, GIT_HOST + 'external/%s' % repo, GIT_HOST)

  # Fallback for old sourceforge URL.
  match = re.match('^https?://(.*).svn.sourceforge.net/svnroot/(.*)/trunk(.*)',
                   svn_url)
  if match:
    repo = '%s%s.git' % (match.group(2), match.group(3))
    return (path, GIT_HOST + 'external/%s' % repo, GIT_HOST)

  # Subdirectories of libaddressinput
  if re.match('^https?://libaddressinput.googlecode.com/svn/trunk', svn_url):
    if 'libaddressinput' in path:
      path = path[:path.index('libaddressinput')] + 'libaddressinput/src'
    return (path, GIT_HOST + 'external/libaddressinput.git', GIT_HOST)

  # Projects on googlecode.com using trunk.
  match = re.match('^https?://(.*).googlecode.com/svn/trunk(.*)', svn_url)
  if match:
    repo = '%s%s.git' % (match.group(1), match.group(2))
    return (path, GIT_HOST + 'external/%s' % repo, GIT_HOST)

  # Projects on googlecode.com using branches.
  # Branches should be automatically included in the projects corresponding
  # 'trunk' mirror as 'branch-heads' refspecs.
  # This makes some broad assumptions about a "standard" branch layout , i.e.:
  #   svn/branches/<branch_name>/<optional_sub_path>
  # This layout can't really be enforced, though it appears to apply to most
  # repos. Outliers will have to be special-cased.
  match = re.match('^https?://(.*).googlecode.com/svn/branches/([^/]+)(.*)',
                   svn_url)
  if match:
    repo = '%s%s.git' % (match.group(1), match.group(3))
    branch_name = match.group(2)
    return (path, GIT_HOST + 'external/%s' % repo, branch_name, GIT_HOST)

  # Projects that are subdirectories of the native_client repository.
  match = re.match('^https?://src.chromium.org/native_client/trunk/(.*)',
                   svn_url)
  if match:
    repo = '%s.git' % match.group(1)
    return (path, GIT_HOST + 'native_client/%s' % repo, GIT_HOST)

  # Projects that are subdirectories of the chromium/{src,tools} repository.
  match = re.match('/trunk/((src|tools)/.*)', svn_url)
  if match:
    repo = '%s.git' % match.group(1)
    return (path, GIT_HOST + 'chromium/%s' % repo, GIT_HOST)

  # Public-header-only blink directory for iOS.
  if BLINK_TRUNK_PUBLIC_RE.match(svn_url):
    return (path, GIT_HOST + 'chromium/blink-public.git', GIT_HOST)

  # Main blink directory.
  if BLINK_TRUNK_RE.match(svn_url):
    return (path, GIT_HOST + 'chromium/blink.git', GIT_HOST)

  # Minimal header-only webkit directories for iOS.
  if svn_url == ('http://svn.webkit.org/repository/webkit/trunk/Source/' +
                 'WebKit/chromium/public'):
    return (path,
            GIT_HOST + 'external/WebKit/Source/WebKit/chromium/public.git',
            GIT_HOST)
  if svn_url == ('http://svn.webkit.org/repository/webkit/trunk/Source/' +
                 'Platform/chromium/public'):
    return (path,
            GIT_HOST + 'external/WebKit/Source/Platform/chromium/public.git',
            GIT_HOST)

  # Ignore all webkit directories (other than the above), since we fetch the
  # whole thing directly for all but iOS.
  if svn_url == '/trunk/deps/third_party/WebKit':
    return

  # blink

  # Subdirectories of the chromium deps/third_party directory.
  match = re.match('/trunk/deps/third_party/(.*)', svn_url)
  if match:
    repo = '%s.git' % match.group(1)
    return (path, GIT_HOST + 'chromium/deps/%s' % repo, GIT_HOST)

  # Subdirectories of the chromium deps/reference_builds directory.
  match = re.match('/trunk/deps/reference_builds/(.*)', svn_url)
  if match:
    repo = '%s.git' % match.group(1)
    return (path, GIT_HOST + 'chromium/reference_builds/%s' % repo, GIT_HOST)

  # Nothing yet? Oops.
  print 'No match for %s' % svn_url
