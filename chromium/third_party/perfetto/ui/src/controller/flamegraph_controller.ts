// Copyright (C) 2019 The Android Open Source Project
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

import {Duration, time} from '../base/time';
import {exists} from '../base/utils';
import {Actions} from '../common/actions';
import {
  defaultViewingOption,
  expandCallsites,
  findRootSize,
  mergeCallsites,
} from '../common/flamegraph_util';
import {
  CallsiteInfo,
  FlamegraphState,
  FlamegraphStateViewingOption,
  isHeapGraphDominatorTreeViewingOption,
  ProfileType,
} from '../common/state';
import {FlamegraphDetails, globals} from '../frontend/globals';
import {publishFlamegraphDetails} from '../frontend/publish';
import {Engine} from '../trace_processor/engine';
import {NUM, STR} from '../trace_processor/query_result';
import {PERF_SAMPLES_PROFILE_TRACK_KIND} from '../tracks/perf_samples_profile';

import {AreaSelectionHandler} from './area_selection_handler';
import {Controller} from './controller';

export function profileType(s: string): ProfileType {
  if (isProfileType(s)) {
    return s;
  }
  if (s.startsWith('heap_profile')) {
    return ProfileType.HEAP_PROFILE;
  }
  throw new Error('Unknown type ${s}');
}

function isProfileType(s: string): s is ProfileType {
  return Object.values(ProfileType).includes(s as ProfileType);
}

function getFlamegraphType(type: ProfileType) {
  switch (type) {
    case ProfileType.HEAP_PROFILE:
    case ProfileType.MIXED_HEAP_PROFILE:
    case ProfileType.NATIVE_HEAP_PROFILE:
    case ProfileType.JAVA_HEAP_SAMPLES:
      return 'native';
    case ProfileType.JAVA_HEAP_GRAPH:
      return 'graph';
    case ProfileType.PERF_SAMPLE:
      return 'perf';
    default:
      const exhaustiveCheck: never = type;
      throw new Error(`Unhandled case: ${exhaustiveCheck}`);
  }
}

export interface FlamegraphControllerArgs {
  engine: Engine;
}
const MIN_PIXEL_DISPLAYED = 1;

class TablesCache {
  private engine: Engine;
  private cache: Map<string, string>;
  private prefix: string;
  private tableId: number;
  private cacheSizeLimit: number;

  constructor(engine: Engine, prefix: string) {
    this.engine = engine;
    this.cache = new Map<string, string>();
    this.prefix = prefix;
    this.tableId = 0;
    this.cacheSizeLimit = 10;
  }

  async getTableName(query: string): Promise<string> {
    let tableName = this.cache.get(query);
    if (tableName === undefined) {
      // TODO(hjd): This should be LRU.
      if (this.cache.size > this.cacheSizeLimit) {
        for (const name of this.cache.values()) {
          await this.engine.query(`drop table ${name}`);
        }
        this.cache.clear();
      }
      tableName = `${this.prefix}_${this.tableId++}`;
      await this.engine.query(
        `create temp table if not exists ${tableName} as ${query}`,
      );
      this.cache.set(query, tableName);
    }
    return tableName;
  }

  hasQuery(query: string): boolean {
    return this.cache.get(query) !== undefined;
  }
}

export class FlamegraphController extends Controller<'main'> {
  private flamegraphDatasets: Map<string, CallsiteInfo[]> = new Map();
  private lastSelectedFlamegraphState?: FlamegraphState;
  private requestingData = false;
  private queuedRequest = false;
  private flamegraphDetails: FlamegraphDetails = {};
  private areaSelectionHandler: AreaSelectionHandler;
  private cache: TablesCache;

  constructor(private args: FlamegraphControllerArgs) {
    super('main');
    this.cache = new TablesCache(args.engine, 'grouped_callsites');
    this.areaSelectionHandler = new AreaSelectionHandler();
  }

