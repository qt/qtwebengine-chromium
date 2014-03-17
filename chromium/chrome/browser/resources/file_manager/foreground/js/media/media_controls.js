// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview MediaControls class implements media playback controls
 * that exist outside of the audio/video HTML element.
 */

/**
 * @param {HTMLElement} containerElement The container for the controls.
 * @param {function} onMediaError Function to display an error message.
 * @constructor
 */
function MediaControls(containerElement, onMediaError) {
  this.container_ = containerElement;
  this.document_ = this.container_.ownerDocument;
  this.media_ = null;

  this.onMediaPlayBound_ = this.onMediaPlay_.bind(this, true);
  this.onMediaPauseBound_ = this.onMediaPlay_.bind(this, false);
  this.onMediaDurationBound_ = this.onMediaDuration_.bind(this);
  this.onMediaProgressBound_ = this.onMediaProgress_.bind(this);
  this.onMediaError_ = onMediaError || function() {};
}

/**
 * Button's state types. Values are used as CSS class names.
 * @enum {string}
 */
MediaControls.ButtonStateType = {
  DEFAULT: 'default',
  PLAYING: 'playing',
  ENDED: 'ended'
};

/**
 * @return {HTMLAudioElement|HTMLVideoElement} The media element.
 */
MediaControls.prototype.getMedia = function() { return this.media_ };

/**
 * Format the time in hh:mm:ss format (omitting redundant leading zeros).
 *
 * @param {number} timeInSec Time in seconds.
 * @return {string} Formatted time string.
 * @private
 */
MediaControls.formatTime_ = function(timeInSec) {
  var seconds = Math.floor(timeInSec % 60);
  var minutes = Math.floor((timeInSec / 60) % 60);
  var hours = Math.floor(timeInSec / 60 / 60);
  var result = '';
  if (hours) result += hours + ':';
  if (hours && (minutes < 10)) result += '0';
  result += minutes + ':';
  if (seconds < 10) result += '0';
  result += seconds;
  return result;
};

/**
 * Create a custom control.
 *
 * @param {string} className Class name.
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @return {HTMLElement} The new control element.
 */
MediaControls.prototype.createControl = function(className, opt_parent) {
  var parent = opt_parent || this.container_;
  var control = this.document_.createElement('div');
  control.className = className;
  parent.appendChild(control);
  return control;
};

/**
 * Create a custom button.
 *
 * @param {string} className Class name.
 * @param {function(Event)} handler Click handler.
 * @param {HTMLElement=} opt_parent Parent element or container if undefined.
 * @param {number=} opt_numStates Number of states, default: 1.
 * @return {HTMLElement} The new button element.
 */
MediaControls.prototype.createButton = function(
    className, handler, opt_parent, opt_numStates) {
  opt_numStates = opt_numStates || 1;

  var button = this.createControl(className, opt_parent);
  button.classList.add('media-button');
  button.addEventListener('click', handler);

  var stateTypes = Object.keys(MediaControls.ButtonStateType);
  for (var state = 0; state != opt_numStates; state++) {
    var stateClass = MediaControls.ButtonStateType[stateTypes[state]];
    this.createControl('normal ' + stateClass, button);
    this.createControl('hover ' + stateClass, button);
    this.createControl('active ' + stateClass, button);
  }
  this.createControl('disabled', button);

  button.setAttribute('state', MediaControls.ButtonStateType.DEFAULT);
  button.addEventListener('click', handler);
  return button;
};

/**
 * Enable/disable controls matching a given selector.
 *
 * @param {string} selector CSS selector.
 * @param {boolean} on True if enable, false if disable.
 * @private
 */
MediaControls.prototype.enableControls_ = function(selector, on) {
  var controls = this.container_.querySelectorAll(selector);
  for (var i = 0; i != controls.length; i++) {
    var classList = controls[i].classList;
    if (on)
      classList.remove('disabled');
    else
      classList.add('disabled');
  }
};

/*
 * Playback control.
 */

/**
 * Play the media.
 */
MediaControls.prototype.play = function() {
  this.media_.play();
};

/**
 * Pause the media.
 */
MediaControls.prototype.pause = function() {
  this.media_.pause();
};

/**
 * @return {boolean} True if the media is currently playing.
 */
MediaControls.prototype.isPlaying = function() {
  return !this.media_.paused && !this.media_.ended;
};

/**
 * Toggle play/pause.
 */
MediaControls.prototype.togglePlayState = function() {
  if (this.isPlaying())
    this.pause();
  else
    this.play();
};

