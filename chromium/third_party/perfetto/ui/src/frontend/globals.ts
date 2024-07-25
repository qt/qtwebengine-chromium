// Copyright (C) 2018 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {BigintMath} from '../base/bigint_math';
import {assertExists} from '../base/logging';
import {createStore, Store} from '../base/store';
import {duration, Span, Time, time, TimeSpan} from '../base/time';
import {Actions, DeferredAction} from '../common/actions';
import {AggregateData} from '../common/aggregation_data';
import {Args} from '../common/arg_types';
import {CommandManager} from '../common/commands';
import {
  ConversionJobName,
  ConversionJobStatus,
} from '../common/conversion_jobs';
import {createEmptyState} from '../common/empty_state';
import {
  HighPrecisionTime,
  HighPrecisionTimeSpan,
} from '../common/high_precision_time';
import {MetricResult} from '../common/metric_data';
import {CurrentSearchResults, SearchSummary} from '../common/search_data';
import {
  CallsiteInfo,
  EngineConfig,
  ProfileType,
  RESOLUTION_DEFAULT,
  State,
  getLegacySelection,
} from '../common/state';
import {TabManager} from '../common/tab_registry';
import {TimestampFormat, timestampFormat} from '../core/timestamp_format';
import {TrackManager} from '../common/track_cache';
import {setPerfHooks} from '../core/perf';
import {raf} from '../core/raf_scheduler';
import {Engine} from '../trace_processor/engine';
import {HttpRpcState} from '../trace_processor/http_rpc_engine';

import {Analytics, initAnalytics} from './analytics';
import {Timeline} from './frontend_local_state';
import {Router} from './router';
import {horizontalScrollToTs} from './scroll_helper';
import {ServiceWorkerController} from './service_worker_controller';
import {SliceSqlId} from './sql_types';
import {PxSpan, TimeScale} from './time_scale';
import {SelectionManager, LegacySelection} from '../core/selection_manager';

const INSTANT_FOCUS_DURATION = 1n;
const INCOMPLETE_SLICE_DURATION = 30_000n;

type Dispatch = (action: DeferredAction) => void;
type TrackDataStore = Map<string, {}>;
type QueryResultsStore = Map<string, {} | undefined>;
type AggregateDataStore = Map<string, AggregateData>;
type Description = Map<string, string>;

export interface SliceDetails {
  ts?: time;
  absTime?: string;
  dur?: duration;
  threadTs?: time;
  threadDur?: duration;
  priority?: number;
  endState?: string | null;
  cpu?: number;
  id?: number;
  threadStateId?: number;
  utid?: number;
  wakeupTs?: time;
  wakerUtid?: number;
  wakerCpu?: number;
  category?: string;
  name?: string;
  tid?: number;
  threadName?: string;
  pid?: number;
  processName?: string;
  uid?: number;
  packageName?: string;
  versionCode?: number;
  args?: Args;
  description?: Description;
}

export interface FlowPoint {
  trackId: number;

  sliceName: string;
  sliceCategory: string;
  sliceId: SliceSqlId;
  sliceStartTs: time;
  sliceEndTs: time;
  // Thread and process info. Only set in sliceSelected not in areaSelected as
  // the latter doesn't display per-flow info and it'd be a waste to join
  // additional tables for undisplayed info in that case. Nothing precludes
  // adding this in a future iteration however.
  threadName: string;
  processName: string;

  depth: number;

  // TODO(altimin): Ideally we should have a generic mechanism for allowing to
  // customise the name here, but for now we are hardcording a few
  // Chrome-specific bits in the query here.
  sliceChromeCustomName?: string;
}

export interface Flow {
  id: number;

  begin: FlowPoint;
  end: FlowPoint;
  dur: duration;

  // Whether this flow connects a slice with its descendant.
  flowToDescendant: boolean;

  category?: string;
  name?: string;
}

export interface CounterDetails {
  startTime?: time;
  value?: number;
  delta?: number;
  duration?: duration;
  name?: string;
}

export interface ThreadStateDetails {
  ts?: time;
  dur?: duration;
}