  run() {
    const [hasAreaChanged, area] = this.areaSelectionHandler.getAreaChange();
    if (hasAreaChanged) {
      const upids = [];
      if (!area) {
        this.checkCompletionAndPublishFlamegraph({
          ...globals.flamegraphDetails,
          isInAreaSelection: false,
        });
        return;
      }
      for (const trackId of area.tracks) {
        const track = globals.state.tracks[trackId];
        if (track?.uri) {
          const trackInfo = globals.trackManager.resolveTrackInfo(track.uri);
          if (trackInfo?.kind === PERF_SAMPLES_PROFILE_TRACK_KIND) {
            exists(trackInfo.upid) && upids.push(trackInfo.upid);
          }
        }
      }
      if (upids.length === 0) {
        this.checkCompletionAndPublishFlamegraph({
          ...globals.flamegraphDetails,
          isInAreaSelection: false,
        });
        return;
      }
      globals.dispatch(
        Actions.openFlamegraph({
          upids,
          start: area.start,
          end: area.end,
          type: ProfileType.PERF_SAMPLE,
          viewingOption: defaultViewingOption(ProfileType.PERF_SAMPLE),
        }),
      );
    }
    const selection = globals.state.currentFlamegraphState;
    if (!selection || !this.shouldRequestData(selection)) {
      return;
    }
    if (this.requestingData) {
      this.queuedRequest = true;
      return;
    }
    this.requestingData = true;

    this.assembleFlamegraphDetails(selection, area !== undefined);
  }

  private async assembleFlamegraphDetails(
    selection: FlamegraphState,
    isInAreaSelection: boolean,
  ) {
    const selectedFlamegraphState = {...selection};
    const flamegraphMetadata = await this.getFlamegraphMetadata(
      selection.type,
      selectedFlamegraphState.start,
      selectedFlamegraphState.end,
      selectedFlamegraphState.upids,
    );
    if (flamegraphMetadata !== undefined) {
      Object.assign(this.flamegraphDetails, flamegraphMetadata);
    }

    // TODO(hjd): Clean this up.
    if (
      this.lastSelectedFlamegraphState &&
      this.lastSelectedFlamegraphState.focusRegex !== selection.focusRegex
    ) {
      this.flamegraphDatasets.clear();
    }

    this.lastSelectedFlamegraphState = {...selection};

    const expandedCallsite =
      selectedFlamegraphState.expandedCallsiteByViewingOption[
        selectedFlamegraphState.viewingOption
      ];
    const expandedId = expandedCallsite ? expandedCallsite.id : -1;
    const rootSize = expandedCallsite?.totalSize;

    const key = `${selectedFlamegraphState.upids};${selectedFlamegraphState.start};${selectedFlamegraphState.end}`;

    try {
      const flamegraphData = await this.getFlamegraphData(
        key,
        /* eslint-disable @typescript-eslint/strict-boolean-expressions */
        selectedFlamegraphState.viewingOption /* eslint-enable */
          ? selectedFlamegraphState.viewingOption
          : defaultViewingOption(selectedFlamegraphState.type),
        selection.start,
        selection.end,
        selectedFlamegraphState.upids,
        selectedFlamegraphState.type,
        selectedFlamegraphState.focusRegex,
      );
      if (
        flamegraphData !== undefined &&
        // eslint-disable-next-line @typescript-eslint/strict-boolean-expressions
        selection &&
        selection.kind === selectedFlamegraphState.kind &&
        selection.start === selectedFlamegraphState.start &&
        selection.end === selectedFlamegraphState.end
      ) {
        const expandedFlamegraphData = expandCallsites(
          flamegraphData,
          expandedId,
        );
        this.prepareAndMergeCallsites(
          expandedFlamegraphData,
          this.lastSelectedFlamegraphState.viewingOption,
          isInAreaSelection,
          rootSize,
          expandedCallsite,
        );
      }
    } finally {
      this.requestingData = false;
      if (this.queuedRequest) {
        this.queuedRequest = false;
        this.run();
      }
    }
  }