/**
 * Toggle play/pause state on a mouse click on the play/pause button. Can be
 * called externally. TODO(mtomasz): Remove it. http://www.crbug.com/254318.
 *
 * @param {Event=} opt_event Mouse click event.
 */
MediaControls.prototype.onPlayButtonClicked = function(opt_event) {
  this.togglePlayState();
};

/**
 * @param {HTMLElement=} opt_parent Parent container.
 */
MediaControls.prototype.initPlayButton = function(opt_parent) {
  this.playButton_ = this.createButton('play media-control',
      this.onPlayButtonClicked.bind(this), opt_parent, 3 /* States. */);
};

/*
 * Time controls
 */

/**
 * The default range of 100 is too coarse for the media progress slider.
 */
MediaControls.PROGRESS_RANGE = 5000;

/**
 * @param {boolean=} opt_seekMark True if the progress slider should have
 *     a seek mark.
 * @param {HTMLElement=} opt_parent Parent container.
 */
MediaControls.prototype.initTimeControls = function(opt_seekMark, opt_parent) {
  var timeControls = this.createControl('time-controls', opt_parent);

  var sliderConstructor =
      opt_seekMark ? MediaControls.PreciseSlider : MediaControls.Slider;

  this.progressSlider_ = new sliderConstructor(
      this.createControl('progress media-control', timeControls),
      0, /* value */
      MediaControls.PROGRESS_RANGE,
      this.onProgressChange_.bind(this),
      this.onProgressDrag_.bind(this));

  var timeBox = this.createControl('time media-control', timeControls);

  this.duration_ = this.createControl('duration', timeBox);
  // Set the initial width to the minimum to reduce the flicker.
  this.duration_.textContent = MediaControls.formatTime_(0);

  this.currentTime_ = this.createControl('current', timeBox);
};

/**
 * @param {number} current Current time is seconds.
 * @param {number} duration Duration in seconds.
 * @private
 */
MediaControls.prototype.displayProgress_ = function(current, duration) {
  var ratio = current / duration;
  this.progressSlider_.setValue(ratio);
  this.currentTime_.textContent = MediaControls.formatTime_(current);
};

/**
 * @param {number} value Progress [0..1].
 * @private
 */
MediaControls.prototype.onProgressChange_ = function(value) {
  if (!this.media_.seekable || !this.media_.duration) {
    console.error('Inconsistent media state');
    return;
  }

  var current = this.media_.duration * value;
  this.media_.currentTime = current;
  this.currentTime_.textContent = MediaControls.formatTime_(current);
};

/**
 * @param {boolean} on True if dragging.
 * @private
 */
MediaControls.prototype.onProgressDrag_ = function(on) {
  if (on) {
    this.resumeAfterDrag_ = this.isPlaying();
    this.media_.pause();
  } else {
    if (this.resumeAfterDrag_) {
      if (this.media_.ended)
        this.onMediaPlay_(false);
      else
        this.media_.play();
    }
    this.updatePlayButtonState_(this.isPlaying());
  }
};

/*
 * Volume controls
 */

/**
 * @param {HTMLElement=} opt_parent Parent element for the controls.
 */
MediaControls.prototype.initVolumeControls = function(opt_parent) {
  var volumeControls = this.createControl('volume-controls', opt_parent);

  this.soundButton_ = this.createButton('sound media-control',
      this.onSoundButtonClick_.bind(this), volumeControls);
  this.soundButton_.setAttribute('level', 3);  // max level.

  this.volume_ = new MediaControls.AnimatedSlider(
      this.createControl('volume media-control', volumeControls),
      1, /* value */
      100 /* range */,
      this.onVolumeChange_.bind(this),
      this.onVolumeDrag_.bind(this));
};

/**
 * Click handler for the sound level button.
 * @private
 */
MediaControls.prototype.onSoundButtonClick_ = function() {
  if (this.media_.volume == 0) {
    this.volume_.setValue(this.savedVolume_ || 1);
  } else {
    this.savedVolume_ = this.media_.volume;
    this.volume_.setValue(0);
  }
  this.onVolumeChange_(this.volume_.getValue());
};

/**
 * @param {number} value Volume [0..1].
 * @return {number} The rough level [0..3] used to pick an icon.
 * @private
 */
MediaControls.getVolumeLevel_ = function(value) {
  if (value == 0) return 0;
  if (value <= 1 / 3) return 1;
  if (value <= 2 / 3) return 2;
  return 3;
};

