// Copyright 2023 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as FrontendHelpers from '../../../../../test/unittests/front_end/helpers/EnvironmentHelpers.js';
import * as Explain from '../../../../panels/explain/explain.js';
import * as ComponentHelpers from '../../helpers/helpers.js';

await ComponentHelpers.ComponentServerSetup.setup();
await FrontendHelpers.initializeGlobalVars();

const ConsoleInsight = Explain.ConsoleInsight;

const component = new ConsoleInsight(
    {
      async buildPrompt() {
        return {prompt: '', sources: []};
      },
    },
    {
      async getInsights() {
        return `## Result

Some text with \`code\`. Some code:
\`\`\`ts
console.log('test');
document.querySelector('test').style = 'black';
\`\`\`

Links: [https://example.com](https://example.com)
Images: ![https://example.com](https://example.com)
`;
      },
    });
document.getElementById('container')?.appendChild(component);
