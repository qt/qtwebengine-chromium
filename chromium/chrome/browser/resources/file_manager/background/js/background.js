// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Number of runtime errors catched in the background page.
 * @type {number}
 */
var JSErrorCount = 0;

/**
 * Counts runtime JavaScript errors.
 */
window.onerror = function() { JSErrorCount++; };

/**
 * Type of a Files.app's instance launch.
 * @enum {number}
 */
var LaunchType = Object.freeze({
  ALWAYS_CREATE: 0,
  FOCUS_ANY_OR_CREATE: 1,
  FOCUS_SAME_OR_CREATE: 2
});

/**
 * Root class of the background page.
 * @constructor
 */
function Background() {
  /**
   * Map of all currently open app windows. The key is an app id.
   * @type {Object.<string, AppWindow>}
   */
  this.appWindows = {};

  /**
   * Synchronous queue for asynchronous calls.
   * @type {AsyncUtil.Queue}
   */
  this.queue = new AsyncUtil.Queue();

  /**
   * Progress center of the background page.
   * @type {ProgressCenter}
   */
  this.progressCenter = new ProgressCenter();

  /**
   * File operation manager.
   * @type {FileOperationManager}
   */
  this.fileOperationManager = FileOperationManager.getInstance();

  /**
   * Event handler for progress center.
   * @type {FileOperationHandler}
   * @private
   */
  this.fileOperationHandler_ = new FileOperationHandler(this);

  /**
   * String assets.
   * @type {Object.<string, string>}
   */
  this.stringData = null;

  /**
   * Callback list to be invoked after initialization.
   * It turns to null after initialization.
   *
   * @type {Array.<function()>}
   * @private
   */
  this.initializeCallbacks_ = [];

  /**
   * Last time when the background page can close.
   *
   * @type {number}
   * @private
   */
  this.lastTimeCanClose_ = null;

  // Seal self.
  Object.seal(this);

  // Initialize handlers.
  chrome.fileBrowserHandler.onExecute.addListener(this.onExecute_.bind(this));
  chrome.app.runtime.onLaunched.addListener(this.onLaunched_.bind(this));
  chrome.app.runtime.onRestarted.addListener(this.onRestarted_.bind(this));
  chrome.contextMenus.onClicked.addListener(
      this.onContextMenuClicked_.bind(this));

  // Fetch strings and initialize the context menu.
  this.queue.run(function(callNextStep) {
    chrome.fileBrowserPrivate.getStrings(function(strings) {
      // Initialize string assets.
      this.stringData = strings;
      loadTimeData.data = strings;
      this.initContextMenu_();

      // Invoke initialize callbacks.
      for (var i = 0; i < this.initializeCallbacks_.length; i++) {
        this.initializeCallbacks_[i]();
      }
      this.initializeCallbacks_ = null;

      callNextStep();
    }.bind(this));
  }.bind(this));
}

/**
 * A number of delay milliseconds from the first call of tryClose to the actual
 * close action.
 * @type {number}
 * @const
 * @private
 */
Background.CLOSE_DELAY_MS_ = 5000;

/**
 * Make a key of window geometry preferences for the given initial URL.
 * @param {string} url Initialize URL that the window has.
 * @return {string} Key of window geometry preferences.
 */
Background.makeGeometryKey = function(url) {
  return 'windowGeometry' + ':' + url;
};

/**
 * Register callback to be invoked after initialization.
 * If the initialization is already done, the callback is invoked immediately.
 *
 * @param {function()} callback Initialize callback to be registered.
 */
Background.prototype.ready = function(callback) {
  if (this.initializeCallbacks_ !== null)
    this.initializeCallbacks_.push(callback);
  else
    callback();
};

/**
 * Checks the current condition of background page and closes it if possible.
 */
Background.prototype.tryClose = function() {
  // If the file operation is going, the background page cannot close.
  if (this.fileOperationManager.hasQueuedTasks()) {
    this.lastTimeCanClose_ = null;
    return;
  }

  var views = chrome.extension.getViews();
  var closing = false;
  for (var i = 0; i < views.length; i++) {
    // If the window that is not the background page itself and it is not
    // closing, the background page cannot close.
    if (views[i] !== window && !views[i].closing) {
      this.lastTimeCanClose_ = null;
      return;
    }
    closing = closing || views[i].closing;
  }

  // If some windows are closing, or the background page can close but could not
  // 5 seconds ago, We need more time for sure.
  if (closing ||
      this.lastTimeCanClose_ === null ||
      Date.now() - this.lastTimeCanClose_ < Background.CLOSE_DELAY_MS_) {
    if (this.lastTimeCanClose_ === null)
      this.lastTimeCanClose_ = Date.now();
    setTimeout(this.tryClose.bind(this), Background.CLOSE_DELAY_MS_);
    return;
  }

  // Otherwise we can close the background page.
  close();
};