/**
 * @param {number} value Volume [0..1].
 * @private
 */
MediaControls.prototype.onVolumeChange_ = function(value) {
  this.media_.volume = value;
  this.soundButton_.setAttribute('level', MediaControls.getVolumeLevel_(value));
};

/**
 * @param {boolean} on True if dragging is in progress.
 * @private
 */
MediaControls.prototype.onVolumeDrag_ = function(on) {
  if (on && (this.media_.volume != 0)) {
    this.savedVolume_ = this.media_.volume;
  }
};

/*
 * Media event handlers.
 */

/**
 * Attach a media element.
 *
 * @param {HTMLMediaElement} mediaElement The media element to control.
 */
MediaControls.prototype.attachMedia = function(mediaElement) {
  this.media_ = mediaElement;

  this.media_.addEventListener('play', this.onMediaPlayBound_);
  this.media_.addEventListener('pause', this.onMediaPauseBound_);
  this.media_.addEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.addEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.addEventListener('error', this.onMediaError_);

  // Reflect the media state in the UI.
  this.onMediaDuration_();
  this.onMediaPlay_(this.isPlaying());
  this.onMediaProgress_();
  if (this.volume_) {
    /* Copy the user selected volume to the new media element. */
    this.media_.volume = this.volume_.getValue();
  }
};

/**
 * Detach media event handlers.
 */
MediaControls.prototype.detachMedia = function() {
  if (!this.media_)
    return;

  this.media_.removeEventListener('play', this.onMediaPlayBound_);
  this.media_.removeEventListener('pause', this.onMediaPauseBound_);
  this.media_.removeEventListener('durationchange', this.onMediaDurationBound_);
  this.media_.removeEventListener('timeupdate', this.onMediaProgressBound_);
  this.media_.removeEventListener('error', this.onMediaError_);

  this.media_ = null;
};

/**
 * Force-empty the media pipeline. This is a workaround for crbug.com/149957.
 * The document is not going to be GC-ed until the last Files app window closes,
 * but we want the media pipeline to deinitialize ASAP to minimize leakage.
 */
MediaControls.prototype.cleanup = function() {
  this.media_.src = '';
  this.media_.load();
  this.detachMedia();
};

/**
 * 'play' and 'pause' event handler.
 * @param {boolean} playing True if playing.
 * @private
 */
MediaControls.prototype.onMediaPlay_ = function(playing) {
  if (this.progressSlider_.isDragging())
    return;

  this.updatePlayButtonState_(playing);
  this.onPlayStateChanged();
};

/**
 * 'durationchange' event handler.
 * @private
 */
MediaControls.prototype.onMediaDuration_ = function() {
  if (!this.media_.duration) {
    this.enableControls_('.media-control', false);
    return;
  }

  this.enableControls_('.media-control', true);

  var sliderContainer = this.progressSlider_.getContainer();
  if (this.media_.seekable)
    sliderContainer.classList.remove('readonly');
  else
    sliderContainer.classList.add('readonly');

  var valueToString = function(value) {
    return MediaControls.formatTime_(this.media_.duration * value);
  }.bind(this);

  this.duration_.textContent = valueToString(1);

  if (this.progressSlider_.setValueToStringFunction)
    this.progressSlider_.setValueToStringFunction(valueToString);

  if (this.media_.seekable)
    this.restorePlayState();
};

/**
 * 'timeupdate' event handler.
 * @private
 */
MediaControls.prototype.onMediaProgress_ = function() {
  if (!this.media_.duration) {
    this.displayProgress_(0, 1);
    return;
  }

  var current = this.media_.currentTime;
  var duration = this.media_.duration;

  if (this.progressSlider_.isDragging())
    return;

  this.displayProgress_(current, duration);

  if (current == duration) {
    this.onMediaComplete();
  }
  this.onPlayStateChanged();
};

/**
 * Called when the media playback is complete.
 */
MediaControls.prototype.onMediaComplete = function() {};

/**
 * Called when play/pause state is changed or on playback progress.
 * This is the right moment to save the play state.
 */
MediaControls.prototype.onPlayStateChanged = function() {};

/**
 * Updates the play button state.
 * @param {boolean} playing If the video is playing.
 * @private
 */
MediaControls.prototype.updatePlayButtonState_ = function(playing) {
  if (playing) {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.PLAYING);
  } else if (!this.media_.ended) {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.DEFAULT);
  } else {
    this.playButton_.setAttribute('state',
                                  MediaControls.ButtonStateType.ENDED);
  }
};

