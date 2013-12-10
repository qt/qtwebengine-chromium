# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core import system_info
from telemetry.page import page as page_module
from telemetry.page import page_set
from telemetry.page import test_expectations

VENDOR_NVIDIA = 0x10DE
VENDOR_AMD = 0x1002
VENDOR_INTEL = 0x8086

class StubPlatform(object):
  def __init__(self, os_name, os_version_name=None):
    self.os_name = os_name
    self.os_version_name = os_version_name

  def GetOSName(self):
    return self.os_name

  def GetOSVersionName(self):
    return self.os_version_name

class StubBrowser(object):
  def __init__(self, platform, gpu, device):
    self.platform = platform
    self.system_info = system_info.SystemInfo.FromDict({
      'model_name': '',
      'gpu': {
        'devices': [
          { 'vendor_id': gpu, 'device_id': device,
            'vendor_string': '', 'device_string': '' },
        ]
      }
    })

  @property
  def supports_system_info(self):
    return False if not self.system_info else True

  def GetSystemInfo(self):
    return self.system_info

class SampleTestExpectations(test_expectations.TestExpectations):
  def SetExpectations(self):
    self.Fail('page1.html', ['win', 'mac'], bug=123)
    self.Fail('page2.html', ['vista'], bug=123)
    self.Fail('page3.html', bug=123)
    self.Fail('page4.*', bug=123)
    self.Fail('http://test.com/page5.html', bug=123)
    self.Fail('page6.html', ['nvidia', 'intel'], bug=123)
    self.Fail('page7.html', [('nvidia', 0x1001), ('nvidia', 0x1002)], bug=123)
    self.Fail('page8.html', ['win', 'intel', ('amd', 0x1001)], bug=123)

class TestExpectationsTest(unittest.TestCase):
  def setUp(self):
    self.expectations = SampleTestExpectations()

  def assertExpectationEquals(self, expected, page, platform='', gpu='',
      device=0):
    result = self.expectations.GetExpectationForPage(StubBrowser(platform, gpu,
        device), page)
    self.assertEquals(expected, result)

  # Pages with no expectations should always return 'pass'
  def testNoExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page0.html', ps)
    self.assertExpectationEquals('pass', page, StubPlatform('win'))

  # Pages with expectations for an OS should only return them when running on
  # that OS
  def testOSExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page1.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page, StubPlatform('mac'))
    self.assertExpectationEquals('pass', page, StubPlatform('linux'))

  # Pages with expectations for an OS version should only return them when
  # running on that OS version
  def testOSVersionExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page2.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win', 'vista'))
    self.assertExpectationEquals('pass', page, StubPlatform('win', 'win7'))

  # Pages with non-conditional expectations should always return that
  # expectation regardless of OS or OS version
  def testConditionlessExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page3.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page, StubPlatform('mac', 'lion'))
    self.assertExpectationEquals('fail', page, StubPlatform('linux'))

  # Expectations with wildcard characters should return for matching patterns
  def testWildcardExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page4.html', ps)
    page_js = page_module.Page('http://test.com/page4.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('fail', page_js, StubPlatform('win'))

  # Expectations with absolute paths should match the entire path
  def testAbsoluteExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page5.html', ps)
    page_org = page_module.Page('http://test.org/page5.html', ps)
    page_https = page_module.Page('https://test.com/page5.html', ps)
    self.assertExpectationEquals('fail', page, StubPlatform('win'))
    self.assertExpectationEquals('pass', page_org, StubPlatform('win'))
    self.assertExpectationEquals('pass', page_https, StubPlatform('win'))

  # Pages with expectations for a GPU should only return them when running with
  # that GPU
  def testGpuExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page6.html', ps)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_INTEL)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_AMD)

  # Pages with expectations for a GPU should only return them when running with
  # that GPU
  def testGpuDeviceIdExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page7.html', ps)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA, device=0x1001)
    self.assertExpectationEquals('fail', page, gpu=VENDOR_NVIDIA, device=0x1002)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_NVIDIA, device=0x1003)
    self.assertExpectationEquals('pass', page, gpu=VENDOR_AMD, device=0x1001)

  # Pages with multiple expectations should only return them when all criteria
  # is met
  def testMultipleExpectations(self):
    ps = page_set.PageSet()
    page = page_module.Page('http://test.com/page8.html', ps)
    self.assertExpectationEquals('fail', page,
        StubPlatform('win'), VENDOR_AMD, 0x1001)
    self.assertExpectationEquals('fail', page,
        StubPlatform('win'), VENDOR_INTEL, 0x1002)
    self.assertExpectationEquals('pass', page,
        StubPlatform('win'), VENDOR_NVIDIA, 0x1001)
    self.assertExpectationEquals('pass', page,
        StubPlatform('mac'), VENDOR_AMD, 0x1001)
    self.assertExpectationEquals('pass', page,
        StubPlatform('win'), VENDOR_AMD, 0x1002)