/**
 * Gets similar windows, it means with the same initial url.
 * @param {string} url URL that the obtained windows have.
 * @return {Array.<AppWindow>} List of similar windows.
 */
Background.prototype.getSimilarWindows = function(url) {
  var result = [];
  for (var appID in this.appWindows) {
    if (this.appWindows[appID].contentWindow.appInitialURL === url)
      result.push(this.appWindows[appID]);
  }
  return result;
};

/**
 * Wrapper for an app window.
 *
 * Expects the following from the app scripts:
 * 1. The page load handler should initialize the app using |window.appState|
 *    and call |util.platform.saveAppState|.
 * 2. Every time the app state changes the app should update |window.appState|
 *    and call |util.platform.saveAppState| .
 * 3. The app may have |unload| function to persist the app state that does not
 *    fit into |window.appState|.
 *
 * @param {string} url App window content url.
 * @param {string} id App window id.
 * @param {Object} options Options object to create it.
 * @constructor
 */
function AppWindowWrapper(url, id, options) {
  this.url_ = url;
  this.id_ = id;
  // Do deep copy for the template of options to assign customized params later.
  this.options_ = JSON.parse(JSON.stringify(options));
  this.window_ = null;
  this.appState_ = null;
  this.openingOrOpened_ = false;
  this.queue = new AsyncUtil.Queue();
  Object.seal(this);
}

/**
 * Shift distance to avoid overlapping windows.
 * @type {number}
 * @const
 */
AppWindowWrapper.SHIFT_DISTANCE = 40;


/**
 * Opens the window.
 *
 * @param {Object} appState App state.
 * @param {function()=} opt_callback Completion callback.
 */
AppWindowWrapper.prototype.launch = function(appState, opt_callback) {
  // Check if the window is opened or not.
  if (this.openingOrOpened_) {
    console.error('The window is already opened.');
    if (opt_callback)
      opt_callback();
    return;
  }
  this.openingOrOpened_ = true;

  // Save application state.
  this.appState_ = appState;

  // Get similar windows, it means with the same initial url, eg. different
  // main windows of Files.app.
  var similarWindows = background.getSimilarWindows(this.url_);

  // Restore maximized windows, to avoid hiding them to tray, which can be
  // confusing for users.
  this.queue.run(function(callback) {
    for (var index = 0; index < similarWindows.length; index++) {
      if (similarWindows[index].isMaximized()) {
        var createWindowAndRemoveListener = function() {
          similarWindows[index].onRestored.removeListener(
              createWindowAndRemoveListener);
          callback();
        };
        similarWindows[index].onRestored.addListener(
            createWindowAndRemoveListener);
        similarWindows[index].restore();
        return;
      }
    }
    // If no maximized windows, then create the window immediately.
    callback();
  });

  // Obtains the last geometry.
  var lastBounds;
  this.queue.run(function(callback) {
    var key = Background.makeGeometryKey(this.url_);
    chrome.storage.local.get(key, function(preferences) {
      if (!chrome.runtime.lastError)
        lastBounds = preferences[key];
      callback();
    });
  }.bind(this));

  // Closure creating the window, once all preprocessing tasks are finished.
  this.queue.run(function(callback) {
    // Apply the last bounds.
    if (lastBounds)
      this.options_.bounds = lastBounds;

    // Create a window.
    chrome.app.window.create(this.url_, this.options_, function(appWindow) {
      this.window_ = appWindow;
      callback();
    }.bind(this));
  }.bind(this));

  // After creating.
  this.queue.run(function(callback) {
    // If there is another window in the same position, shift the window.
    var makeBoundsKey = function(bounds) {
      return bounds.left + '/' + bounds.top;
    };
    var notAvailablePositions = {};
    for (var i = 0; i < similarWindows.length; i++) {
      var key = makeBoundsKey(similarWindows[i].getBounds());
      notAvailablePositions[key] = true;
    }
    var candidateBounds = this.window_.getBounds();
    while (true) {
      var key = makeBoundsKey(candidateBounds);
      if (!notAvailablePositions[key])
        break;
      // Make the position available to avoid an infinite loop.
      notAvailablePositions[key] = false;
      var nextLeft = candidateBounds.left + AppWindowWrapper.SHIFT_DISTANCE;
      var nextRight = nextLeft + candidateBounds.width;
      candidateBounds.left = nextRight >= screen.availWidth ?
          nextRight % screen.availWidth : nextLeft;
      var nextTop = candidateBounds.top + AppWindowWrapper.SHIFT_DISTANCE;
      var nextBottom = nextTop + candidateBounds.height;
      candidateBounds.top = nextBottom >= screen.availHeight ?
          nextBottom % screen.availHeight : nextTop;
    }
    this.window_.moveTo(candidateBounds.left, candidateBounds.top);

    // Save the properties.
    var appWindow = this.window_;
    background.appWindows[this.id_] = appWindow;
    var contentWindow = appWindow.contentWindow;
    contentWindow.appID = this.id_;
    contentWindow.appState = this.appState_;
    contentWindow.appInitialURL = this.url_;
    if (window.IN_TEST)
      contentWindow.IN_TEST = true;

    // Register event listners.
    appWindow.onBoundsChanged.addListener(this.onBoundsChanged_.bind(this));
    appWindow.onClosed.addListener(this.onClosed_.bind(this));

    // Callback.
    if (opt_callback)
      opt_callback();
    callback();
  }.bind(this));
};

