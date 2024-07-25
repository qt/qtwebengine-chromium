// Copyright (C) 2022 The Android Open Source Project
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

import m from 'mithril';
import {v4 as uuidv4} from 'uuid';

import {Actions} from '../../common/actions';
import {globals} from '../../frontend/globals';
import {
  EngineProxy,
  Plugin,
  PluginContextTrace,
  PluginDescriptor,
  TrackContext,
} from '../../public';
import {ChromeSliceTrack} from '../chrome_slices';
import {
  VISUALISED_ARGS_SLICE_TRACK_URI,
  VisualisedArgsState,
} from '../../frontend/visualized_args_tracks';
import {Button} from '../../widgets/button';
import {Icons} from '../../base/semantic_icons';

export class VisualisedArgsTrack extends ChromeSliceTrack {
  private helperViewName: string;

  constructor(
    engine: EngineProxy,
    maxDepth: number,
    trackKey: string,
    trackId: number,
    private argName: string,
  ) {
    const uuid = uuidv4();
    const namespace = `__arg_visualisation_helper_${argName}_${uuid}`;
    const escapedNamespace = namespace.replace(/[^a-zA-Z]/g, '_');
    super(engine, maxDepth, trackKey, trackId, escapedNamespace);
    this.helperViewName = `${escapedNamespace}_slice`;
  }

  async onCreate(_ctx: TrackContext): Promise<void> {
    // Create the helper view - just one which is relevant to this slice
    await this.engine.query(`
          create view ${this.helperViewName} as
          with slice_with_arg as (
            select
              slice.id,
              slice.track_id,
              slice.ts,
              slice.dur,
              slice.thread_dur,
              NULL as cat,
              args.display_value as name
            from slice
            join args using (arg_set_id)
            where args.key='${this.argName}'
          )
          select
            *,
            (select count()
            from ancestor_slice(s1.id) s2
            join slice_with_arg s3 on s2.id=s3.id
            ) as depth
          from slice_with_arg s1
          order by id;
      `);
  }

  async onDestroy(): Promise<void> {
    if (this.engine.isAlive) {
      await this.engine.query(`drop view ${this.helperViewName}`);
    }
  }

  getFont() {
    return 'italic 11px Roboto';
  }

  getTrackShellButtons(): m.Children {
    return m(Button, {
      onclick: () => {
        // This behavior differs to the original behavior a little.
        // Originally, hitting the close button on a single track removed ALL
        // tracks with this argName, whereas this one only closes the single
        // track.
        // This will be easily fixable once we transition to using dynamic
        // tracks instead of this "initial state" approach to add these tracks.
        globals.dispatch(Actions.removeTracks({trackKeys: [this.trackKey]}));
      },
      icon: Icons.Close,
      title: 'Close',
      compact: true,
    });
  }
}

class VisualisedArgsPlugin implements Plugin {
  async onTraceLoad(ctx: PluginContextTrace): Promise<void> {
    ctx.registerTrack({
      uri: VISUALISED_ARGS_SLICE_TRACK_URI,
      tags: {
        metric: true, // TODO(stevegolton): Is this track really a metric?
      },
      trackFactory: (trackCtx) => {
        // TODO(stevegolton): Validate params properly. Note, this is no
        // worse than the situation we had before with track config.
        const params = trackCtx.params as VisualisedArgsState;
        return new VisualisedArgsTrack(
          ctx.engine,
          params.maxDepth,
          trackCtx.trackKey,
          params.trackId,
          params.argName,
        );
      },
    });
  }
}

export const plugin: PluginDescriptor = {
  pluginId: 'perfetto.VisualisedArgs',
  plugin: VisualisedArgsPlugin,
};
