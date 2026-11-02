// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Foundation from '../../foundation/foundation.js';
import * as UI from '../../ui/legacy/legacy.js';

export class SimpleApp implements UI.App.App {
  readonly #universe: Foundation.Universe.Universe;
  constructor(universe: Foundation.Universe.Universe) {
    this.#universe = universe;
  }

  presentUI(document: Document): void {
    const rootView = new UI.RootView.RootView(this.#universe);
    UI.InspectorView.InspectorView.instance().show(rootView.element);
    rootView.attachToDocument(document);
    rootView.focus();
  }
}

export class SimpleAppProvider implements UI.AppProvider.AppProvider {
  createApp(universe: Foundation.Universe.Universe): UI.App.App {
    return new SimpleApp(universe);
  }
}
