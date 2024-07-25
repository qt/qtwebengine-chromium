// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';
import * as Platform from '../../core/platform/platform.js';
import * as Root from '../../core/root/root.js';
import * as SDK from '../../core/sdk/sdk.js';
import type * as Protocol from '../../generated/protocol.js';
import * as Bindings from '../../models/bindings/bindings.js';
import * as TimelineModel from '../../models/timeline_model/timeline_model.js';
import * as TraceEngine from '../../models/trace/trace.js';
import * as AnnotationsManager from '../../services/annotations_manager/annotations_manager.js';
import * as TraceBounds from '../../services/trace_bounds/trace_bounds.js';
import * as CodeHighlighter from '../../ui/components/code_highlighter/code_highlighter.js';
// eslint-disable-next-line rulesdir/es_modules_import
import codeHighlighterStyles from '../../ui/components/code_highlighter/codeHighlighter.css.js';
import * as PerfUI from '../../ui/legacy/components/perf_ui/perf_ui.js';
// eslint-disable-next-line rulesdir/es_modules_import
import imagePreviewStyles from '../../ui/legacy/components/utils/imagePreview.css.js';
import * as LegacyComponents from '../../ui/legacy/components/utils/utils.js';
// eslint-disable-next-line rulesdir/es_modules_import
import inspectorCommonStyles from '../../ui/legacy/inspectorCommon.css.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as ThemeSupport from '../../ui/legacy/theme_support/theme_support.js';

import {CLSRect} from './CLSLinkifier.js';
import * as TimelineComponents from './components/components.js';
import {
  type CategoryPalette,
  getCategoryStyles,
  getEventStyle,
  maybeInitSylesMap,
  type TimelineCategory,
  TimelineRecordStyle,
  visibleTypes,
} from './EventUICategory.js';
import * as Extensions from './extensions/extensions.js';
import {Tracker} from './FreshRecording.js';
import {titleForInteractionEvent} from './InteractionsTrackAppender.js';
import {SourceMapsResolver} from './SourceMapsResolver.js';
import {targetForEvent} from './TargetForEvent.js';
import {TimelinePanel} from './TimelinePanel.js';
import {TimelineSelection} from './TimelineSelection.js';

