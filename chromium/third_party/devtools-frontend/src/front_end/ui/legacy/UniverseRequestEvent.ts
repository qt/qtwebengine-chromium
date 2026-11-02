// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as Foundation from '../../foundation/foundation.js';

export class UniverseRequestEvent extends Event {
  static readonly eventName = 'universerequest';

  /**
   * The `Universe` will be filled in by the `RootView` in the event handler.
   * Widget.ts dispatches a new UniverseRequestEvent, and retrieves the Universe from the event right after.
   */
  universe?: Foundation.Universe.Universe;

  constructor() {
    super(UniverseRequestEvent.eventName, {bubbles: true, composed: true});
  }
}
