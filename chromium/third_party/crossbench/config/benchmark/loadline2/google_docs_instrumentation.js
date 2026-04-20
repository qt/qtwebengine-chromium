// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href ===
    'https://docs.google.com/document/d/13AWeOGqtSkfpPK7meqE_X-GQQggwx4JJ1vc0' +
        'YGvKg34/edit#heading=h.gjdgxs') {
  const widget_selector = 'div.navigation-widget-empty-content';
  const zoom_button_id = 'zoomSelect';
  let complete = false;

  const widget_observer = new MutationObserver(unused => {
    // Corresponds to docs.png in crbug.com/372457479#comment9. The historical
    // context is that this story used LCP on loadline v1, and so for v2 we
    // picked the corresponding LCP element on pixel 9.
    if (document.querySelector(widget_selector)) {
      widget_observer.disconnect();
      performance.mark('LoadLine2/google_doc/visual')
    }
  });

  // The page ignores simulated clicks, so we generate and dispatch a sequence
  // of events instead.
  function simulateRealisticClick(element) {
    const rect = element.getBoundingClientRect();
    const properties = {
      bubbles: true,
      cancelable: true,
      view: window,
      clientX: rect.left + rect.width / 2,
      clientY: rect.top + rect.height / 2,
    };
    element.dispatchEvent(new MouseEvent('mousedown', properties));
    element.dispatchEvent(new MouseEvent('mouseup', properties));
    element.dispatchEvent(new MouseEvent('click', properties));
  }

  const button_observer = new MutationObserver(unused => {
    const button = document.getElementById(zoom_button_id);
    if (!button) {
      return;
    }
    button_observer.disconnect();

    click = function() {
      if (complete)
        return;
      simulateRealisticClick(button);
      setTimeout(click, 10);
    };
    click();
  });

  const menu_observer = new MutationObserver(unused => {
    const menu = document
                     .evaluate(
                         '//div[text()=\'100%\']', document, null,
                         XPathResult.FIRST_ORDERED_NODE_TYPE, null)
                     .singleNodeValue.parentElement.parentElement;
    if (!menu) {
      return;
    }
    menu_observer.disconnect();

    if (menu.style.display !== 'none') {
      performance.mark('LoadLine2/google_doc/interactive');
      complete = true;
      return;
    }

    const attribute_observer = new MutationObserver(() => {
      if (menu.style.display !== 'none') {
        attribute_observer.disconnect();
        performance.mark('LoadLine2/google_doc/interactive');
        complete = true;
      }
    });
    attribute_observer.observe(menu, {attributes: true});
  });

  widget_observer.observe(document, {childList: true, subtree: true});
  button_observer.observe(document, {childList: true, subtree: true});
  menu_observer.observe(document, {childList: true, subtree: true});
}