  private shouldRequestData(selection: FlamegraphState) {
    return (
      selection.kind === 'FLAMEGRAPH_STATE' &&
      (this.lastSelectedFlamegraphState === undefined ||
        this.lastSelectedFlamegraphState.start !== selection.start ||
        this.lastSelectedFlamegraphState.end !== selection.end ||
        this.lastSelectedFlamegraphState.type !== selection.type ||
        !FlamegraphController.areArraysEqual(
          this.lastSelectedFlamegraphState.upids,
          selection.upids,
        ) ||
        this.lastSelectedFlamegraphState.viewingOption !==
          selection.viewingOption ||
        this.lastSelectedFlamegraphState.focusRegex !== selection.focusRegex ||
        this.lastSelectedFlamegraphState.expandedCallsiteByViewingOption[
          selection.viewingOption
        ] !==
          selection.expandedCallsiteByViewingOption[selection.viewingOption])
    );
  }

  private prepareAndMergeCallsites(
    flamegraphData: CallsiteInfo[],
    viewingOption: FlamegraphStateViewingOption,
    isInAreaSelection: boolean,
    rootSize?: number,
    expandedCallsite?: CallsiteInfo,
  ) {
    this.flamegraphDetails.flamegraph = mergeCallsites(
      flamegraphData,
      this.getMinSizeDisplayed(flamegraphData, rootSize),
    );
    this.flamegraphDetails.expandedCallsite = expandedCallsite;
    this.flamegraphDetails.viewingOption = viewingOption;
    this.flamegraphDetails.isInAreaSelection = isInAreaSelection;
    this.checkCompletionAndPublishFlamegraph(this.flamegraphDetails);
  }

  private async checkCompletionAndPublishFlamegraph(
    flamegraphDetails: FlamegraphDetails,
  ) {
    flamegraphDetails.graphIncomplete =
      (
        await this.args.engine.query(`select value from stats
       where severity = 'error' and name = 'heap_graph_non_finalized_graph'`)
      ).firstRow({value: NUM}).value > 0;
    flamegraphDetails.graphLoading = false;
    publishFlamegraphDetails(flamegraphDetails);
  }

  async getFlamegraphData(
    baseKey: string,
    viewingOption: FlamegraphStateViewingOption,
    start: time,
    end: time,
    upids: number[],
    type: ProfileType,
    focusRegex: string,
  ): Promise<CallsiteInfo[]> {
    let currentData: CallsiteInfo[];
    const key = `${baseKey}-${viewingOption}`;
    if (this.flamegraphDatasets.has(key)) {
      currentData = this.flamegraphDatasets.get(key)!;
    } else {
      publishFlamegraphDetails({
        ...globals.flamegraphDetails,
        graphLoading: true,
      });
      // Collecting data for drawing flamegraph for selected profile.
      // Data needs to be in following format:
      // id, name, parent_id, depth, total_size
      const tableName = await this.prepareViewsAndTables(
        start,
        end,
        upids,
        type,
        focusRegex,
        viewingOption,
      );
      currentData = await this.getFlamegraphDataFromTables(
        tableName,
        viewingOption,
        focusRegex,
      );
      this.flamegraphDatasets.set(key, currentData);
    }
    return currentData;
  }

