# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import csv
import multiprocessing
import os
import sys

from telemetry.core import exceptions
from telemetry.core import util
from telemetry.core.platform import profiler

sys.path.append(os.path.join(util.GetTelemetryDir(), 'third_party', 'internal'))
try:
  import monsoon  # pylint: disable=F0401
except ImportError:
  monsoon = None


def _CollectData(output_path, is_collecting):
  mon = monsoon.Monsoon(wait=False)
  mon.SetMaxCurrent(2.0)
  # Note: Telemetry requires the device to be connected by USB, but that
  # puts it in charging mode. This increases the power consumption.
  mon.SetUsbPassthrough(1)
  # Nominal Li-ion voltage is 3.7V, but it puts out 4.2V at max capacity. Use
  # 4.0V to simulate a "~80%" charged battery. Google "li-ion voltage curve".
  # This is true only for a single cell. (Most smartphones, some tablets.)
  mon.SetVoltage(4.0)

  samples = []
  try:
    mon.StartDataCollection()
    # Do one CollectData() to make the Monsoon set up, which takes about
    # 0.3 seconds, and only signal that we've started after that.
    mon.CollectData()
    is_collecting.set()
    while is_collecting.is_set():
      samples += mon.CollectData()
  finally:
    mon.StopDataCollection()

  # Add x-axis labels.
  plot_data = [(i / 5000., sample) for i, sample in enumerate(samples)]

  # Print data in csv.
  with open(output_path, 'w') as output_file:
    output_writer = csv.writer(output_file)
    output_writer.writerows(plot_data)
    output_file.flush()

  print 'To view the Monsoon profile, run:'
  print ('  echo "set datafile separator \',\'; plot \'%s\' with lines" | '
      'gnuplot --persist' % output_path)


class MonsoonProfiler(profiler.Profiler):
  """Profiler that tracks current using Monsoon Power Monitor.

  http://www.msoon.com/LabEquipment/PowerMonitor/
  The Monsoon device measures current in amps at 5000 samples/second.
  """
  def __init__(self, browser_backend, platform_backend, output_path):
    super(MonsoonProfiler, self).__init__(
        browser_backend, platform_backend, output_path)
    # We collect the data in a separate process, so we can continuously
    # read the samples from the USB port while running the test.
    self._is_collecting = multiprocessing.Event()
    self._collector = multiprocessing.Process(
        target=_CollectData, args=(output_path, self._is_collecting))
    self._collector.start()
    if not self._is_collecting.wait(timeout=0.5):
      self._collector.terminate()
      raise exceptions.ProfilingException('Failed to start data collection.')

  @classmethod
  def name(cls):
    return 'monsoon'

  @classmethod
  def is_supported(cls, browser_type):
    if not monsoon:
      return False
    try:
      monsoon.Monsoon(wait=False)
    except IOError:
      return False
    else:
      return True

  def CollectProfile(self):
    self._is_collecting.clear()
    self._collector.join()
    return [self._output_path]