const UIStrings = {
  /**
   *@description Text that only contain a placeholder
   *@example {100ms (at 200ms)} PH1
   */
  emptyPlaceholder: '{PH1}',  // eslint-disable-line rulesdir/l10n_no_locked_or_placeholder_only_phrase
  /**
   *@description Text that refers to updated priority of network request
   */
  initialPriority: 'Initial Priority',
  /**
   *@description Text for timestamps of items
   */
  timestamp: 'Timestamp',
  /**
   *@description Text shown next to the interaction event's ID in the detail view.
   */
  interactionID: 'ID',
  /**
   *@description Text shown next to the interaction event's input delay time in the detail view.
   */
  inputDelay: 'Input delay',
  /**
   *@description Text shown next to the interaction event's thread processing duration in the detail view.
   */
  processingDuration: 'Processing duration',
  /**
   *@description Text shown next to the interaction event's presentation delay time in the detail view.
   */
  presentationDelay: 'Presentation delay',
  /**
   *@description Text shown when the user has selected an event that represents script compiliation.
   */
  compile: 'Compile',
  /**
   *@description Text shown when the user selects an event that represents script parsing.
   */
  parse: 'Parse',
  /**
   *@description Text with two placeholders separated by a colon
   *@example {Node removed} PH1
   *@example {div#id1} PH2
   */
  sS: '{PH1}: {PH2}',
  /**
   *@description Details text used to show the amount of data collected.
   *@example {30 MB} PH1
   */
  sCollected: '{PH1} collected',
  /**
   *@description Text used to show a URL to a script and the relevant line numbers.
   *@example {https://example.com/foo.js} PH1
   *@example {2} PH2
   *@example {4} PH3
   */
  sSs: '{PH1} [{PH2}…{PH3}]',
  /**
   *@description Text used to show a URL to a script and the starting line
   *             number - used when there is no end line number available.
   *@example {https://example.com/foo.js} PH1
   *@example {2} PH2
   */
  sSSquareBrackets: '{PH1} [{PH2}…]',
  /**
   *@description Text that is usually a hyperlink to more documentation
   */
  learnMore: 'Learn more',
  /**
   *@description Text referring to the status of the browser's compilation cache.
   */
  compilationCacheStatus: 'Compilation cache status',
  /**
   *@description Text referring to the size of the browser's compiliation cache.
   */
  compilationCacheSize: 'Compilation cache size',
  /**
   *@description Text in Timeline UIUtils of the Performance panel. "Compilation
   * cache" refers to the code cache described at
   * https://v8.dev/blog/code-caching-for-devs . This label is followed by the
   * type of code cache data used, either "normal" or "full" as described in the
   * linked article.
   */
  compilationCacheKind: 'Compilation cache kind',
  /**
   *@description Text used to inform the user that the script they are looking
   *             at was loaded from the browser's cache.
   */
  scriptLoadedFromCache: 'script loaded from cache',
  /**
   *@description Text to inform the user that the script they are looking at
   *             was unable to be loaded from the browser's cache.
   */
  failedToLoadScriptFromCache: 'failed to load script from cache',
  /**
   *@description Text to inform the user that the script they are looking at was not eligible to be loaded from the browser's cache.
   */
  scriptNotEligibleToBeLoadedFromCache: 'script not eligible',
  /**
   *@description Text for the total time of something
   */
  totalTime: 'Total Time',
  /**
   *@description Time of a single activity, as opposed to the total time
   */
  selfTime: 'Self Time',
  /**
   *@description Label in the summary view in the Performance panel for a number which indicates how much managed memory has been reclaimed by performing Garbage Collection
   */
  collected: 'Collected',
  /**
   *@description Text for a programming function
   */
  function: 'Function',
  /**
   *@description Text for referring to the ID of a timer.
   */
  timerId: 'Timer ID',
  /**
   *@description Text for referring to a timer that has timed-out and therefore is being removed.
   */
  timeout: 'Timeout',
  /**
   *@description Text used to indicate that a timer is repeating (e.g. every X seconds) rather than a one off.
   */
  repeats: 'Repeats',
  /**
   *@description Text for referring to the ID of a callback function installed by an event.
   */
  callbackId: 'Callback ID',
  /**
   *@description Text that refers to the network request method
   */
  requestMethod: 'Request Method',
  /**
   *@description Text to show the priority of an item
   */
  priority: 'Priority',
  /**
   *@description Text used when referring to the data sent in a network request that is encoded as a particular file format.
   */
  encodedData: 'Encoded Data',
  /**
   *@description Text used to refer to the data sent in a network request that has been decoded.
   */
  decodedBody: 'Decoded Body',
  /**
   *@description Text for a module, the programming concept
   */
  module: 'Module',
  /**
   *@description Label for a group of JavaScript files
   */
  script: 'Script',
  /**
   *@description Text used to tell a user that a compilation trace event was streamed.
   */
  streamed: 'Streamed',
  /**
   *@description Text to indicate if a compilation event was eager.
   */
  eagerCompile: 'Compiling all functions eagerly',
  /**
   *@description Text to refer to the URL associated with a given event.
   */
  url: 'Url',
  /**
   *@description Text to indicate to the user the size of the cache (as a filesize - e.g. 5mb).
   */
  producedCacheSize: 'Produced Cache Size',
  /**
   *@description Text to indicate to the user the amount of the cache (as a filesize - e.g. 5mb) that has been used.
   */
  consumedCacheSize: 'Consumed Cache Size',
  /**
   *@description Title for a group of cities
   */
  location: 'Location',
  /**
   *@description Text used to show a coordinate pair (e.g. (3, 2)).
   *@example {2} PH1
   *@example {2} PH2
   */
  sSCurlyBrackets: '({PH1}, {PH2})',
  /**
   *@description Text used to indicate to the user they are looking at the physical dimensions of a shape that was drawn by the browser.
   */
  dimensions: 'Dimensions',
  /**
   *@description Text used to show the user the dimensions of a shape and indicate its area (e.g. 3x2).
   *@example {2} PH1
   *@example {2} PH2
   */
  sSDimensions: '{PH1} × {PH2}',
  /**
   *@description Related node label in Timeline UIUtils of the Performance panel
   */
  layerRoot: 'Layer Root',
  /**
   *@description Related node label in Timeline UIUtils of the Performance panel
   */
  ownerElement: 'Owner Element',
  /**
   *@description Text used to show the user the URL of the image they are viewing.
   */
  imageUrl: 'Image URL',
  /**
   *@description Text used to show the user that the URL they are viewing is loading a CSS stylesheet.
   */
  stylesheetUrl: 'Stylesheet URL',
  /**
   *@description Text used next to a number to show the user how many elements were affected.
   */
  elementsAffected: 'Elements Affected',
  /**
   *@description Text used next to a number to show the user how many nodes required the browser to update and re-layout the page.
   */
  nodesThatNeedLayout: 'Nodes That Need Layout',
  /**
   *@description Text used to show the amount in a subset - e.g. "2 of 10".
   *@example {2} PH1
   *@example {10} PH2
   */
  sOfS: '{PH1} of {PH2}',
  /**
   *@description Related node label in Timeline UIUtils of the Performance panel
   */
  layoutRoot: 'Layout root',
  /**
   *@description Text used when viewing an event that can have a custom message attached.
   */
  message: 'Message',
  /**
   *@description Text used to tell the user they are viewing an event that has a function embedded in it, which is referred to as the "callback function".
   */
  callbackFunction: 'Callback Function',
  /**
   *@description The current state of an item
   */
  state: 'State',
  /**
   *@description Text used to show the relevant range of a file - e.g. "lines 2-10".
   */
  range: 'Range',
  /**
   *@description Text used to refer to the amount of time some event or code was given to complete within.
   */
  allottedTime: 'Allotted Time',
  /**
   *@description Text used to tell a user that a particular event or function was automatically run by a timeout.
   */
  invokedByTimeout: 'Invoked by Timeout',
  /**
   *@description Text that refers to some types
   */
  type: 'Type',
  /**
   *@description Text for the size of something
   */
  size: 'Size',
  /**
   *@description Text for the details of something
   */
  details: 'Details',
  /**
   *@description Title in Timeline for Cumulative Layout Shifts
   */
  cumulativeLayoutShifts: 'Cumulative Layout Shifts',
  /**
   *@description Text for the link to the evolved CLS website
   */
  evolvedClsLink: 'evolved',
  /**
   *@description Warning in Timeline that CLS can cause a poor user experience. It contains a link to inform developers about the recent changes to how CLS is measured. The new CLS metric is said to have evolved from the previous version.
   *@example {Link to web.dev/metrics} PH1
   *@example {Link to web.dev/evolving-cls which will always have the text 'evolved'} PH2
   */
  sCLSInformation: '{PH1} can result in poor user experiences. It has recently {PH2}.',
  /**
   *@description Text to indicate an item is a warning
   */
  warning: 'Warning',
  /**
   *@description Title for the Timeline CLS Score
   */
  score: 'Score',
  /**
   *@description Text in Timeline for the cumulative CLS score
   */
  cumulativeScore: 'Cumulative Score',
  /**
   *@description Text in Timeline for the current CLS score
   */
  currentClusterScore: 'Current Cluster Score',
  /**
   *@description Text in Timeline for the current CLS cluster
   */
  currentClusterId: 'Current Cluster ID',
  /**
   *@description Text in Timeline for whether input happened recently
   */
  hadRecentInput: 'Had recent input',
  /**
   *@description Text in Timeline indicating that input has happened recently
   */
  yes: 'Yes',
  /**
   *@description Text in Timeline indicating that input has not happened recently
   */
  no: 'No',
  /**
   *@description Label for Cumulative Layout records, indicating where they moved from
   */
  movedFrom: 'Moved from',
  /**
   *@description Label for Cumulative Layout records, indicating where they moved to
   */
  movedTo: 'Moved to',
  /**
   *@description Text that indicates a particular HTML element or node is related to what the user is viewing.
   */
  relatedNode: 'Related Node',
  /**
   *@description Text for previewing items
   */
  preview: 'Preview',
  /**
   *@description Text used to refer to the total time summed up across multiple events.
   */
  aggregatedTime: 'Aggregated Time',
  /**
   *@description Text to indicate to the user they are viewing an event representing a network request.
   */
  networkRequest: 'Network request',
  /**
   *@description Text used to indicate if a network request was loaded from the cache.
   */
  loadFromCache: 'load from cache',
  /**
   *@description Text used to indicate if a network request was transferred over the network (rather than being loaded from the cache.)
   */
  networkTransfer: 'network transfer',
  /**
   *@description Text used to show the total time a network request spent loading a resource.
   *@example {1ms} PH1
   *@example {network transfer} PH2
   *@example {1ms} PH3
   */
  SSSResourceLoading: ' ({PH1} {PH2} + {PH3} resource loading)',
  /**
   *@description Text for the duration of something
   */
  duration: 'Duration',
  /**
   *@description Text used to show the mime-type of the data transferred with a network request (e.g. "application/json").
   */
  mimeType: 'Mime Type',
  /**
   *@description Text used to show the user that a request was served from the browser's in-memory cache.
   */
  FromMemoryCache: ' (from memory cache)',
  /**
   *@description Text used to show the user that a request was served from the browser's file cache.
   */
  FromCache: ' (from cache)',
  /**
   *@description Label for a network request indicating that it was a HTTP2 server push instead of a regular network request, in the Performance panel
   */
  FromPush: ' (from push)',
  /**
   *@description Text used to show a user that a request was served from an installed, active service worker.
   */
  FromServiceWorker: ' (from `service worker`)',
  /**
   *@description Text for the stack trace of the initiator of something. The Initiator is the event or factor that directly triggered or precipitated a subsequent action.
   */
  initiatorStackTrace: 'Initiator Stack Trace',
  /**
   *@description Text for the event initiated by another one
   */
  initiatedBy: 'Initiated by',
  /**
   *@description Text for the event that is an initiator for another one
   */
  initiatorFor: 'Initiator for',
  /**
   *@description Text for the underlying data behing a specific flamechart selection. Trace events are the browser instrumentation that are emitted as JSON objects.
   */
  traceEvent: 'Trace Event',
  /**
   *@description Call site stack label in Timeline UIUtils of the Performance panel
   */
  timerInstalled: 'Timer Installed',
  /**
   *@description Call site stack label in Timeline UIUtils of the Performance panel
   */
  animationFrameRequested: 'Animation Frame Requested',
  /**
   *@description Call site stack label in Timeline UIUtils of the Performance panel
   */
  idleCallbackRequested: 'Idle Callback Requested',
  /**
   *@description Stack label in Timeline UIUtils of the Performance panel
   */
  recalculationForced: 'Recalculation Forced',
  /**
   *@description Call site stack label in Timeline UIUtils of the Performance panel
   */
  firstLayoutInvalidation: 'First Layout Invalidation',
  /**
   *@description Stack label in Timeline UIUtils of the Performance panel
   */
  layoutForced: 'Layout Forced',
  /**
   *@description Text for the execution stack trace
   */
  stackTrace: 'Stack Trace',
  /**
   *@description Text used to show any invalidations for a particular event that caused the browser to have to do more work to update the page.
   */
  invalidations: 'Invalidations',
  /**
   * @description Text in Timeline UIUtils of the Performance panel. Phrase is followed by a number of milliseconds.
   * Some events or tasks might have been only started, but have not ended yet. Such events or tasks are considered
   * "pending".
   */
  pendingFor: 'Pending for',
  /**
   *@description Noun label for a stack trace which indicates the first time some condition was invalidated.
   */
  firstInvalidated: 'First Invalidated',
  /**
   *@description Title of the paint profiler, old name of the performance pane
   */
  paintProfiler: 'Paint Profiler',
  /**
   *@description Text in Timeline Flame Chart View of the Performance panel
   *@example {Frame} PH1
   *@example {10ms} PH2
   */
  sAtS: '{PH1} at {PH2}',
  /**
   *@description Text used next to a time to indicate that the particular event took that much time itself. In context this might look like "3ms blink.console (self)"
   *@example {blink.console} PH1
   */
  sSelf: '{PH1} (self)',
  /**
   *@description Text used next to a time to indicate that the event's children took that much time. In context this might look like "3ms blink.console (children)"
   *@example {blink.console} PH1
   */
  sChildren: '{PH1} (children)',
  /**
   *@description Text used to show the user how much time the browser spent on rendering (drawing the page onto the screen).
   */
  timeSpentInRendering: 'Time spent in rendering',
  /**
   *@description Text for a rendering frame
   */
  frame: 'Frame',
  /**
   *@description Text used to refer to the duration of an event at a given offset - e.g. "2ms at 10ms" which can be read as "2ms starting after 10ms".
   *@example {10ms} PH1
   *@example {10ms} PH2
   */
  sAtSParentheses: '{PH1} (at {PH2})',
  /**
   *@description Text of a DOM element in Timeline UIUtils of the Performance panel
   */
  UnknownNode: '[ unknown node ]',
  /**
   *@description Text used to refer to a particular element and the file it was referred to in.
   *@example {node} PH1
   *@example {app.js} PH2
   */
  invalidationWithCallFrame: '{PH1} at {PH2}',
  /**
   *@description Text indicating that something is outside of the Performace Panel Timeline Minimap range
   */
  outsideBreadcrumbRange: '(outside of the breadcrumb range)',
  /**
   *@description Text indicating that something is hidden from the Performace Panel Timeline
   */
  entryIsHidden: '(entry is hidden)',
  /**
   * @description Title of a row in the details view for a `Recalculate Styles` event that contains more info about selector stats tracing.
   */
  selectorStatsTitle: 'Selector Stats',
  /**
   * @description Info text that explains to the user how to enable selector stats tracing.
   * @example {Setting Name} PH1
   */
  sSelectorStatsInfo: 'Select "{PH1}" to collect detailed CSS selector matching statistics.',
};
const str_ = i18n.i18n.registerUIStrings('panels/timeline/TimelineUIUtils.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

let eventDispatchDesciptors: EventDispatchTypeDescriptor[];

let colorGenerator: Common.Color.Generator;

const requestPreviewElements = new WeakMap<TraceEngine.Types.TraceEvents.SyntheticNetworkRequest, HTMLImageElement>();

type LinkifyLocationOptions = {
  scriptId: Protocol.Runtime.ScriptId|null,
  url: string,
  lineNumber: number,
  columnNumber?: number,
  isFreshRecording?: boolean, target: SDK.Target.Target|null, linkifier: LegacyComponents.Linkifier.Linkifier,
};

export class TimelineUIUtils {
  static frameDisplayName(frame: Protocol.Runtime.CallFrame): string {
    if (!TimelineModel.TimelineJSProfile.TimelineJSProfileProcessor.isNativeRuntimeFrame(frame)) {
      return UI.UIUtils.beautifyFunctionName(frame.functionName);
    }
    const nativeGroup = TimelineModel.TimelineJSProfile.TimelineJSProfileProcessor.nativeGroup(frame.functionName);
    switch (nativeGroup) {
      case TimelineModel.TimelineJSProfile.TimelineJSProfileProcessor.NativeGroups.Compile:
        return i18nString(UIStrings.compile);
      case TimelineModel.TimelineJSProfile.TimelineJSProfileProcessor.NativeGroups.Parse:
        return i18nString(UIStrings.parse);
    }
    return frame.functionName;
  }

  static testContentMatching(
      traceEvent: TraceEngine.Legacy.CompatibleTraceEvent, regExp: RegExp,
      traceParsedData?: TraceEngine.Handlers.Types.TraceParseData): boolean {
    const title = TimelineUIUtils.eventStyle(traceEvent).title;
    const tokens = [title];

    if (TraceEngine.Legacy.eventIsFromNewEngine(traceEvent) &&
        TraceEngine.Types.TraceEvents.isProfileCall(traceEvent)) {
      // In the future this case will not be possible - wherever we call this
      // function we will be able to pass in the data from the new engine. But
      // currently this is called in a variety of places including from the
      // legacy model which does not have a reference to the new engine's data.
      // So if we are missing the data, we just fallback to the name from the
      // callFrame.
      if (!traceParsedData || !traceParsedData.Samples) {
        tokens.push(traceEvent.callFrame.functionName);
      } else {
        tokens.push(
            TraceEngine.Handlers.ModelHandlers.Samples.getProfileCallFunctionName(traceParsedData.Samples, traceEvent));
      }
    }
    const url = urlForEvent(traceParsedData ?? null, traceEvent);
    if (url) {
      tokens.push(url);
    }
    // This works for both legacy and new engine events.
    appendObjectProperties(traceEvent.args, 2);
    const result = tokens.join('|').match(regExp);
    return result ? result.length > 0 : false;

    interface ContentObject {
      [x: string]: number|string|ContentObject;
    }
    function appendObjectProperties(object: ContentObject, depth: number): void {
      if (!depth) {
        return;
      }
      for (const key in object) {
        const value = object[key];
        if (typeof value === 'string') {
          tokens.push(value);
        } else if (typeof value === 'number') {
          tokens.push(String(value));
        } else if (typeof value === 'object' && value !== null) {
          appendObjectProperties(value, depth - 1);
        }
      }
    }
  }

  static eventStyle(event: TraceEngine.Legacy.CompatibleTraceEvent): TimelineRecordStyle {
    const eventStyles = maybeInitSylesMap();
    if (TraceEngine.Legacy.eventHasCategory(event, TimelineModel.TimelineModel.TimelineModelImpl.Category.Console) ||
        TraceEngine.Legacy.eventHasCategory(event, TimelineModel.TimelineModel.TimelineModelImpl.Category.UserTiming)) {
      return new TimelineRecordStyle(event.name, getCategoryStyles()['scripting']);
    }

    if (TraceEngine.Legacy.eventIsFromNewEngine(event)) {
      if (TraceEngine.Types.TraceEvents.isProfileCall(event)) {
        if (event.callFrame.functionName === '(idle)') {
          return new TimelineRecordStyle(event.name, getCategoryStyles().idle);
        }
      }
      const defaultStyles = new TimelineRecordStyle(event.name, getCategoryStyles().other);
      return getEventStyle(event.name as TraceEngine.Types.TraceEvents.KnownEventName) || defaultStyles;
    }

    let result = eventStyles[event.name as TraceEngine.Types.TraceEvents.KnownEventName];
    // If there's no defined RecordStyle for this event, define as other & hidden.
    if (!result) {
      result = new TimelineRecordStyle(event.name, getCategoryStyles()['other'], true);
      eventStyles[event.name as TraceEngine.Types.TraceEvents.KnownEventName] = result;
    }
    return result;
  }

  static eventColor(event: TraceEngine.Legacy.CompatibleTraceEvent): string {
    if (TraceEngine.Legacy.eventIsFromNewEngine(event) && TraceEngine.Types.TraceEvents.isProfileCall(event)) {
      const frame = event.callFrame;
      if (TimelineUIUtils.isUserFrame(frame)) {
        return TimelineUIUtils.colorForId(frame.url);
      }
    }
    if (TraceEngine.Legacy.eventIsFromNewEngine(event) &&
        TraceEngine.Types.Extensions.isSyntheticExtensionEntry(event)) {
      return Extensions.ExtensionUI.extensionEntryColor(event);
    }
    let parsedColor = TimelineUIUtils.eventStyle(event).category.getComputedColorValue();
    // This event is considered idle time but still rendered as a scripting event here
    // to connect the StreamingCompileScriptParsing events it belongs to.
    if (event.name === TimelineModel.TimelineModel.RecordType.StreamingCompileScriptWaiting) {
      parsedColor = getCategoryStyles().scripting.getComputedColorValue();
      if (!parsedColor) {
        throw new Error('Unable to parse color from getCategoryStyles().scripting.color');
      }
    }
    return parsedColor;
  }

  static eventTitle(event: TraceEngine.Legacy.CompatibleTraceEvent): string {
    // Profile call events do not have a args.data property, thus, we
    // need to check for profile calls in the beginning of this
    // function.
    if (TraceEngine.Legacy.eventIsFromNewEngine(event) && TraceEngine.Types.TraceEvents.isProfileCall(event)) {
      const maybeResolvedName = SourceMapsResolver.resolvedNodeNameForEntry(event);
      const displayName = maybeResolvedName || TimelineUIUtils.frameDisplayName(event.callFrame);
      return displayName;
    }
    const recordType = TimelineModel.TimelineModel.RecordType;
    const eventData = event.args['data'];

    if (event.name === 'EventTiming') {
      let payload: TraceEngine.Types.TraceEvents.TraceEventData|null = null;
      if (event instanceof TraceEngine.Legacy.PayloadEvent) {
        payload = event.rawPayload();
      } else if (TraceEngine.Legacy.eventIsFromNewEngine(event)) {
        payload = event;
      }

      if (payload && TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(payload)) {
        return titleForInteractionEvent(payload);
      }
    }
    const title = TimelineUIUtils.eventStyle(event).title;
    if (TraceEngine.Legacy.eventHasCategory(event, TimelineModel.TimelineModel.TimelineModelImpl.Category.Console)) {
      return title;
    }
    if (event.name === recordType.TimeStamp) {
      return i18nString(UIStrings.sS, {PH1: title, PH2: eventData['message']});
    }
    if (event.name === recordType.Animation && eventData && eventData['name']) {
      return i18nString(UIStrings.sS, {PH1: title, PH2: eventData['name']});
    }
    if (event.name === recordType.EventDispatch && eventData && eventData['type']) {
      return i18nString(UIStrings.sS, {PH1: title, PH2: eventData['type']});
    }
    return title;
  }

  static isUserFrame(frame: Protocol.Runtime.CallFrame): boolean {
    return frame.scriptId !== '0' && !(frame.url && frame.url.startsWith('native '));
  }

  static syntheticNetworkRequestCategory(request: TraceEngine.Types.TraceEvents.SyntheticNetworkRequest):
      NetworkCategory {
    switch (request.args.data.mimeType) {
      case 'text/html':
        return NetworkCategory.HTML;
      case 'application/javascript':
      case 'application/x-javascript':
      case 'text/javascript':
        return NetworkCategory.Script;
      case 'text/css':
        return NetworkCategory.Style;
      case 'audio/ogg':
      case 'image/gif':
      case 'image/jpeg':
      case 'image/png':
      case 'image/svg+xml':
      case 'image/webp':
      case 'image/x-icon':
      case 'font/opentype':
      case 'font/woff2':
      case 'font/ttf':
      case 'application/font-woff':
        return NetworkCategory.Media;
      default:
        return NetworkCategory.Other;
    }
  }

  static networkCategoryColor(category: NetworkCategory): string {
    let cssVarName = '--app-color-system';
    switch (category) {
      case NetworkCategory.HTML:
        cssVarName = '--app-color-loading';
        break;
      case NetworkCategory.Script:
        cssVarName = '--app-color-scripting';
        break;
      case NetworkCategory.Style:
        cssVarName = '--app-color-rendering';
        break;
      case NetworkCategory.Media:
        cssVarName = '--app-color-painting';
        break;
      default:
        cssVarName = '--app-color-system';
        break;
    }
    return ThemeSupport.ThemeSupport.instance().getComputedValue(cssVarName);
  }

  static async buildDetailsTextForTraceEvent(
      event: TraceEngine.Legacy.Event|TraceEngine.Types.TraceEvents.TraceEventData,
      traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null): Promise<string|null> {
    const recordType = TimelineModel.TimelineModel.RecordType;
    let detailsText;
    const eventData = event.args['data'];
    switch (event.name) {
      case recordType.GCEvent:
      case recordType.MajorGC:
      case recordType.MinorGC: {
        const delta = event.args['usedHeapSizeBefore'] - event.args['usedHeapSizeAfter'];
        detailsText = i18nString(UIStrings.sCollected, {PH1: Platform.NumberUtilities.bytesToString(delta)});
        break;
      }
      case recordType.FunctionCall: {
        const {lineNumber, columnNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
        if (lineNumber !== undefined && columnNumber !== undefined) {
          detailsText = eventData.url + ':' + (lineNumber + 1) + ':' + (columnNumber + 1);
        }
        break;
      }
      case recordType.JSRoot:
      case recordType.JSFrame:
      case recordType.JSIdleFrame:
      case recordType.JSSystemFrame:
        detailsText = TimelineUIUtils.frameDisplayName(eventData);
        break;
      case recordType.EventDispatch:
        detailsText = eventData ? eventData['type'] : null;
        break;
      case recordType.Paint: {
        const width = TimelineUIUtils.quadWidth(eventData.clip);
        const height = TimelineUIUtils.quadHeight(eventData.clip);
        if (width && height) {
          detailsText = i18nString(UIStrings.sSDimensions, {PH1: width, PH2: height});
        }
        break;
      }
      case recordType.ParseHTML: {
        const startLine = event.args['beginData']['startLine'];
        const endLine = event.args['endData'] && event.args['endData']['endLine'];
        const url = Bindings.ResourceUtils.displayNameForURL(event.args['beginData']['url']);
        if (endLine >= 0) {
          detailsText = i18nString(UIStrings.sSs, {PH1: url, PH2: startLine + 1, PH3: endLine + 1});
        } else {
          detailsText = i18nString(UIStrings.sSSquareBrackets, {PH1: url, PH2: startLine + 1});
        }
        break;
      }
      case recordType.CompileModule:
      case recordType.CacheModule:
        detailsText = Bindings.ResourceUtils.displayNameForURL(event.args['fileName']);
        break;
      case recordType.CompileScript:
      case recordType.CacheScript:
      case recordType.EvaluateScript: {
        const {lineNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
        const url = eventData && eventData['url'];
        if (url) {
          detailsText = Bindings.ResourceUtils.displayNameForURL(url) + ':' + ((lineNumber || 0) + 1);
        }
        break;
      }
      case recordType.WasmCompiledModule:
      case recordType.WasmModuleCacheHit: {
        const url = event.args['url'];
        if (url) {
          detailsText = Bindings.ResourceUtils.displayNameForURL(url);
        }
        break;
      }

      case recordType.StreamingCompileScript:
      case recordType.BackgroundDeserialize:
      case recordType.XHRReadyStateChange:
      case recordType.XHRLoad: {
        const url = eventData['url'];
        if (url) {
          detailsText = Bindings.ResourceUtils.displayNameForURL(url);
        }
        break;
      }
      case recordType.TimeStamp:
        detailsText = eventData['message'];
        break;

      case recordType.WebSocketCreate:
      case recordType.WebSocketSendHandshakeRequest:
      case recordType.WebSocketReceiveHandshakeResponse:
      case recordType.WebSocketDestroy:
      case recordType.ResourceWillSendRequest:
      case recordType.ResourceSendRequest:
      case recordType.ResourceReceivedData:
      case recordType.ResourceReceiveResponse:
      case recordType.ResourceFinish:
      case recordType.PaintImage:
      case recordType.DecodeImage:
      case recordType.DecodeLazyPixelRef: {
        const url = urlForEvent(traceParsedData, event);
        if (url) {
          detailsText = Bindings.ResourceUtils.displayNameForURL(url);
        }
        break;
      }

      case recordType.EmbedderCallback:
        detailsText = eventData['callbackName'];
        break;

      case recordType.Animation:
        detailsText = eventData && eventData['name'];
        break;

      case recordType.AsyncTask:
        detailsText = eventData ? eventData['name'] : null;
        break;

      default:
        if (TraceEngine.Legacy.eventHasCategory(
                event, TimelineModel.TimelineModel.TimelineModelImpl.Category.Console)) {
          detailsText = null;
        } else {
          detailsText = linkifyTopCallFrameAsText();
        }
        break;
    }

    return detailsText;

    function linkifyTopCallFrameAsText(): string|null {
      const frame = TraceEngine.Legacy.eventIsFromNewEngine(event) ?
          TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(event)?.at(0) :
          null;
      if (!frame) {
        return null;
      }

      return frame.url + ':' + (frame.lineNumber + 1) + ':' + (frame.columnNumber + 1);
    }
  }

  static async buildDetailsNodeForTraceEvent(
      event: TraceEngine.Legacy.CompatibleTraceEvent, target: SDK.Target.Target|null,
      linkifier: LegacyComponents.Linkifier.Linkifier, isFreshRecording = false,
      traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null): Promise<Node|null> {
    const recordType = TimelineModel.TimelineModel.RecordType;
    let details: HTMLElement|HTMLSpanElement|(Element | null)|Text|null = null;
    let detailsText;
    const eventData = event.args['data'];
    switch (event.name) {
      case recordType.GCEvent:
      case recordType.MajorGC:
      case recordType.MinorGC:
      case recordType.EventDispatch:
      case recordType.Paint:
      case recordType.Animation:
      case recordType.EmbedderCallback:
      case recordType.ParseHTML:
      case recordType.WasmStreamFromResponseCallback:
      case recordType.WasmCompiledModule:
      case recordType.WasmModuleCacheHit:
      case recordType.WasmCachedModule:
      case recordType.WasmModuleCacheInvalid:
      case recordType.WebSocketCreate:
      case recordType.WebSocketSendHandshakeRequest:
      case recordType.WebSocketReceiveHandshakeResponse:
      case recordType.WebSocketDestroy: {
        detailsText = await TimelineUIUtils.buildDetailsTextForTraceEvent(event, traceParsedData);
        break;
      }

      case recordType.PaintImage:
      case recordType.DecodeImage:
      case recordType.DecodeLazyPixelRef:
      case recordType.XHRReadyStateChange:
      case recordType.XHRLoad:
      case recordType.ResourceWillSendRequest:
      case recordType.ResourceSendRequest:
      case recordType.ResourceReceivedData:
      case recordType.ResourceReceiveResponse:
      case recordType.ResourceFinish: {
        const url = urlForEvent(traceParsedData, event);
        if (url) {
          const options = {
            tabStop: true,
            showColumnNumber: false,
            inlineFrameIndex: 0,
          };
          details = LegacyComponents.Linkifier.Linkifier.linkifyURL(url, options);
        }
        break;
      }

      case recordType.JSRoot:
      case recordType.FunctionCall:
      case recordType.JSIdleFrame:
      case recordType.JSSystemFrame:
      case recordType.JSFrame: {
        details = document.createElement('span');
        UI.UIUtils.createTextChild(details, TimelineUIUtils.frameDisplayName(eventData));
        const {lineNumber, columnNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
        const location = this.linkifyLocation({
          scriptId: eventData['scriptId'],
          url: eventData['url'],
          lineNumber: lineNumber || 0,
          columnNumber: columnNumber,
          target,
          isFreshRecording,
          linkifier,
        });
        if (location) {
          UI.UIUtils.createTextChild(details, ' @ ');
          details.appendChild(location);
        }
        break;
      }

      case recordType.CompileModule:
      case recordType.CacheModule: {
        details = this.linkifyLocation({
          scriptId: null,
          url: event.args['fileName'],
          lineNumber: 0,
          columnNumber: 0,
          target,
          isFreshRecording,
          linkifier,
        });
        break;
      }

      case recordType.CompileScript:
      case recordType.CacheScript:
      case recordType.EvaluateScript: {
        const url = eventData['url'];
        if (url) {
          const {lineNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
          details = this.linkifyLocation({
            scriptId: null,
            url,
            lineNumber: lineNumber || 0,
            columnNumber: 0,
            target,
            isFreshRecording,
            linkifier,
          });
        }
        break;
      }

      case recordType.BackgroundDeserialize:
      case recordType.StreamingCompileScript: {
        const url = eventData['url'];
        if (url) {
          details = this.linkifyLocation(
              {scriptId: null, url, lineNumber: 0, columnNumber: 0, target, isFreshRecording, linkifier});
        }
        break;
      }
      case TraceEngine.Types.TraceEvents.KnownEventName.ProfileCall: {
        details = document.createElement('span');
        // This check is only added for convenience with the type checker.
        if (!TraceEngine.Legacy.eventIsFromNewEngine(event) || !TraceEngine.Types.TraceEvents.isProfileCall(event)) {
          break;
        }
        const maybeResolvedName = SourceMapsResolver.resolvedNodeNameForEntry(event);
        const functionName = maybeResolvedName || TimelineUIUtils.frameDisplayName(event.callFrame);
        UI.UIUtils.createTextChild(details, functionName);
        const location = this.linkifyLocation({
          scriptId: event.callFrame['scriptId'],
          url: event.callFrame['url'],
          lineNumber: event.callFrame['lineNumber'],
          columnNumber: event.callFrame['columnNumber'],
          target,
          isFreshRecording,
          linkifier,
        });
        if (location) {
          UI.UIUtils.createTextChild(details, ' @ ');
          details.appendChild(location);
        }
        break;
      }

      default: {
        if (TraceEngine.Legacy.eventHasCategory(
                event, TimelineModel.TimelineModel.TimelineModelImpl.Category.Console)) {
          detailsText = null;
        } else {
          details = TraceEngine.Legacy.eventIsFromNewEngine(event) ?
              this.linkifyTopCallFrame(event, target, linkifier, isFreshRecording) :
              null;
        }
        break;
      }
    }

    if (!details && detailsText) {
      details = document.createTextNode(detailsText);
    }
    return details;
  }

  static linkifyLocation(linkifyOptions: LinkifyLocationOptions): Element|null {
    const {scriptId, url, lineNumber, columnNumber, isFreshRecording, linkifier, target} = linkifyOptions;
    const options = {
      lineNumber,
      columnNumber,
      showColumnNumber: true,
      inlineFrameIndex: 0,
      className: 'timeline-details',
      tabStop: true,
    };
    if (isFreshRecording) {
      return linkifier.linkifyScriptLocation(
          target, scriptId, url as Platform.DevToolsPath.UrlString, lineNumber, options);
    }
    return LegacyComponents.Linkifier.Linkifier.linkifyURL(url as Platform.DevToolsPath.UrlString, options);
  }

  static linkifyTopCallFrame(
      event: TraceEngine.Types.TraceEvents.TraceEventData, target: SDK.Target.Target|null,
      linkifier: LegacyComponents.Linkifier.Linkifier, isFreshRecording = false): Element|null {
    let frame = TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(event)?.[0];
    if (TraceEngine.Types.TraceEvents.isProfileCall(event)) {
      frame = event.callFrame;
    }
    if (!frame) {
      return null;
    }
    const options = {
      className: 'timeline-details',
      tabStop: true,
      inlineFrameIndex: 0,
      showColumnNumber: true,
      columnNumber: frame.columnNumber,
      lineNumber: frame.lineNumber,
    };
    if (isFreshRecording) {
      return linkifier.maybeLinkifyConsoleCallFrame(target, frame, {showColumnNumber: true, inlineFrameIndex: 0});
    }
    return LegacyComponents.Linkifier.Linkifier.linkifyURL(frame.url as Platform.DevToolsPath.UrlString, options);
  }

  static buildDetailsNodeForMarkerEvents(event: TraceEngine.Types.TraceEvents.MarkerEvent): HTMLElement {
    let link = 'https://web.dev/user-centric-performance-metrics/';
    let name = 'page performance metrics';
    switch (event.name) {
      case TraceEngine.Types.TraceEvents.KnownEventName.MarkLCPCandidate:
        link = 'https://web.dev/lcp/';
        name = 'largest contentful paint';
        break;
      case TraceEngine.Types.TraceEvents.KnownEventName.MarkFCP:
        link = 'https://web.dev/first-contentful-paint/';
        name = 'first contentful paint';
        break;
      default:
        break;
    }

    const html = UI.Fragment.html`<div>${
        UI.XLink.XLink.create(
            link, i18nString(UIStrings.learnMore), undefined, undefined, 'learn-more')} about ${name}.</div>`;
    return html as HTMLElement;
  }

  static buildConsumeCacheDetails(
      eventData: {
        consumedCacheSize?: number,
        cacheRejected?: boolean,
        cacheKind?: string,
      },
      contentHelper: TimelineDetailsContentHelper): void {
    if (typeof eventData.consumedCacheSize === 'number') {
      contentHelper.appendTextRow(
          i18nString(UIStrings.compilationCacheStatus), i18nString(UIStrings.scriptLoadedFromCache));
      contentHelper.appendTextRow(
          i18nString(UIStrings.compilationCacheSize),
          Platform.NumberUtilities.bytesToString(eventData.consumedCacheSize));
      const cacheKind = eventData.cacheKind;
      if (cacheKind) {
        contentHelper.appendTextRow(i18nString(UIStrings.compilationCacheKind), cacheKind);
      }
    } else if ('cacheRejected' in eventData && eventData['cacheRejected']) {
      // Version mismatch or similar.
      contentHelper.appendTextRow(
          i18nString(UIStrings.compilationCacheStatus), i18nString(UIStrings.failedToLoadScriptFromCache));
    } else {
      contentHelper.appendTextRow(
          i18nString(UIStrings.compilationCacheStatus), i18nString(UIStrings.scriptNotEligibleToBeLoadedFromCache));
    }
  }

  static async buildTraceEventDetails(
      traceParseData: TraceEngine.Handlers.Types.TraceParseData,
      event: TraceEngine.Types.TraceEvents.TraceEventData,
      linkifier: LegacyComponents.Linkifier.Linkifier,
      detailed: boolean,
      ): Promise<DocumentFragment> {
    const maybeTarget = maybeTargetForEvent(traceParseData, event);
    const {duration, selfTime} = TraceEngine.Helpers.Timing.eventTimingsMilliSeconds(event);

    const relatedNodesMap = await TraceEngine.Extras.FetchNodes.extractRelatedDOMNodesFromEvent(
        traceParseData,
        event,
    );

    if (maybeTarget) {
      // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
      if (typeof event[previewElementSymbol] === 'undefined') {
        let previewElement: (Element|null)|null = null;
        const url = urlForEvent(traceParseData, event);
        if (url) {
          previewElement = await LegacyComponents.ImagePreview.ImagePreview.build(maybeTarget, url, false, {
            imageAltText: LegacyComponents.ImagePreview.ImagePreview.defaultAltTextForImageURL(url),
            precomputedFeatures: undefined,
          });
        } else if (TraceEngine.Types.TraceEvents.isTraceEventPaint(event)) {
          previewElement = await TimelineUIUtils.buildPicturePreviewContent(traceParseData, event, maybeTarget);
        }
        // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
        event[previewElementSymbol] = previewElement;
      }
    }

    const recordTypes = TimelineModel.TimelineModel.RecordType;

    if (event.name === recordTypes.LayoutShift) {
      // Ensure that there are no pie charts or extended info for layout shifts.
      detailed = false;
    }

    // This message may vary per event.name;
    let relatedNodeLabel;

    const contentHelper = new TimelineDetailsContentHelper(maybeTargetForEvent(traceParseData, event), linkifier);

    const defaultColorForEvent = this.eventColor(event);
    const isMarker =
        traceParseData && TraceEngine.Legacy.eventIsFromNewEngine(event) && isMarkerEvent(traceParseData, event);
    const color = isMarker ? TimelineUIUtils.markerStyleForEvent(event).color : defaultColorForEvent;

    contentHelper.addSection(TimelineUIUtils.eventTitle(event), color);

    // TODO: as part of the removal of the old engine, produce a typesafe way
    // to look up args and data for events.
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const unsafeEventArgs = event.args as Record<string, any>;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const unsafeEventData = event.args?.data as Record<string, any>;
    const initiator = traceParseData.Initiators.eventToInitiator.get(event) ?? null;
    const initiatorFor = traceParseData.Initiators.initiatorToEvents.get(event) ?? null;

    let url: Platform.DevToolsPath.UrlString|null = null;

    if (TraceEngine.Legacy.eventIsFromNewEngine(event) && traceParseData) {
      const warnings = TimelineComponents.DetailsView.buildWarningElementsForEvent(event, traceParseData);
      for (const warning of warnings) {
        contentHelper.appendElementRow(i18nString(UIStrings.warning), warning, true);
      }
    }
    if (detailed && !Number.isNaN(duration || 0)) {
      contentHelper.appendTextRow(
          i18nString(UIStrings.totalTime), i18n.TimeUtilities.millisToString(duration || 0, true));
      contentHelper.appendTextRow(i18nString(UIStrings.selfTime), i18n.TimeUtilities.millisToString(selfTime, true));
    }

    if (traceParseData.Meta.traceIsGeneric) {
      TimelineUIUtils.renderEventJson(event, contentHelper);
      return contentHelper.fragment;
    }

    if (TraceEngine.Types.TraceEvents.isTraceEventV8Compile(event)) {
      url = event.args.data?.url as Platform.DevToolsPath.UrlString;
      if (url) {
        const {lineNumber, columnNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
        contentHelper.appendLocationRow(i18nString(UIStrings.script), url, lineNumber || 0, columnNumber);
      }
      const isEager = Boolean(event.args.data?.eager);
      if (isEager) {
        contentHelper.appendTextRow(i18nString(UIStrings.eagerCompile), true);
      }

      const isStreamed = Boolean(event.args.data?.streamed);
      contentHelper.appendTextRow(
          i18nString(UIStrings.streamed),
          isStreamed + (isStreamed ? '' : `: ${event.args.data?.notStreamedReason || ''}`));
      if (event.args.data) {
        TimelineUIUtils.buildConsumeCacheDetails(event.args.data, contentHelper);
      }
    }

    if (TraceEngine.Types.Extensions.isSyntheticExtensionEntry(event)) {
      for (const [key, value] of event.args.detailsPairs || []) {
        contentHelper.appendTextRow(key, value);
      }
    }

    const isFreshRecording = Boolean(traceParseData && Tracker.instance().recordingIsFresh(traceParseData));

    switch (event.name) {
      case recordTypes.GCEvent:
      case recordTypes.MajorGC:
      case recordTypes.MinorGC: {
        const delta = unsafeEventArgs['usedHeapSizeBefore'] - unsafeEventArgs['usedHeapSizeAfter'];
        contentHelper.appendTextRow(i18nString(UIStrings.collected), Platform.NumberUtilities.bytesToString(delta));
        break;
      }

      case recordTypes.JSRoot:
      case recordTypes.JSFrame:
      case TraceEngine.Types.TraceEvents.KnownEventName.ProfileCall:
      case recordTypes.JSIdleFrame:
      case recordTypes.JSSystemFrame:
      case recordTypes.FunctionCall: {
        const detailsNode = await TimelineUIUtils.buildDetailsNodeForTraceEvent(
            event, maybeTargetForEvent(traceParseData, event), linkifier, isFreshRecording, traceParseData);
        if (detailsNode) {
          contentHelper.appendElementRow(i18nString(UIStrings.function), detailsNode);
        }
        break;
      }

      case recordTypes.TimerFire:
      case recordTypes.TimerInstall:
      case recordTypes.TimerRemove: {
        contentHelper.appendTextRow(i18nString(UIStrings.timerId), unsafeEventData.timerId);

        if (event.name === recordTypes.TimerInstall) {
          contentHelper.appendTextRow(
              i18nString(UIStrings.timeout), i18n.TimeUtilities.millisToString(unsafeEventData['timeout']));
          contentHelper.appendTextRow(i18nString(UIStrings.repeats), !unsafeEventData['singleShot']);
        }
        break;
      }

      case recordTypes.FireAnimationFrame: {
        contentHelper.appendTextRow(i18nString(UIStrings.callbackId), unsafeEventData['id']);
        break;
      }

      case recordTypes.CompileModule: {
        contentHelper.appendLocationRow(i18nString(UIStrings.module), unsafeEventArgs['fileName'], 0);
        break;
      }
      case recordTypes.CompileScript: {
        // This case is handled above
        break;
      }

      case recordTypes.CacheModule: {
        url = unsafeEventData && unsafeEventData['url'] as Platform.DevToolsPath.UrlString;
        contentHelper.appendTextRow(
            i18nString(UIStrings.compilationCacheSize),
            Platform.NumberUtilities.bytesToString(unsafeEventData['producedCacheSize']));
        break;
      }

      case recordTypes.CacheScript: {
        url = unsafeEventData && unsafeEventData['url'] as Platform.DevToolsPath.UrlString;
        if (url) {
          const {lineNumber, columnNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
          contentHelper.appendLocationRow(i18nString(UIStrings.script), url, lineNumber || 0, columnNumber);
        }
        contentHelper.appendTextRow(
            i18nString(UIStrings.compilationCacheSize),
            Platform.NumberUtilities.bytesToString(unsafeEventData['producedCacheSize']));
        break;
      }

      case recordTypes.EvaluateScript: {
        url = unsafeEventData && unsafeEventData['url'] as Platform.DevToolsPath.UrlString;
        if (url) {
          const {lineNumber, columnNumber} = getZeroIndexedLineAndColumnNumbersForEvent(event);
          contentHelper.appendLocationRow(i18nString(UIStrings.script), url, lineNumber || 0, columnNumber);
        }
        break;
      }

      case recordTypes.WasmStreamFromResponseCallback:
      case recordTypes.WasmCompiledModule:
      case recordTypes.WasmCachedModule:
      case recordTypes.WasmModuleCacheHit:
      case recordTypes.WasmModuleCacheInvalid: {
        if (unsafeEventData) {
          url = unsafeEventArgs['url'] as Platform.DevToolsPath.UrlString;
          if (url) {
            contentHelper.appendTextRow(i18nString(UIStrings.url), url);
          }
          const producedCachedSize = unsafeEventArgs['producedCachedSize'];
          if (producedCachedSize) {
            contentHelper.appendTextRow(i18nString(UIStrings.producedCacheSize), producedCachedSize);
          }
          const consumedCachedSize = unsafeEventArgs['consumedCachedSize'];
          if (consumedCachedSize) {
            contentHelper.appendTextRow(i18nString(UIStrings.consumedCacheSize), consumedCachedSize);
          }
        }
        break;
      }

      // @ts-ignore Fall-through intended.
      case recordTypes.Paint: {
        const clip = unsafeEventData['clip'];
        contentHelper.appendTextRow(
            i18nString(UIStrings.location), i18nString(UIStrings.sSCurlyBrackets, {PH1: clip[0], PH2: clip[1]}));
        const clipWidth = TimelineUIUtils.quadWidth(clip);
        const clipHeight = TimelineUIUtils.quadHeight(clip);
        contentHelper.appendTextRow(
            i18nString(UIStrings.dimensions), i18nString(UIStrings.sSDimensions, {PH1: clipWidth, PH2: clipHeight}));
      }

      case recordTypes.PaintSetup:
      case recordTypes.Rasterize:
      case recordTypes.ScrollLayer: {
        relatedNodeLabel = i18nString(UIStrings.layerRoot);
        break;
      }

      case recordTypes.PaintImage:
      case recordTypes.DecodeLazyPixelRef:
      case recordTypes.DecodeImage:
      case recordTypes.DrawLazyPixelRef: {
        relatedNodeLabel = i18nString(UIStrings.ownerElement);
        url = urlForEvent(traceParseData, event);
        if (url) {
          const options = {
            tabStop: true,
            showColumnNumber: false,
            inlineFrameIndex: 0,
          };
          contentHelper.appendElementRow(
              i18nString(UIStrings.imageUrl), LegacyComponents.Linkifier.Linkifier.linkifyURL(url, options));
        }
        break;
      }

      case recordTypes.ParseAuthorStyleSheet: {
        url = unsafeEventData['styleSheetUrl'] as Platform.DevToolsPath.UrlString;
        if (url) {
          const options = {
            tabStop: true,
            showColumnNumber: false,
            inlineFrameIndex: 0,
          };
          contentHelper.appendElementRow(
              i18nString(UIStrings.stylesheetUrl), LegacyComponents.Linkifier.Linkifier.linkifyURL(url, options));
        }
        break;
      }

      case recordTypes.UpdateLayoutTree: {
        contentHelper.appendTextRow(i18nString(UIStrings.elementsAffected), unsafeEventArgs['elementCount']);

        const selectorStatsSetting =
            Common.Settings.Settings.instance().createSetting('timeline-capture-selector-stats', false);
        if (!selectorStatsSetting.get()) {
          const note = document.createElement('span');
          note.textContent = i18nString(UIStrings.sSelectorStatsInfo, {PH1: selectorStatsSetting.title()});
          contentHelper.appendElementRow(i18nString(UIStrings.selectorStatsTitle), note);
        }

        break;
      }

      case recordTypes.Layout: {
        const beginData = unsafeEventArgs['beginData'];
        contentHelper.appendTextRow(
            i18nString(UIStrings.nodesThatNeedLayout),
            i18nString(UIStrings.sOfS, {PH1: beginData['dirtyObjects'], PH2: beginData['totalObjects']}));
        relatedNodeLabel = i18nString(UIStrings.layoutRoot);
        break;
      }

      case recordTypes.ConsoleTime: {
        contentHelper.appendTextRow(i18nString(UIStrings.message), event.name);
        break;
      }

      case recordTypes.WebSocketCreate:
      case recordTypes.WebSocketSendHandshakeRequest:
      case recordTypes.WebSocketReceiveHandshakeResponse:
      case recordTypes.WebSocketDestroy: {
        // The events will be from tthe new engine; as we remove the old engine we can remove these checks.
        if (TraceEngine.Legacy.eventIsFromNewEngine(event) &&
            TraceEngine.Types.TraceEvents.isWebSocketTraceEvent(event) && traceParseData) {
          const rows = TimelineComponents.DetailsView.buildRowsForWebSocketEvent(event, traceParseData);
          for (const {key, value} of rows) {
            contentHelper.appendTextRow(key, value);
          }
        }
        break;
      }

      case recordTypes.EmbedderCallback: {
        contentHelper.appendTextRow(i18nString(UIStrings.callbackFunction), unsafeEventData['callbackName']);
        break;
      }

      case recordTypes.Animation: {
        if (TraceEngine.Legacy.phaseForEvent(event) === TraceEngine.Types.TraceEvents.Phase.ASYNC_NESTABLE_INSTANT) {
          contentHelper.appendTextRow(i18nString(UIStrings.state), unsafeEventData['state']);
        }
        break;
      }

      case recordTypes.ParseHTML: {
        const beginData = unsafeEventArgs['beginData'];
        const startLine = beginData['startLine'] - 1;
        const endLine = unsafeEventArgs['endData'] ? unsafeEventArgs['endData']['endLine'] - 1 : undefined;
        url = beginData['url'];
        if (url) {
          contentHelper.appendLocationRange(i18nString(UIStrings.range), url, startLine, endLine);
        }
        break;
      }

      // @ts-ignore Fall-through intended.
      case recordTypes.FireIdleCallback: {
        contentHelper.appendTextRow(
            i18nString(UIStrings.allottedTime),
            i18n.TimeUtilities.millisToString(unsafeEventData['allottedMilliseconds']));
        contentHelper.appendTextRow(i18nString(UIStrings.invokedByTimeout), unsafeEventData['timedOut']);
      }

      case recordTypes.RequestIdleCallback:
      case recordTypes.CancelIdleCallback: {
        contentHelper.appendTextRow(i18nString(UIStrings.callbackId), unsafeEventData['id']);
        break;
      }

      case recordTypes.EventDispatch: {
        contentHelper.appendTextRow(i18nString(UIStrings.type), unsafeEventData['type']);
        break;
      }

      // @ts-ignore Fall-through intended.
      case recordTypes.MarkLCPCandidate: {
        contentHelper.appendTextRow(i18nString(UIStrings.type), String(unsafeEventData['type']));
        contentHelper.appendTextRow(i18nString(UIStrings.size), String(unsafeEventData['size']));
      }

      case recordTypes.MarkFirstPaint:
      case recordTypes.MarkFCP:
      case recordTypes.MarkLoad:
      case recordTypes.MarkDOMContent: {
        // Because the TimingsTrack has been migrated to the new engine, we
        // know that this conditonal will be true, but it is here to satisfy
        // TypeScript. That is also why there is no else branch for this -
        // there is no way in which timings here can be the legacy
        // SDKTraceEvent class.
        if (traceParseData && TraceEngine.Legacy.eventIsFromNewEngine(event)) {
          const adjustedEventTimeStamp = timeStampForEventAdjustedForClosestNavigationIfPossible(
              event,
              traceParseData,
          );

          contentHelper.appendTextRow(
              i18nString(UIStrings.timestamp), i18n.TimeUtilities.preciseMillisToString(adjustedEventTimeStamp, 1));

          if (TraceEngine.Legacy.eventIsFromNewEngine(event) &&
              TraceEngine.Types.TraceEvents.isTraceEventMarkerEvent(event)) {
            contentHelper.appendElementRow(
                i18nString(UIStrings.details), TimelineUIUtils.buildDetailsNodeForMarkerEvents(event));
          }
        }

        break;
      }

      case recordTypes.EventTiming: {
        const detailsNode = await TimelineUIUtils.buildDetailsNodeForTraceEvent(
            event, maybeTargetForEvent(traceParseData, event), linkifier, isFreshRecording, traceParseData);
        if (detailsNode) {
          contentHelper.appendElementRow(i18nString(UIStrings.details), detailsNode);
        }
        if (TraceEngine.Types.TraceEvents.isSyntheticInteractionEvent(event)) {
          const inputDelay = TraceEngine.Helpers.Timing.formatMicrosecondsTime(event.inputDelay);
          const mainThreadTime = TraceEngine.Helpers.Timing.formatMicrosecondsTime(event.mainThreadHandling);
          const presentationDelay = TraceEngine.Helpers.Timing.formatMicrosecondsTime(event.presentationDelay);
          contentHelper.appendTextRow(i18nString(UIStrings.interactionID), event.interactionId);
          contentHelper.appendTextRow(i18nString(UIStrings.inputDelay), inputDelay);
          contentHelper.appendTextRow(i18nString(UIStrings.processingDuration), mainThreadTime);
          contentHelper.appendTextRow(i18nString(UIStrings.presentationDelay), presentationDelay);
        }
        break;
      }

      case recordTypes.LayoutShift: {
        if (!TraceEngine.Legacy.eventIsFromNewEngine(event) ||
            !TraceEngine.Types.TraceEvents.isSyntheticLayoutShift(event)) {
          console.error('Unexpected type for LayoutShift event');
          break;
        }
        const layoutShift = event as TraceEngine.Types.TraceEvents.SyntheticLayoutShift;
        const layoutShiftEventData = layoutShift.args.data;
        const warning = document.createElement('span');
        const clsLink = UI.XLink.XLink.create(
            'https://web.dev/cls/', i18nString(UIStrings.cumulativeLayoutShifts), undefined, undefined,
            'cumulative-layout-shifts');
        const evolvedClsLink = UI.XLink.XLink.create(
            'https://web.dev/evolving-cls/', i18nString(UIStrings.evolvedClsLink), undefined, undefined, 'evolved-cls');

        warning.appendChild(
            i18n.i18n.getFormatLocalizedString(str_, UIStrings.sCLSInformation, {PH1: clsLink, PH2: evolvedClsLink}));
        contentHelper.appendElementRow(i18nString(UIStrings.warning), warning, true);
        if (!layoutShiftEventData) {
          break;
        }
        contentHelper.appendTextRow(i18nString(UIStrings.score), layoutShiftEventData['score'].toPrecision(4));
        contentHelper.appendTextRow(
            i18nString(UIStrings.cumulativeScore), layoutShiftEventData['cumulative_score'].toPrecision(4));
        contentHelper.appendTextRow(
            i18nString(UIStrings.currentClusterId), layoutShift.parsedData.sessionWindowData.id);
        contentHelper.appendTextRow(
            i18nString(UIStrings.currentClusterScore),
            layoutShift.parsedData.sessionWindowData.cumulativeWindowScore.toPrecision(4));
        contentHelper.appendTextRow(
            i18nString(UIStrings.hadRecentInput),
            unsafeEventData['had_recent_input'] ? i18nString(UIStrings.yes) : i18nString(UIStrings.no));

        for (const impactedNode of unsafeEventData['impacted_nodes']) {
          const oldRect = new CLSRect(impactedNode['old_rect']);
          const newRect = new CLSRect(impactedNode['new_rect']);

          const linkedOldRect = await Common.Linkifier.Linkifier.linkify(oldRect);
          const linkedNewRect = await Common.Linkifier.Linkifier.linkify(newRect);

          contentHelper.appendElementRow(i18nString(UIStrings.movedFrom), linkedOldRect);
          contentHelper.appendElementRow(i18nString(UIStrings.movedTo), linkedNewRect);
        }

        break;
      }

      default: {
        const detailsNode = await TimelineUIUtils.buildDetailsNodeForTraceEvent(
            event, maybeTargetForEvent(traceParseData, event), linkifier, isFreshRecording, traceParseData);
        if (detailsNode) {
          contentHelper.appendElementRow(i18nString(UIStrings.details), detailsNode);
        }
        break;
      }
    }
    const relatedNodes = relatedNodesMap?.values() || [];
    for (const relatedNode of relatedNodes) {
      if (relatedNode) {
        const nodeSpan = await Common.Linkifier.Linkifier.linkify(relatedNode);
        contentHelper.appendElementRow(relatedNodeLabel || i18nString(UIStrings.relatedNode), nodeSpan);
      }
    }

    // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
    if (event[previewElementSymbol]) {
      contentHelper.addSection(i18nString(UIStrings.preview));
      // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
      contentHelper.appendElementRow('', event[previewElementSymbol]);
    }

    if (TraceEngine.Legacy.eventIsFromNewEngine(event) && traceParseData) {
      const stackTrace = TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(event);
      if (initiator || initiatorFor || stackTrace || traceParseData?.Invalidations.invalidationsForEvent.get(event)) {
        await TimelineUIUtils.generateCauses(event, contentHelper, traceParseData);
      }
    }

    if (Root.Runtime.experiments.isEnabled(Root.Runtime.ExperimentName.TIMELINE_DEBUG_MODE)) {
      TimelineUIUtils.renderEventJson(event, contentHelper);
    }

    const stats: {
      [x: string]: number,
    } = {};
    const showPieChart =
        detailed && traceParseData && TimelineUIUtils.aggregatedStatsForTraceEvent(stats, traceParseData, event);
    if (showPieChart) {
      contentHelper.addSection(i18nString(UIStrings.aggregatedTime));
      const pieChart = TimelineUIUtils.generatePieChart(stats, TimelineUIUtils.eventStyle(event).category, selfTime);
      contentHelper.appendElementRow('', pieChart);
    }

    return contentHelper.fragment;
  }

  static statsForTimeRange(
      events: TraceEngine.Types.TraceEvents.TraceEventData[], startTime: TraceEngine.Types.Timing.MilliSeconds,
      endTime: TraceEngine.Types.Timing.MilliSeconds): {
    [x: string]: number,
  } {
    if (!events.length) {
      return {'idle': endTime - startTime};
    }

    buildRangeStatsCacheIfNeeded(events);
    const aggregatedStats = subtractStats(aggregatedStatsAtTime(endTime), aggregatedStatsAtTime(startTime));
    const aggregatedTotal = Object.values(aggregatedStats).reduce((a, b) => a + b, 0);
    aggregatedStats['idle'] = Math.max(0, endTime - startTime - aggregatedTotal);
    return aggregatedStats;

    function aggregatedStatsAtTime(time: number): {
      [x: string]: number,
    } {
      const stats: {
        [x: string]: number,
      } = {};
      // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
      const cache = events[categoryBreakdownCacheSymbol];
      for (const category in cache) {
        const categoryCache = cache[category];
        const index =
            Platform.ArrayUtilities.upperBound(categoryCache.time, time, Platform.ArrayUtilities.DEFAULT_COMPARATOR);
        let value;
        if (index === 0) {
          value = 0;
        } else if (index === categoryCache.time.length) {
          value = categoryCache.value[categoryCache.value.length - 1];
        } else {
          const t0 = categoryCache.time[index - 1];
          const t1 = categoryCache.time[index];
          const v0 = categoryCache.value[index - 1];
          const v1 = categoryCache.value[index];
          value = v0 + (v1 - v0) * (time - t0) / (t1 - t0);
        }
        stats[category] = value;
      }
      return stats;
    }

    function subtractStats(
        a: {
          [x: string]: number,
        },
        b: {
          [x: string]: number,
        }): {
      [x: string]: number,
    } {
      const result = Object.assign({}, a);
      for (const key in b) {
        result[key] -= b[key];
      }
      return result;
    }

    function buildRangeStatsCacheIfNeeded(events: TraceEngine.Types.TraceEvents.TraceEventData[]): void {
      // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
      if (events[categoryBreakdownCacheSymbol]) {
        return;
      }

      // aggeregatedStats is a map by categories. For each category there's an array
      // containing sorted time points which records accumulated value of the category.
      const aggregatedStats: {
        [x: string]: {
          time: number[],
          value: number[],
        },
      } = {};
      const categoryStack: string[] = [];
      let lastTime = 0;
      TraceEngine.Helpers.Trace.forEachEvent(events, {
        onStartEvent,
        onEndEvent,
      });

      function updateCategory(category: string, time: number): void {
        let statsArrays: {
          time: number[],
          value: number[],
        } = aggregatedStats[category];
        if (!statsArrays) {
          statsArrays = {time: [], value: []};
          aggregatedStats[category] = statsArrays;
        }
        if (statsArrays.time.length && statsArrays.time[statsArrays.time.length - 1] === time || lastTime > time) {
          return;
        }
        const lastValue = statsArrays.value.length > 0 ? statsArrays.value[statsArrays.value.length - 1] : 0;
        statsArrays.value.push(lastValue + time - lastTime);
        statsArrays.time.push(time);
      }

      function categoryChange(from: string|null, to: string|null, time: number): void {
        if (from) {
          updateCategory(from, time);
        }
        lastTime = time;
        if (to) {
          updateCategory(to, time);
        }
      }

      function onStartEvent(e: TraceEngine.Types.TraceEvents.TraceEventData): void {
        const {startTime} = TraceEngine.Helpers.Timing.eventTimingsMilliSeconds(e);
        const category = getEventStyle(e.name as TraceEngine.Types.TraceEvents.KnownEventName)?.category.name ||
            getCategoryStyles().other.name;
        const parentCategory = categoryStack.length ? categoryStack[categoryStack.length - 1] : null;
        if (category !== parentCategory) {
          categoryChange(parentCategory || null, category, startTime);
        }
        categoryStack.push(category);
      }

      function onEndEvent(e: TraceEngine.Types.TraceEvents.TraceEventData): void {
        const {endTime} = TraceEngine.Helpers.Timing.eventTimingsMilliSeconds(e);
        const category = categoryStack.pop();
        const parentCategory = categoryStack.length ? categoryStack[categoryStack.length - 1] : null;
        if (category !== parentCategory) {
          categoryChange(category || null, parentCategory || null, endTime || 0);
        }
      }

      const obj = (events as Object);
      // @ts-ignore TODO(crbug.com/1011811): Remove symbol usage.
      obj[categoryBreakdownCacheSymbol] = aggregatedStats;
    }
  }

  static async buildSyntheticNetworkRequestDetails(
      traceParseData: TraceEngine.Handlers.Types.TraceParseData|null,
      event: TraceEngine.Types.TraceEvents.SyntheticNetworkRequest,
      linkifier: LegacyComponents.Linkifier.Linkifier): Promise<DocumentFragment> {
    const maybeTarget = maybeTargetForEvent(traceParseData, event);
    const contentHelper = new TimelineDetailsContentHelper(maybeTarget, linkifier);

    const category = TimelineUIUtils.syntheticNetworkRequestCategory(event);
    const color = TimelineUIUtils.networkCategoryColor(category);
    contentHelper.addSection(i18nString(UIStrings.networkRequest), color);

    const options = {
      tabStop: true,
      showColumnNumber: false,
      inlineFrameIndex: 0,
    };
    contentHelper.appendElementRow(
        i18n.i18n.lockedString('URL'),
        LegacyComponents.Linkifier.Linkifier.linkifyURL(
            event.args.data.url as Platform.DevToolsPath.UrlString, options));

    // The time from queueing the request until resource processing is finished.
    const fullDuration = event.dur;
    if (isFinite(fullDuration)) {
      let textRow = TraceEngine.Helpers.Timing.formatMicrosecondsTime(fullDuration);
      // The time from queueing the request until the download is finished. This
      // corresponds to the total time reported for the request in the network tab.
      const networkDuration = event.args.data.syntheticData.finishTime - event.ts;
      // The time it takes to make the resource available to the renderer process.
      const processingDuration = event.ts + event.dur - event.args.data.syntheticData.finishTime;
      if (isFinite(networkDuration) && isFinite(processingDuration)) {
        const networkDurationStr =
            TraceEngine.Helpers.Timing.formatMicrosecondsTime(networkDuration as TraceEngine.Types.Timing.MicroSeconds);
        const processingDurationStr = TraceEngine.Helpers.Timing.formatMicrosecondsTime(
            processingDuration as TraceEngine.Types.Timing.MicroSeconds);
        const cached = event.args.data.syntheticData.isMemoryCached || event.args.data.syntheticData.isDiskCached;
        const cacheOrNetworkLabel =
            cached ? i18nString(UIStrings.loadFromCache) : i18nString(UIStrings.networkTransfer);
        textRow += i18nString(
            UIStrings.SSSResourceLoading,
            {PH1: networkDurationStr, PH2: cacheOrNetworkLabel, PH3: processingDurationStr});
      }
      contentHelper.appendTextRow(i18nString(UIStrings.duration), textRow);
    }

    if (event.args.data.requestMethod) {
      contentHelper.appendTextRow(i18nString(UIStrings.requestMethod), event.args.data.requestMethod);
    }

    if (event.args.data.initialPriority) {
      const initialPriority = PerfUI.NetworkPriorities.uiLabelForNetworkPriority(event.args.data.initialPriority);
      contentHelper.appendTextRow(i18nString(UIStrings.initialPriority), initialPriority);
    }

    const priority = PerfUI.NetworkPriorities.uiLabelForNetworkPriority(event.args.data.priority);

    contentHelper.appendTextRow(i18nString(UIStrings.priority), priority);

    if (event.args.data.mimeType) {
      contentHelper.appendTextRow(i18nString(UIStrings.mimeType), event.args.data.mimeType);
    }
    let lengthText = '';
    if (event.args.data.syntheticData.isMemoryCached) {
      lengthText += i18nString(UIStrings.FromMemoryCache);
    } else if (event.args.data.syntheticData.isDiskCached) {
      lengthText += i18nString(UIStrings.FromCache);
    } else if (event.args.data.timing?.pushStart) {
      lengthText += i18nString(UIStrings.FromPush);
    }
    if (event.args.data.fromServiceWorker) {
      lengthText += i18nString(UIStrings.FromServiceWorker);
    }
    if (event.args.data.encodedDataLength || !lengthText) {
      lengthText = `${Platform.NumberUtilities.bytesToString(event.args.data.encodedDataLength)}${lengthText}`;
    }
    contentHelper.appendTextRow(i18nString(UIStrings.encodedData), lengthText);
    if (event.args.data.decodedBodyLength) {
      contentHelper.appendTextRow(
          i18nString(UIStrings.decodedBody), Platform.NumberUtilities.bytesToString(event.args.data.decodedBodyLength));
    }
    const title = i18nString(UIStrings.initiatedBy);

    const topFrame = TraceEngine.Legacy.eventIsFromNewEngine(event) ?
        TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(event)?.at(0) :
        null;
    if (topFrame) {
      const link = linkifier.maybeLinkifyConsoleCallFrame(
          maybeTarget, topFrame, {tabStop: true, inlineFrameIndex: 0, showColumnNumber: true});
      if (link) {
        contentHelper.appendElementRow(title, link);
      }
    }

    if (!requestPreviewElements.get(event) && event.args.data.url && maybeTarget) {
      const previewElement =
          (await LegacyComponents.ImagePreview.ImagePreview.build(
               maybeTarget, event.args.data.url as Platform.DevToolsPath.UrlString, false, {
                 imageAltText: LegacyComponents.ImagePreview.ImagePreview.defaultAltTextForImageURL(
                     event.args.data.url as Platform.DevToolsPath.UrlString),
                 precomputedFeatures: undefined,
               }) as HTMLImageElement);

      requestPreviewElements.set(event, previewElement);
    }

    const requestPreviewElement = requestPreviewElements.get(event);
    if (requestPreviewElement) {
      contentHelper.appendElementRow(i18nString(UIStrings.preview), requestPreviewElement);
    }
    return contentHelper.fragment;
  }

  private static renderEventJson(
      event: TraceEngine.Legacy.CompatibleTraceEvent, contentHelper: TimelineDetailsContentHelper): void {
    contentHelper.addSection(i18nString(UIStrings.traceEvent));

    const eventWithArgsFirst = {
      ...{args: event.args},
      ...event,
    };
    const indentLength = Common.Settings.Settings.instance().moduleSetting('text-editor-indent').get().length;
    // Elide if the data is huge. Then remove the initial new-line for a denser UI
    const eventStr = JSON.stringify(eventWithArgsFirst, null, indentLength).slice(0, 3000).replace(/{\n  /, '{ ');

    // Use CodeHighlighter for syntax highlighting.
    const highlightContainer = document.createElement('div');
    const shadowRoot = highlightContainer.attachShadow({mode: 'open'});
    shadowRoot.adoptedStyleSheets = [inspectorCommonStyles, codeHighlighterStyles];
    const elem = shadowRoot.createChild('div');
    elem.classList.add('monospace', 'source-code');
    elem.textContent = eventStr;
    void CodeHighlighter.CodeHighlighter.highlightNode(elem, 'text/javascript');
    contentHelper.appendElementRow('', highlightContainer);
  }

  static stackTraceFromCallFrames(callFrames: Protocol.Runtime.CallFrame[]|
                                  TraceEngine.Types.TraceEvents.TraceEventCallFrame[]): Protocol.Runtime.StackTrace {
    return {callFrames: callFrames} as Protocol.Runtime.StackTrace;
  }

  private static async generateCauses(
      event: TraceEngine.Types.TraceEvents.TraceEventData, contentHelper: TimelineDetailsContentHelper,
      traceParseData: TraceEngine.Handlers.Types.TraceParseData): Promise<void> {
    const {startTime} = TraceEngine.Legacy.timesForEventInMilliseconds(event);
    let initiatorStackLabel = i18nString(UIStrings.initiatorStackTrace);
    let stackLabel = i18nString(UIStrings.stackTrace);

    switch (event.name) {
      case TraceEngine.Types.TraceEvents.KnownEventName.TimerFire:
        initiatorStackLabel = i18nString(UIStrings.timerInstalled);
        break;
      case TraceEngine.Types.TraceEvents.KnownEventName.FireAnimationFrame:
        initiatorStackLabel = i18nString(UIStrings.animationFrameRequested);
        break;
      case TraceEngine.Types.TraceEvents.KnownEventName.FireIdleCallback:
        initiatorStackLabel = i18nString(UIStrings.idleCallbackRequested);
        break;
      case TraceEngine.Types.TraceEvents.KnownEventName.UpdateLayoutTree:
        initiatorStackLabel = i18nString(UIStrings.firstInvalidated);
        stackLabel = i18nString(UIStrings.recalculationForced);
        break;
      case TraceEngine.Types.TraceEvents.KnownEventName.Layout:
        initiatorStackLabel = i18nString(UIStrings.firstLayoutInvalidation);
        stackLabel = i18nString(UIStrings.layoutForced);
        break;
    }

    const stackTrace = TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(event);
    if (stackTrace && stackTrace.length) {
      contentHelper.addSection(stackLabel);
      contentHelper.createChildStackTraceElement(TimelineUIUtils.stackTraceFromCallFrames(stackTrace));
    }

    const initiator = traceParseData.Initiators.eventToInitiator.get(event);
    const initiatorFor = traceParseData.Initiators.initiatorToEvents.get(event);
    const invalidations = traceParseData.Invalidations.invalidationsForEvent.get(event);

    if (initiator) {
      // If we have an initiator for the event, we can show its stack trace, a link to reveal the initiator,
      // and the time since the initiator (Pending For).
      const stackTrace = TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(initiator);
      if (stackTrace) {
        contentHelper.addSection(initiatorStackLabel);
        contentHelper.createChildStackTraceElement(TimelineUIUtils.stackTraceFromCallFrames(stackTrace.map(frame => {
          return {
            ...frame,
            scriptId: String(frame.scriptId) as Protocol.Runtime.ScriptId,
          };
        })));
      }

      const link = this.createEntryLink(initiator);
      contentHelper.appendElementRow(i18nString(UIStrings.initiatedBy), link);

      const {startTime: initiatorStartTime} = TraceEngine.Legacy.timesForEventInMilliseconds(initiator);
      const delay = startTime - initiatorStartTime;
      contentHelper.appendTextRow(i18nString(UIStrings.pendingFor), i18n.TimeUtilities.preciseMillisToString(delay, 1));
    }

    if (initiatorFor) {
      // If the event is an initiator for some entries, add links to reveal them.
      const links = document.createElement('div');
      initiatorFor.map((initiator, i) => {
        links.appendChild(this.createEntryLink(initiator));
        // Add space between each link if it's not last
        if (i < initiatorFor.length - 1) {
          links.append(' ');
        }
      });
      contentHelper.appendElementRow(UIStrings.initiatorFor, links);
    }

    if (invalidations && invalidations.length) {
      contentHelper.addSection(i18nString(UIStrings.invalidations));
      await TimelineUIUtils.generateInvalidationsList(invalidations, contentHelper);
    }
  }

  private static createEntryLink(entry: TraceEngine.Types.TraceEvents.TraceEventData): HTMLElement {
    const link = document.createElement('span');

    const traceBoundsState = TraceBounds.TraceBounds.BoundsManager.instance().state();

    if (!traceBoundsState) {
      console.error('Tried to link to an entry without any traceBoundsState. This should never happen.');
      return link;
    }

    // Check is the entry is outside of the current breadcrumb. If it is, don't create a link to navigate to it because there is no way to navigate outside breadcrumb without removing it. Instead, just display the name and "outside breadcrumb" text
    // Consider entry outside breadcrumb only if it is fully outside. If a part of it is visible, we can still select it.
    const isEntryOutsideBreadcrumb = traceBoundsState.micro.minimapTraceBounds.min > entry.ts + (entry.dur || 0) ||
        traceBoundsState.micro.minimapTraceBounds.max < entry.ts;

    // Check if it is in the hidden array
    const isEntryHidden =
        AnnotationsManager.AnnotationsManager.AnnotationsManager.maybeInstance()?.getEntriesFilter().inEntryInvisible(
            entry);

    if (!isEntryOutsideBreadcrumb) {
      link.classList.add('devtools-link');
      UI.ARIAUtils.markAsLink(link);
      link.tabIndex = 0;
      link.addEventListener('click', () => {
        TimelinePanel.instance().select(TimelineSelection.fromTraceEvent((entry)));
      });

      link.addEventListener('keydown', event => {
        if (event.key === 'Enter') {
          TimelinePanel.instance().select(TimelineSelection.fromTraceEvent((entry)));
          event.consume(true);
        }
      });
    }

    if (isEntryHidden) {
      link.textContent = this.eventTitle(entry) + ' ' + i18nString(UIStrings.entryIsHidden);
    } else if (isEntryOutsideBreadcrumb) {
      link.textContent = this.eventTitle(entry) + ' ' + i18nString(UIStrings.outsideBreadcrumbRange);
    } else {
      link.textContent = this.eventTitle(entry);
    }

    return link;
  }

  private static async generateInvalidationsList(
      invalidations: TraceEngine.Types.TraceEvents.SyntheticInvalidation[],
      contentHelper: TimelineDetailsContentHelper): Promise<void> {
    const {groupedByReason, backendNodeIds} = TimelineComponents.DetailsView.generateInvalidationsList(invalidations);

    let relatedNodesMap: Map<number, SDK.DOMModel.DOMNode|null>|null = null;
    const target = SDK.TargetManager.TargetManager.instance().primaryPageTarget();
    const domModel = target?.model(SDK.DOMModel.DOMModel);
    if (domModel) {
      relatedNodesMap = await domModel.pushNodesByBackendIdsToFrontend(backendNodeIds);
    }

    Object.keys(groupedByReason).forEach(reason => {
      TimelineUIUtils.generateInvalidationsForReason(reason, groupedByReason[reason], relatedNodesMap, contentHelper);
    });
  }

  private static generateInvalidationsForReason(
      reason: string, invalidations: TraceEngine.Types.TraceEvents.SyntheticInvalidation[],
      relatedNodesMap: Map<number, SDK.DOMModel.DOMNode|null>|null, contentHelper: TimelineDetailsContentHelper): void {
    function createLinkForInvalidationNode(invalidation: TraceEngine.Types.TraceEvents.SyntheticInvalidation):
        HTMLSpanElement {
      const node = (invalidation.nodeId && relatedNodesMap) ? relatedNodesMap.get(invalidation.nodeId) : null;
      if (node) {
        const nodeSpan = document.createElement('span');
        void Common.Linkifier.Linkifier.linkify(node).then(link => nodeSpan.appendChild(link));
        return nodeSpan;
      }
      if (invalidation.nodeName) {
        const nodeSpan = document.createElement('span');
        nodeSpan.textContent = invalidation.nodeName;
        return nodeSpan;
      }
      const nodeSpan = document.createElement('span');
      UI.UIUtils.createTextChild(nodeSpan, i18nString(UIStrings.UnknownNode));
      return nodeSpan;
    }

    const generatedItems = new Set<string>();

    for (const invalidation of invalidations) {
      const stackTrace = TraceEngine.Helpers.Trace.getZeroIndexedStackTraceForEvent(invalidation);
      let scriptLink: HTMLElement|null = null;
      const callFrame = stackTrace?.at(0);
      if (callFrame) {
        scriptLink = contentHelper.linkifier()?.maybeLinkifyScriptLocation(
                         SDK.TargetManager.TargetManager.instance().rootTarget(),
                         callFrame.scriptId as Protocol.Runtime.ScriptId,
                         callFrame.url as Platform.DevToolsPath.UrlString,
                         callFrame.lineNumber,
                         ) ||
            null;
      }

      const niceNodeLink = createLinkForInvalidationNode(invalidation);

      const text = scriptLink ?
          i18n.i18n.getFormatLocalizedString(
              str_, UIStrings.invalidationWithCallFrame, {PH1: niceNodeLink, PH2: scriptLink}) as HTMLElement :
          niceNodeLink;

      // Sometimes we can get different Invalidation events which cause
      // the same text for the same element for the same reason to be
      // generated. Rather than show the user duplicates, if we have
      // generated text that looks identical to this before, we will
      // bail.
      const generatedText: string = (typeof text === 'string' ? text : text.innerText);
      if (generatedItems.has(generatedText)) {
        continue;
      }

      generatedItems.add(generatedText);
      contentHelper.appendElementRow(reason, text);
    }
  }

  private static aggregatedStatsForTraceEvent(
      total: {
        [x: string]: number,
      },
      traceParseData: TraceEngine.Handlers.Types.TraceParseData,
      event: TraceEngine.Legacy.CompatibleTraceEvent): boolean {
    const events = traceParseData.Renderer?.allTraceEntries || [];
    const {startTime, endTime} = TraceEngine.Legacy.timesForEventInMilliseconds(event);
    function eventComparator(startTime: number, e: TraceEngine.Types.TraceEvents.TraceEventData): number {
      const {startTime: eventStartTime} = TraceEngine.Legacy.timesForEventInMilliseconds(e);
      return startTime - eventStartTime;
    }

    const index = Platform.ArrayUtilities.binaryIndexOf(events, startTime, eventComparator);
    // Not a main thread event?
    if (index < 0) {
      return false;
    }
    let hasChildren = false;
    if (endTime) {
      for (let i = index; i < events.length; i++) {
        const nextEvent = events[i];
        const {startTime: nextEventStartTime, selfTime: nextEventSelfTime} =
            TraceEngine.Legacy.timesForEventInMilliseconds(nextEvent);
        if (nextEventStartTime >= endTime) {
          break;
        }
        if (!nextEvent.selfTime) {
          continue;
        }
        if (TraceEngine.Legacy.threadIDForEvent(nextEvent) !== TraceEngine.Legacy.threadIDForEvent(event)) {
          continue;
        }
        if (i > index) {
          hasChildren = true;
        }
        const categoryName = TimelineUIUtils.eventStyle(nextEvent).category.name;
        total[categoryName] = (total[categoryName] || 0) + nextEventSelfTime;
      }
    }
    if (TraceEngine.Types.TraceEvents.isAsyncPhase(TraceEngine.Legacy.phaseForEvent(event))) {
      if (endTime) {
        let aggregatedTotal = 0;
        for (const categoryName in total) {
          aggregatedTotal += total[categoryName];
        }
        total['idle'] = Math.max(0, endTime - startTime - aggregatedTotal);
      }
      return false;
    }
    return hasChildren;
  }

  static async buildPicturePreviewContent(
      traceData: TraceEngine.Handlers.Types.TraceParseData, event: TraceEngine.Types.TraceEvents.TraceEventPaint,
      target: SDK.Target.Target): Promise<Element|null> {
    const snapshotEvent = traceData.LayerTree.paintsToSnapshots.get(event);
    if (!snapshotEvent) {
      return null;
    }

    const paintProfilerModel = target.model(SDK.PaintProfiler.PaintProfilerModel);
    if (!paintProfilerModel) {
      return null;
    }
    const snapshot = await paintProfilerModel.loadSnapshot(snapshotEvent.args.snapshot.skp64);
    if (!snapshot) {
      return null;
    }

    const snapshotWithRect = {
      snapshot,
      rect: snapshotEvent.args.snapshot.params?.layer_rect,
    };

    if (!snapshotWithRect) {
      return null;
    }
    const imageURLPromise = snapshotWithRect.snapshot.replay();
    snapshotWithRect.snapshot.release();
    const imageURL = await imageURLPromise as Platform.DevToolsPath.UrlString;
    if (!imageURL) {
      return null;
    }
    const stylesContainer = document.createElement('div');
    const shadowRoot = stylesContainer.attachShadow({mode: 'open'});
    shadowRoot.adoptedStyleSheets = [imagePreviewStyles];
    const container = shadowRoot.createChild('div') as HTMLDivElement;
    container.classList.add('image-preview-container', 'vbox', 'link');
    const img = (container.createChild('img') as HTMLImageElement);
    img.src = imageURL;
    img.alt = LegacyComponents.ImagePreview.ImagePreview.defaultAltTextForImageURL(imageURL);
    const paintProfilerButton = container.createChild('a');
    paintProfilerButton.textContent = i18nString(UIStrings.paintProfiler);
    UI.ARIAUtils.markAsLink(container);
    container.tabIndex = 0;
    container.addEventListener(
        'click', () => TimelinePanel.instance().select(TimelineSelection.fromTraceEvent(event)), false);
    container.addEventListener('keydown', keyEvent => {
      if (keyEvent.key === 'Enter') {
        TimelinePanel.instance().select(TimelineSelection.fromTraceEvent(event));
        keyEvent.consume(true);
      }
    });
    return stylesContainer;
  }

  static createEventDivider(event: TraceEngine.Legacy.CompatibleTraceEvent, zeroTime: number): Element {
    const eventDivider = document.createElement('div');
    eventDivider.classList.add('resources-event-divider');
    const {startTime: eventStartTime} = TraceEngine.Legacy.timesForEventInMilliseconds(event);

    const startTime = i18n.TimeUtilities.millisToString(eventStartTime - zeroTime);
    UI.Tooltip.Tooltip.install(
        eventDivider, i18nString(UIStrings.sAtS, {PH1: TimelineUIUtils.eventTitle(event), PH2: startTime}));
    const style = TimelineUIUtils.markerStyleForEvent(event);
    if (style.tall) {
      eventDivider.style.backgroundColor = style.color;
    }
    return eventDivider;
  }

  static visibleEventsFilter(): TimelineModel.TimelineModelFilter.TimelineModelFilter {
    return new TimelineModel.TimelineModelFilter.TimelineVisibleEventsFilter(visibleTypes());
  }

  // Included only for layout tests.
  // TODO(crbug.com/1386091): Fix/port layout tests and remove.
  static categories(): CategoryPalette {
    return getCategoryStyles();
  }

  static generatePieChart(
      aggregatedStats: {
        [x: string]: number,
      },
      selfCategory?: TimelineCategory, selfTime?: number): Element {
    let total = 0;
    for (const categoryName in aggregatedStats) {
      total += aggregatedStats[categoryName];
    }

    const element = document.createElement('div');
    element.classList.add('timeline-details-view-pie-chart-wrapper');
    element.classList.add('hbox');

    const pieChart = new PerfUI.PieChart.PieChart();
    const slices: {
      value: number,
      color: string,
      title: string,
    }[] = [];

    function appendLegendRow(name: string, title: string, value: number, color: string): void {
      if (!value) {
        return;
      }
      slices.push({value, color, title});
    }

    // In case of self time, first add self, then children of the same category.
    if (selfCategory) {
      if (selfTime) {
        appendLegendRow(
            selfCategory.name, i18nString(UIStrings.sSelf, {PH1: selfCategory.title}), selfTime,
            selfCategory.getCSSValue());
      }
      // Children of the same category.
      const categoryTime = aggregatedStats[selfCategory.name];
      const value = categoryTime - (selfTime || 0);
      if (value > 0) {
        appendLegendRow(
            selfCategory.name, i18nString(UIStrings.sChildren, {PH1: selfCategory.title}), value,
            selfCategory.getCSSValue());
      }
    }

    // Add other categories.
    for (const categoryName in getCategoryStyles()) {
      const category = getCategoryStyles()[categoryName as keyof CategoryPalette];
      if (categoryName === selfCategory?.name) {
        // Do not add an entry for this event's self category because 2
        // entries for it where added just before this for loop (for
        // self and children times).
        continue;
      }
      appendLegendRow(category.name, category.title, aggregatedStats[category.name], category.getCSSValue());
    }

    pieChart.data = {
      chartName: i18nString(UIStrings.timeSpentInRendering),
      size: 110,
      formatter: (value: number) => i18n.TimeUtilities.preciseMillisToString(value),
      showLegend: true,
      total,
      slices,
    };
    const pieChartContainer = element.createChild('div', 'vbox');
    pieChartContainer.appendChild(pieChart);

    return element;
  }

  static generateDetailsContentForFrame(
      frame: TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame, filmStrip: TraceEngine.Extras.FilmStrip.Data|null,
      filmStripFrame: TraceEngine.Extras.FilmStrip.Frame|null): DocumentFragment {
    const contentHelper = new TimelineDetailsContentHelper(null, null);
    contentHelper.addSection(i18nString(UIStrings.frame));

    const duration = TimelineUIUtils.frameDuration(frame);
    contentHelper.appendElementRow(i18nString(UIStrings.duration), duration);
    if (filmStrip && filmStripFrame) {
      const filmStripPreview = document.createElement('div');
      filmStripPreview.classList.add('timeline-filmstrip-preview');
      void UI.UIUtils.loadImage(filmStripFrame.screenshotEvent.args.dataUri)
          .then(image => image && filmStripPreview.appendChild(image));
      contentHelper.appendElementRow('', filmStripPreview);
      filmStripPreview.addEventListener('click', frameClicked.bind(null, filmStrip, filmStripFrame), false);
    }

    function frameClicked(
        filmStrip: TraceEngine.Extras.FilmStrip.Data, filmStripFrame: TraceEngine.Extras.FilmStrip.Frame): void {
      PerfUI.FilmStripView.Dialog.fromFilmStrip(filmStrip, filmStripFrame.index);
    }

    return contentHelper.fragment;
  }

  static frameDuration(frame: TraceEngine.Handlers.ModelHandlers.Frames.TimelineFrame): Element {
    const offsetMilli = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(frame.startTimeOffset);
    const durationMilli = TraceEngine.Helpers.Timing.microSecondsToMilliseconds(
        TraceEngine.Types.Timing.MicroSeconds(frame.endTime - frame.startTime));

    const durationText = i18nString(UIStrings.sAtSParentheses, {
      PH1: i18n.TimeUtilities.millisToString(durationMilli, true),
      PH2: i18n.TimeUtilities.millisToString(offsetMilli, true),
    });
    return i18n.i18n.getFormatLocalizedString(str_, UIStrings.emptyPlaceholder, {PH1: durationText});
  }

  static quadWidth(quad: number[]): number {
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[2], 2) + Math.pow(quad[1] - quad[3], 2)));
  }

  static quadHeight(quad: number[]): number {
    return Math.round(Math.sqrt(Math.pow(quad[0] - quad[6], 2) + Math.pow(quad[1] - quad[7], 2)));
  }

  static eventDispatchDesciptors(): EventDispatchTypeDescriptor[] {
    if (eventDispatchDesciptors) {
      return eventDispatchDesciptors;
    }
    const lightOrange = 'hsl(40,100%,80%)';
    const orange = 'hsl(40,100%,50%)';
    const green = 'hsl(90,100%,40%)';
    const purple = 'hsl(256,100%,75%)';
    eventDispatchDesciptors = [
      new EventDispatchTypeDescriptor(
          1, lightOrange, ['mousemove', 'mouseenter', 'mouseleave', 'mouseout', 'mouseover']),
      new EventDispatchTypeDescriptor(
          1, lightOrange, ['pointerover', 'pointerout', 'pointerenter', 'pointerleave', 'pointermove']),
      new EventDispatchTypeDescriptor(2, green, ['wheel']),
      new EventDispatchTypeDescriptor(3, orange, ['click', 'mousedown', 'mouseup']),
      new EventDispatchTypeDescriptor(3, orange, ['touchstart', 'touchend', 'touchmove', 'touchcancel']),
      new EventDispatchTypeDescriptor(
          3, orange, ['pointerdown', 'pointerup', 'pointercancel', 'gotpointercapture', 'lostpointercapture']),
      new EventDispatchTypeDescriptor(3, purple, ['keydown', 'keyup', 'keypress']),
    ];
    return eventDispatchDesciptors;
  }

  static markerShortTitle(event: TraceEngine.Legacy.Event): string|null {
    const recordTypes = TimelineModel.TimelineModel.RecordType;
    switch (event.name) {
      case recordTypes.MarkDOMContent:
        return i18n.i18n.lockedString('DCL');
      case recordTypes.MarkLoad:
        return i18n.i18n.lockedString('L');
      case recordTypes.MarkFirstPaint:
        return i18n.i18n.lockedString('FP');
      case recordTypes.MarkFCP:
        return i18n.i18n.lockedString('FCP');
      case recordTypes.MarkLCPCandidate:
        return i18n.i18n.lockedString('LCP');
    }
    return null;
  }

  static markerStyleForEvent(event: TraceEngine.Legacy.Event|
                             TraceEngine.Types.TraceEvents.TraceEventData): TimelineMarkerStyle {
    const tallMarkerDashStyle = [6, 4];
    const title = TimelineUIUtils.eventTitle(event);
    const recordTypes = TimelineModel.TimelineModel.RecordType;

    if (event.name !== recordTypes.NavigationStart &&
        (TraceEngine.Legacy.eventHasCategory(event, TimelineModel.TimelineModel.TimelineModelImpl.Category.Console) ||
         TraceEngine.Legacy.eventHasCategory(
             event, TimelineModel.TimelineModel.TimelineModelImpl.Category.UserTiming))) {
      return {
        title: title,
        dashStyle: tallMarkerDashStyle,
        lineWidth: 0.5,
        color: TraceEngine.Legacy.eventHasCategory(
                   event, TimelineModel.TimelineModel.TimelineModelImpl.Category.UserTiming) ?
            'purple' :
            'orange',
        tall: false,
        lowPriority: false,
      };
    }
    let tall = false;
    let color = 'grey';
    switch (event.name) {
      case recordTypes.NavigationStart:
        color = '#FF9800';
        tall = true;
        break;
      case recordTypes.FrameStartedLoading:
        color = 'green';
        tall = true;
        break;
      case recordTypes.MarkDOMContent:
        color = '#0867CB';
        tall = true;
        break;
      case recordTypes.MarkLoad:
        color = '#B31412';
        tall = true;
        break;
      case recordTypes.MarkFirstPaint:
        color = '#228847';
        tall = true;
        break;
      case recordTypes.MarkFCP:
        color = '#1A6937';
        tall = true;
        break;
      case recordTypes.MarkLCPCandidate:
        color = '#1A3422';
        tall = true;
        break;
      case recordTypes.TimeStamp:
        color = 'orange';
        break;
    }
    return {
      title: title,
      dashStyle: tallMarkerDashStyle,
      lineWidth: 0.5,
      color: color,
      tall: tall,
      lowPriority: false,
    };
  }

  static colorForId(id: string): string {
    if (!colorGenerator) {
      colorGenerator =
          new Common.Color.Generator({min: 30, max: 330, count: undefined}, {min: 50, max: 80, count: 3}, 85);
      colorGenerator.setColorForID('', '#f2ecdc');
    }
    return colorGenerator.colorForID(id);
  }

  static displayNameForFrame(frame: TraceEngine.Types.TraceEvents.TraceFrame, trimAt: number = 80): string {
    const url = frame.url as Platform.DevToolsPath.UrlString;
    return Common.ParsedURL.schemeIs(url, 'about:') ? `"${Platform.StringUtilities.trimMiddle(frame.name, trimAt)}"` :
                                                      frame.url.slice(0, trimAt);
  }
}

export const enum NetworkCategory {
  HTML = 'HTML',
  Script = 'Script',
  Style = 'Style',
  Media = 'Media',
  Other = 'Other',
}

export const aggregatedStatsKey = Symbol('aggregatedStats');

export const previewElementSymbol = Symbol('previewElement');

export class EventDispatchTypeDescriptor {
  priority: number;
  color: string;
  eventTypes: string[];

  constructor(priority: number, color: string, eventTypes: string[]) {
    this.priority = priority;
    this.color = color;
    this.eventTypes = eventTypes;
  }
}

export class TimelineDetailsContentHelper {
  fragment: DocumentFragment;
  private linkifierInternal: LegacyComponents.Linkifier.Linkifier|null;
  private target: SDK.Target.Target|null;
  element: HTMLDivElement;
  private tableElement: HTMLElement;

  constructor(target: SDK.Target.Target|null, linkifier: LegacyComponents.Linkifier.Linkifier|null) {
    this.fragment = document.createDocumentFragment();

    this.linkifierInternal = linkifier;
    this.target = target;

    this.element = document.createElement('div');
    this.element.classList.add('timeline-details-view-block');
    this.tableElement = this.element.createChild('div', 'vbox timeline-details-chip-body');
    this.fragment.appendChild(this.element);
  }

  addSection(title: string, swatchColor?: string): void {
    if (!this.tableElement.hasChildNodes()) {
      this.element.removeChildren();
    } else {
      this.element = document.createElement('div');
      this.element.classList.add('timeline-details-view-block');
      this.fragment.appendChild(this.element);
    }

    if (title) {
      const titleElement = this.element.createChild('div', 'timeline-details-chip-title');
      if (swatchColor) {
        titleElement.createChild('div').style.backgroundColor = swatchColor;
      }
      UI.UIUtils.createTextChild(titleElement, title);
    }

    this.tableElement = this.element.createChild('div', 'vbox timeline-details-chip-body');
    this.fragment.appendChild(this.element);
  }

  linkifier(): LegacyComponents.Linkifier.Linkifier|null {
    return this.linkifierInternal;
  }

  appendTextRow(title: string, value: string|number|boolean): void {
    const rowElement = this.tableElement.createChild('div', 'timeline-details-view-row');
    rowElement.createChild('div', 'timeline-details-view-row-title').textContent = title;
    rowElement.createChild('div', 'timeline-details-view-row-value').textContent = value.toString();
  }

  appendElementRow(title: string, content: string|Node, isWarning?: boolean, isStacked?: boolean): void {
    const rowElement = this.tableElement.createChild('div', 'timeline-details-view-row');
    rowElement.setAttribute('data-row-title', title);
    if (isWarning) {
      rowElement.classList.add('timeline-details-warning');
    }
    if (isStacked) {
      rowElement.classList.add('timeline-details-stack-values');
    }
    const titleElement = rowElement.createChild('div', 'timeline-details-view-row-title');
    titleElement.textContent = title;
    const valueElement = rowElement.createChild('div', 'timeline-details-view-row-value');
    if (content instanceof Node) {
      valueElement.appendChild(content);
    } else {
      UI.UIUtils.createTextChild(valueElement, content || '');
    }
  }

  appendLocationRow(title: string, url: string, startLine: number, startColumn?: number): void {
    if (!this.linkifierInternal) {
      return;
    }

    const options = {
      tabStop: true,
      columnNumber: startColumn,
      showColumnNumber: true,
      inlineFrameIndex: 0,
    };
    const link = this.linkifierInternal.maybeLinkifyScriptLocation(
        this.target, null, url as Platform.DevToolsPath.UrlString, startLine, options);
    if (!link) {
      return;
    }
    this.appendElementRow(title, link);
  }

  appendLocationRange(title: string, url: Platform.DevToolsPath.UrlString, startLine: number, endLine?: number): void {
    if (!this.linkifierInternal || !this.target) {
      return;
    }
    const locationContent = document.createElement('span');
    const link = this.linkifierInternal.maybeLinkifyScriptLocation(
        this.target, null, url, startLine, {tabStop: true, inlineFrameIndex: 0});
    if (!link) {
      return;
    }
    locationContent.appendChild(link);
    UI.UIUtils.createTextChild(
        locationContent, Platform.StringUtilities.sprintf(' [%s…%s]', startLine + 1, (endLine || 0) + 1 || ''));
    this.appendElementRow(title, locationContent);
  }

  createChildStackTraceElement(stackTrace: Protocol.Runtime.StackTrace): void {
    if (!this.linkifierInternal) {
      return;
    }

    const stackTraceElement =
        this.tableElement.createChild('div', 'timeline-details-view-row timeline-details-stack-values');
    const callFrameContents = LegacyComponents.JSPresentationUtils.buildStackTracePreviewContents(
        this.target, this.linkifierInternal, {stackTrace, tabStops: true, showColumnNumber: true});
    stackTraceElement.appendChild(callFrameContents.element);
  }
}

export const categoryBreakdownCacheSymbol = Symbol('categoryBreakdownCache');
export interface TimelineMarkerStyle {
  title: string;
  color: string;
  lineWidth: number;
  dashStyle: number[];
  tall: boolean;
  lowPriority: boolean;
}

/**
 * Given a particular event, this method can adjust its timestamp by
 * substracting the timestamp of the previous navigation. This helps in cases
 * where the user has navigated multiple times in the trace, so that we can show
 * the LCP (for example) relative to the last navigation.
 **/
export function timeStampForEventAdjustedForClosestNavigationIfPossible(
    event: TraceEngine.Types.TraceEvents.TraceEventData,
    traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null): TraceEngine.Types.Timing.MilliSeconds {
  if (!traceParsedData) {
    const {startTime} = TraceEngine.Legacy.timesForEventInMilliseconds(event);
    return startTime;
  }

  const time = TraceEngine.Helpers.Timing.timeStampForEventAdjustedByClosestNavigation(
      event,
      traceParsedData.Meta.traceBounds,
      traceParsedData.Meta.navigationsByNavigationId,
      traceParsedData.Meta.navigationsByFrameId,
  );
  return TraceEngine.Helpers.Timing.microSecondsToMilliseconds(time);
}

/**
 * Determines if an event is potentially a marker event. A marker event here
 * is a single moment in time that we want to highlight on the timeline, such as
 * the LCP time. This method does not filter out events: for example, it treats
 * every LCP Candidate event as a potential marker event.
 **/
export function isMarkerEvent(
    traceParseData: TraceEngine.Handlers.Types.TraceParseData,
    event: TraceEngine.Types.TraceEvents.TraceEventData): boolean {
  const {KnownEventName} = TraceEngine.Types.TraceEvents;

  if (event.name === KnownEventName.TimeStamp) {
    return true;
  }

  if (TraceEngine.Types.TraceEvents.isTraceEventFirstContentfulPaint(event) ||
      TraceEngine.Types.TraceEvents.isTraceEventFirstPaint(event)) {
    return event.args.frame === traceParseData.Meta.mainFrameId;
  }

  if (TraceEngine.Types.TraceEvents.isTraceEventMarkDOMContent(event) ||
      TraceEngine.Types.TraceEvents.isTraceEventMarkLoad(event) ||
      TraceEngine.Types.TraceEvents.isTraceEventLargestContentfulPaintCandidate(event)) {
    // isOutermostMainFrame was added in 2022, so we fallback to isMainFrame
    // for older traces.
    if (!event.args.data) {
      return false;
    }
    const {isOutermostMainFrame, isMainFrame} = event.args.data;
    if (typeof isOutermostMainFrame !== 'undefined') {
      // If isOutermostMainFrame is defined we want to use that and not
      // fallback to isMainFrame, even if isOutermostMainFrame is false. Hence
      // this check.
      return isOutermostMainFrame;
    }
    return Boolean(isMainFrame);
  }

  return false;
}

// This function only exists to abstract dealing with two different event types
// whilst we are in the middle of the migration. We are working on removing the
// CompatibleTraceEvent type, at which point this method can be removed and we
// can use the method in TargetForEvent.ts directly.
function maybeTargetForEvent(
    traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null,
    event: TraceEngine.Legacy.CompatibleTraceEvent,
    ): SDK.Target.Target|null {
  // Both these conditionals should not happen in theory; all events in the UI
  // now are powered by the new engine. But until we fully remove the old model
  // and its types, we need to satisfy TypeScript.
  if (!TraceEngine.Legacy.eventIsFromNewEngine(event)) {
    return null;
  }
  if (!traceParsedData) {
    return null;
  }

  return targetForEvent(traceParsedData, event);
}

// This function only exists to abstract dealing with two different event types
// whilst we are in the middle of the migration. We are working on removing the
// CompatibleTraceEvent type, at which point this method can be removed and we
// can use the method in TraceEngine.Helpers directly
function getZeroIndexedLineAndColumnNumbersForEvent(event: TraceEngine.Legacy.CompatibleTraceEvent): {
  lineNumber?: number,
  columnNumber?: number,
} {
  if (TraceEngine.Legacy.eventIsFromNewEngine(event)) {
    return TraceEngine.Helpers.Trace.getZeroIndexedLineAndColumnForEvent(event);
  }
  return {
    lineNumber: undefined,
    columnNumber: undefined,
  };
}

// TODO: once the CompatibleTraceEvent type is removed, this function can be
// updated to take only new types.
export function urlForEvent(
    traceParsedData: TraceEngine.Handlers.Types.TraceParseData|null,
    event: TraceEngine.Legacy.CompatibleTraceEvent): Platform.DevToolsPath.UrlString|null {
  if (!traceParsedData || !TraceEngine.Legacy.eventIsFromNewEngine(event)) {
    return null;
  }

  // DecodeImage events use the URL from the relevant PaintImage event.
  if (TraceEngine.Types.TraceEvents.isTraceEventDecodeImage(event)) {
    const paintEvent = traceParsedData.ImagePainting.paintImageForEvent.get(event);
    return paintEvent ? urlForEvent(traceParsedData, paintEvent) : null;
  }

  // DrawLazyPixelRef events use the URL from the relevant PaintImage event.
  if (TraceEngine.Types.TraceEvents.isTraceEventDrawLazyPixelRef(event) && event.args?.LazyPixelRef) {
    const paintEvent = traceParsedData.ImagePainting.paintImageByDrawLazyPixelRef.get(event.args.LazyPixelRef);
    return paintEvent ? urlForEvent(traceParsedData, paintEvent) : null;
  }

  // ParseHTML events store the URL under beginData, not data.
  if (TraceEngine.Types.TraceEvents.isTraceEventParseHTML(event)) {
    return event.args.beginData.url as Platform.DevToolsPath.UrlString;
  }

  // For all other events, try to see if the URL is provided, else return null.
  if (event.args?.data?.url) {
    return event.args.data.url as Platform.DevToolsPath.UrlString;
  }

  return null;
}