/**
 * Handles the onClosed extension API event.
 * @private
 */
AppWindowWrapper.prototype.onClosed_ = function() {
  // Unload the window.
  var appWindow = this.window_;
  var contentWindow = this.window_.contentWindow;
  if (contentWindow.unload)
    contentWindow.unload();
  this.window_ = null;
  this.openingOrOpened_ = false;

  // Updates preferences.
  if (contentWindow.saveOnExit) {
    contentWindow.saveOnExit.forEach(function(entry) {
      util.AppCache.update(entry.key, entry.value);
    });
  }
  chrome.storage.local.remove(this.id_);  // Forget the persisted state.

  // Remove the window from the set.
  delete background.appWindows[this.id_];

  // If there is no application window, reset window ID.
  if (!Object.keys(background.appWindows).length)
    nextFileManagerWindowID = 0;
  background.tryClose();
};

/**
 * Handles onBoundsChanged extension API event.
 * @private
 */
AppWindowWrapper.prototype.onBoundsChanged_ = function() {
  var preferences = {};
  preferences[Background.makeGeometryKey(this.url_)] =
      this.window_.getBounds();
  chrome.storage.local.set(preferences);
};

/**
 * Wrapper for a singleton app window.
 *
 * In addition to the AppWindowWrapper requirements the app scripts should
 * have |reload| method that re-initializes the app based on a changed
 * |window.appState|.
 *
 * @param {string} url App window content url.
 * @param {Object|function()} options Options object or a function to return it.
 * @constructor
 */
function SingletonAppWindowWrapper(url, options) {
  AppWindowWrapper.call(this, url, url, options);
}

/**
 * Inherits from AppWindowWrapper.
 */
SingletonAppWindowWrapper.prototype = {__proto__: AppWindowWrapper.prototype};

/**
 * Open the window.
 *
 * Activates an existing window or creates a new one.
 *
 * @param {Object} appState App state.
 * @param {function()=} opt_callback Completion callback.
 */
SingletonAppWindowWrapper.prototype.launch = function(appState, opt_callback) {
  // If the window is not opened yet, just call the parent method.
  if (!this.openingOrOpened_) {
    AppWindowWrapper.prototype.launch.call(this, appState, opt_callback);
    return;
  }

  // If the window is already opened, reload the window.
  // The queue is used to wait until the window is opened.
  this.queue.run(function(nextStep) {
    this.window_.contentWindow.appState = appState;
    this.window_.contentWindow.reload();
    this.window_.focus();
    if (opt_callback)
      opt_callback();
    nextStep();
  }.bind(this));
};

/**
 * Reopen a window if its state is saved in the local storage.
 */
SingletonAppWindowWrapper.prototype.reopen = function() {
  chrome.storage.local.get(this.id_, function(items) {
    var value = items[this.id_];
    if (!value)
      return;  // No app state persisted.

    try {
      var appState = JSON.parse(value);
    } catch (e) {
      console.error('Corrupt launch data for ' + this.id_, value);
      return;
    }
    this.launch(appState);
  }.bind(this));
};

