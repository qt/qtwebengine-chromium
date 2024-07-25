// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotNullOrUndefined} from '../../core/platform/platform.js';

import {type Loggable} from './Loggable.js';
import {type LoggingConfig, VisualElements} from './LoggingConfig.js';
import {getLoggingState, type LoggingState} from './LoggingState.js';

let veDebuggingEnabled = false;
let debugPopover: HTMLElement|null = null;
const nonDomDebugElements = new WeakMap<Loggable, HTMLElement>();

function setVeDebuggingEnabled(enabled: boolean): void {
  veDebuggingEnabled = enabled;
  if (enabled && !debugPopover) {
    debugPopover = document.createElement('div');
    debugPopover.classList.add('ve-debug');
    debugPopover.style.position = 'absolute';
    debugPopover.style.bottom = '100px';
    debugPopover.style.left = '100px';
    debugPopover.style.background = 'black';
    debugPopover.style.color = 'white';
    debugPopover.style.zIndex = '100000';
    document.body.appendChild(debugPopover);
  }
}

// @ts-ignore
globalThis.setVeDebuggingEnabled = setVeDebuggingEnabled;

export function processForDebugging(loggable: Loggable): void {
  const loggingState = getLoggingState(loggable);
  if (!veDebuggingEnabled || !loggingState || loggingState.processedForDebugging) {
    return;
  }
  if (loggable instanceof Element) {
    processElementForDebugging(loggable, loggingState);
  } else {
    processNonDomLoggableForDebugging(loggable, loggingState);
  }
}

function showDebugPopover(content: string): void {
  if (!debugPopover) {
    return;
  }
  debugPopover.style.display = 'block';
  debugPopover.innerHTML = content;
}

function processElementForDebugging(element: Element, loggingState: LoggingState): void {
  if (element.tagName === 'OPTION') {
    if (loggingState.parent?.selectOpen && debugPopover) {
      debugPopover.innerHTML += '<br>' + debugString(loggingState.config);
      loggingState.processedForDebugging = true;
    }
  } else {
    (element as HTMLElement).style.outline = 'solid 1px red';
    element.addEventListener('mouseenter', () => {
      assertNotNullOrUndefined(debugPopover);
      const pathToRoot = [loggingState];
      let ancestor = loggingState.parent;
      while (ancestor) {
        pathToRoot.push(ancestor);
        ancestor = ancestor.parent;
      }
      showDebugPopover(pathToRoot.map(s => debugString(s.config)).join('<br>'));
    }, {capture: true});
    element.addEventListener('mouseleave', () => {
      assertNotNullOrUndefined(debugPopover);
      debugPopover.style.display = 'none';
    }, {capture: true});
    loggingState.processedForDebugging = true;
  }
}

export function processEventForDebugging(event: string, state: LoggingState|null, extraInfo?: Entry): void {
  const veDebugLoggingEnabled = localStorage.getItem('veDebugLoggingEnabled');
  if (!veDebuggingEnabled && !veDebugLoggingEnabled) {
    return;
  }

  const entry: Entry =
      {event, ve: state ? VisualElements[state?.config.ve] : undefined, context: state?.config.context, ...extraInfo};
  for (const stringKey in entry) {
    const key = stringKey as keyof Entry;
    if (typeof entry[key] === 'undefined') {
      delete entry[key];
    }
  }

  if (veDebuggingEnabled) {
    showDebugPopover(`${Object.entries(entry).map(([k, v]) => `${k}: ${v}`).join('; ')}`);
  }
  const time = Date.now() - sessionStartTime;
  maybeLogDebugEvent({...entry, veid: state?.veid, time});
}

type Entry = {
  event?: string,
  ve?: string,
  context?: string,
  veid?: number,
  children?: Entry[],
  parent?: number,
  time?: number,
  width?: number,
  height?: number,
  mouseButton?: number,
  doubleClick?: boolean,
};

