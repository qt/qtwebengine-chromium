// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the app API.

var GetAvailability = requireNative('v8_context').GetAvailability;
if (!GetAvailability('app').is_available) {
  exports.chromeApp = {};
  exports.onInstallStateResponse = function(){};
  return;
}

var appNatives = requireNative('app');
var process = requireNative('process');
var extensionId = process.GetExtensionId();
var logActivity = requireNative('activityLogger');

function wrapForLogging(fun) {
  if (!extensionId)
    return fun;  // nothing interesting to log without an extension

  return function() {
    // TODO(ataly): We need to make sure we use the right prototype for
    // fun.apply. Array slice can either be rewritten or similarly defined.
    logActivity.LogAPICall(extensionId, "app." + fun.name,
        $Array.slice(arguments));
    return $Function.apply(fun, this, arguments);
  };
}

// This becomes chrome.app
var app = {
  getIsInstalled: wrapForLogging(appNatives.GetIsInstalled),
  getDetails: wrapForLogging(appNatives.GetDetails),
  getDetailsForFrame: wrapForLogging(appNatives.GetDetailsForFrame),
  runningState: wrapForLogging(appNatives.GetRunningState)
};

// Tricky; "getIsInstalled" is actually exposed as the getter "isInstalled",
// but we don't have a way to express this in the schema JSON (nor is it
// worth it for this one special case).
//
// So, define it manually, and let the getIsInstalled function act as its
// documentation.
app.__defineGetter__('isInstalled', wrapForLogging(appNatives.GetIsInstalled));

// Called by app_bindings.cc.
function onInstallStateResponse(state, callbackId) {
  var callback = callbacks[callbackId];
  delete callbacks[callbackId];
  if (typeof(callback) == 'function') {
    try {
      callback(state);
    } catch (e) {
      console.error('Exception in chrome.app.installState response handler: ' +
                    e.stack);
    }
  }
}

// TODO(kalman): move this stuff to its own custom bindings.
var callbacks = {};
var nextCallbackId = 1;

app.installState = function getInstallState(callback) {
  var callbackId = nextCallbackId++;
  callbacks[callbackId] = callback;
  appNatives.GetInstallState(callbackId);
};
if (extensionId)
  app.installState = wrapForLogging(app.installState);

// This must match InstallAppBindings() in
// chrome/renderer/extensions/dispatcher.cc.
exports.chromeApp = app;
exports.onInstallStateResponse = onInstallStateResponse;