/**
 * Prefix for the file manager window ID.
 */
var FILES_ID_PREFIX = 'files#';

/**
 * Regexp matching a file manager window ID.
 */
var FILES_ID_PATTERN = new RegExp('^' + FILES_ID_PREFIX + '(\\d*)$');

/**
 * Value of the next file manager window ID.
 */
var nextFileManagerWindowID = 0;

/**
 * File manager window create options.
 * @type {Object}
 * @const
 */
var FILE_MANAGER_WINDOW_CREATE_OPTIONS = Object.freeze({
  bounds: Object.freeze({
    left: Math.round(window.screen.availWidth * 0.1),
    top: Math.round(window.screen.availHeight * 0.1),
    width: Math.round(window.screen.availWidth * 0.8),
    height: Math.round(window.screen.availHeight * 0.8)
  }),
  minWidth: 320,
  minHeight: 240,
  frame: 'none',
  hidden: true,
  transparentBackground: true
});

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
function launchFileManager(opt_appState, opt_id, opt_type, opt_callback) {
  var type = opt_type || LaunchType.ALWAYS_CREATE;

  // Wait until all windows are created.
  background.queue.run(function(onTaskCompleted) {
    // Check if there is already a window with the same path. If so, then
    // reuse it instead of opening a new one.
    if (type == LaunchType.FOCUS_SAME_OR_CREATE ||
        type == LaunchType.FOCUS_ANY_OR_CREATE) {
      if (opt_appState && opt_appState.defaultPath) {
        for (var key in background.appWindows) {
          if (!key.match(FILES_ID_PATTERN))
            continue;

          var contentWindow = background.appWindows[key].contentWindow;
          if (contentWindow.appState &&
              opt_appState.defaultPath == contentWindow.appState.defaultPath) {
            background.appWindows[key].focus();
            if (opt_callback)
              opt_callback(key);
            onTaskCompleted();
            return;
          }
        }
      }
    }

    // Focus any window if none is focused. Try restored first.
    if (type == LaunchType.FOCUS_ANY_OR_CREATE) {
      // If there is already a focused window, then finish.
      for (var key in background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        // The isFocused() method should always be available, but in case
        // Files.app's failed on some error, wrap it with try catch.
        try {
          if (background.appWindows[key].contentWindow.isFocused()) {
            if (opt_callback)
              opt_callback(key);
            onTaskCompleted();
            return;
          }
        } catch (e) {
          console.error(e.message);
        }
      }
      // Try to focus the first non-minimized window.
      for (var key in background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        if (!background.appWindows[key].isMinimized()) {
          background.appWindows[key].focus();
          if (opt_callback)
            opt_callback(key);
          onTaskCompleted();
          return;
        }
      }
      // Restore and focus any window.
      for (var key in background.appWindows) {
        if (!key.match(FILES_ID_PATTERN))
          continue;

        background.appWindows[key].focus();
        if (opt_callback)
          opt_callback(key);
        onTaskCompleted();
        return;
      }
    }

    // Create a new instance in case of ALWAYS_CREATE type, or as a fallback
    // for other types.

    var id = opt_id || nextFileManagerWindowID;
    nextFileManagerWindowID = Math.max(nextFileManagerWindowID, id + 1);
    var appId = FILES_ID_PREFIX + id;

    var appWindow = new AppWindowWrapper(
        'main.html',
        appId,
        FILE_MANAGER_WINDOW_CREATE_OPTIONS);
    appWindow.launch(opt_appState || {}, function() {
      if (opt_callback)
        opt_callback(appId);
      onTaskCompleted();
    });
  });
}

/**
 * Executes a file browser task.
 *
 * @param {string} action Task id.
 * @param {Object} details Details object.
 * @private
 */
