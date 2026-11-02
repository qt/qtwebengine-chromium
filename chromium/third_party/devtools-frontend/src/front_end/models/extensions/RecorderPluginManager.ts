// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as Root from '../../core/root/root.js';

import type {RecorderExtensionEndpoint} from './RecorderExtensionEndpoint.js';

export interface ViewDescriptor {
  id: string;
  title: string;
  pagePath: string;
  onShown: () => void;
  onHidden: () => void;
  extensionOrigin: string;
}

export class RecorderPluginManager extends Common.ObjectWrapper.ObjectWrapper<EventTypes> {
  #plugins = new Set<RecorderExtensionEndpoint>();
  #views = new Map<string, ViewDescriptor>();

  static instance(opts?: {forceNew: boolean}): RecorderPluginManager {
    if (!Root.DevToolsContext.globalInstance().has(RecorderPluginManager) || opts?.forceNew) {
      Root.DevToolsContext.globalInstance().set(RecorderPluginManager, new RecorderPluginManager());
    }

    return Root.DevToolsContext.globalInstance().get(RecorderPluginManager);
  }

  static removeInstance(): void {
    Root.DevToolsContext.globalInstance().delete(RecorderPluginManager);
  }

  addPlugin(plugin: RecorderExtensionEndpoint): void {
    this.#plugins.add(plugin);
    this.dispatchEventToListeners(Events.PLUGIN_ADDED, plugin);
  }

  removePlugin(plugin: RecorderExtensionEndpoint): void {
    this.#plugins.delete(plugin);
    this.dispatchEventToListeners(Events.PLUGIN_REMOVED, plugin);
  }

  plugins(): RecorderExtensionEndpoint[] {
    return Array.from(this.#plugins.values());
  }

  registerView(descriptor: ViewDescriptor): void {
    this.#views.set(descriptor.id, descriptor);
    this.dispatchEventToListeners(Events.VIEW_REGISTERED, descriptor);
  }

  views(): ViewDescriptor[] {
    return Array.from(this.#views.values());
  }

  getViewDescriptor(id: string): ViewDescriptor|undefined {
    return this.#views.get(id);
  }

  showView(id: string): void {
    const descriptor = this.#views.get(id);
    if (!descriptor) {
      throw new Error(`View with id ${id} is not found.`);
    }
    this.dispatchEventToListeners(Events.SHOW_VIEW_REQUESTED, descriptor);
  }
}

export const enum Events {
  PLUGIN_ADDED = 'pluginAdded',
  PLUGIN_REMOVED = 'pluginRemoved',
  VIEW_REGISTERED = 'viewRegistered',
  SHOW_VIEW_REQUESTED = 'showViewRequested',
}

export interface EventTypes {
  [Events.PLUGIN_ADDED]: RecorderExtensionEndpoint;
  [Events.PLUGIN_REMOVED]: RecorderExtensionEndpoint;
  [Events.VIEW_REGISTERED]: ViewDescriptor;
  [Events.SHOW_VIEW_REQUESTED]: ViewDescriptor;
}