/**
 * Restore play state. Base implementation is empty.
 */
MediaControls.prototype.restorePlayState = function() {};

/**
 * Encode current state into the page URL or the app state.
 */
MediaControls.prototype.encodeState = function() {
  if (!this.media_.duration)
    return;

  if (window.appState) {
    window.appState.time = this.media_.currentTime;
    util.saveAppState();
    return;
  }

  var playState = JSON.stringify({
      play: this.isPlaying(),
      time: this.media_.currentTime
    });

  var newLocation = document.location.origin + document.location.pathname +
      document.location.search + '#' + playState;

  document.location.href = newLocation;
};

/**
 * Decode current state from the page URL or the app state.
 * @return {boolean} True if decode succeeded.
 */
MediaControls.prototype.decodeState = function() {
  if (window.appState) {
    if (!('time' in window.appState))
      return false;
    // There is no page reload for apps v2, only app restart.
    // Always restart in paused state.
    this.media_.currentTime = appState.time;
    this.pause();
    return true;
  }

  var hash = document.location.hash.substring(1);
  if (hash) {
    try {
      var playState = JSON.parse(hash);
      if (!('time' in playState))
        return false;

      this.media_.currentTime = playState.time;

      if (playState.play)
        this.play();
      else
        this.pause();

      return true;
    } catch (e) {
      console.warn('Cannot decode play state');
    }
  }
  return false;
};

/**
 * Remove current state from the page URL or the app state.
 */
MediaControls.prototype.clearState = function() {
  if (window.appState) {
    if ('time' in window.appState)
      delete window.appState.time;
    util.saveAppState();
    return;
  }

  var newLocation = document.location.origin + document.location.pathname +
      document.location.search + '#';

  document.location.href = newLocation;
};

/**
 * Create a customized slider control.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @param {function(number)} onChange Value change handler.
 * @param {function(boolean)} onDrag Drag begin/end handler.
 * @constructor
 */

MediaControls.Slider = function(container, value, range, onChange, onDrag) {
  this.container_ = container;
  this.onChange_ = onChange;
  this.onDrag_ = onDrag;

  var document = this.container_.ownerDocument;

  this.container_.classList.add('custom-slider');

  this.input_ = document.createElement('input');
  this.input_.type = 'range';
  this.input_.min = 0;
  this.input_.max = range;
  this.input_.value = value * range;
  this.container_.appendChild(this.input_);

  this.input_.addEventListener(
      'change', this.onInputChange_.bind(this));
  this.input_.addEventListener(
      'mousedown', this.onInputDrag_.bind(this, true));
  this.input_.addEventListener(
      'mouseup', this.onInputDrag_.bind(this, false));

  this.bar_ = document.createElement('div');
  this.bar_.className = 'bar';
  this.container_.appendChild(this.bar_);

  this.filled_ = document.createElement('div');
  this.filled_.className = 'filled';
  this.bar_.appendChild(this.filled_);

  var leftCap = document.createElement('div');
  leftCap.className = 'cap left';
  this.bar_.appendChild(leftCap);

  var rightCap = document.createElement('div');
  rightCap.className = 'cap right';
  this.bar_.appendChild(rightCap);

  this.value_ = value;
  this.setFilled_(value);
};

/**
 * @return {HTMLElement} The container element.
 */
MediaControls.Slider.prototype.getContainer = function() {
  return this.container_;
};

/**
 * @return {HTMLElement} The standard input element.
 * @private
 */
MediaControls.Slider.prototype.getInput_ = function() {
  return this.input_;
};

/**
 * @return {HTMLElement} The slider bar element.
 */
MediaControls.Slider.prototype.getBar = function() {
  return this.bar_;
};

/**
 * @return {number} [0..1] The current value.
 */
MediaControls.Slider.prototype.getValue = function() {
  return this.value_;
};

/**
 * @param {number} value [0..1].
 */
MediaControls.Slider.prototype.setValue = function(value) {
  this.value_ = value;
  this.setValueToUI_(value);
};

/**
 * Fill the given proportion the slider bar (from the left).
 *
 * @param {number} proportion [0..1].
 * @private
 */
MediaControls.Slider.prototype.setFilled_ = function(proportion) {
  this.filled_.style.width = proportion * 100 + '%';
};

/**
 * Get the value from the input element.
 *
 * @return {number} Value [0..1].
 * @private
 */
