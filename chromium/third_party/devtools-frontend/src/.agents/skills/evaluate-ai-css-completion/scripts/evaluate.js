// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/* eslint-disable no-console */

const puppeteer = require('puppeteer-core');

async function runEvaluation() {
  console.log('Connecting to Chrome...');
  const browser = await puppeteer.connect({browserURL: 'http://127.0.0.1:9222', defaultViewport: null});

  try {
    console.log('Opening test page (example.com)...');
    const testPage = await browser.newPage();
    await testPage.goto('https://example.com');

    console.log('Waiting for DevTools target...');
    const devtoolsTarget = await browser.waitForTarget(t => {
      return t.url().startsWith('devtools://') || t.url().includes('inspector.html');
    }, {timeout: 10000});

    console.log('Connecting to DevTools page...');
    const devtoolsPage = await devtoolsTarget.page();
    if (!devtoolsPage) {
      throw new Error('Could not get DevTools page object. Make sure DevTools is undocked or opened in a tab.');
    }
    console.log('DevTools URL:', devtoolsPage.url());

    devtoolsPage.on('console', msg => console.log('BROWSER LOG:', msg.text()));

    console.log('Reloading DevTools to ensure patch is loaded...');
    try {
      await devtoolsPage.setCacheEnabled(false);
      await devtoolsPage.reload({waitUntil: 'domcontentloaded', timeout: 10000});
    } catch (e) {
      console.log('Reload warning (proceeding anyway):', e.message);
    }

    await devtoolsPage.evaluate(() => localStorage.setItem('debugAiCodeCompletionEnabled', 'true'));
    await devtoolsPage.evaluate(() => localStorage.setItem('debugAiServicesEnabled', 'true'));
    await devtoolsPage.evaluate(() => localStorage.setItem('debugAiAssistancePanelEnabled', 'true'));
    await devtoolsPage.evaluate(() => localStorage.setItem('aiAssistanceStructuredLogEnabled', 'true'));

    console.log('Waiting for DevTools to initialize and hook to be available...');
    try {
      await devtoolsPage.waitForFunction(() => typeof self.testCss?.getCases === 'function', {timeout: 15000});
    } catch {
      throw new Error('testCss.getCases hook not found. Did you apply the patch and build? Is this the local build?');
    }

    console.log('Fetching test cases from DevTools...');
    const hostConfig = await devtoolsPage.evaluate(() => self.testCss.getHostConfig());
    console.log('Host Config:', JSON.stringify(hostConfig, null, 2));
    const testCases = await devtoolsPage.evaluate(() => self.testCss.getCases());
    console.log(`Found ${testCases.length} test cases. Running each case 5 times.`);

    let totalQueries = 0;
    for (const tc of testCases) {
      console.log(`\nRunning: ${tc.name}`);
      let successCount = 0;
      const suggestions = new Set();

      for (let run = 1; run <= 5; run++) {
        // Delay to avoid rate limiting
        await new Promise(resolve => setTimeout(resolve, 1200));
        try {
          const result = await devtoolsPage.evaluate(async (url, prefix, suffix, additionalFiles) => {
            return await self.testCss.evaluate(url, prefix, suffix, additionalFiles);
          }, tc.url, tc.prefix, tc.suffix, tc.additionalFiles);

          if (result.hasSuggestion) {
            successCount++;
            for (const s of result.suggestions) {
              suggestions.add(s);
            }
          }
        } catch (e) {
          console.error(`  [Run ${run}] Failed:`, e.message);
        }

        totalQueries++;
        if (totalQueries % 20 === 0) {
          console.log('[INFO] Pausing for 8 seconds to prevent rate limiting...');
          await new Promise(resolve => setTimeout(resolve, 8000));
        }
      }

      console.log(`  Result: [Success Rate: ${successCount}/5]`);
      if (suggestions.size > 0) {
        console.log('  Unique Suggestions:', Array.from(suggestions));
      }
    }

    // Cleanup tabs
    await devtoolsPage.close();
    await testPage.close();
  } finally {
    console.log('Disconnecting from Chrome...');
    await browser.disconnect();
  }
}

runEvaluation().catch(console.error);