export interface FlamegraphDetails {
  type?: ProfileType;
  id?: number;
  start?: time;
  dur?: duration;
  pids?: number[];
  upids?: number[];
  flamegraph?: CallsiteInfo[];
  expandedCallsite?: CallsiteInfo;
  viewingOption?: string;
  expandedId?: number;
  // isInAreaSelection is true if a flamegraph is part of the current area
  // selection.
  isInAreaSelection?: boolean;
  // When heap_graph_non_finalized_graph has a count >0, we mark the graph
  // as incomplete.
  graphIncomplete?: boolean;
  // About to show a new graph whose data is not ready yet.
  graphLoading?: boolean;
}

export interface CpuProfileDetails {
  id?: number;
  ts?: time;
  utid?: number;
  stack?: CallsiteInfo[];
}

export interface QuantizedLoad {
  start: time;
  end: time;
  load: number;
}
type OverviewStore = Map<string, QuantizedLoad[]>;

export interface ThreadDesc {
  utid: number;
  tid: number;
  threadName: string;
  pid?: number;
  procName?: string;
  cmdline?: string;
}
type ThreadMap = Map<number, ThreadDesc>;

function getRoot() {
  // Works out the root directory where the content should be served from
  // e.g. `http://origin/v1.2.3/`.
  const script = document.currentScript as HTMLScriptElement;

  // Needed for DOM tests, that do not have script element.
  if (script === null) {
    return '';
  }

  let root = script.src;
  root = root.substr(0, root.lastIndexOf('/') + 1);
  return root;
}

// Options for globals.makeSelection().
export interface MakeSelectionOpts {
  // Whether to switch to the current selection tab or not. Default = true.
  switchToCurrentSelectionTab?: boolean;

  // Whether to cancel the current search selection. Default = true.
  clearSearch?: boolean;
}

// All of these control additional things we can do when doing a
// selection.
export interface LegacySelectionArgs {
  clearSearch: boolean;
  switchToCurrentSelectionTab: boolean;
  pendingScrollId: number | undefined;
}

/**
 * Global accessors for state/dispatch in the frontend.
 */
class Globals {
  readonly root = getRoot();

  private _testing = false;
  private _dispatch?: Dispatch = undefined;
  private _store = createStore<State>(createEmptyState());
  private _timeline?: Timeline = undefined;
  private _serviceWorkerController?: ServiceWorkerController = undefined;
  private _logging?: Analytics = undefined;
  private _isInternalUser: boolean | undefined = undefined;

  // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
  private _trackDataStore?: TrackDataStore = undefined;
  private _queryResults?: QueryResultsStore = undefined;
  private _overviewStore?: OverviewStore = undefined;
  private _aggregateDataStore?: AggregateDataStore = undefined;
  private _threadMap?: ThreadMap = undefined;
  private _sliceDetails?: SliceDetails = undefined;
  private _threadStateDetails?: ThreadStateDetails = undefined;
  private _connectedFlows?: Flow[] = undefined;
  private _selectedFlows?: Flow[] = undefined;
  private _visibleFlowCategories?: Map<string, boolean> = undefined;
  private _counterDetails?: CounterDetails = undefined;
  private _flamegraphDetails?: FlamegraphDetails = undefined;
  private _cpuProfileDetails?: CpuProfileDetails = undefined;
  private _numQueriesQueued = 0;
  private _bufferUsage?: number = undefined;
  private _recordingLog?: string = undefined;
  private _traceErrors?: number = undefined;
  private _metricError?: string = undefined;
  private _metricResult?: MetricResult = undefined;
  private _jobStatus?: Map<ConversionJobName, ConversionJobStatus> = undefined;
  private _router?: Router = undefined;
  private _embeddedMode?: boolean = undefined;
  private _hideSidebar?: boolean = undefined;
  private _cmdManager = new CommandManager();
  private _realtimeOffset = Time.ZERO;
  private _utcOffset = Time.ZERO;
  private _traceTzOffset = Time.ZERO;
  private _tabManager = new TabManager();
  private _trackManager = new TrackManager(this._store);
  private _selectionManager = new SelectionManager(this._store);
  private _hasFtrace: boolean = false;

  scrollToTrackKey?: string | number;
  httpRpcState: HttpRpcState = {connected: false};
  newVersionAvailable = false;
  showPanningHint = false;

