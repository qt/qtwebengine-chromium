# Copyright (C) 2013 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Usage: PYTHON_PATH=/path/to/appengine_sdk python loghandler_unittest.py.

import dev_appserver
dev_appserver.fix_sys_path()

import unittest
import webapp2

from google.appengine.ext import testbed

import main


class TestHandlers(unittest.TestCase):
    def setUp(self):
        self.testbed = testbed.Testbed()
        self.testbed.activate()
        self.testbed.init_datastore_v3_stub()
        self.testbed.init_memcache_stub()

    def test_update_log(self):
        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'on'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'off'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Wrote new log entry.')

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Wrote new log entry.')

        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'off'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'off'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Added to existing log entry.')

        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'off'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'on'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Wrote new no needs rebaseline log.')

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Overwrote existing no needs rebaseline log.')

        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'off'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'off'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Previous entry was a no need rebaseline log. Writing a new log.')

    def test_update_log_first_entry_without_new_entry_param(self):
        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'off'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'off'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Wrote new log entry.')

    def test_update_log_first_entry_no_needs_rebaseline_param(self):
        request = webapp2.Request.blank('/updatelog')
        request.method = 'POST'
        request.POST[main.LOG_PARAM] = 'data to log'
        request.POST[main.NEW_ENTRY_PARAM] = 'off'
        request.POST[main.NO_NEEDS_REBASELINE_PARAM] = 'on'

        response = request.get_response(main.app)
        self.assertEqual(response.status_int, 200)
        self.assertEqual(response.body, 'Wrote new log entry.')


if __name__ == '__main__':
    unittest.main()
