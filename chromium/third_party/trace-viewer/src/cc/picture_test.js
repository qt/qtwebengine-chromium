// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

base.require('cc');
base.require('cc.picture');
base.require('cc.layer_tree_host_impl_test_data');
base.require('tracing.importer.trace_event_importer');
base.require('tracing.trace_model');

base.unittest.testSuite('cc.picture', function() {
  test('basic', function() {
    var m = new tracing.TraceModel(g_catLTHIEvents);
    var p = base.dictionaryValues(m.processes)[0];

    var instance = p.objects.getAllInstancesNamed('cc::Picture')[0];
    var snapshot = instance.snapshots[0];

    assertTrue(snapshot instanceof cc.PictureSnapshot);
    instance.wasDeleted(150);
  });

  test('getOps', function() {
    var m = new tracing.TraceModel(g_catLTHIEvents);
    var p = base.dictionaryValues(m.processes)[0];

    var instance = p.objects.getAllInstancesNamed('cc::Picture')[0];
    var snapshot = instance.snapshots[0];

    var ops = snapshot.getOps();
    if (!ops)
      return;
    assertEquals(627, ops.length);

    var op0 = ops[0];
    assertEquals('Save', op0.cmd_string);
    assertTrue(op0.info instanceof Array);
  });
});