  // TODO(hjd): Remove once we no longer need to update UUID on redraw.
  private _publishRedraw?: () => void = undefined;

  private _currentSearchResults: CurrentSearchResults = {
    eventIds: new Float64Array(0),
    tses: new BigInt64Array(0),
    utids: new Float64Array(0),
    trackKeys: [],
    sources: [],
    totalResults: 0,
  };
  searchSummary: SearchSummary = {
    tsStarts: new BigInt64Array(0),
    tsEnds: new BigInt64Array(0),
    count: new Uint8Array(0),
  };

  engines = new Map<string, Engine>();

  initialize(dispatch: Dispatch, router: Router) {
    this._dispatch = dispatch;
    this._router = router;
    this._timeline = new Timeline();

    setPerfHooks(
      () => this.state.perfDebug,
      () => this.dispatch(Actions.togglePerfDebug({})),
    );

    this._serviceWorkerController = new ServiceWorkerController();
    this._testing =
      /* eslint-disable @typescript-eslint/strict-boolean-expressions */
      self.location && self.location.search.indexOf('testing=1') >= 0;
    /* eslint-enable */
    this._logging = initAnalytics();

    // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
    this._trackDataStore = new Map<string, {}>();
    this._queryResults = new Map<string, {}>();
    this._overviewStore = new Map<string, QuantizedLoad[]>();
    this._aggregateDataStore = new Map<string, AggregateData>();
    this._threadMap = new Map<number, ThreadDesc>();
    this._sliceDetails = {};
    this._connectedFlows = [];
    this._selectedFlows = [];
    this._visibleFlowCategories = new Map<string, boolean>();
    this._counterDetails = {};
    this._threadStateDetails = {};
    this._flamegraphDetails = {};
    this._cpuProfileDetails = {};
    this.engines.clear();
    this._selectionManager.clear();
  }

  // Only initialises the store - useful for testing.
  initStore(initialState: State) {
    this._store = createStore(initialState);
  }

  get router(): Router {
    return assertExists(this._router);
  }

  get publishRedraw(): () => void {
    return this._publishRedraw || (() => {});
  }

  set publishRedraw(f: () => void) {
    this._publishRedraw = f;
  }

  get state(): State {
    return assertExists(this._store).state;
  }

  get store(): Store<State> {
    return assertExists(this._store);
  }

  get dispatch(): Dispatch {
    return assertExists(this._dispatch);
  }

  dispatchMultiple(actions: DeferredAction[]): void {
    const dispatch = this.dispatch;
    for (const action of actions) {
      dispatch(action);
    }
  }

  get timeline() {
    return assertExists(this._timeline);
  }

  get logging() {
    return assertExists(this._logging);
  }

  get serviceWorkerController() {
    return assertExists(this._serviceWorkerController);
  }

  // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
  get overviewStore(): OverviewStore {
    return assertExists(this._overviewStore);
  }

  get trackDataStore(): TrackDataStore {
    return assertExists(this._trackDataStore);
  }

  get queryResults(): QueryResultsStore {
    return assertExists(this._queryResults);
  }

  get threads() {
    return assertExists(this._threadMap);
  }

  get sliceDetails() {
    return assertExists(this._sliceDetails);
  }

  set sliceDetails(click: SliceDetails) {
    this._sliceDetails = assertExists(click);
  }

  get threadStateDetails() {
    return assertExists(this._threadStateDetails);
  }

  set threadStateDetails(click: ThreadStateDetails) {
    this._threadStateDetails = assertExists(click);
  }

  get connectedFlows() {
    return assertExists(this._connectedFlows);
  }

  set connectedFlows(connectedFlows: Flow[]) {
    this._connectedFlows = assertExists(connectedFlows);
  }

  get selectedFlows() {
    return assertExists(this._selectedFlows);
  }

  set selectedFlows(selectedFlows: Flow[]) {
    this._selectedFlows = assertExists(selectedFlows);
  }

  get visibleFlowCategories() {
    return assertExists(this._visibleFlowCategories);
  }

  set visibleFlowCategories(visibleFlowCategories: Map<string, boolean>) {
    this._visibleFlowCategories = assertExists(visibleFlowCategories);
  }

  get counterDetails() {
    return assertExists(this._counterDetails);
  }