  async getFlamegraphDataFromTables(
    tableName: string,
    viewingOption: FlamegraphStateViewingOption,
    focusRegex: string,
  ) {
    let orderBy = '';
    let totalColumnName:
      | 'cumulativeSize'
      | 'cumulativeAllocSize'
      | 'cumulativeCount'
      | 'cumulativeAllocCount' = 'cumulativeSize';
    let selfColumnName: 'size' | 'count' = 'size';
    // TODO(fmayer): Improve performance so this is no longer necessary.
    // Alternatively consider collapsing frames of the same label.
    const maxDepth = 100;
    switch (viewingOption) {
      case FlamegraphStateViewingOption.ALLOC_SPACE_MEMORY_ALLOCATED_KEY:
        orderBy = `where cumulative_alloc_size > 0 and depth < ${maxDepth} order by depth, parent_id,
            cumulative_alloc_size desc, name`;
        totalColumnName = 'cumulativeAllocSize';
        selfColumnName = 'size';
        break;
      case FlamegraphStateViewingOption.OBJECTS_ALLOCATED_NOT_FREED_KEY:
        orderBy = `where cumulative_count > 0 and depth < ${maxDepth} order by depth, parent_id,
            cumulative_count desc, name`;
        totalColumnName = 'cumulativeCount';
        selfColumnName = 'count';
        break;
      case FlamegraphStateViewingOption.OBJECTS_ALLOCATED_KEY:
        orderBy = `where cumulative_alloc_count > 0 and depth < ${maxDepth} order by depth, parent_id,
            cumulative_alloc_count desc, name`;
        totalColumnName = 'cumulativeAllocCount';
        selfColumnName = 'count';
        break;
      case FlamegraphStateViewingOption.PERF_SAMPLES_KEY:
      case FlamegraphStateViewingOption.SPACE_MEMORY_ALLOCATED_NOT_FREED_KEY:
        orderBy = `where cumulative_size > 0 and depth < ${maxDepth} order by depth, parent_id,
            cumulative_size desc, name`;
        totalColumnName = 'cumulativeSize';
        selfColumnName = 'size';
        break;
      case FlamegraphStateViewingOption.DOMINATOR_TREE_OBJ_COUNT_KEY:
        orderBy = `where depth < ${maxDepth} order by depth,
          cumulativeCount desc, name`;
        totalColumnName = 'cumulativeCount';
        selfColumnName = 'count';
        break;
      case FlamegraphStateViewingOption.DOMINATOR_TREE_OBJ_SIZE_KEY:
        orderBy = `where depth < ${maxDepth} order by depth,
          cumulativeSize desc, name`;
        totalColumnName = 'cumulativeSize';
        selfColumnName = 'size';
        break;
      default:
        const exhaustiveCheck: never = viewingOption;
        throw new Error(`Unhandled case: ${exhaustiveCheck}`);
        break;
    }

    const callsites = await this.args.engine.query(`
      SELECT
      id as hash,
      IFNULL(IFNULL(DEMANGLE(name), name), '[NULL]') as name,
      IFNULL(parent_id, -1) as parentHash,
      depth,
      cumulative_size as cumulativeSize,
      cumulative_alloc_size as cumulativeAllocSize,
      cumulative_count as cumulativeCount,
      cumulative_alloc_count as cumulativeAllocCount,
      map_name as mapping,
      size,
      count,
      IFNULL(source_file, '') as sourceFile,
      IFNULL(line_number, -1) as lineNumber
      from ${tableName} ${orderBy}`);

    const flamegraphData: CallsiteInfo[] = [];
    const hashToindex: Map<number, number> = new Map();
    const it = callsites.iter({
      hash: NUM,
      name: STR,
      parentHash: NUM,
      depth: NUM,
      cumulativeSize: NUM,
      cumulativeAllocSize: NUM,
      cumulativeCount: NUM,
      cumulativeAllocCount: NUM,
      mapping: STR,
      sourceFile: STR,
      lineNumber: NUM,
      size: NUM,
      count: NUM,
    });
    for (let i = 0; it.valid(); ++i, it.next()) {
      const hash = it.hash;
      let name = it.name;
      const parentHash = it.parentHash;
      const depth = it.depth;
      const totalSize = it[totalColumnName];
      const selfSize = it[selfColumnName];
      const mapping = it.mapping;
      const highlighted =
        focusRegex !== '' &&
        name.toLocaleLowerCase().includes(focusRegex.toLocaleLowerCase());
      const parentId = hashToindex.has(+parentHash)
        ? hashToindex.get(+parentHash)!
        : -1;

      let location: string | undefined;
      if (/[a-zA-Z]/i.test(it.sourceFile)) {
        location = it.sourceFile;
        if (it.lineNumber !== -1) {
          location += `:${it.lineNumber}`;
        }
      }

      if (depth === maxDepth - 1) {
        name += ' [tree truncated]';
      }
      // Instead of hash, we will store index of callsite in this original array
      // as an id of callsite. That way, we have quicker access to parent and it
      // will stay unique:
      hashToindex.set(hash, i);

      flamegraphData.push({
        id: i,
        totalSize,
        depth,
        parentId,
        name,
        selfSize,
        mapping,
        merged: false,
        highlighted,
        location,
      });
    }
    return flamegraphData;
  }

