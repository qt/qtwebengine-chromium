// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests the list of uploads from the backend.
 */
function requestUploads() {
  chrome.send('requestWebRtcLogsList');
}

/**
 * Callback from backend with the list of uploads. Builds the UI.
 * @param {array} uploads The list of uploads.
 * @param {string} version The browser version.
 */
function updateWebRtcLogsList(uploads, version) {
  $('log-banner').textContent = loadTimeData.getStringF('webrtcLogCountFormat',
                                                        uploads.length);

  var logSection = $('log-list');

  // Clear any previous list.
  logSection.textContent = '';

  for (var i = 0; i < uploads.length; i++) {
    var upload = uploads[i];

    var logBlock = document.createElement('div');
    var title = document.createElement('h3');
    title.textContent = loadTimeData.getStringF('webrtcLogHeaderFormat',
                                                upload['id']);
    logBlock.appendChild(title);
    var date = document.createElement('p');
    date.textContent = loadTimeData.getStringF('webrtcLogTimeFormat',
                                               upload['time']);
    logBlock.appendChild(date);
    var linkBlock = document.createElement('p');
    var link = document.createElement('a');
    var commentLines = [
      'Chrome Version: ' + version,
      // TODO(tbreisacher): fill in the OS automatically?
      'Operating System: e.g., "Windows 7", "Mac OSX 10.6"',
      '',
      'URL (if applicable) where the problem occurred:',
      '',
      'Can you reproduce this problem?',
      '',
      'What steps will reproduce this problem? (or if it\'s not ' +
      'reproducible, what were you doing just before the problem)?',
      '',
      '1.', '2.', '3.',
      '',
      '*Please note that issues filed with no information filled in ' +
      'above will be marked as WontFix*',
      '',
      '****DO NOT CHANGE BELOW THIS LINE****',
      'report_id:' + upload.id
    ];
    var params = {
      template: 'Defect report from user',
      comment: commentLines.join('\n'),
    };
    var href = 'http://code.google.com/p/chromium/issues/entry';
    for (var param in params) {
      href = appendParam(href, param, params[param]);
    }
    link.href = href;
    link.target = '_blank';
    link.textContent = loadTimeData.getString('bugLinkText');
    linkBlock.appendChild(link);
    logBlock.appendChild(linkBlock);
    logSection.appendChild(logBlock);
  }

  $('no-logs').hidden = uploads.length != 0;
}

document.addEventListener('DOMContentLoaded', requestUploads);