  set counterDetails(click: CounterDetails) {
    this._counterDetails = assertExists(click);
  }

  get aggregateDataStore(): AggregateDataStore {
    return assertExists(this._aggregateDataStore);
  }

  get flamegraphDetails() {
    return assertExists(this._flamegraphDetails);
  }

  set flamegraphDetails(click: FlamegraphDetails) {
    this._flamegraphDetails = assertExists(click);
  }

  get traceErrors() {
    return this._traceErrors;
  }

  setTraceErrors(arg: number) {
    this._traceErrors = arg;
  }

  get metricError() {
    return this._metricError;
  }

  setMetricError(arg: string) {
    this._metricError = arg;
  }

  get metricResult() {
    return this._metricResult;
  }

  setMetricResult(result: MetricResult) {
    this._metricResult = result;
  }

  get cpuProfileDetails() {
    return assertExists(this._cpuProfileDetails);
  }

  set cpuProfileDetails(click: CpuProfileDetails) {
    this._cpuProfileDetails = assertExists(click);
  }

  set numQueuedQueries(value: number) {
    this._numQueriesQueued = value;
  }

  get numQueuedQueries() {
    return this._numQueriesQueued;
  }

  get bufferUsage() {
    return this._bufferUsage;
  }

  get recordingLog() {
    return this._recordingLog;
  }

  get currentSearchResults() {
    return this._currentSearchResults;
  }

  set currentSearchResults(results: CurrentSearchResults) {
    this._currentSearchResults = results;
  }

  set hasFtrace(value: boolean) {
    this._hasFtrace = value;
  }

  get hasFtrace(): boolean {
    return this._hasFtrace;
  }

  getConversionJobStatus(name: ConversionJobName): ConversionJobStatus {
    return this.getJobStatusMap().get(name) || ConversionJobStatus.NotRunning;
  }

  setConversionJobStatus(name: ConversionJobName, status: ConversionJobStatus) {
    const map = this.getJobStatusMap();
    if (status === ConversionJobStatus.NotRunning) {
      map.delete(name);
    } else {
      map.set(name, status);
    }
  }

  private getJobStatusMap(): Map<ConversionJobName, ConversionJobStatus> {
    if (!this._jobStatus) {
      this._jobStatus = new Map();
    }
    return this._jobStatus;
  }

  get embeddedMode(): boolean {
    return !!this._embeddedMode;
  }

  set embeddedMode(value: boolean) {
    this._embeddedMode = value;
  }

  get hideSidebar(): boolean {
    return !!this._hideSidebar;
  }

  set hideSidebar(value: boolean) {
    this._hideSidebar = value;
  }

  setBufferUsage(bufferUsage: number) {
    this._bufferUsage = bufferUsage;
  }

  setTrackData(id: string, data: {}) {
    this.trackDataStore.set(id, data);
  }

  setRecordingLog(recordingLog: string) {
    this._recordingLog = recordingLog;
  }

  setAggregateData(kind: string, data: AggregateData) {
    this.aggregateDataStore.set(kind, data);
  }

  getCurResolution(): duration {
    // Truncate the resolution to the closest power of 2 (in nanosecond space).
    // We choose to work in ns space because resolution is consumed be track
    // controllers for quantization and they rely on resolution to be a power
    // of 2 in nanosecond form. This is property does not hold if we work in
    // second space.
    //
    // This effectively means the resolution changes approximately every 6 zoom
    // levels. Logic: each zoom level represents a delta of 0.1 * (visible
    // window span). Therefore, zooming out by six levels is 1.1^6 ~= 2.
    // Similarily, zooming in six levels is 0.9^6 ~= 0.5.
    const timeScale = this.timeline.visibleTimeScale;
    // TODO(b/186265930): Remove once fixed:
    if (timeScale.pxSpan.delta === 0) {
      console.error(`b/186265930: Bad pxToSec suppressed`);
      return RESOLUTION_DEFAULT;
    }

    const timePerPx = timeScale.pxDeltaToDuration(this.quantPx);

    return BigintMath.bitFloor(timePerPx.toTime('floor'));
  }

  getCurrentEngine(): EngineConfig | undefined {
    return this.state.engine;
  }

