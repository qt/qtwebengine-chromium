---
name: evaluate-ai-css-completion
description: Expose a temporary evaluation hook in DevTools and run a Puppeteer script to validate CSS code completion trigger rates.
---

# Evaluate CSS Completion Skill

This skill allows you to temporarily expose an evaluation hook in DevTools to measure CSS code completion trigger rates using a Puppeteer script.

> [!WARNING]
> **NEVER commit the `devtools_app.ts` patch.** It is only for local evaluation. Always revert it before uploading your CL.

## Step 1: Apply the Temporary Patch

Modify `front_end/entrypoints/devtools_app/devtools_app.ts` to expose the global `testCssCompletion` hook.

Add the following code at the end of `front_end/entrypoints/devtools_app/devtools_app.ts`:

```typescript
// --- TEMPORARY EVALUATION HOOK ---
// TEMPORARY PATCH - REMOVE BEFORE COMMIT

import * as Host from '../../core/host/host.js';
import * as AiCodeCompletion from '../../models/ai_code_completion/ai_code_completion.js';

(self as any).testCss = {
  getCases() {
    return [
      {
        name: 'Generic CSS Test Case',
        url: null,
        prefix: 'h1 { font-s',
        suffix: ' }',
      },
      // Add more test cases here
    ];
  },

  async evaluate(uiSourceCodeUrl: string | null, prefix: string, suffix: string, additionalFiles?: any[]) {
    const aidaClient = new Host.AidaClient.AidaClient();
    const completion = new AiCodeCompletion.AiCodeCompletion.AiCodeCompletion(
      {aidaClient},
      AiCodeCompletion.AiCodeCompletion.ContextFlavor.STYLES
    );

    const formattedAdditionalFiles = additionalFiles?.map(f => ({
      path: f.path || f.name,
      content: f.content || f.text,
      included_reason: f.included_reason ?? Host.AidaClient.Reason.RELATED_FILE,
    }));

    const result = await completion.completeCode(
      prefix,
      suffix,
      prefix.length,
      Host.AidaClient.AidaInferenceLanguage.CSS,
      formattedAdditionalFiles
    );

    return {
      hasSuggestion: result.response !== null && result.response.generatedSamples.length > 0,
      suggestions: result.response?.generatedSamples.map(s => s.generationString) ?? [],
      injectedFiles: formattedAdditionalFiles?.map(f => f.path) ?? [],
    };
  }
};
// ----------------------------------
```

## Step 2: Launch Chrome with Local DevTools

Use the `npm start` script to build DevTools, launch Chrome Canary with remote debugging, and disable watch mode (so we build manually instead).

You must specify a persistent user data directory so you can log in once and reuse the session:

```bash
npm start -- --no-watch --browser=canary --remote-debugging-port=9222 --user-data-dir=/tmp/devtools-ai-evaluate-css-completion
```

> [!IMPORTANT]
> **Instructions for the Agent:**
> After launching Chrome, you MUST print the following checklist to the user and wait for their explicit confirmation before running the evaluation script:
> 1. Sign in to Chrome with your corporate account in the new window.
> 2. Open DevTools, go to **Settings** (gear icon) > **AI Innovations**, and ensure **Code Completions** is enabled.
> 3. Focus the DevTools window and press `Cmd + Option + I` (on macOS) or `Ctrl + Shift + I` (on Windows/Linux) to open DevTools-on-DevTools (inspecting DevTools itself). Go to the Network tab of the second DevTools instance and check the **Disable cache** checkbox. This ensures your local builds are loaded instead of cached versions when reloaded.
> 4. Ask the user to reply when they are ready.

## Step 3: Configure Test Cases

Before running the evaluation, configure your test cases directly in `front_end/entrypoints/devtools_app/devtools_app.ts` inside the `getCases()` method of the patch you applied in Step 1.

After modifying the test cases, manually run a build to compile the changes:
```bash
autoninja -C out/Default
```

For example, to test spacing sensitivity, update the returned array:
```typescript
  getCases() {
    return [
      {
        name: 'With CSS Context',
        url: null,
        prefix: 'h1 { font-s',
        suffix: ' }',
        additionalFiles: [
          {
            path: 'other.css',
            content: 'body { color: red; }',
          }
        ]
      }
    ];
  }
```

## Step 4: Run the Evaluation Script

Run the Puppeteer script to execute the evaluation:

```bash
node .agents/skills/evaluate-ai-css-completion/scripts/evaluate.js
```

The script will connect to the DevTools instance, trigger completions, and output the results.

### Debugging and Diagnostics

If you get 0% trigger rate or unexpected errors, check the following:

1.  **Browser Logs:** The evaluation script forwards browser console logs (prefixed with `BROWSER LOG:`). Look for these in your terminal.
2.  **Host Config:** The script prints the DevTools `Host Config` at the start. Verify that `"devToolsAiCodeCompletionStyles": { "enabled": true }` is present.
3.  **GCA/AIDA Logs:** The script automatically enables verbose AIDA logging. Look for `GCA request succeeded:` in the logs:
    *   If the response has `"usageMetadata": { "candidatesTokenCount": X }` (where X > 0) but the `candidates` array is missing or empty, the backend generated tokens but they were filtered out (e.g., due to safety classifiers or recitation blocks).
4.  **Context Requirement:** The CSS model is sensitive to context. A minimal prompt like `h1 { font-s` might not trigger AIDA. Ensure your test cases have enough context in the `prefix` (e.g., several preceding CSS rules) or pass them via `additionalFiles` to simulate a larger stylesheet.

## Step 5: Revert the Patch

Once the evaluation is complete and you have recorded the results, revert the patch in `devtools_app.ts`:

```bash
git checkout front_end/entrypoints/devtools_app/devtools_app.ts
```
