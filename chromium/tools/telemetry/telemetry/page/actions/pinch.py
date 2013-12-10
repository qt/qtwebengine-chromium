# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os

from telemetry.core import util
from telemetry.page.actions import page_action

class PinchAction(page_action.PageAction):
  def __init__(self, attributes=None):
    super(PinchAction, self).__init__(attributes)

  def WillRunAction(self, page, tab):
    for js_file in ['gesture_common.js', 'pinch.js']:
      with open(os.path.join(os.path.dirname(__file__), js_file)) as f:
        js = f.read()
        tab.ExecuteJavaScript(js)


    done_callback = 'function() { window.__pinchActionDone = true; }'
    tab.ExecuteJavaScript("""
        window.__pinchActionDone = false;
        window.__pinchAction = new __PinchAction(%s);"""
        % done_callback)

  def RunAction(self, page, tab, previous_action):
    zoom_in = True
    if hasattr(self, 'zoom_in'):
      zoom_in = self.zoom_in

    pixels_to_move = 4000
    if hasattr(self, 'pixels_to_move'):
      pixels_to_move = self.pixels_to_move

    tab.ExecuteJavaScript('window.__pinchAction.start(%s, %f)'
                          % ("true" if zoom_in else "false", pixels_to_move))

    # Poll for pinch action completion.
    util.WaitFor(lambda: tab.EvaluateJavaScript(
        'window.__pinchActionDone'), 60)

  def CanBeBound(self):
    return True

  def CustomizeBrowserOptions(self, options):
    options.AppendExtraBrowserArgs('--enable-gpu-benchmarking')

  def BindMeasurementJavaScript(self, tab, start_js, stop_js):
    # Make the pinch action start and stop measurement automatically.
    tab.ExecuteJavaScript("""
        window.__pinchAction.beginMeasuringHook = function() { %s };
        window.__pinchAction.endMeasuringHook = function() { %s };
    """ % (start_js, stop_js))
