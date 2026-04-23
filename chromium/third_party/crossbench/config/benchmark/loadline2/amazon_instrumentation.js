// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href ===
    'https://www.amazon.co.uk/NIVEA-Suncream-Spray-Protect-Moisture/dp/B001B0OJXM') {
  const button_selector = 'a[id=nav-hamburger-menu]';
  const menu_selector = '.hmenu';
  const buy_id = 'buy-now-button';

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
    const menu = document.querySelector(menu_selector);
    if (!button || !menu) {
      return;
    }

    button_observer.disconnect();

    const attribute_observer = new MutationObserver(() => {
      if (menu.classList.contains('hmenu-visible')) {
        attribute_observer.disconnect();
        performance.mark('LoadLine2/amazon_product/interactive');
        onFrameRendered(() => {
          performance.mark('LoadLine2/amazon_product/interactive_raf');
        });
      }
    });
    attribute_observer.observe(menu, {attributes: true});
    button.click();
  });

  const buy_observer = new MutationObserver(mutations => {
    const buy = document.getElementById(buy_id);
    if (!buy) {
      return;
    }
    buy_observer.disconnect();
    performance.mark('LoadLine2/amazon_product/visual');
    onFrameRendered(() => {
      performance.mark('LoadLine2/amazon_product/visual_raf');
    });
  });

  buy_observer.observe(document, {childList: true, subtree: true});
  button_observer.observe(document, {childList: true, subtree: true});
}
