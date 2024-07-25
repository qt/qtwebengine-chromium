// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as TraceEngine from '../../models/trace/trace.js';
import {describeWithEnvironment} from '../../testing/EnvironmentHelpers.js';
import {TraceLoader} from '../../testing/TraceLoader.js';

import * as Timeline from './timeline.js';

class MockViewDelegate implements Timeline.TimelinePanel.TimelineModeViewDelegate {
  select(_selection: Timeline.TimelineSelection.TimelineSelection|null): void {
  }
  selectEntryAtTime(_events: TraceEngine.Types.TraceEvents.TraceEventData[]|null, _time: number): void {
  }
  highlightEvent(_event: TraceEngine.Legacy.CompatibleTraceEvent|null): void {
  }
}

function getRowDataForDetailsElement(details: HTMLElement) {
  return Array.from(details.querySelectorAll<HTMLDivElement>('.timeline-details-view-row')).map(row => {
    const title = row.querySelector<HTMLDivElement>('.timeline-details-view-row-title')?.innerText;
    const value = row.querySelector<HTMLDivElement>('.timeline-details-view-row-value')?.innerText;
    return {title, value};
  });
}

describeWithEnvironment('TimelineDetailsView', function() {
  const mockViewDelegate = new MockViewDelegate();
  it('displays the details of a network request event correctly', async function() {
    const traceParsedData = await TraceLoader.traceEngine(this, 'lcp-web-font.json.gz');
    const detailsView = new Timeline.TimelineDetailsView.TimelineDetailsView(mockViewDelegate);

    const networkRequests = traceParsedData.NetworkRequests.byTime;
    const cssRequest = networkRequests.find(request => {
      return request.args.data.url === 'https://chromedevtools.github.io/performance-stories/lcp-web-font/app.css';
    });
    if (!cssRequest) {
      throw new Error('Could not find expected network request.');
    }
    const selection = Timeline.TimelineSelection.TimelineSelection.fromTraceEvent(cssRequest);

    await detailsView.setModel(traceParsedData, null);
    await detailsView.setSelection(selection);

    const detailsContentElement = detailsView.getDetailsContentElementForTest();
    assert.strictEqual(detailsContentElement.childNodes.length, 1);
    const rowData = getRowDataForDetailsElement(detailsContentElement);

    assert.deepEqual(
        rowData,
        [
          {title: 'URL', value: 'chromedevtools.github.io/performance-stories/lcp-web-font/app.css'},
          {title: 'Duration', value: '12.582ms (8.291ms load from cache + 4.291ms resource loading)'},
          {title: 'Request Method', value: 'GET'},
          {title: 'Initial Priority', value: 'Highest'},
          {title: 'Priority', value: 'Highest'},
          {title: 'Mime Type', value: 'text/css'},
          {title: 'Encoded Data', value: ' (from cache)'},
          {title: 'Decoded Body', value: '96 B'},
        ],
    );
  });
});
