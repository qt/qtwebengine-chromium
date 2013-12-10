# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import unittest

from telemetry.core.backends.chrome import websocket

class TestWebSocket(unittest.TestCase):
  def testExports(self):
    self.assertNotEqual(websocket.create_connection, None)
