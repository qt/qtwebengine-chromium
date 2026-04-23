// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href === 'https://en.m.wikipedia.org/wiki/Taylor_Swift') {
  const language_button_selector = '.language-selector';
  const language_xpath = '//span[text()=\'Afrikaans\']';
  const image_selector = '.infobox-image';

  function onFrameRendered(callback) {
    // The first rAF requests a frame to be rendered. When it's done, the
    // second rAF is called. So the callback is invoked when the first frame
    // has been rendered.
    // This is a poor approximation of when the frame is actually shown on the
    // device screen, since it ignores all work beyond Renderer process
    // (GPU process/surfaceflinger). But it's the best we can do using pure
    // WebAPI.
    requestAnimationFrame(() => {
      requestAnimationFrame(callback);
    });
  }

  const button_observer = new MutationObserver(mutations => {
    const button = document.querySelector(language_button_selector);

    if (!button) {
      return;
    }
    button_observer.disconnect();
    button.click();
  });

  const language_observer = new MutationObserver(mutations => {
    const language = document
                         .evaluate(
                             language_xpath, document, null,
                             XPathResult.FIRST_ORDERED_NODE_TYPE, null)
                         .singleNodeValue;

    if (!language) {
      return;
    }
    performance.mark('LoadLine2/wikipedia_article/interactive');
    language_observer.disconnect();
    onFrameRendered(() => {
      performance.mark('LoadLine2/wikipedia_article/interactive_raf');
    });
  });

  const image_observer = new MutationObserver(mutations => {
    const image = document.querySelector(image_selector);
    if (!image) {
      return;
    }
    image_observer.disconnect();
    performance.mark('LoadLine2/wikipedia_article/visual');
    onFrameRendered(() => {
      performance.mark('LoadLine2/wikipedia_article/visual_raf');
    });
  });

  button_observer.observe(document, {childList: true, subtree: true});
  language_observer.observe(document, {childList: true, subtree: true});
  image_observer.observe(document, {childList: true, subtree: true});
}
