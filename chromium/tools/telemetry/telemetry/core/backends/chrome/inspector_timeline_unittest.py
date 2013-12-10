# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from telemetry.core import util
from telemetry.core.backends.chrome import inspector_timeline
from telemetry.unittest import tab_test_case

class InspectorTimelineTabTest(tab_test_case.TabTestCase):
  def _StartServer(self):
    self._browser.SetHTTPServerDirectories(util.GetUnittestDataDir())

  def _WaitForAnimationFrame(self):
    def _IsDone():
      js_is_done = """done"""
      return bool(self._tab.EvaluateJavaScript(js_is_done))
    util.WaitFor(_IsDone, 5)

  def testGotTimeline(self):
    with inspector_timeline.InspectorTimeline.Recorder(self._tab):
      self._tab.ExecuteJavaScript(
"""
var done = false;
function sleep(ms) {
  var endTime = (new Date().getTime()) + ms;
  while ((new Date().getTime()) < endTime);
}
window.webkitRequestAnimationFrame(function() { sleep(10); done = true; });
""")
      self._WaitForAnimationFrame()

    r = self._tab.timeline_model.GetAllEventsOfName('FireAnimationFrame')
    self.assertTrue(len(r) > 0)
    self.assertTrue(r[0].duration > 0)