MediaControls.Slider.prototype.getValueFromUI_ = function() {
  return this.input_.value / this.input_.max;
};

/**
 * Update the UI with the current value.
 *
 * @param {number} value [0..1].
 * @private
 */
MediaControls.Slider.prototype.setValueToUI_ = function(value) {
  this.input_.value = value * this.input_.max;
  this.setFilled_(value);
};

/**
 * Compute the proportion in which the given position divides the slider bar.
 *
 * @param {number} position in pixels.
 * @return {number} [0..1] proportion.
 */
MediaControls.Slider.prototype.getProportion = function(position) {
  var rect = this.bar_.getBoundingClientRect();
  return Math.max(0, Math.min(1, (position - rect.left) / rect.width));
};

/**
 * 'change' event handler.
 * @private
 */
MediaControls.Slider.prototype.onInputChange_ = function() {
  this.value_ = this.getValueFromUI_();
  this.setFilled_(this.value_);
  this.onChange_(this.value_);
};

/**
 * @return {boolean} True if dragging is in progress.
 */
MediaControls.Slider.prototype.isDragging = function() {
  return this.isDragging_;
};

/**
 * Mousedown/mouseup handler.
 * @param {boolean} on True if the mouse is down.
 * @private
 */
MediaControls.Slider.prototype.onInputDrag_ = function(on) {
  this.isDragging_ = on;
  this.onDrag_(on);
};

/**
 * Create a customized slider with animated thumb movement.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @param {function(number)} onChange Value change handler.
 * @param {function(boolean)} onDrag Drag begin/end handler.
 * @param {function(number):string} formatFunction Value formatting function.
 * @constructor
 */
MediaControls.AnimatedSlider = function(
    container, value, range, onChange, onDrag, formatFunction) {
  MediaControls.Slider.apply(this, arguments);
};

MediaControls.AnimatedSlider.prototype = {
  __proto__: MediaControls.Slider.prototype
};

/**
 * Number of animation steps.
 */
MediaControls.AnimatedSlider.STEPS = 10;

/**
 * Animation duration.
 */
MediaControls.AnimatedSlider.DURATION = 100;

/**
 * @param {number} value [0..1].
 * @private
 */
MediaControls.AnimatedSlider.prototype.setValueToUI_ = function(value) {
  if (this.animationInterval_) {
    clearInterval(this.animationInterval_);
  }
  var oldValue = this.getValueFromUI_();
  var step = 0;
  this.animationInterval_ = setInterval(function() {
      step++;
      var currentValue = oldValue +
          (value - oldValue) * (step / MediaControls.AnimatedSlider.STEPS);
      MediaControls.Slider.prototype.setValueToUI_.call(this, currentValue);
      if (step == MediaControls.AnimatedSlider.STEPS) {
        clearInterval(this.animationInterval_);
      }
    }.bind(this),
    MediaControls.AnimatedSlider.DURATION / MediaControls.AnimatedSlider.STEPS);
};

/**
 * Create a customized slider with a precise time feedback.
 *
 * The time value is shown above the slider bar at the mouse position.
 *
 * @param {HTMLElement} container The containing div element.
 * @param {number} value Initial value [0..1].
 * @param {number} range Number of distinct slider positions to be supported.
 * @param {function(number)} onChange Value change handler.
 * @param {function(boolean)} onDrag Drag begin/end handler.
 * @param {function(number):string} formatFunction Value formatting function.
 * @constructor
 */
MediaControls.PreciseSlider = function(
    container, value, range, onChange, onDrag, formatFunction) {
  MediaControls.Slider.apply(this, arguments);

  var doc = this.container_.ownerDocument;

  /**
   * @type {function(number):string}
   * @private
   */
  this.valueToString_ = null;

  this.seekMark_ = doc.createElement('div');
  this.seekMark_.className = 'seek-mark';
  this.getBar().appendChild(this.seekMark_);

  this.seekLabel_ = doc.createElement('div');
  this.seekLabel_.className = 'seek-label';
  this.seekMark_.appendChild(this.seekLabel_);

  this.getContainer().addEventListener(
      'mousemove', this.onMouseMove_.bind(this));
  this.getContainer().addEventListener(
      'mouseout', this.onMouseOut_.bind(this));
};

MediaControls.PreciseSlider.prototype = {
  __proto__: MediaControls.Slider.prototype
};

/**
 * Show the seek mark after a delay.
 */
MediaControls.PreciseSlider.SHOW_DELAY = 200;

