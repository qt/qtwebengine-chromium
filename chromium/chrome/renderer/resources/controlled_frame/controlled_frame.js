// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements chrome-specific <controlledframe> Element.

var ControlledFrameImpl = require('controlledFrameImpl').ControlledFrameImpl;
var forwardApiMethods = require('guestViewContainerElement').forwardApiMethods;
var registerElement = require('guestViewContainerElement').registerElement;
var WebViewAttributeNames = require('webViewConstants').WebViewAttributeNames;
var WebViewElement = require('webViewElement').WebViewElement;
var WebViewInternal = getInternalApi('webViewInternal');
var WEB_VIEW_API_METHODS = require('webViewApiMethods').WEB_VIEW_API_METHODS;

class ControlledFrameElement extends WebViewElement {
  static get observedAttributes() {
    return WebViewAttributeNames;
  }

  constructor() {
    super();
    privates(this).internal = new ControlledFrameImpl(this);
    privates(this).originalGo = originalGo;
  }
}

// Forward remaining ControlledFrameElement.foo* method calls to
// ChromeWebViewImpl.foo* or WebViewInternal.foo*.
forwardApiMethods(
    ControlledFrameElement, ControlledFrameImpl, WebViewInternal,
    WEB_VIEW_API_METHODS);

// Since |back| and |forward| are implemented in terms of |go|, we need to
// keep a reference to the real |go| function, since user code may override
// ControlledFrameElement.prototype.go|.
var originalGo = ControlledFrameElement.prototype.go;

registerElement('ControlledFrame', ControlledFrameElement);