  private async prepareViewsAndTables(
    start: time,
    end: time,
    upids: number[],
    type: ProfileType,
    focusRegex: string,
    viewingOption: FlamegraphStateViewingOption,
  ): Promise<string> {
    const flamegraphType = getFlamegraphType(type);
    if (type === ProfileType.PERF_SAMPLE) {
      let upid: string;
      let upidGroup: string;
      if (upids.length > 1) {
        upid = `NULL`;
        upidGroup = `'${FlamegraphController.serializeUpidGroup(upids)}'`;
      } else {
        upid = `${upids[0]}`;
        upidGroup = `NULL`;
      }
      return this.cache.getTableName(
        `select id, name, map_name, parent_id, depth, cumulative_size,
          cumulative_alloc_size, cumulative_count, cumulative_alloc_count,
          size, alloc_size, count, alloc_count, source_file, line_number
          from experimental_flamegraph(
            '${flamegraphType}',
            NULL,
            '>=${start},<=${end}',
            ${upid},
            ${upidGroup},
            '${focusRegex}'
          )`,
      );
    }
    if (
      type === ProfileType.JAVA_HEAP_GRAPH &&
      isHeapGraphDominatorTreeViewingOption(viewingOption)
    ) {
      return this.cache.getTableName(
        await this.loadHeapGraphDominatorTreeQuery(upids[0], end),
      );
    }
    return this.cache.getTableName(
      `select id, name, map_name, parent_id, depth, cumulative_size,
          cumulative_alloc_size, cumulative_count, cumulative_alloc_count,
          size, alloc_size, count, alloc_count, source_file, line_number
          from experimental_flamegraph(
            '${flamegraphType}',
            ${end},
            NULL,
            ${upids[0]},
            NULL,
            '${focusRegex}'
          )`,
    );
  }

