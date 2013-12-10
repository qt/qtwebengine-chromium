# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os

from metrics import Metric

class SpeedIndexMetric(Metric):
  """The speed index metric is one way of measuring page load speed.

  It is meant to approximate user perception of page load speed, and it
  is based on the amount of time that it takes to paint to the visual
  portion of the screen. It includes paint events that occur after the
  onload event, and it doesn't include time loading things off-screen.

  This speed index metric is based on the devtools speed index at
  WebPageTest.org (WPT). For more info see: http://goo.gl/e7AH5l
  """
  def __init__(self):
    super(SpeedIndexMetric, self).__init__()
    self._script_is_loaded = False
    self._is_finished = False
    with open(os.path.join(os.path.dirname(__file__), 'speedindex.js')) as f:
      self._js = f.read()

  def Start(self, _, tab):
    """Start recording events.

    This method should be called in the WillNavigateToPage method of
    a PageMeasurement, so that all the events can be captured. If it's called
    in DidNavigateToPage, that will be too late.
    """
    tab.StartTimelineRecording()

  def Stop(self, _, tab):
    """Stop timeline recording."""
    assert self.IsFinished(tab)
    tab.StopTimelineRecording()

  def AddResults(self, tab, results):
    """Calculate the speed index and add it to the results."""
    events = tab.timeline_model.GetAllEvents()
    results.Add('speed_index', 'ms', _SpeedIndex(events))

  def IsFinished(self, tab):
    """Decide whether the timeline recording should be stopped.

    When the timeline recording is stopped determines which paint events
    are used in the speed index metric calculation. In general, the recording
    should continue if there has just been some data received, because
    this suggests that painting may continue.

    A page may repeatedly request resources in an infinite loop; a timeout
    should be placed in any measurement that uses this metric, e.g.:
      def IsDone():
        return self._speedindex.IsFinished(tab)
      util.WaitFor(IsDone, 60)

    Returns:
      True if 2 seconds have passed since last resource received, false
      otherwise.
    """
    if self._is_finished:
      return True

    # The script that provides the function window.timeSinceLastResponseMs()
    # needs to be loaded for this function, but it must be loaded AFTER
    # the Start method is called, because if the Start method is called in
    # the PageMeasurement's WillNavigateToPage function, then it will
    # not be available here. The script should only be loaded once, so that
    # variables in the script don't get reset.
    if not self._script_is_loaded:
      tab.ExecuteJavaScript(self._js)
      self._script_is_loaded = True

    time_since_last_response_ms = tab.EvaluateJavaScript(
        "window.timeSinceLastResponseAfterLoadMs()")
    self._is_finished = time_since_last_response_ms > 2000
    return self._is_finished


def _SpeedIndex(events):
  """Calculate the speed index of a page load from a list of events.

  The speed index number conceptually represents the number of milliseconds
  that the page was "visually incomplete". If the page were 0% complete for
  1000 ms, then the score would be 1000; if it were 0% complete for 100 ms
  then 90% complete (ie 10% incomplete) for 900 ms, then the score would be
  1.0*100 + 0.1*900 = 190.

  Args:
    events: A list of telemetry.core.timeline.slice.Slice objects

  Returns:
    A single number, milliseconds of visual incompleteness.
  """
  paint_events = _IncludedPaintEvents(events)
  time_area_dict = _TimeAreaDict(paint_events)
  time_completeness_dict = _TimeCompletenessDict(time_area_dict)
  # The first time interval starts from the start of the first event.
  prev_time = events[0].start
  prev_completeness = 0.0
  speed_index = 0.0
  for time, completeness in sorted(time_completeness_dict.items()):
    # Add the incemental value for the interval just before this event.
    elapsed_time = time - prev_time
    incompleteness = (1.0 - prev_completeness)
    speed_index += elapsed_time * incompleteness

    # Update variables for next iteration.
    prev_completeness = completeness
    prev_time = time

  return speed_index


