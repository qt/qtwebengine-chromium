// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @type {FileManager}
 */
var fileManager;

/**
 * Indicates if the DOM and scripts have been already loaded.
 * @type {boolean}
 */
var pageLoaded = false;

/**
 * Kick off the file manager dialog.
 * Called by main.html after the DOM has been parsed.
 */
function init() {
  // Initializes UI and starts the File Manager dialog.
  fileManager.initializeUI(document.body, function() {
    chrome.test.sendMessage('ready');
    metrics.recordInterval('Load.Total');
  });
}

// Create the File Manager object. Note, that the DOM, nor any external
// scripts may not be ready yet.
fileManager = new FileManager();

// Initialize the core stuff, which doesn't require access to DOM nor to
// additional scripts.
fileManager.initializeCore();

// Final initialization is performed after all scripts and Dom is loaded.
util.addPageLoadHandler(init);

metrics.recordInterval('Load.Script');  // Must be the last line.