  makeSelection(action: DeferredAction<{}>, opts: MakeSelectionOpts = {}) {
    const {switchToCurrentSelectionTab = true, clearSearch = true} = opts;
    const currentSelectionTabUri = 'current_selection';

    // A new selection should cancel the current search selection.
    clearSearch && globals.dispatch(Actions.setSearchIndex({index: -1}));

    if (switchToCurrentSelectionTab) {
      globals.dispatch(Actions.showTab({uri: currentSelectionTabUri}));
    }
    globals.dispatch(action);
  }

  setLegacySelection(
    legacySelection: LegacySelection,
    args: LegacySelectionArgs,
  ): void {
    this._selectionManager.setLegacy(legacySelection);
    if (args.clearSearch) {
      globals.dispatch(Actions.setSearchIndex({index: -1}));
    }
    if (args.pendingScrollId !== undefined) {
      globals.dispatch(
        Actions.setPendingScrollId({
          pendingScrollId: args.pendingScrollId,
        }),
      );
    }
    if (args.switchToCurrentSelectionTab) {
      globals.dispatch(Actions.showTab({uri: 'current_selection'}));
    }
  }

  clearSelection(): void {
    globals.dispatch(Actions.setSearchIndex({index: -1}));
    this._selectionManager.clear();
  }

  resetForTesting() {
    this._dispatch = undefined;
    this._timeline = undefined;
    this._serviceWorkerController = undefined;

    // TODO(hjd): Unify trackDataStore, queryResults, overviewStore, threads.
    this._trackDataStore = undefined;
    this._queryResults = undefined;
    this._overviewStore = undefined;
    this._threadMap = undefined;
    this._sliceDetails = undefined;
    this._threadStateDetails = undefined;
    this._aggregateDataStore = undefined;
    this._numQueriesQueued = 0;
    this._metricResult = undefined;
    this._currentSearchResults = {
      eventIds: new Float64Array(0),
      tses: new BigInt64Array(0),
      utids: new Float64Array(0),
      trackKeys: [],
      sources: [],
      totalResults: 0,
    };
  }

  // This variable is set by the is_internal_user.js script if the user is a
  // googler. This is used to avoid exposing features that are not ready yet
  // for public consumption. The gated features themselves are not secret.
  // If a user has been detected as a Googler once, make that sticky in
  // localStorage, so that we keep treating them as such when they connect over
  // public networks.
  get isInternalUser() {
    if (this._isInternalUser === undefined) {
      this._isInternalUser = localStorage.getItem('isInternalUser') === '1';
    }
    return this._isInternalUser;
  }

  set isInternalUser(value: boolean) {
    localStorage.setItem('isInternalUser', value ? '1' : '0');
    this._isInternalUser = value;
    raf.scheduleFullRedraw();
  }

  get testing() {
    return this._testing;
  }

  // Used when switching to the legacy TraceViewer UI.
  // Most resources are cleaned up by replacing the current |window| object,
  // however pending RAFs and workers seem to outlive the |window| and need to
  // be cleaned up explicitly.
  shutdown() {
    raf.shutdown();
  }

  // Get a timescale that covers the entire trace
  getTraceTimeScale(pxSpan: PxSpan): TimeScale {
    const {start, end} = this.state.traceTime;
    const traceTime = HighPrecisionTimeSpan.fromTime(start, end);
    return TimeScale.fromHPTimeSpan(traceTime, pxSpan);
  }

  // Get the trace time bounds
  stateTraceTime(): Span<HighPrecisionTime> {
    const {start, end} = this.state.traceTime;
    return HighPrecisionTimeSpan.fromTime(start, end);
  }

  stateTraceTimeTP(): Span<time, duration> {
    const {start, end} = this.state.traceTime;
    return new TimeSpan(start, end);
  }

  // Get the state version of the visible time bounds
  stateVisibleTime(): Span<time, duration> {
    const {start, end} = this.state.frontendLocalState.visibleState;
    return new TimeSpan(start, end);
  }

  // How many pixels to use for one quanta of horizontal resolution
  get quantPx(): number {
    const quantPx = (self as {} as {quantPx: number | undefined}).quantPx;
    return quantPx ?? 1;
  }