Background.prototype.onExecute_ = function(action, details) {
  var urls = details.entries.map(function(e) { return e.toURL(); });

  switch (action) {
    case 'play':
      launchAudioPlayer({items: urls, position: 0});
      break;

    case 'watch':
      launchVideoPlayer(urls[0]);
      break;

    default:
      var launchEnable = null;
      var queue = new AsyncUtil.Queue();
      queue.run(function(nextStep) {
        // If it is not auto-open (triggered by mounting external devices), we
        // always launch Files.app.
        if (action != 'auto-open') {
          launchEnable = true;
          nextStep();
          return;
        }
        // If the disable-default-apps flag is on, Files.app is not opened
        // automatically on device mount because it obstculs the manual test.
        chrome.commandLinePrivate.hasSwitch('disable-default-apps',
                                            function(flag) {
          launchEnable = !flag;
          nextStep();
        });
      });
      queue.run(function(nextStep) {
        if (!launchEnable) {
          nextStep();
          return;
        }

        // Every other action opens a Files app window.
        var appState = {
          params: {
            action: action
          },
          defaultPath: details.entries[0].fullPath
        };
        // For mounted devices just focus any Files.app window. The mounted
        // volume will appear on the navigation list.
        var type = action == 'auto-open' ? LaunchType.FOCUS_ANY_OR_CREATE :
            LaunchType.FOCUS_SAME_OR_CREATE;
        launchFileManager(appState, /* App ID */ undefined, type, nextStep);
      });
      break;
  }
};

/**
 * Audio player window create options.
 * @type {Object}
 * @const
 */
var AUDIO_PLAYER_CREATE_OPTIONS = Object.freeze({
  type: 'panel',
  hidden: true,
  minHeight: 35 + 58,
  minWidth: 280,
  height: 35 + 58,
  width: 280
});

var audioPlayer = new SingletonAppWindowWrapper('mediaplayer.html',
                                                AUDIO_PLAYER_CREATE_OPTIONS);

/**
 * Launch the audio player.
 * @param {Object} playlist Playlist.
 */
function launchAudioPlayer(playlist) {
  audioPlayer.launch(playlist);
}

var videoPlayer = new SingletonAppWindowWrapper('video_player.html',
                                                {hidden: true});

/**
 * Launch the video player.
 * @param {string} url Video url.
 */
function launchVideoPlayer(url) {
  videoPlayer.launch({url: url});
}

/**
 * Launches the app.
 * @private
 */
Background.prototype.onLaunched_ = function() {
  if (nextFileManagerWindowID == 0) {
    // The app just launched. Remove window state records that are not needed
    // any more.
    chrome.storage.local.get(function(items) {
      for (var key in items) {
        if (items.hasOwnProperty(key)) {
          if (key.match(FILES_ID_PATTERN))
            chrome.storage.local.remove(key);
        }
      }
    });
  }
  launchFileManager(null, null, LaunchType.FOCUS_ANY_OR_CREATE);
};

/**
 * Restarted the app, restore windows.
 * @private
 */
Background.prototype.onRestarted_ = function() {
  // Reopen file manager windows.
  chrome.storage.local.get(function(items) {
    for (var key in items) {
      if (items.hasOwnProperty(key)) {
        var match = key.match(FILES_ID_PATTERN);
        if (match) {
          var id = Number(match[1]);
          try {
            var appState = JSON.parse(items[key]);
            launchFileManager(appState, id);
          } catch (e) {
            console.error('Corrupt launch data for ' + id);
          }
        }
      }
    }
  });

  // Reopen sub-applications.
  audioPlayer.reopen();
  videoPlayer.reopen();
};

/**
 * Handles clicks on a custom item on the launcher context menu.
 * @param {OnClickData} info Event details.
 * @private
 */
Background.prototype.onContextMenuClicked_ = function(info) {
  if (info.menuItemId == 'new-window') {
    // Find the focused window (if any) and use it's current path for the
    // new window. If not found, then launch with the default path.
    for (var key in background.appWindows) {
      try {
        if (background.appWindows[key].contentWindow.isFocused()) {
          var appState = {
            defaultPath: background.appWindows[key].contentWindow.
                appState.defaultPath
          };
          launchFileManager(appState);
          return;
        }
      } catch (ignore) {
        // The isFocused method may not be defined during initialization.
        // Therefore, wrapped with a try-catch block.
      }
    }

    // Launch with the default path.
    launchFileManager();
  }
};

/**
 * Initializes the context menu. Recreates if already exists.
 * @private
 */
Background.prototype.initContextMenu_ = function() {
  try {
    chrome.contextMenus.remove('new-window');
  } catch (ignore) {
    // There is no way to detect if the context menu is already added, therefore
    // try to recreate it every time.
  }
  chrome.contextMenus.create({
    id: 'new-window',
    contexts: ['launcher'],
    title: str('NEW_WINDOW_BUTTON_LABEL')
  });
};

/**
 * Singleton instance of Background.
 * @type {Background}
 */
window.background = new Background();
