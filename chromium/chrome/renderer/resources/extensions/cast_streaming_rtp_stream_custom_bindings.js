// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Cast Streaming RtpStream API.

var binding = require('binding').Binding.create('cast.streaming.rtpStream');
var natives = requireNative('cast_streaming_natives');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('destroy',
      function(transportId) {
        natives.DestroyCastRtpStream(transportId);
  });
  apiFunctions.setHandleRequest('getCaps',
      function(transportId) {
        return natives.GetCapsCastRtpStream(transportId);
  });
  apiFunctions.setHandleRequest('start',
      function(transportId, params) {
        natives.StartCastRtpStream(transportId, params);
  });
  apiFunctions.setHandleRequest('stop',
      function(transportId) {
        natives.StopCastRtpStream(transportId);
  });
});

exports.binding = binding.generate();
