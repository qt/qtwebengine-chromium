// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Rendering for iframed most visited thumbnails.
 */

window.addEventListener('DOMContentLoaded', function() {
  'use strict';

  fillMostVisited(document.location, function(params, data) {
    function logEvent(eventName) {
      chrome.embeddedSearch.newTabPage.logEvent(eventName);
    }
    function logImpression(tileIndex, provider) {
      chrome.embeddedSearch.newTabPage.logImpression(tileIndex, provider);
    }
    function showDomainElement() {
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ERROR);
      var link = createMostVisitedLink(
          params, data.url, data.title, undefined, data.ping, data.provider);
      var domain = document.createElement('div');
      domain.textContent = data.domain;
      link.appendChild(domain);
      document.body.appendChild(link);
    }
    // Called on intentionally empty tiles for which the visuals are handled
    // externally by the page itself.
    function showEmptyTile() {
      var link = createMostVisitedLink(
          params, data.url, data.title, undefined, data.ping, data.provider);
      document.body.appendChild(link);
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_EXTERNAL_TILE);
    }
    function createAndAppendThumbnail(isVisible) {
      var image = new Image();
      image.onload = function() {
        var shadow = document.createElement('span');
        shadow.classList.add('shadow');
        var link = createMostVisitedLink(
            params, data.url, data.title, undefined, data.ping, data.provider);
        link.appendChild(shadow);
        link.appendChild(image);
        // We add 'position: absolute' in anticipation that there could be more
        // than one thumbnail. This will superpose the elements.
        link.style.position = 'absolute';
        document.body.appendChild(link);
      };
      if (!isVisible) {
        image.style.visibility = 'hidden';
      }
      return image;
    }
    // Log an impression if we know the position of the tile.
    if (isFinite(params.pos) && data.provider) {
      logImpression(parseInt(params.pos, 10), data.provider);
    }

    if (data.thumbnailUrl) {
      var image = createAndAppendThumbnail(true);
      // If a backup thumbnail URL was provided, preload it in case the first
      // thumbnail errors. The backup thumbnail is always preloaded so that the
      // server can't gain knowledge on the local thumbnail DB by specifying a
      // second URL that is only sometimes fetched.
      if (data.thumbnailUrl2) {
        var image2 = createAndAppendThumbnail(false);
        var imageFailed = false;
        var image2Failed = false;
        image2.onerror = function() {
          image2Failed = true;
          image2.style.visibility = 'hidden';
          if (imageFailed) {
            showDomainElement();
          }
        };
        image2.src = data.thumbnailUrl2;
        // The first thumbnail's onerror function will swap the visibility of
        // the two thumbnails.
        image.onerror = function() {
          logEvent(NTP_LOGGING_EVENT_TYPE.NTP_FALLBACK_THUMBNAIL_USED);
          imageFailed = true;
          image.style.visibility = 'hidden';
          if (image2Failed) {
            showDomainElement();
          } else {
            image2.style.visibility = 'visible';
          }
        };
        logEvent(NTP_LOGGING_EVENT_TYPE.NTP_FALLBACK_THUMBNAIL_REQUESTED);
      } else {
        image.onerror = showDomainElement;
      }
      image.src = data.thumbnailUrl;
      logEvent(NTP_LOGGING_EVENT_TYPE.NTP_THUMBNAIL_ATTEMPT);
    } else if (data.domain) {
      showDomainElement();
    } else {
      showEmptyTile();
    }
  });
});
