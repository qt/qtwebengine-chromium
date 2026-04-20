// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href === 'https://www.google.com/search?q=cats') {
  const searchbox_observer = new MutationObserver(mutations => {
    const searchbox = document.querySelector('textarea');

    if (!searchbox) {
      return;
    }
    searchbox_observer.disconnect();

    const suggestions_observer = new MutationObserver(() => {
      // Phone and tablet page versions have suggestions list implemented
      // in a different way; we check for either of them to be present.
      const tablet_suggestions =  document.querySelectorAll('.G43f7e');
      const phone_suggestions = document.querySelector('.aajZCb');
      if (tablet_suggestions.length > 1 || (phone_suggestions !== null &&
              phone_suggestions.childElementCount > 0)) {
        suggestions_observer.disconnect();
        performance.mark('LoadLine2/google_search_result/interactive');
      }
    });
    suggestions_observer.observe(document, {childList: true, subtree: true});

    searchbox.focus();
    searchbox.click();
  });

  const overview_observer = new MutationObserver(unused => {
    if (document.querySelector('.a-no-hover-decoration')) {
      performance.mark('LoadLine2/google_search_result/visual');
      overview_observer.disconnect();
    }
  });

  overview_observer.observe(document, {childList: true, subtree: true});
  searchbox_observer.observe(document, {childList: true, subtree: true});
}
