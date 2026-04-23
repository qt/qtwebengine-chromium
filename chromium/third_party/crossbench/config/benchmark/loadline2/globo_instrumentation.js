// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href === 'https://www.globo.com/') {
  const button_selector = 'button[aria-label=Consent]';
  const banner_selector = 'div[class=fc-consent-root]';
  const headline_selector = '.post__title';
  let banner_observer;

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
    const button = document.querySelector(button_selector);
    if (!button) {
      return;
    }
    button_observer.disconnect();
    const banner_node = document.querySelector(banner_selector);
    banner_observer = new MutationObserver(mutations => {
      for (const mutation of mutations) {
        for (const node of mutation.removedNodes) {
          if (node === banner_node) {
            performance.mark('LoadLine2/globo_homepage/interactive');
            banner_observer.disconnect();
            onFrameRendered(() => {
              performance.mark('LoadLine2/globo_homepage/interactive_raf');
            });
            return;
          }
        }
      }
    });
    banner_observer.observe(banner_node.parentNode, {childList: true});
    button.click();
  });

  const headline_observer = new MutationObserver(mutations => {
    const headline = document.querySelector(headline_selector);
    if (!headline) {
      return;
    }
    headline_observer.disconnect();
    performance.mark('LoadLine2/globo_homepage/visual');
    onFrameRendered(() => {
      performance.mark('LoadLine2/globo_homepage/visual_raf');
    });
  });

  button_observer.observe(document, {childList: true, subtree: true});
  headline_observer.observe(document, {childList: true, subtree: true});
}
