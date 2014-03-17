// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This view displays controls for capturing network events.
 */
var CaptureView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function CaptureView() {
    assertFirstConstructorCall(CaptureView);

    // Call superclass's constructor.
    superClass.call(this, CaptureView.MAIN_BOX_ID);

    var byteLoggingCheckbox = $(CaptureView.BYTE_LOGGING_CHECKBOX_ID);
    byteLoggingCheckbox.onclick = this.onSetByteLogging_.bind(this);

    $(CaptureView.LIMIT_CHECKBOX_ID).onclick = this.onChangeLimit_.bind(this);

    $(CaptureView.STOP_BUTTON_ID).onclick =
        this.onStopButtonClicked_.bind(this);
    $(CaptureView.RESET_BUTTON_ID).onclick =
        this.onResetButtonClicked_.bind(this);

    if (byteLoggingCheckbox.checked) {
      // The code to display a warning on ExportView relies on bytelogging
      // being off by default. If this ever changes, the code will need to
      // be updated.
      throw 'Not expecting byte logging to be enabled!';
    }

    new MouseOverHelp(CaptureView.LIMIT_HELP_ID,
                      CaptureView.LIMIT_HELP_HOVER_ID);

    new MouseOverHelp(CaptureView.BYTE_LOGGING_HELP_ID,
                      CaptureView.BYTE_LOGGING_HELP_HOVER_ID);

    this.onChangeLimit_();
  }

  CaptureView.TAB_ID = 'tab-handle-capture';
  CaptureView.TAB_NAME = 'Capture';
  CaptureView.TAB_HASH = '#capture';

  // IDs for special HTML elements in capture_view.html
  CaptureView.MAIN_BOX_ID = 'capture-view-tab-content';
  CaptureView.BYTE_LOGGING_CHECKBOX_ID = 'capture-view-byte-logging-checkbox';
  CaptureView.LIMIT_CHECKBOX_ID = 'capture-view-limit-checkbox';
  CaptureView.LIMIT_HELP_ID = 'capture-view-limit-help';
  CaptureView.LIMIT_HELP_HOVER_ID = 'capture-view-limit-help-hover';
  CaptureView.BYTE_LOGGING_HELP_ID = 'capture-view-byte-logging-help';
  CaptureView.BYTE_LOGGING_HELP_HOVER_ID =
      'capture-view-byte-logging-help-hover';
  CaptureView.STOP_BUTTON_ID = 'capture-view-stop-button';
  CaptureView.RESET_BUTTON_ID = 'capture-view-reset-button';

  cr.addSingletonGetter(CaptureView);

  CaptureView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    /**
     * Called when a log file is loaded, after clearing the old log entries and
     * loading the new ones.  Returns false to indicate the view should
     * be hidden.
     */
    onLoadLogFinish: function(data) {
      return false;
    },

    /**
     * Depending on the value of the checkbox, enables or disables logging of
     * actual bytes transferred.
     */
    onSetByteLogging_: function() {
      var byteLoggingCheckbox = $(CaptureView.BYTE_LOGGING_CHECKBOX_ID);

      if (byteLoggingCheckbox.checked) {
        g_browser.setLogLevel(LogLevelType.LOG_ALL);

        // Once we enable byte logging, all bets are off on what gets captured.
        // Have the export view warn that the "strip cookies" option is
        // ineffective from this point on.
        //
        // In theory we could clear this warning after unchecking the box and
        // then deleting all the events which had been captured. We don't
        // currently do that; if you want the warning to go away, will need to
        // reload.
        ExportView.getInstance().showPrivacyWarning();
      } else {
        g_browser.setLogLevel(LogLevelType.LOG_ALL_BUT_BYTES);
      }
    },

    onChangeLimit_: function() {
      var limitCheckbox = $(CaptureView.LIMIT_CHECKBOX_ID);

      // Default to unlimited.
      var softLimit = Infinity;
      var hardLimit = Infinity;

      if (limitCheckbox.checked) {
        // The chosen limits are kind of arbitrary. I based it off the
        // following observation:
        //   A user-submitted log file which spanned a 7 hour time period
        //   comprised 778,235 events and required 128MB of JSON.
        //
        // That feels too big. Assuming it was representative, then scaling
        // by a factor of 4 should translate into a 32MB log file and cover
        // close to 2 hours of events, which feels better.
        //
        // A large gap is left between the hardLimit and softLimit to avoid
        // resetting the events often.
        hardLimit = 300000;
        softLimit = 150000;
      }

      EventsTracker.getInstance().setLimits(softLimit, hardLimit);
    },

    onStopButtonClicked_: function() {
      MainView.getInstance().switchToViewOnlyMode();
    },

    onResetButtonClicked_: function() {
      EventsTracker.getInstance().deleteAllLogEntries();
    },
  };

  return CaptureView;
})();
