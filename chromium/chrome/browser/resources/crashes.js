// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Requests the list of crashes from the backend.
 */
function requestCrashes() {
  chrome.send('requestCrashList');
}

/**
 * Callback from backend with the list of crashes. Builds the UI.
 * @param {boolean} enabled Whether or not crash reporting is enabled.
 * @param {array} crashes The list of crashes.
 * @param {string} version The browser version.
 */
function updateCrashList(enabled, crashes, version) {
  $('countBanner').textContent = loadTimeData.getStringF('crashCountFormat',
                                                         crashes.length);

  var crashSection = $('crashList');

  $('enabledMode').hidden = !enabled;
  $('disabledMode').hidden = enabled;

  if (!enabled)
    return;

  // Clear any previous list.
  crashSection.textContent = '';

  for (var i = 0; i < crashes.length; i++) {
    var crash = crashes[i];

    var crashBlock = document.createElement('div');
    var title = document.createElement('h3');
    title.textContent = loadTimeData.getStringF('crashHeaderFormat',
                                                crash['id']);
    crashBlock.appendChild(title);
    var date = document.createElement('p');
    date.textContent = loadTimeData.getStringF('crashTimeFormat',
                                               crash['time']);
    crashBlock.appendChild(date);
    var linkBlock = document.createElement('p');
    var link = document.createElement('a');
    var commentLines = [
      'Chrome Version: ' + version,
      // TODO(tbreisacher): fill in the OS automatically?
      'Operating System: e.g., "Windows 7", "Mac OSX 10.6"',
      '',
      'URL (if applicable) where crash occurred:',
      '',
      'Can you reproduce this crash?',
      '',
      'What steps will reproduce this crash? (or if it\'s not ' +
      'reproducible, what were you doing just before the crash)?',
      '',
      '1.', '2.', '3.',
      '',
      '*Please note that issues filed with no information filled in ' +
      'above will be marked as WontFix*',
      '',
      '****DO NOT CHANGE BELOW THIS LINE****',
      'report_id:' + crash.id
    ];
    var params = {
      template: 'Crash Report',
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
    crashBlock.appendChild(linkBlock);
    crashSection.appendChild(crashBlock);
  }

  $('noCrashes').hidden = crashes.length != 0;
}

document.addEventListener('DOMContentLoaded', requestCrashes);
