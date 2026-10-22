// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as SDK from '../../../../core/sdk/sdk.js';
import type * as Protocol from '../../../../generated/protocol.js';
import * as Bindings from '../../../../models/bindings/bindings.js';
import * as StackTrace from '../../../../models/stack_trace/stack_trace.js';
import * as Workspace from '../../../../models/workspace/workspace.js';
import {renderElementIntoDOM} from '../../../../testing/DOMHelpers.js';
import {createTarget} from '../../../../testing/EnvironmentHelpers.js';
import {describeWithMockConnection} from '../../../../testing/MockConnection.js';

import * as Components from './utils.js';

describeWithMockConnection('JSPresentationUtils', () => {
  function setUpEnvironment() {
    const target = createTarget();
    const workspace = Workspace.Workspace.WorkspaceImpl.instance();
    const targetManager = target.targetManager();
    const resourceMapping = new Bindings.ResourceMapping.ResourceMapping(targetManager, workspace);
    const ignoreListManager = Workspace.IgnoreListManager.IgnoreListManager.instance({forceNew: true});
    const debuggerWorkspaceBinding = Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding.instance({
      forceNew: true,
      resourceMapping,
      targetManager,
      ignoreListManager,
      workspace,
    });
    return {target, debuggerWorkspaceBinding};
  }

  async function createStackTrace(
      target: SDK.Target.Target, debuggerWorkspaceBinding: Bindings.DebuggerWorkspaceBinding.DebuggerWorkspaceBinding) {
    const url = 'https://www.google.com/script.js';
    const scriptId = '1' as Protocol.Runtime.ScriptId;
    return await debuggerWorkspaceBinding.createStackTraceFromProtocolRuntime(
        {
          callFrames: [
            {scriptId, url, lineNumber: 0, columnNumber: 10, functionName: 'foo'},
            {scriptId, url: 'bar.js', lineNumber: 1, columnNumber: 20, functionName: 'bar'},
            {scriptId, url: 'baz.js', lineNumber: 2, columnNumber: 30, functionName: 'baz'},
          ],
        },
        target);
  }

  it('renders stack trace, and re-renders on update', async () => {
    const {target, debuggerWorkspaceBinding} = setUpEnvironment();
    const stackTrace = await createStackTrace(target, debuggerWorkspaceBinding);
    const component = new Components.JSPresentationUtils.StackTracePreviewContent();
    component.options = {tabStops: false};
    component.stackTrace = stackTrace;
    await component.updateComplete;

    assert.lengthOf(component.linkElements, 3);
    assert.strictEqual(component.linkElements[0].textContent, 'www.google.com/script.js:1');

    // Modify stack trace and re-render.
    // @ts-expect-error
    stackTrace.syncFragment.frames[0].line = 100;
    stackTrace.dispatchEventToListeners(StackTrace.StackTrace.Events.UPDATED);
    await component.updateComplete;

    assert.lengthOf(component.linkElements, 3);
    assert.strictEqual(component.linkElements[0].textContent, 'www.google.com/script.js:101');
  });

  it('renders expandable stack trace', async () => {
    const {target, debuggerWorkspaceBinding} = setUpEnvironment();
    const stackTrace = await createStackTrace(target, debuggerWorkspaceBinding);

    const component = new Components.JSPresentationUtils.StackTracePreviewContent();
    component.options = {expandable: true};
    renderElementIntoDOM(component);
    assert.isFalse(component.hasContent());
    component.stackTrace = stackTrace;
    await component.updateComplete;
    assert.deepEqual(component.contentElement.deepInnerText().split('\n'), ['\tfoo\t@\twww.google.com/script.js:1']);
    const expandButton = component.contentElement.querySelector('button');
    assert.exists(expandButton);
    expandButton.click();
    await component.updateComplete;
    assert.isTrue(component.hasContent());
    assert.deepEqual(component.contentElement.deepInnerText().split('\n'), [
      '\tfoo\t@\twww.google.com/script.js:1',
      '\tbar\t@\tbar.js:2',
      '\tbaz\t@\tbaz.js:3',
    ]);
  });
});
