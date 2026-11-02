// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {TSESTree} from '@typescript-eslint/utils';

import {createRule} from './utils/ruleCreator.ts';

const DISALLOWED_CLASSES = new Set([
  // go/keep-sorted start
  'AutofillManager',
  'AutomaticFileSystemManager',
  'AutomaticFileSystemWorkspaceBinding',
  'BreakpointManager',
  'CPUThrottlingManager',
  'CSSWorkspaceBinding',
  'Console',
  'CrUXManager',
  'DOMDebuggerManager',
  'DOMModelUndoStack',
  'DebuggerWorkspaceBinding',
  'DeviceModeModel',
  'EmulatedDevicesList',
  'EventBreakpointsManager',
  'FileManager',
  'FileSystemWorkspaceBinding',
  'FrameManager',
  'GdpClient',
  'HostConfigTracker',
  'IgnoreListManager',
  'IsolateManager',
  'IsolatedFileSystemManager',
  'JavaScriptMetadataImpl',
  'LiveMetrics',
  'LogManager',
  'MultitargetNetworkManager',
  'NetworkLog',
  'NetworkPersistenceManager',
  'NetworkProjectManager',
  'PageResourceLoader',
  'PersistenceImpl',
  'ProjectSettingsModel',
  'ResourceMapping',
  'Settings',
  'TargetManager',
  'WorkspaceDiffImpl',
  'WorkspaceImpl',
  // go/keep-sorted end
]);

function getClassName(node: TSESTree.Expression): string|null {
  if (node.type === 'Identifier') {
    return node.name;
  }
  if (node.type === 'MemberExpression') {
    if (node.property.type === 'Identifier') {
      return node.property.name;
    }
  }
  return null;
}

export default createRule({
  name: 'no-instance-of-migrated-singletons',
  meta: {
    type: 'problem',
    docs: {
      description: 'Prevent calls to static instance methods for migrated singletons',
      category: 'Possible Errors',
    },
    messages: {
      noInstanceCall:
          'Do not call {{className}}.instance(). Use constructor injection or retrieve it from the Universe/context instead.',
    },
    schema: [],
  },
  defaultOptions: [],
  create(context) {
    return {
      CallExpression(node) {
        if (node.callee.type !== 'MemberExpression') {
          return;
        }
        const memberExpr = node.callee;
        if (memberExpr.property.type !== 'Identifier' || memberExpr.property.name !== 'instance') {
          return;
        }

        const className = getClassName(memberExpr.object);
        if (className && DISALLOWED_CLASSES.has(className)) {
          context.report({
            node,
            messageId: 'noInstanceCall',
            data: {
              className,
            },
          });
        }
      },
    };
  },
});
