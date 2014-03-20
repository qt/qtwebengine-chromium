/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

(function () {

module("svn-log");

var kExampleCommitDataXML =
   "<feed xmlns='http://www.w3.org/2005/Atom'>\n" +
   "<title>blink, branch master</title>\n" +
   "<subtitle>Mirror of the Chromium Blink repository.</subtitle>\n" +
   "<link rel='alternate' type='text/html' href='http://blink.lc/blink/'/>\n" +
   "<entry>\n" +
   "<title>Throw SecurityError when setting 'Replaceable' properties cross-origin.</title>\n" +
   "<updated>2013-09-30T20:22:01Z</updated>\n" +
   "<author>\n" +
   "<name>mkwst@chromium.org</name>\n" +
   "</author>\n" +
   "<published>2013-09-30T20:22:01Z</published>\n" +
   "<link rel='alternate' type='text/html' href='http://blink.lc/blink/commit/?id=723e62a4a4e093435b4772b4839aa3fd7cf6b991'/>\n" +
   "<id>723e62a4a4e093435b4772b4839aa3fd7cf6b991</id>\n" +
   "<content type='text'>\n" +
   "This matches Gecko's behavior for these types of properties.\n" +
   "\n" +
   "BUG=17325\n" +
   "R=jochen@chromium.org\n" +
   "CC=abarth@chromium.org\n" +
   "\n" +
   "Review URL: https://chromiumcodereview.appspot.com/25022002\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158545 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</content>\n" +
   "<content type='xhtml'>\n" +
   "<div xmlns='http://www.w3.org/1999/xhtml'>\n" +
   "<pre>\n" +
   "This matches Gecko's behavior for these types of properties.\n" +
   "\n" +
   "BUG=17325\n" +
   "R=jochen@chromium.org\n" +
   "CC=abarth@chromium.org\n" +
   "\n" +
   "Review URL: https://chromiumcodereview.appspot.com/25022002\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158545 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</pre>\n" +
   "</div>\n" +
   "</content>\n" +
   "</entry>\n" +
   "<entry>\n" +
   "<title>Fix one more layering violation caught by check-blink-deps</title>\n" +
   "<updated>2013-09-30T19:36:21Z</updated>\n" +
   "<author>\n" +
   "<name>eseidel@chromium.org</name>\n" +
   "</author>\n" +
   "<published>2013-09-30T19:36:21Z</published>\n" +
   "<link rel='alternate' type='text/html' href='http://blink.lc/blink/commit/?id=51e5c70050dcb0980eb31f112d0cd948f3ece820'/>\n" +
   "<id>51e5c70050dcb0980eb31f112d0cd948f3ece820</id>\n" +
   "<content type='text'>\n" +
   "core/platform may not depend on core/ even for testing.\n" +
   "\n" +
   "BUG=301947\n" +
   "R=abarth@chromium.org, abarth\n" +
   "\n" +
   "Review URL: https://codereview.chromium.org/25284004\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158544 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</content>\n" +
   "<content type='xhtml'>\n" +
   "<div xmlns='http://www.w3.org/1999/xhtml'>\n" +
   "<pre>\n" +
   "core/platform may not depend on core/ even for testing.\n" +
   "\n" +
   "BUG=301947\n" +
   "R=abarth@chromium.org, abarth\n" +
   "\n" +
   "Review URL: https://codereview.chromium.org/25284004\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158544 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</pre>\n" +
   "</div>\n" +
   "</content>\n" +
   "</entry>\n" +
   "<entry>\n" +
   "<title>Update DEPS include_rules after addition of root-level platform directory</title>\n" +
   "<updated>2013-09-30T19:28:49Z</updated>\n" +
   "<author>\n" +
   "<name>eseidel@chromium.org</name>\n" +
   "</author>\n" +
   "<published>2013-09-30T19:28:49Z</published>\n" +
   "<link rel='alternate' type='text/html' href='http://blink.lc/blink/commit/?id=227add0156e8ab272abcd3368dfc0b5a91f35749'/>\n" +
   "<id>227add0156e8ab272abcd3368dfc0b5a91f35749</id>\n" +
   "<content type='text'>\n" +
   "These were all failures noticed when running check-blink-deps\n" +
   "\n" +
   "R=abarth@chromium.org, abarth\n" +
   "BUG=301947\n" +
   "\n" +
   "Review URL: https://codereview.chromium.org/25275005\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158543 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</content>\n" +
   "<content type='xhtml'>\n" +
   "<div xmlns='http://www.w3.org/1999/xhtml'>\n" +
   "<pre>\n" +
   "These were all failures noticed when running check-blink-deps\n" +
   "\n" +
   "R=abarth@chromium.org, abarth\n" +
   "BUG=301947\n" +
   "\n" +
   "Review URL: https://codereview.chromium.org/25275005\n" +
   "\n" +
   "git-svn-id: svn://svn.chromium.org/blink/trunk@158543 bbb929c8-8fbe-4397-9dbb-9b2b20218538\n" +
   "</pre>\n" +
   "</div>\n" +
   "</content>\n" +
   "</entry>\n" +
   "</feed>\n";


var kExampleCommitDataList = [{
    "revision": 158545,
    "title": "This matches Gecko's behavior for these types of properties.",
    "time": "2013-09-30T20:22:01Z",
    "summary": "This matches Gecko's behavior for these types of properties.",
    "author": "mkwst@chromium.org",
    "reviewer": "jochen@chromium",
    "bugID": 17325,
    "revertedRevision": undefined
  },
  {
    "revision": 158544,
    "title": "core/platform may not depend on core/ even for testing.",
    "time": "2013-09-30T19:36:21Z",
    "summary": "core/platform may not depend on core/ even for testing.",
    "author": "eseidel@chromium.org",
    "reviewer": "abarth@chromium",
    "bugID": 301947,
    "revertedRevision": undefined
  },
  {
    "revision": 158543,
    "title": "These were all failures noticed when running check-blink-deps",
    "time": "2013-09-30T19:28:49Z",
    "summary": "These were all failures noticed when running check-blink-deps",
    "author": "eseidel@chromium.org",
    "reviewer": "abarth@chromium",
    "bugID": 301947,
    "revertedRevision": undefined
  }
];

test("changesetURL", 1, function() {
    equals(trac.changesetURL(1234), "http://src.chromium.org/viewvc/blink?view=rev&revision=1234");
});

test("recentCommitData", 3, function() {
    var simulator = new NetworkSimulator();
    simulator.get = function(url, callback)
    {
        equals(url, 'http://blink.lc/blink/atom');
        simulator.scheduleCallback(function() {
            var parser = new DOMParser();
            var responseDOM = parser.parseFromString(kExampleCommitDataXML, "application/xml");
            callback(responseDOM);
        });
    };

    simulator.runTest(function() {
        trac.recentCommitData('trunk', 10, function(commitDataList) {
            $.each(commitDataList, function(index, commitData) {
                // Including the entire message makes the deepEqual below to unwieldy.
                delete commitData.message;
            });
            deepEqual(commitDataList, kExampleCommitDataList);
        });
    });
});

})();