  get commandManager(): CommandManager {
    return assertExists(this._cmdManager);
  }

  // This is the ts value at the time of the Unix epoch.
  // Normally some large negative value, because the unix epoch is normally in
  // the past compared to ts=0.
  get realtimeOffset(): time {
    return this._realtimeOffset;
  }

  set realtimeOffset(time: time) {
    this._realtimeOffset = time;
  }

  // This is the timestamp that we should use for our offset when in UTC mode.
  // Usually the most recent UTC midnight compared to the trace start time.
  get utcOffset(): time {
    return this._utcOffset;
  }

  set utcOffset(offset: time) {
    this._utcOffset = offset;
  }

  // Trace TZ is like UTC but keeps into account also the timezone_off_mins
  // recorded into the trace, to show timestamps in the device local time.
  get traceTzOffset(): time {
    return this._traceTzOffset;
  }

  set traceTzOffset(offset: time) {
    this._traceTzOffset = offset;
  }

  get tabManager() {
    return this._tabManager;
  }

  get trackManager() {
    return this._trackManager;
  }

  // Offset between t=0 and the configured time domain.
  timestampOffset(): time {
    const fmt = timestampFormat();
    switch (fmt) {
      case TimestampFormat.Timecode:
      case TimestampFormat.Seconds:
        return this.state.traceTime.start;
      case TimestampFormat.Raw:
      case TimestampFormat.RawLocale:
        return Time.ZERO;
      case TimestampFormat.UTC:
        return this.utcOffset;
      case TimestampFormat.TraceTz:
        return this.traceTzOffset;
      default:
        const x: never = fmt;
        throw new Error(`Unsupported format ${x}`);
    }
  }

  // Convert absolute time to domain time.
  toDomainTime(ts: time): time {
    return Time.sub(ts, this.timestampOffset());
  }

  findTimeRangeOfSelection(): {start: time; end: time} {
    const selection = getLegacySelection(this.state);
    let start = Time.INVALID;
    let end = Time.INVALID;
    if (selection === null) {
      return {start, end};
    } else if (
      selection.kind === 'SLICE' ||
      selection.kind === 'CHROME_SLICE'
    ) {
      const slice = this.sliceDetails;
      if (slice.ts && slice.dur !== undefined && slice.dur > 0) {
        start = slice.ts;
        end = Time.add(start, slice.dur);
      } else if (slice.ts) {
        start = slice.ts;
        // This will handle either:
        // a)slice.dur === -1 -> unfinished slice
        // b)slice.dur === 0  -> instant event
        end =
          slice.dur === -1n
            ? Time.add(start, INCOMPLETE_SLICE_DURATION)
            : Time.add(start, INSTANT_FOCUS_DURATION);
      }
    } else if (selection.kind === 'THREAD_STATE') {
      const threadState = this.threadStateDetails;
      // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
      if (threadState.ts && threadState.dur) {
        start = threadState.ts;
        end = Time.add(start, threadState.dur);
      }
    } else if (selection.kind === 'COUNTER') {
      start = selection.leftTs;
      end = selection.rightTs;
    } else if (selection.kind === 'AREA') {
      const selectedArea = this.state.areas[selection.areaId];
      // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
      if (selectedArea) {
        start = selectedArea.start;
        end = selectedArea.end;
      }
    } else if (selection.kind === 'NOTE') {
      const selectedNote = this.state.notes[selection.id];
      // Notes can either be default or area notes. Area notes are handled
      // above in the AREA case.
      // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
      if (selectedNote && selectedNote.noteType === 'DEFAULT') {
        start = selectedNote.timestamp;
        end = Time.add(selectedNote.timestamp, INSTANT_FOCUS_DURATION);
      }
    } else if (selection.kind === 'LOG') {
      // TODO(hjd): Make focus selection work for logs.
    } else if (selection.kind === 'GENERIC_SLICE') {
      start = selection.start;
      if (selection.duration > 0) {
        end = Time.add(start, selection.duration);
      } else {
        end = Time.add(start, INSTANT_FOCUS_DURATION);
      }
    }

    return {start, end};
  }

  panToTimestamp(ts: time): void {
    horizontalScrollToTs(ts);
  }
}

export const globals = new Globals();
