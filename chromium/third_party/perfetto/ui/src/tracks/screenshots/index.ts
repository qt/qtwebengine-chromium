// Copyright (C) 2023 The Android Open Source Project
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

import {uuidv4} from '../../base/uuid';
import {AddTrackArgs} from '../../common/actions';
import {GenericSliceDetailsTabConfig} from '../../frontend/generic_slice_details_tab';
import {NamedSliceTrackTypes} from '../../frontend/named_slice_track';
import {
  BottomTabToSCSAdapter,
  NUM,
  Plugin,
  PluginContextTrace,
  PluginDescriptor,
  PrimaryTrackSortKey,
} from '../../public';
import {Engine} from '../../trace_processor/engine';
import {
  CustomSqlDetailsPanelConfig,
  CustomSqlTableDefConfig,
  CustomSqlTableSliceTrack,
} from '../custom_sql_table_slices';

import {ScreenshotTab} from './screenshot_panel';

class ScreenshotsTrack extends CustomSqlTableSliceTrack<NamedSliceTrackTypes> {
  static readonly kind = 'dev.perfetto.ScreenshotsTrack';

  getSqlDataSource(): CustomSqlTableDefConfig {
    return {
      sqlTableName: 'android_screenshots',
      columns: ['*'],
    };
  }

  getDetailsPanel(): CustomSqlDetailsPanelConfig {
    return {
      kind: ScreenshotTab.kind,
      config: {
        sqlTableName: this.tableName,
        title: 'Screenshots',
      },
    };
  }
}

export type DecideTracksResult = {
  tracksToAdd: AddTrackArgs[];
};

// TODO(stevegolton): Use suggestTrack().
export async function decideTracks(
  engine: Engine,
): Promise<DecideTracksResult> {
  const result: DecideTracksResult = {
    tracksToAdd: [],
  };

  const res = await engine.query(
    'select count() as count from android_screenshots',
  );
  const {count} = res.firstRow({count: NUM});

  if (count > 0) {
    result.tracksToAdd.push({
      uri: 'perfetto.Screenshots',
      name: 'Screenshots',
      trackSortKey: PrimaryTrackSortKey.ASYNC_SLICE_TRACK,
    });
  }

  return result;
}

class ScreenshotsPlugin implements Plugin {
  async onTraceLoad(ctx: PluginContextTrace): Promise<void> {
    await ctx.engine.query(`INCLUDE PERFETTO MODULE android.screenshots`);

    const res = await ctx.engine.query(
      'select count() as count from android_screenshots',
    );
    const {count} = res.firstRow({count: NUM});

    if (count > 0) {
      const displayName = 'Screenshots';
      const uri = 'perfetto.Screenshots';
      ctx.registerTrack({
        uri,
        displayName,
        kind: ScreenshotsTrack.kind,
        trackFactory: ({trackKey}) => {
          return new ScreenshotsTrack({
            engine: ctx.engine,
            trackKey,
          });
        },
      });

      ctx.registerDetailsPanel(
        new BottomTabToSCSAdapter({
          tabFactory: (selection) => {
            if (
              selection.kind === 'GENERIC_SLICE' &&
              selection.detailsPanelConfig.kind === ScreenshotTab.kind
            ) {
              const config = selection.detailsPanelConfig.config;
              return new ScreenshotTab({
                config: config as GenericSliceDetailsTabConfig,
                engine: ctx.engine,
                uuid: uuidv4(),
              });
            }
            return undefined;
          },
        }),
      );
    }
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'perfetto.Screenshots',
  plugin: ScreenshotsPlugin,
};