/**
 * Hide the seek mark for this long after changing the position with a click.
 */
MediaControls.PreciseSlider.HIDE_AFTER_MOVE_DELAY = 2500;

/**
 * Hide the seek mark for this long after changing the position with a drag.
 */
MediaControls.PreciseSlider.HIDE_AFTER_DRAG_DELAY = 750;

/**
 * Default hide timeout (no hiding).
 */
MediaControls.PreciseSlider.NO_AUTO_HIDE = 0;

/**
 * @param {function(number):string} func Value formatting function.
 */
MediaControls.PreciseSlider.prototype.setValueToStringFunction =
    function(func) {
  this.valueToString_ = func;

  /* It is not completely accurate to assume that the max value corresponds
   to the longest string, but generous CSS padding will compensate for that. */
  var labelWidth = this.valueToString_(1).length / 2 + 1;
  this.seekLabel_.style.width = labelWidth + 'em';
  this.seekLabel_.style.marginLeft = -labelWidth / 2 + 'em';
};

/**
 * Show the time above the slider.
 *
 * @param {number} ratio [0..1] The proportion of the duration.
 * @param {number} timeout Timeout in ms after which the label should be hidden.
 *     MediaControls.PreciseSlider.NO_AUTO_HIDE means show until the next call.
 * @private
 */
MediaControls.PreciseSlider.prototype.showSeekMark_ =
    function(ratio, timeout) {
  // Do not update the seek mark for the first 500ms after the drag is finished.
  if (this.latestMouseUpTime_ && (this.latestMouseUpTime_ + 500 > Date.now()))
    return;

  this.seekMark_.style.left = ratio * 100 + '%';

  if (ratio < this.getValue()) {
    this.seekMark_.classList.remove('inverted');
  } else {
    this.seekMark_.classList.add('inverted');
  }
  this.seekLabel_.textContent = this.valueToString_(ratio);

  this.seekMark_.classList.add('visible');

  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  if (timeout != MediaControls.PreciseSlider.NO_AUTO_HIDE) {
    this.seekMarkTimer_ = setTimeout(this.hideSeekMark_.bind(this), timeout);
  }
};

/**
 * @private
 */
MediaControls.PreciseSlider.prototype.hideSeekMark_ = function() {
  this.seekMarkTimer_ = null;
  this.seekMark_.classList.remove('visible');
};

/**
 * 'mouseout' event handler.
 * @param {Event} e Event.
 * @private
 */
MediaControls.PreciseSlider.prototype.onMouseMove_ = function(e) {
  this.latestSeekRatio_ = this.getProportion(e.clientX);

  var self = this;
  function showMark() {
    if (!self.isDragging()) {
      self.showSeekMark_(self.latestSeekRatio_,
          MediaControls.PreciseSlider.HIDE_AFTER_MOVE_DELAY);
    }
  }

  if (this.seekMark_.classList.contains('visible')) {
    showMark();
  } else if (!this.seekMarkTimer_) {
    this.seekMarkTimer_ =
        setTimeout(showMark, MediaControls.PreciseSlider.SHOW_DELAY);
  }
};

/**
 * 'mouseout' event handler.
 * @param {Event} e Event.
 * @private
 */
MediaControls.PreciseSlider.prototype.onMouseOut_ = function(e) {
  for (var element = e.relatedTarget; element; element = element.parentNode) {
    if (element == this.getContainer())
      return;
  }
  if (this.seekMarkTimer_) {
    clearTimeout(this.seekMarkTimer_);
    this.seekMarkTimer_ = null;
  }
  this.hideSeekMark_();
};

/**
 * 'change' event handler.
 * @private
 */
MediaControls.PreciseSlider.prototype.onInputChange_ = function() {
  MediaControls.Slider.prototype.onInputChange_.apply(this, arguments);
  if (this.isDragging()) {
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.NO_AUTO_HIDE);
  }
};

/**
 * Mousedown/mouseup handler.
 * @param {boolean} on True if the mouse is down.
 * @private
 */
MediaControls.PreciseSlider.prototype.onInputDrag_ = function(on) {
  MediaControls.Slider.prototype.onInputDrag_.apply(this, arguments);

  if (on) {
    // Dragging started, align the seek mark with the thumb position.
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.NO_AUTO_HIDE);
  } else {
    // Just finished dragging.
    // Show the label for the last time with a shorter timeout.
    this.showSeekMark_(
        this.getValue(), MediaControls.PreciseSlider.HIDE_AFTER_DRAG_DELAY);
    this.latestMouseUpTime_ = Date.now();
  }
};

