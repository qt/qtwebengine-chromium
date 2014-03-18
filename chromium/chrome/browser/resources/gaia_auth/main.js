// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Authenticator class wraps the communications between Gaia and its host.
 */
function Authenticator() {
}

/**
 * Gaia auth extension url origin.
 * @type {string}
 */
Authenticator.THIS_EXTENSION_ORIGIN =
    'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik';

/**
 * Singleton getter of Authenticator.
 * @return {Object} The singleton instance of Authenticator.
 */
Authenticator.getInstance = function() {
  if (!Authenticator.instance_) {
    Authenticator.instance_ = new Authenticator();
  }
  return Authenticator.instance_;
};

Authenticator.prototype = {
  email_: null,
  password_: null,
  attemptToken_: null,

  // Input params from extension initialization URL.
  inputLang_: undefined,
  intputEmail_: undefined,

  samlPageLoaded_: false,
  samlSupportChannel_: null,

  GAIA_URL: 'https://accounts.google.com/',
  GAIA_PAGE_PATH: 'ServiceLogin?skipvpage=true&sarp=1&rm=hide',
  PARENT_PAGE: 'chrome://oobe/',
  SERVICE_ID: 'chromeoslogin',
  CONTINUE_URL: Authenticator.THIS_EXTENSION_ORIGIN + '/success.html',

  initialize: function() {
    var params = getUrlSearchParams(location.search);
    this.parentPage_ = params.parentPage || this.PARENT_PAGE;
    this.gaiaUrl_ = params.gaiaUrl || this.GAIA_URL;
    this.gaiaPath_ = params.gaiaPath || this.GAIA_PAGE_PATH;
    this.inputLang_ = params.hl;
    this.inputEmail_ = params.email;
    this.service_ = params.service || this.SERVICE_ID;
    this.continueUrl_ = params.continueUrl || this.CONTINUE_URL;
    this.continueUrlWithoutParams_ = stripParams(this.continueUrl_);
    this.inlineMode_ = params.inlineMode == '1';
    this.constrained_ = params.constrained == '1';
    this.partitionId_ = params.partitionId || '';
    this.initialFrameUrl_ = params.frameUrl || this.constructInitialFrameUrl_();
    this.initialFrameUrlWithoutParams_ = stripParams(this.initialFrameUrl_);
    this.loaded_ = false;

    document.addEventListener('DOMContentLoaded', this.onPageLoad.bind(this));
    document.addEventListener('enableSAML', this.onEnableSAML_.bind(this));
  },

  isGaiaMessage_: function(msg) {
    // Not quite right, but good enough.
    return this.gaiaUrl_.indexOf(msg.origin) == 0 ||
           this.GAIA_URL.indexOf(msg.origin) == 0;
  },

  isInternalMessage_: function(msg) {
    return msg.origin == Authenticator.THIS_EXTENSION_ORIGIN;
  },

  isParentMessage_: function(msg) {
    return msg.origin == this.parentPage_;
  },

  constructInitialFrameUrl_: function() {
    var url = this.gaiaUrl_ + this.gaiaPath_;

    url = appendParam(url, 'service', this.service_);
    url = appendParam(url, 'continue', this.continueUrl_);
    if (this.inputLang_)
      url = appendParam(url, 'hl', this.inputLang_);
    if (this.inputEmail_)
      url = appendParam(url, 'Email', this.inputEmail_);

    return url;
  },

  /** Callback when all loads in the gaia webview is complete. */
  onWebviewLoadstop_: function(gaiaFrame) {
    // Report the current state to the parent which will then update the
    // browser history so that later it could respond properly to back/forward.
    var msg = {
      'method': 'reportState',
      'src': gaiaFrame.src
    };
    window.parent.postMessage(msg, this.parentPage_);

    if (gaiaFrame.src.lastIndexOf(
        this.continueUrlWithoutParams_, 0) == 0) {
      // Detect when login is finished by the load stop event of the continue
      // URL. Cannot reuse the login complete flow in success.html, because
      // webview does not support extension pages yet.
      gaiaFrame.hidden = true;
      msg = {'method': 'completeLogin'};
      window.parent.postMessage(msg, this.parentPage_);
      return;
    }

    if (gaiaFrame.src.lastIndexOf(this.gaiaUrl_, 0) == 0) {
      gaiaFrame.executeScript({file: 'inline_injected.js'}, function() {
        // Send an initial message to gaia so that it has an JavaScript
        // reference to the embedder.
        gaiaFrame.contentWindow.postMessage('', gaiaFrame.src);
      });
    }

    this.loaded_ || this.onLoginUILoaded();
  },

  /**
   * Callback when the gaia webview attempts to open a new window.
   */
  onWebviewNewWindow_: function(gaiaFrame, e) {
    window.open(e.targetUrl, '_blank');
    e.window.discard();
  },

  onWebviewRequestCompleted_: function(details) {
    if (details.url.lastIndexOf(this.continueUrlWithoutParams_, 0) == 0) {
      return;
    }

    var headers = details.responseHeaders;
    for (var i = 0; headers && i < headers.length; ++i) {
      if (headers[i].name.toLowerCase() == 'google-accounts-embedded') {
        return;
      }
    }
    var msg = {
      'method': 'switchToFullTab',
      'url': details.url
    };
    window.parent.postMessage(msg, this.parentPage_);
  },

  loadFrame_: function() {
    var gaiaFrame = $('gaia-frame');
    gaiaFrame.partition = this.partitionId_;
    gaiaFrame.src = this.initialFrameUrl_;
    if (this.inlineMode_) {
      gaiaFrame.addEventListener(
          'loadstop', this.onWebviewLoadstop_.bind(this, gaiaFrame));
      gaiaFrame.addEventListener(
          'newwindow', this.onWebviewNewWindow_.bind(this, gaiaFrame));
    }
    if (this.constrained_) {
      gaiaFrame.request.onCompleted.addListener(
          this.onWebviewRequestCompleted_.bind(this),
          {urls: ['<all_urls>'], types: ['main_frame']},
          ['responseHeaders']);
    }
  },

  completeLogin: function(username, password) {
    var msg = {
      'method': 'completeLogin',
      'email': username,
      'password': password
    };
    window.parent.postMessage(msg, this.parentPage_);
    if (this.samlSupportChannel_)
      this.samlSupportChannel_.send({name: 'resetAuth'});
  },

  onPageLoad: function(e) {
    window.addEventListener('message', this.onMessage.bind(this), false);
    this.loadFrame_();
  },

  /**
   * Invoked when 'enableSAML' event is received to initialize SAML support.
   */
  onEnableSAML_: function() {
    this.samlPageLoaded_ = false;

    this.samlSupportChannel_ = new Channel();
    this.samlSupportChannel_.connect('authMain');
    this.samlSupportChannel_.registerMessage(
        'onAuthPageLoaded', this.onAuthPageLoaded_.bind(this));
    this.samlSupportChannel_.send({
      name: 'setGaiaUrl',
      gaiaUrl: this.gaiaUrl_
    });
  },

  /**
   * Invoked when the background page sends 'onHostedPageLoaded' message.
   * @param {!Object} msg Details sent with the message.
   */
  onAuthPageLoaded_: function(msg) {
    this.samlPageLoaded_ = msg.url.indexOf(this.gaiaUrl_) != 0;
    window.parent.postMessage({
      'method': 'authPageLoaded',
      'isSAML': this.samlPageLoaded_
    }, this.parentPage_);
  },

  onLoginUILoaded: function() {
    var msg = {
      'method': 'loginUILoaded'
    };
    window.parent.postMessage(msg, this.parentPage_);
    if (this.inlineMode_) {
      $('gaia-frame').focus();
    }
    this.loaded_ = true;
  },

  onConfirmLogin_: function() {
    if (!this.samlPageLoaded_) {
      this.completeLogin(this.email_, this.password_);
      return;
    }

    this.samlSupportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          if (passwords.length == 0) {
            window.parent.postMessage(
                {method: 'noPassword', email: this.email_},
                this.parentPage_);
          } else {
            window.parent.postMessage(
                {method: 'confirmPassword', email: this.email_},
                this.parentPage_);
          }
        }.bind(this));
  },

  onVerifyConfirmedPassword_: function(password) {
    this.samlSupportChannel_.sendWithCallback(
        {name: 'getScrapedPasswords'},
        function(passwords) {
          for (var i = 0; i < passwords.length; ++i) {
            if (passwords[i] == password) {
              this.completeLogin(this.email_, passwords[i]);
              return;
            }
          }
          window.parent.postMessage(
              {method: 'confirmPassword', email: this.email_},
              this.parentPage_);
        }.bind(this));
  },

  onMessage: function(e) {
    var msg = e.data;
    if (msg.method == 'attemptLogin' && this.isGaiaMessage_(e)) {
      this.email_ = msg.email;
      this.password_ = msg.password;
      this.attemptToken_ = msg.attemptToken;
      this.samlPageLoaded_ = false;
      if (this.samlSupportChannel_)
        this.samlSupportChannel_.send({name: 'startAuth'});
    } else if (msg.method == 'clearOldAttempts' && this.isGaiaMessage_(e)) {
      this.email_ = null;
      this.password_ = null;
      this.attemptToken_ = null;
      this.samlPageLoaded_ = false;
      this.onLoginUILoaded();
      if (this.samlSupportChannel_)
        this.samlSupportChannel_.send({name: 'resetAuth'});
    } else if (msg.method == 'confirmLogin' && this.isInternalMessage_(e)) {
      if (this.attemptToken_ == msg.attemptToken)
        this.onConfirmLogin_();
      else
        console.error('Authenticator.onMessage: unexpected attemptToken!?');
    } else if (msg.method == 'verifyConfirmedPassword' &&
               this.isParentMessage_(e)) {
      this.onVerifyConfirmedPassword_(msg.password);
    } else if (msg.method == 'navigate' &&
               this.isParentMessage_(e)) {
       $('gaia-frame').src = msg.src;
    } else {
      console.error('Authenticator.onMessage: unknown message + origin!?');
    }
  }
};

Authenticator.getInstance().initialize();
