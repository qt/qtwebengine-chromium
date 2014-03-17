// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Offline message screen implementation.
 */

login.createScreen('ErrorMessageScreen', 'error-message', function() {
  // Link which starts guest session for captive portal fixing.
  /** @const */ var FIX_CAPTIVE_PORTAL_ID = 'captive-portal-fix-link';

  /** @const */ var FIX_PROXY_SETTINGS_ID = 'proxy-settings-fix-link';

  // Id of the element which holds current network name.
  /** @const */ var CURRENT_NETWORK_NAME_ID = 'captive-portal-network-name';

  // Link which triggers frame reload.
  /** @const */ var RELOAD_PAGE_ID = 'proxy-error-signin-retry-link';

  // Array of the possible UI states of the screen. Must be in the
  // same order as ErrorScreen::UIState enum values.
  /** @const */ var UI_STATES = [
    ERROR_SCREEN_UI_STATE.UNKNOWN,
    ERROR_SCREEN_UI_STATE.UPDATE,
    ERROR_SCREEN_UI_STATE.SIGNIN,
    ERROR_SCREEN_UI_STATE.MANAGED_USER_CREATION_FLOW,
    ERROR_SCREEN_UI_STATE.KIOSK_MODE,
    ERROR_SCREEN_UI_STATE.LOCAL_STATE_ERROR
  ];

  // Possible error states of the screen.
  /** @const */ var ERROR_STATE = {
    UNKNOWN: 'error-state-unknown',
    PORTAL: 'error-state-portal',
    OFFLINE: 'error-state-offline',
    PROXY: 'error-state-proxy',
    AUTH_EXT_TIMEOUT: 'error-state-auth-ext-timeout'
  };

  // Possible error states of the screen. Must be in the same order as
  // ErrorScreen::ErrorState enum values.
  /** @const */ var ERROR_STATES = [
    ERROR_STATE.UNKNOWN,
    ERROR_STATE.PORTAL,
    ERROR_STATE.OFFLINE,
    ERROR_STATE.PROXY,
    ERROR_STATE.AUTH_EXT_TIMEOUT
  ];

  return {
    EXTERNAL_API: [
      'updateLocalizedContent',
      'onBeforeShow',
      'onBeforeHide',
      'allowGuestSignin',
      'allowOfflineLogin',
      'setUIState',
      'setErrorState'
    ],

    // Error screen initial UI state.
    ui_state_: ERROR_SCREEN_UI_STATE.UNKNOWN,

    // Error screen initial error state.
    error_state_: ERROR_STATE.UNKNOWN,

    /** @override */
    decorate: function() {
      cr.ui.DropDown.decorate($('offline-networks-list'));
      this.updateLocalizedContent();
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      $('captive-portal-message-text').innerHTML = loadTimeData.getStringF(
        'captivePortalMessage',
        '<b id="' + CURRENT_NETWORK_NAME_ID + '"></b>',
        '<a id="' + FIX_CAPTIVE_PORTAL_ID + '" class="signin-link" href="#">',
        '</a>');
      $(FIX_CAPTIVE_PORTAL_ID).onclick = function() {
        chrome.send('showCaptivePortal');
      };

      $('captive-portal-proxy-message-text').innerHTML =
        loadTimeData.getStringF(
          'captivePortalProxyMessage',
          '<a id="' + FIX_PROXY_SETTINGS_ID + '" class="signin-link" href="#">',
          '</a>');
      $(FIX_PROXY_SETTINGS_ID).onclick = function() {
        chrome.send('openProxySettings');
      };
      $('update-proxy-message-text').innerHTML = loadTimeData.getStringF(
          'updateProxyMessageText',
          '<a id="update-proxy-error-fix-proxy" class="signin-link" href="#">',
          '</a>');
      $('update-proxy-error-fix-proxy').onclick = function() {
        chrome.send('openProxySettings');
      };
      $('signin-proxy-message-text').innerHTML = loadTimeData.getStringF(
          'signinProxyMessageText',
          '<a id="' + RELOAD_PAGE_ID + '" class="signin-link" href="#">',
          '</a>',
          '<a id="signin-proxy-error-fix-proxy" class="signin-link" href="#">',
          '</a>');
      $(RELOAD_PAGE_ID).onclick = function() {
        var gaiaScreen = $(SCREEN_GAIA_SIGNIN);
        // Schedules an immediate retry.
        gaiaScreen.doReload();
      };
      $('signin-proxy-error-fix-proxy').onclick = function() {
        chrome.send('openProxySettings');
      };

      $('error-guest-signin').innerHTML = loadTimeData.getStringF(
          'guestSignin',
          '<a id="error-guest-signin-link" class="signin-link" href="#">',
          '</a>');
      $('error-guest-signin-link').onclick = function() {
        chrome.send('launchIncognito');
      };

      $('error-offline-login').innerHTML = loadTimeData.getStringF(
          'offlineLogin',
          '<a id="error-offline-login-link" class="signin-link" href="#">',
          '</a>');
      $('error-offline-login-link').onclick = function() {
        chrome.send('offlineLogin');
      };
      this.onContentChange_();
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      cr.ui.Oobe.clearErrors();
      cr.ui.DropDown.show('offline-networks-list', false);
      if (data === undefined)
        return;
      if ('uiState' in data)
        this.setUIState(data['uiState']);
      if ('errorState' in data && 'network' in data)
        this.setErrorState(data['errorState'], data['network']);
      if ('guestSigninAllowed' in data)
        this.allowGuestSignin(data['guestSigninAllowed']);
      if ('offlineLoginAllowed' in data)
        this.allowOfflineLogin(data['offlineLoginAllowed']);
    },

    /**
     * Event handler that is invoked just before the screen is hidden.
     */
    onBeforeHide: function() {
      cr.ui.DropDown.hide('offline-networks-list');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var rebootButton = this.ownerDocument.createElement('button');
      rebootButton.textContent = loadTimeData.getString('rebootButton');
      rebootButton.classList.add('show-with-ui-state-kiosk-mode');
      rebootButton.addEventListener('click', function(e) {
        chrome.send('rebootButtonClicked');
        e.stopPropagation();
      });
      buttons.push(rebootButton);

      var spacer = this.ownerDocument.createElement('div');
      spacer.classList.add('button-spacer');
      spacer.classList.add('show-with-ui-state-kiosk-mode');
      buttons.push(spacer);

      var powerwashButton = this.ownerDocument.createElement('button');
      powerwashButton.id = 'error-message-restart-and-powerwash-button';
      powerwashButton.textContent =
        loadTimeData.getString('localStateErrorPowerwashButton');
      powerwashButton.classList.add('show-with-ui-state-local-state-error');
      powerwashButton.addEventListener('click', function(e) {
        chrome.send('localStateErrorPowerwashButtonClicked');
        e.stopPropagation();
      });
      buttons.push(powerwashButton);

      return buttons;
    },

    /**
      * Sets current UI state of the screen.
      * @param {string} ui_state New UI state of the screen.
      * @private
      */
    setUIState_: function(ui_state) {
      this.classList.remove(this.ui_state);
      this.ui_state = ui_state;
      this.classList.add(this.ui_state);

      if (ui_state == ERROR_SCREEN_UI_STATE.LOCAL_STATE_ERROR) {
        // Hide header bar and progress dots, because there are no way
        // from the error screen about broken local state.
        Oobe.getInstance().headerHidden = true;
        $('progress-dots').hidden = true;
      }
      this.onContentChange_();
    },

    /**
      * Sets current error state of the screen.
      * @param {string} error_state New error state of the screen.
      * @param {string} network Name of the current network
      * @private
      */
    setErrorState_: function(error_state, network) {
      this.classList.remove(this.error_state);
      $(CURRENT_NETWORK_NAME_ID).textContent = network;
      this.error_state = error_state;
      this.classList.add(this.error_state);
      this.onContentChange_();
    },

    /* Method called after content of the screen changed.
     * @private
     */
    onContentChange_: function() {
      if (Oobe.getInstance().currentScreen === this)
        Oobe.getInstance().updateScreenSize(this);
    },

    /**
     * Prepares error screen to show guest signin link.
     * @private
     */
    allowGuestSignin: function(allowed) {
      this.classList.toggle('allow-guest-signin', allowed);
      this.onContentChange_();
    },

    /**
     * Prepares error screen to show offline login link.
     * @private
     */
    allowOfflineLogin: function(allowed) {
      this.classList.toggle('allow-offline-login', allowed);
      this.onContentChange_();
    },

    /**
      * Sets current UI state of the screen.
      * @param {number} ui_state New UI state of the screen.
      * @private
      */
    setUIState: function(ui_state) {
      this.setUIState_(UI_STATES[ui_state]);
    },

    /**
      * Sets current error state of the screen.
      * @param {number} error_state New error state of the screen.
      * @param {string} network Name of the current network
      * @private
      */
    setErrorState: function(error_state, network) {
      this.setErrorState_(ERROR_STATES[error_state], network);
    }
  };
});
