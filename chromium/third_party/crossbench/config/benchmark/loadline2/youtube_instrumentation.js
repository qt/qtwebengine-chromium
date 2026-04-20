// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (window.location.href ===
    'https://www.youtube.com/watch?v=WuS9kPNAXHw&themeRefresh=1') {
  const button_selector =
      'div.body.style-scope.ytd-consent-bump-v2-lightbox > div.eom-buttons.style-scope.ytd-consent-bump-v2-lightbox > div:nth-child(1) > ytd-button-renderer:nth-child(1) > yt-button-shape > button';
  const banner_selector =
      'ytd-consent-bump-v2-lightbox > tp-yt-paper-dialog[id=dialog]';
  const comment_id = 'comment-teaser';

  const button_observer = new MutationObserver(mutations => {
    const button = document.querySelector(button_selector);
    if (!button) {
      return;
    }
    const banner_node = document.querySelector(banner_selector);
    if (!banner_node) {
      return;
    }
    button_observer.disconnect();
    const banner_observer = new MutationObserver(function(e) {
      for (m of e) {
        if (m.type === 'attributes' && banner_node.style.display === 'none') {
          banner_observer.disconnect();
          performance.mark('LoadLine2/youtube_video/interactive');
          break;
        }
      }
    });
    banner_observer.observe(
        banner_node, {attributes: true, attributeFilter: ['style']});
    button.click();
  });

  const comment_observer = new MutationObserver(mutations => {
    const comment = document.getElementById(comment_id);
    if (!comment) {
      return;
    }
    comment_observer.disconnect();
    performance.mark('LoadLine2/youtube_video/visual');
  });

  comment_observer.observe(document, {childList: true, subtree: true});
  button_observer.observe(document, {childList: true, subtree: true});
}