/**
 * Create video controls.
 *
 * @param {HTMLElement} containerElement The container for the controls.
 * @param {function} onMediaError Function to display an error message.
 * @param {function(string):string} stringFunction Function providing localized
 *     strings.
 * @param {function=} opt_fullScreenToggle Function to toggle fullscreen mode.
 * @param {HTMLElement=} opt_stateIconParent The parent for the icon that
 *     gives visual feedback when the playback state changes.
 * @constructor
 */
function VideoControls(containerElement, onMediaError, stringFunction,
   opt_fullScreenToggle, opt_stateIconParent) {
  MediaControls.call(this, containerElement, onMediaError);
  this.stringFunction_ = stringFunction;

  this.container_.classList.add('video-controls');
  this.initPlayButton();
  this.initTimeControls(true /* show seek mark */);
  this.initVolumeControls();

  if (opt_fullScreenToggle) {
    this.fullscreenButton_ =
        this.createButton('fullscreen', opt_fullScreenToggle);
  }

  if (opt_stateIconParent) {
    this.stateIcon_ = this.createControl(
        'playback-state-icon', opt_stateIconParent);
    this.textBanner_ = this.createControl('text-banner', opt_stateIconParent);
  }

  var videoControls = this;
  chrome.mediaPlayerPrivate.onTogglePlayState.addListener(
      function() { videoControls.togglePlayStateWithFeedback(); });
}

/**
 * No resume if we are within this margin from the start or the end.
 */
VideoControls.RESUME_MARGIN = 0.03;

/**
 * No resume for videos shorter than this.
 */
VideoControls.RESUME_THRESHOLD = 5 * 60; // 5 min.

/**
 * When resuming rewind back this much.
 */
VideoControls.RESUME_REWIND = 5;  // seconds.

VideoControls.prototype = { __proto__: MediaControls.prototype };

/**
 * Shows icon feedback for the current state of the video player.
 * @private
 */
VideoControls.prototype.showIconFeedback_ = function() {
  this.stateIcon_.removeAttribute('state');
  setTimeout(function() {
    this.stateIcon_.setAttribute('state', this.isPlaying() ? 'play' : 'pause');
  }.bind(this), 0);
};

/**
 * Shows a text banner.
 *
 * @param {string} identifier String identifier.
 * @private
 */
VideoControls.prototype.showTextBanner_ = function(identifier) {
  this.textBanner_.removeAttribute('visible');
  this.textBanner_.textContent = this.stringFunction_(identifier);
  setTimeout(function() {
    this.textBanner_.setAttribute('visible', 'true');
  }.bind(this), 0);
};

/**
 * Toggle play/pause state on a mouse click on the play/pause button. Can be
 * called externally.
 *
 * @param {Event} event Mouse click event.
 */
VideoControls.prototype.onPlayButtonClicked = function(event) {
  if (event.ctrlKey) {
    this.toggleLoopedModeWithFeedback(true);
    if (!this.isPlaying())
      this.togglePlayState();
  } else {
    this.togglePlayState();
  }
};

/**
 * Media completion handler.
 */
VideoControls.prototype.onMediaComplete = function() {
  this.onMediaPlay_(false);  // Just update the UI.
  this.savePosition();  // This will effectively forget the position.
};

/**
 * Toggles the looped mode with feedback.
 * @param {boolean} on Whether enabled or not.
 */
VideoControls.prototype.toggleLoopedModeWithFeedback = function(on) {
  if (!this.getMedia().duration)
    return;
  this.toggleLoopedMode(on);
  if (on) {
    // TODO(mtomasz): Simplify, crbug.com/254318.
    this.showTextBanner_('GALLERY_VIDEO_LOOPED_MODE');
  }
};

/**
 * Toggles the looped mode.
 * @param {boolean} on Whether enabled or not.
 */
VideoControls.prototype.toggleLoopedMode = function(on) {
  this.getMedia().loop = on;
};

/**
 * Toggles play/pause state and flash an icon over the video.
 */
VideoControls.prototype.togglePlayStateWithFeedback = function() {
  if (!this.getMedia().duration)
    return;

  this.togglePlayState();
  this.showIconFeedback_();
};

/**
 * Toggles play/pause state.
 */
