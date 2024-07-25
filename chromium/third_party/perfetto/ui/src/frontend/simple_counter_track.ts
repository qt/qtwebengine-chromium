// Copyright (C) 2024 The Android Open Source Project
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
import {EngineProxy, TrackContext} from '../public';
import {BaseCounterTrack, CounterOptions} from './base_counter_track';
import {CounterColumns, SqlDataSource} from './debug_tracks';
import {Disposable, DisposableCallback} from '../base/disposable';
import {uuidv4Sql} from '../base/uuid';

export type SimpleCounterTrackConfig = {
  data: SqlDataSource;
  columns: CounterColumns;
  options?: Partial<CounterOptions>;
};

export class SimpleCounterTrack extends BaseCounterTrack {
  private config: SimpleCounterTrackConfig;
  private sqlTableName: string;

  constructor(
    engine: EngineProxy,
    ctx: TrackContext,
    config: SimpleCounterTrackConfig,
  ) {
    super({
      engine,
      trackKey: ctx.trackKey,
      options: config.options,
    });
    this.config = config;
    this.sqlTableName = `__simple_counter_${uuidv4Sql()}`;
  }

  async onInit(): Promise<Disposable> {
    const trash = await super.onInit();
    await this.createTrackTable();
    return new DisposableCallback(() => {
      trash.dispose();
      this.dropTrackTable();
    });
  }

  getTrackShellButtons(): m.Children {
    return this.getCounterContextMenu();
  }

  getSqlSource(): string {
    return `select * from ${this.sqlTableName}`;
  }

  private async createTrackTable(): Promise<void> {
    await this.engine.query(`
        create table ${this.sqlTableName} as
        with data as (
          ${this.config.data.sqlSource}
        )
        select
          ${this.config.columns.ts} as ts,
          ${this.config.columns.value} as value
        from data
        order by ts;`);
  }

  private async dropTrackTable(): Promise<void> {
    if (this.engine.isAlive) {
      await this.engine.query(`drop table if exists ${this.sqlTableName}`);
    }
  }
}
