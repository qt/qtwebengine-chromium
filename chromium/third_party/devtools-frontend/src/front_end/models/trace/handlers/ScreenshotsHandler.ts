// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Helpers from '../helpers/helpers.js';
import * as Types from '../types/types.js';

import type {HandlerName} from './types.js';

let legacyScreenshotEvents: Types.Events.LegacyScreenshot[] = [];
let modernScreenshotEvents: Types.Events.Screenshot[] = [];
let syntheticScreenshots: Types.Events.LegacySyntheticScreenshot[] = [];

export function reset(): void {
  legacyScreenshotEvents = [];
  syntheticScreenshots = [];
  modernScreenshotEvents = [];
}

export function handleEvent(event: Types.Events.Event): void {
  if (Types.Events.isLegacyScreenshot(event)) {
    legacyScreenshotEvents.push(event);
  } else if (Types.Events.isScreenshot(event)) {
    modernScreenshotEvents.push(event);
  }
}

export async function finalize(): Promise<void> {
  for (const snapshotEvent of legacyScreenshotEvents) {
    const {cat, name, ph, pid, tid} = snapshotEvent;
    const syntheticEvent =
        Helpers.SyntheticEvents.SyntheticEventsManager.registerSyntheticEvent<Types.Events.LegacySyntheticScreenshot>({
          rawSourceEvent: snapshotEvent,
          cat,
          name,
          ph,
          pid,
          tid,
          // TODO(paulirish, crbug.com/41363012): fix snapshot timestamps.
          ts: snapshotEvent.ts,
          args: {
            dataUri: `data:image/jpg;base64,${snapshotEvent.args.snapshot}`,
          },
        });
    syntheticScreenshots.push(syntheticEvent);
  }
}

export function screenshotImageDataUri(event: Types.Events.LegacySyntheticScreenshot|Types.Events.Screenshot): string {
  if (Types.Events.isLegacySyntheticScreenshot(event)) {
    return event.args.dataUri;
  }
  return `data:image/jpg;base64,${event.args.snapshot}`;
}

export interface Data {
  // These are nullable because in January 2025 a CL in Chromium
  // crrev.com/c/6197645 landed which changed the format of screenshots. For a
  // given trace, it can have either "legacy" screenshot events, or "modern"
  // screenshot events, but no trace can ever contain both.
  // So, if either of these arrays are empty, we instead return `null`. This forces consumers to check the presence of the array.
  // Traces can have no screenshots if the trace category is not enabled, so it
  // is possible for a trace to return null for both of these arrays.
  legacySyntheticScreenshots: Types.Events.LegacySyntheticScreenshot[]|null;
  screenshots: Types.Events.Screenshot[]|null;
}

export function data(): Data {
  return {
    legacySyntheticScreenshots: syntheticScreenshots.length ? syntheticScreenshots : null,
    screenshots: modernScreenshotEvents.length ? modernScreenshotEvents : null,
  };
}

export function deps(): HandlerName[] {
  return ['Meta'];
}