export function processImpressionsForDebugging(states: LoggingState[]): void {
  if (!localStorage.getItem('veDebugLoggingEnabled')) {
    return;
  }
  const impressions = new Map<number, Entry>();
  for (const state of states) {
    const entry: Entry = {
      event: 'Impression',
      ve: VisualElements[state.config.ve],
    };
    if (state.config.context) {
      entry.context = state.config.context;
    }
    if (state.size) {
      entry.width = state.size.width;
      entry.height = state.size.height;
    }
    entry.veid = state.veid,

    impressions.set(state.veid, entry);
    if (!state.parent || !impressions.has(state.parent?.veid)) {
      entry.parent = state.parent?.veid;
    } else {
      const parent = impressions.get(state.parent?.veid) as Entry;
      parent.children = parent.children || [];
      parent.children.push(entry);
    }
  }

  const entries = [...impressions.values()].filter(i => 'parent' in i);
  if (entries.length === 1) {
    entries[0].time = Date.now() - sessionStartTime;
    maybeLogDebugEvent(entries[0]);
  } else {
    maybeLogDebugEvent({event: 'Impression', children: entries, time: Date.now() - sessionStartTime});
  }
}

function processNonDomLoggableForDebugging(loggable: Loggable, loggingState: LoggingState): void {
  let debugElement = nonDomDebugElements.get(loggable);
  if (!debugElement) {
    debugElement = document.createElement('div');
    debugElement.classList.add('ve-debug');
    debugElement.style.background = 'black';
    debugElement.style.color = 'white';
    debugElement.style.zIndex = '100000';
    debugElement.textContent = debugString(loggingState.config);
    nonDomDebugElements.set(loggable, debugElement);
    setTimeout(() => {
      if (!loggingState.size?.width || !loggingState.size?.height) {
        debugElement?.parentElement?.removeChild(debugElement);
        nonDomDebugElements.delete(loggable);
      }
    }, 10000);
  }
  const parentDebugElement =
      parent instanceof HTMLElement ? parent : nonDomDebugElements.get(parent as Loggable) || debugPopover;
  assertNotNullOrUndefined(parentDebugElement);
  if (!parentDebugElement.classList.contains('ve-debug')) {
    debugElement.style.position = 'absolute';
    parentDebugElement.insertBefore(debugElement, parentDebugElement.firstChild);
  } else {
    debugElement.style.marginLeft = '10px';
    parentDebugElement.appendChild(debugElement);
  }
}

export function debugString(config: LoggingConfig): string {
  const components = [VisualElements[config.ve]];
  if (config.context) {
    components.push(`context: ${config.context}`);
  }
  if (config.parent) {
    components.push(`parent: ${config.parent}`);
  }
  if (config.track) {
    components.push(`track: ${
        Object.entries(config.track)
            .map(([key, value]) => `${key}${typeof value === 'string' ? `: ${value}` : ''}`)
            .join(', ')}`);
  }
  return components.join('; ');
}

const veDebugEventsLog: Entry[] = [];

function maybeLogDebugEvent(entry: Entry): void {
  if (!localStorage.getItem('veDebugLoggingEnabled')) {
    return;
  }
  veDebugEventsLog.push(entry);
  // eslint-disable-next-line no-console
  console.info('VE Debug:', entry);
}

function setVeDebugLoggingEnabled(enabled: boolean): void {
  if (enabled) {
    localStorage.setItem('veDebugLoggingEnabled', 'true');
  } else {
    localStorage.removeItem('veDebugLoggingEnabled');
  }
}

function findVeDebugImpression(veid: number, includeAncestorChain?: boolean): Entry|undefined {
  const findImpression = (entry: Entry): Entry|undefined => {
    if (entry.event === 'Impression' && entry.veid === veid) {
      return entry;
    }
    let i = 0;
    for (const childEntry of entry.children || []) {
      const matchingEntry = findImpression(childEntry);
      if (matchingEntry) {
        if (includeAncestorChain) {
          const children = [];
          children[i] = matchingEntry;
          return {...entry, children};
        }
        return matchingEntry;
      }
      ++i;
    }
    return undefined;
  };
  return findImpression({children: veDebugEventsLog});
}

let sessionStartTime: number = Date.now();

export function processStartLoggingForDebugging(): void {
  sessionStartTime = Date.now();
  maybeLogDebugEvent({event: 'SessionStart'});
}

// @ts-ignore
globalThis.setVeDebugLoggingEnabled = setVeDebugLoggingEnabled;
// @ts-ignore
globalThis.veDebugEventsLog = veDebugEventsLog;
// @ts-ignore
globalThis.findVeDebugImpression = findVeDebugImpression;