def _TimeCompletenessDict(time_area_dict):
  """Make a dictionary of time to visual completeness.

  In the WPT PHP implementation, this is also called 'visual progress'.
  """
  total_area = sum(time_area_dict.values())
  assert total_area > 0.0, 'Total paint event area must be greater than 0.'
  completeness = 0.0
  time_completeness_dict = {}
  for time, area in sorted(time_area_dict.items()):
    completeness += float(area) / total_area
    # Visual progress is rounded to the nearest percentage point as in WPT.
    time_completeness_dict[time] = round(completeness, 2)
  return time_completeness_dict


def _IncludedPaintEvents(events):
  """Get all events that are counted in the calculation of the speed index.

  There's one category of paint event that's filtered out: paint events
  that occur before the first 'ResourceReceiveResponse' and 'Layout' events.

  Previously in the WPT speed index, paint events that contain children paint
  events were also filtered out.
  """
  def FirstLayoutTime(events):
    """Get the start time of the first layout after a resource received."""
    has_received_response = False
    for event in events:
      if event.name == 'ResourceReceiveResponse':
        has_received_response = True
      elif has_received_response and event.name == 'Layout':
        return event.start
    assert False, 'There were no layout events after resource receive events.'

  paint_events = [e for e in events
                  if e.start >= FirstLayoutTime(events) and e.name == 'Paint']
  return paint_events


def _TimeAreaDict(paint_events):
  """Make a dict from time to adjusted area value for events at that time.

  The adjusted area value of each paint event is determined by how many paint
  events cover the same rectangle, and whether it's a full-window paint event.
  "Adjusted area" can also be thought of as "points" of visual completeness --
  each rectangle has a certain number of points and these points are
  distributed amongst the paint events that paint that rectangle.

  Args:
    paint_events: A list of paint events

  Returns:
    A dictionary of times of each paint event (in milliseconds) to the
    adjusted area that the paint event is worth.
  """
  grouped = _GroupEventByRectangle(paint_events)
  # Note: It is assumed here that the fullscreen area is considered to be
  # area of the largest paint event that has NOT been filtered out.
  fullscreen_area = max([_RectangleArea(rect) for rect in grouped.keys()])
  event_area_dict = collections.defaultdict(int)

  for rectangle in grouped:
    # The area points for each rectangle are divided up among the paint
    # events in that rectangle.
    area = _RectangleArea(rectangle)
    update_count = len(grouped[rectangle])
    adjusted_area = float(area) / update_count

    # Paint events for the largest-area rectangle are counted as 50%.
    if area == fullscreen_area:
      adjusted_area /= 2

    for event in grouped[rectangle]:
      # The end time for an event is used for that event's time.
      event_time = event.end
      event_area_dict[event_time] += adjusted_area

  return event_area_dict


def _GetRectangle(paint_event):
  """Get the specific rectangle on the screen for a paint event.

  Each paint event belongs to a frame (as in html <frame> or <iframe>).
  This, together with location and dimensions, comprises a rectangle.
  In the WPT source, this 'rectangle' is also called a 'region'.
  """
  def GetBox(quad):
    """Convert the "clip" data in a paint event to coordinates and dimensions.

    In the timeline data from devtools, paint rectangle dimensions are
    represented x-y coordinates of four corners, clockwise from the top-left.
    See: function WebInspector.TimelinePresentationModel.quadFromRectData
    in file src/out/Debug/obj/gen/devtools/TimelinePanel.js.
    """
    x0, y0, _, _, x1, y1, _, _ = quad
    width, height = (x1 - x0), (y1 - y0)
    return (x0, y0, width, height)

  assert paint_event.name == 'Paint'
  frame = paint_event.args['frameId']
  x, y, width, height = GetBox(paint_event.args['data']['clip'])
  return (frame, x, y, width, height)


def _RectangleArea(rectangle):
  """Get the area of a rectangle as returned by _GetRectangle, above."""
  # Width and height are the last two items in the 5-tuple.
  return rectangle[3] * rectangle[4]


def _GroupEventByRectangle(paint_events):
  """Group all paint events according to the rectangle that they update."""
  result = collections.defaultdict(list)
  for event in paint_events:
    assert event.name == 'Paint'
    result[_GetRectangle(event)].append(event)
  return result