  private async loadHeapGraphDominatorTreeQuery(upid: number, timestamp: time) {
    const outputTableName = `heap_graph_type_dominated_${upid}_${timestamp}`;
    const outputQuery = `SELECT * FROM ${outputTableName}`;
    if (this.cache.hasQuery(outputQuery)) {
      return outputQuery;
    }

    this.args.engine.query(`
    INCLUDE PERFETTO MODULE memory.heap_graph_dominator_tree;

    -- heap graph dominator tree with objects as nodes and all relavant
    -- object self stats and dominated stats
    CREATE PERFETTO TABLE _heap_graph_object_dominated AS
    SELECT
     node.id,
     node.idom_id,
     node.dominated_obj_count,
     node.dominated_size_bytes + node.dominated_native_size_bytes AS dominated_size,
     node.depth,
     obj.type_id,
     obj.root_type,
     obj.self_size + obj.native_size AS self_size
    FROM memory_heap_graph_dominator_tree node
    JOIN heap_graph_object obj USING(id)
    WHERE obj.upid = ${upid} AND obj.graph_sample_ts = ${timestamp}
    -- required to accelerate the recursive cte below
    ORDER BY idom_id;

    -- calculate for each object node in the dominator tree the
    -- HASH(path of type_id's from the super root to the object)
    CREATE PERFETTO TABLE _dominator_tree_path_hash AS
    WITH RECURSIVE _tree_visitor(id, path_hash) AS (
      SELECT
        id,
        HASH(
          CAST(type_id AS TEXT) || '-' || IFNULL(root_type, '')
        ) AS path_hash
      FROM _heap_graph_object_dominated
      WHERE depth = 1
      UNION ALL
      SELECT
        child.id,
        HASH(CAST(parent.path_hash AS TEXT) || '/' || CAST(type_id AS TEXT)) AS path_hash
      FROM _heap_graph_object_dominated child
      JOIN _tree_visitor parent ON child.idom_id = parent.id
    )
    SELECT * from _tree_visitor
    ORDER BY id;

    -- merge object nodes with the same path into one "class type node", so the
    -- end result is a tree where nodes are identified by their types and the
    -- dominator relationships are preserved.
    CREATE PERFETTO TABLE ${outputTableName} AS
    SELECT
      map.path_hash as id,
      COALESCE(cls.deobfuscated_name, cls.name, '[NULL]') || IIF(
        node.root_type IS NOT NULL,
        ' [' || node.root_type || ']', ''
      ) AS name,
      IFNULL(parent_map.path_hash, -1) AS parent_id,
      node.depth - 1 AS depth,
      sum(dominated_size) AS cumulative_size,
      -1 AS cumulative_alloc_size,
      sum(dominated_obj_count) AS cumulative_count,
      -1 AS cumulative_alloc_count,
      '' as map_name,
      '' as source_file,
      -1 as line_number,
      sum(self_size) AS size,
      count(*) AS count
    FROM _heap_graph_object_dominated node
    JOIN _dominator_tree_path_hash map USING(id)
    LEFT JOIN _dominator_tree_path_hash parent_map ON node.idom_id = parent_map.id
    JOIN heap_graph_class cls ON node.type_id = cls.id
    GROUP BY map.path_hash, name, parent_id, depth, map_name, source_file, line_number;

    -- These are intermediates and not needed
    DROP TABLE _heap_graph_object_dominated;
    DROP TABLE _dominator_tree_path_hash;
    `);

    return outputQuery;
  }

  getMinSizeDisplayed(
    flamegraphData: CallsiteInfo[],
    rootSize?: number,
  ): number {
    const timeState = globals.state.frontendLocalState.visibleState;
    const dur = globals.stateVisibleTime().duration;
    // TODO(stevegolton): Does this actually do what we want???
    let width = Duration.toSeconds(dur / timeState.resolution);
    // TODO(168048193): Remove screen size hack:
    width = Math.max(width, 800);
    if (rootSize === undefined) {
      rootSize = findRootSize(flamegraphData);
    }
    return (MIN_PIXEL_DISPLAYED * rootSize) / width;
  }

  async getFlamegraphMetadata(
    type: ProfileType,
    start: time,
    end: time,
    upids: number[],
  ): Promise<FlamegraphDetails | undefined> {
    // Don't do anything if selection of the marker stayed the same.
    if (
      this.lastSelectedFlamegraphState !== undefined &&
      this.lastSelectedFlamegraphState.start === start &&
      this.lastSelectedFlamegraphState.end === end &&
      FlamegraphController.areArraysEqual(
        this.lastSelectedFlamegraphState.upids,
        upids,
      )
    ) {
      return undefined;
    }

    // Collecting data for more information about profile, such as:
    // total memory allocated, memory that is allocated and not freed.
    const upidGroup = FlamegraphController.serializeUpidGroup(upids);

    const result = await this.args.engine.query(
      `select pid from process where upid in (${upidGroup})`,
    );
    const it = result.iter({pid: NUM});
    const pids = [];
    for (let i = 0; it.valid(); ++i, it.next()) {
      pids.push(it.pid);
    }
    return {start, dur: end - start, pids, upids, type};
  }

  private static areArraysEqual(a: number[], b: number[]) {
    if (a.length !== b.length) {
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (a[i] !== b[i]) {
        return false;
      }
    }
    return true;
  }

  private static serializeUpidGroup(upids: number[]) {
    return new Array(upids).join();
  }
}