VideoControls.prototype.togglePlayState = function() {
  if (this.isPlaying()) {
    // User gave the Pause command. Save the state and reset the loop mode.
    this.toggleLoopedMode(false);
    this.savePosition();
  }
  MediaControls.prototype.togglePlayState.apply(this, arguments);
};

/**
 * Saves the playback position to the persistent storage.
 * @param {boolean=} opt_sync True if the position must be saved synchronously
 *     (required when closing app windows).
 */
VideoControls.prototype.savePosition = function(opt_sync) {
  if (!this.media_.duration ||
      this.media_.duration < VideoControls.RESUME_THRESHOLD) {
    return;
  }

  var ratio = this.media_.currentTime / this.media_.duration;
  var position;
  if (ratio < VideoControls.RESUME_MARGIN ||
      ratio > (1 - VideoControls.RESUME_MARGIN)) {
    // We are too close to the beginning or the end.
    // Remove the resume position so that next time we start from the beginning.
    position = null;
  } else {
    position = Math.floor(
        Math.max(0, this.media_.currentTime - VideoControls.RESUME_REWIND));
  }

  if (opt_sync) {
    // Packaged apps cannot save synchronously.
    // Pass the data to the background page.
    if (!window.saveOnExit)
      window.saveOnExit = [];
    window.saveOnExit.push({ key: this.media_.src, value: position });
  } else {
    util.AppCache.update(this.media_.src, position);
  }
};

/**
 * Resumes the playback position saved in the persistent storage.
 */
VideoControls.prototype.restorePlayState = function() {
  if (this.media_.duration >= VideoControls.RESUME_THRESHOLD) {
    util.AppCache.getValue(this.media_.src, function(position) {
      if (position)
        this.media_.currentTime = position;
    }.bind(this));
  }
};

/**
 * Updates style to best fit the size of the container.
 */
VideoControls.prototype.updateStyle = function() {
  // We assume that the video controls element fills the parent container.
  // This is easier than adding margins to this.container_.clientWidth.
  var width = this.container_.parentNode.clientWidth;

  // Set the margin to 5px for width >= 400, 0px for width < 160,
  // interpolate linearly in between.
  this.container_.style.margin =
      Math.ceil((Math.max(160, Math.min(width, 400)) - 160) / 48) + 'px';

  var hideBelow = function(selector, limit) {
    this.container_.querySelector(selector).style.display =
        width < limit ? 'none' : '-webkit-box';
  }.bind(this);

  hideBelow('.time', 350);
  hideBelow('.volume', 275);
  hideBelow('.volume-controls', 210);
  hideBelow('.fullscreen', 150);
};

/**
 * Creates audio controls.
 *
 * @param {HTMLElement} container Parent container.
 * @param {function(boolean)} advanceTrack Parameter: true=forward.
 * @param {function} onError Error handler.
 * @constructor
 */
function AudioControls(container, advanceTrack, onError) {
  MediaControls.call(this, container, onError);

  this.container_.classList.add('audio-controls');

  this.advanceTrack_ = advanceTrack;

  this.initPlayButton();
  this.initTimeControls(false /* no seek mark */);
  /* No volume controls */
  this.createButton('previous', this.onAdvanceClick_.bind(this, false));
  this.createButton('next', this.onAdvanceClick_.bind(this, true));

  var audioControls = this;
  chrome.mediaPlayerPrivate.onNextTrack.addListener(
      function() { audioControls.onAdvanceClick_(true); });
  chrome.mediaPlayerPrivate.onPrevTrack.addListener(
      function() { audioControls.onAdvanceClick_(false); });
  chrome.mediaPlayerPrivate.onTogglePlayState.addListener(
      function() { audioControls.togglePlayState(); });
}

AudioControls.prototype = { __proto__: MediaControls.prototype };

/**
 * Media completion handler. Advances to the next track.
 */
AudioControls.prototype.onMediaComplete = function() {
  this.advanceTrack_(true);
};

/**
 * The track position after which "previous" button acts as "restart".
 */
AudioControls.TRACK_RESTART_THRESHOLD = 5;  // seconds.

/**
 * @param {boolean} forward True if advancing forward.
 * @private
 */
AudioControls.prototype.onAdvanceClick_ = function(forward) {
  if (!forward &&
      (this.getMedia().currentTime > AudioControls.TRACK_RESTART_THRESHOLD)) {
    // We are far enough from the beginning of the current track.
    // Restart it instead of than skipping to the previous one.
    this.getMedia().currentTime = 0;
  } else {
    this.advanceTrack_(forward);
  }
};
