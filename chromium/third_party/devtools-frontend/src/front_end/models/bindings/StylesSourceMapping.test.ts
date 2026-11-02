// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';
import sinon from 'sinon';

import type * as Common from '../../core/common/common.js';
import * as Platform from '../../core/platform/platform.js';
import * as ProtocolClient from '../../core/protocol_client/protocol_client.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as Protocol from '../../generated/protocol.js';
import {setupLocaleHooks} from '../../testing/LocaleHelpers.js';
import {MockCDPConnection} from '../../testing/MockCDPConnection.js';
import {setupRuntimeHooks} from '../../testing/RuntimeHelpers.js';
import {setupSettingsHooks} from '../../testing/SettingsHelpers.js';
import {TestUniverse} from '../../testing/TestUniverse.js';
import {createFileSystemUISourceCode} from '../../testing/UISourceCodeHelpers.js';
import * as Persistence from '../persistence/persistence.js';
import * as Workspace from '../workspace/workspace.js';

import * as Bindings from './bindings.js';

const {urlString} = Platform.DevToolsPath;

describe('StylesSourceMapping', () => {
  setupLocaleHooks();
  setupSettingsHooks();
  setupRuntimeHooks();

  let universe: TestUniverse;

  beforeEach(() => {
    universe = new TestUniverse();
  });

  it('does not overwrite CSS files when CSS model reports error on getStyleSheetText', async () => {
    const connection = new MockCDPConnection();

    // Stub DOM.enable and CSS.enable to succeed
    connection.setSuccessHandler('DOM.enable', () => ({}));
    connection.setSuccessHandler('CSS.enable', () => ({}));

    // Stub CSS.setStyleSheetText to succeed
    connection.setSuccessHandler('CSS.setStyleSheetText', () => ({sourceMapURL: ''}));

    // Stub CSS.getStyleSheetText to fail
    connection.setFailureHandler('CSS.getStyleSheetText',
                                 () => ({
                                   message: 'FAKE PROTOCOL ERROR',
                                   code: ProtocolClient.CDPConnection.CDPErrorStatus.DEVTOOLS_STUB_ERROR,
                                 }));

    const target = universe.createTarget({connection});
    const cssModel = target.model(SDK.CSSModel.CSSModel);
    assert.exists(cssModel);

    // Eagerly instantiate bindings and persistence
    void universe.cssWorkspaceBinding;
    const persistence = universe.persistence;

    const styleSheetId = 'stylesheet' as Protocol.DOM.StyleSheetId;
    const frameId = 'frame' as Protocol.Page.FrameId;
    const sourceURL = urlString`http://example.com/simple.css`;

    const headerPayload: Protocol.CSS.CSSStyleSheetHeader = {
      styleSheetId,
      frameId,
      sourceURL,
      origin: Protocol.CSS.StyleSheetOrigin.Regular,
      title: 'simple.css',
      disabled: false,
      isInline: false,
      isMutable: false,
      isConstructed: false,
      loadingFailed: false,
      startLine: 0,
      startColumn: 0,
      length: 0,
      endLine: 0,
      endColumn: 0,
    };

    // Wait for the StylesSourceMapping to create the network UISourceCode
    const networkUISourceCodePromise = new Promise<Workspace.UISourceCode.UISourceCode>(resolve => {
      const listener = (event: Common.EventTarget.EventTargetEvent<Workspace.UISourceCode.UISourceCode>) => {
        if (event.data.project().type() === Workspace.Workspace.projectTypes.Network &&
            event.data.url() === sourceURL) {
          universe.workspace.removeEventListener(Workspace.Workspace.Events.UISourceCodeAdded, listener);
          resolve(event.data);
        }
      };
      universe.workspace.addEventListener(Workspace.Workspace.Events.UISourceCodeAdded, listener);
    });

    cssModel.styleSheetAdded(headerPayload);
    const networkUISourceCode = await networkUISourceCodePromise;

    // Create filesystem UISourceCode
    const fileSystemPath = urlString`file://path/to/filesystem`;
    const fileSystemFileUrl = urlString`${fileSystemPath + '/simple.css'}`;
    const origContent = 'body {\n    color: red;\n}\n';

    const {uiSourceCode: fileSystemUiSourceCode, project} = createFileSystemUISourceCode({
      url: fileSystemFileUrl,
      mimeType: 'text/css',
      content: origContent,
      fileSystemPath,
      autoMapping: true,
      type: Persistence.PlatformFileSystem.PlatformFileSystemType.WORKSPACE_PROJECT,
      universe,
    });

    // Verify binding is created.
    // Manually bind them.
    const binding = new Persistence.Persistence.PersistenceBinding(networkUISourceCode, fileSystemUiSourceCode);
    await persistence.addBindingForTest(binding);

    // Verify initial content of filesystem
    await fileSystemUiSourceCode.requestContentData();
    assert.strictEqual(fileSystemUiSourceCode.workingCopy(), origContent);

    const styleFileProto = Bindings.StylesSourceMapping.StyleFile.prototype as unknown as {
      styleFileSyncedForTest: () => void,
    };
    const syncStub = sinon.stub(styleFileProto, 'styleFileSyncedForTest');
    const syncPromise = new Promise<void>(resolve => {
      syncStub.callsFake(() => {
        resolve();
      });
    });

    // Call setStyleSheetText on CSSModel
    const setStyleSheetTextPromise = cssModel.setStyleSheetText(styleSheetId, 'body {color: blue}', true);

    // Wait for sync (which should fail and return early)
    await syncPromise;
    await setStyleSheetTextPromise;

    // Verify filesystem content is NOT changed
    assert.strictEqual(fileSystemUiSourceCode.workingCopy(), origContent);

    // Restore stubs
    syncStub.restore();
    project.dispose();
  });
});
