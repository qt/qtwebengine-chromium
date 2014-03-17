// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Utilities for rendering most visited thumbnails and titles.
 */

<include src="instant_iframe_validation.js">

/**
 * Enum for the different types of events that are logged from the NTP.
 * @enum {number}
 * @const
 */
var NTP_LOGGING_EVENT_TYPE = {
  // The user moused over an NTP tile or title.
  NTP_MOUSEOVER: 0,
  // The page attempted to load a thumbnail image.
  NTP_THUMBNAIL_ATTEMPT: 1,
  // There was an error in loading both the thumbnail image and the fallback
  // (if it was provided), resulting in a grey tile.
  NTP_THUMBNAIL_ERROR: 2,
  // The page attempted to load a thumbnail URL while a fallback thumbnail was
  // provided.
  NTP_FALLBACK_THUMBNAIL_REQUESTED: 3,
  // The primary thumbnail image failed to load and caused us to use the
  // secondary thumbnail as a fallback.
  NTP_FALLBACK_THUMBNAIL_USED: 4,
  // The suggestion is coming from the server.
  NTP_SERVER_SIDE_SUGGESTION: 5,
  // The suggestion is coming from the client.
  NTP_CLIENT_SIDE_SUGGESTION: 6,
  // The visuals of that tile are handled externally by the page itself.
  NTP_EXTERNAL_TILE: 7
};

/**
 * Type of the impression provider for a generic client-provided suggestion.
 * @type {string}
 * @const
 */
var CLIENT_PROVIDER_NAME = 'client';

/**
 * Type of the impression provider for a generic server-provided suggestion.
 * @type {string}
 * @const
 */
var SERVER_PROVIDER_NAME = 'server';

/**
 * Parses query parameters from Location.
 * @param {string} location The URL to generate the CSS url for.
 * @return {Object} Dictionary containing name value pairs for URL.
 */
function parseQueryParams(location) {
  var params = Object.create(null);
  var query = location.search.substring(1);
  var vars = query.split('&');
  for (var i = 0; i < vars.length; i++) {
    var pair = vars[i].split('=');
    var k = decodeURIComponent(pair[0]);
    if (k in params) {
      // Duplicate parameters are not allowed to prevent attackers who can
      // append things to |location| from getting their parameter values to
      // override legitimate ones.
      return Object.create(null);
    } else {
      params[k] = decodeURIComponent(pair[1]);
    }
  }
  return params;
}


/**
 * Creates a new most visited link element.
 * @param {Object} params URL parameters containing styles for the link.
 * @param {string} href The destination for the link.
 * @param {string} title The title for the link.
 * @param {string|undefined} text The text for the link or none.
 * @param {string|undefined} ping If specified, a location relative to the
 *     referrer of this iframe, to ping when the link is clicked. Only works if
 *     the referrer is HTTPS.
 * @param {string|undefined} provider A provider name (max 8 alphanumeric
 *     characters) used for logging. Undefined if suggestion is not coming from
 *     the server.
 * @return {HTMLAnchorElement} A new link element.
 */
function createMostVisitedLink(params, href, title, text, ping, provider) {
  var styles = getMostVisitedStyles(params, !!text);
  var link = document.createElement('a');
  link.style.color = styles.color;
  link.style.fontSize = styles.fontSize + 'px';
  if (styles.fontFamily)
    link.style.fontFamily = styles.fontFamily;
  link.href = href;
  if ('pos' in params && isFinite(params.pos)) {
    link.ping = '/log.html?pos=' + params.pos;
    if (provider)
      link.ping += '&pr=' + provider;
    // If a ping parameter was specified, add it to the list of pings, relative
    // to the referrer of this iframe, which is the default search provider.
    if (ping) {
      var parentUrl = document.createElement('a');
      parentUrl.href = document.referrer;
      if (parentUrl.protocol == 'https:') {
        link.ping += ' ' + parentUrl.origin + '/' + ping;
      }
    }
  }
  link.title = title;
  link.target = '_top';
  // Exclude links from the tab order.  The tabIndex is added to the thumbnail
  // parent container instead.
  link.tabIndex = '-1';
  if (text)
    link.textContent = text;
  link.addEventListener('mouseover', function() {
    var ntpApiHandle = chrome.embeddedSearch.newTabPage;
    ntpApiHandle.logEvent(NTP_LOGGING_EVENT_TYPE.NTP_MOUSEOVER);
  });
  return link;
}


/**
 * Decodes most visited styles from URL parameters.
 * - f: font-family
 * - fs: font-size as a number in pixels.
 * - c: A hexadecimal number interpreted as a hex color code.
 * @param {Object.<string, string>} params URL parameters specifying style.
 * @param {boolean} isTitle if the style is for the Most Visited Title.
 * @return {Object} Styles suitable for CSS interpolation.
 */
function getMostVisitedStyles(params, isTitle) {
  var styles = {
    color: '#777',
    fontFamily: '',
    fontSize: 11
  };
  var apiHandle = chrome.embeddedSearch.newTabPage;
  var themeInfo = apiHandle.themeBackgroundInfo;
  if (isTitle && themeInfo && !themeInfo.usingDefaultTheme) {
    styles.color = convertArrayToRGBAColor(themeInfo.textColorRgba) ||
        styles.color;
  } else if ('c' in params) {
    styles.color = convertToHexColor(parseInt(params.c, 16)) || styles.color;
  }
  if ('f' in params && /^[-0-9a-zA-Z ,]+$/.test(params.f))
    styles.fontFamily = params.f;
  if ('fs' in params && isFinite(parseInt(params.fs, 10)))
    styles.fontSize = parseInt(params.fs, 10);
  return styles;
}


/**
 * @param {string} location A location containing URL parameters.
 * @param {function(Object, Object)} fill A function called with styles and
 *     data to fill.
 */
function fillMostVisited(location, fill) {
  var params = parseQueryParams(document.location);
  params.rid = parseInt(params.rid, 10);
  if (!isFinite(params.rid) && !params.url)
    return;
  // Log whether the suggestion was obtained from the server or the client.
  chrome.embeddedSearch.newTabPage.logEvent(params.url ?
      NTP_LOGGING_EVENT_TYPE.NTP_SERVER_SIDE_SUGGESTION :
      NTP_LOGGING_EVENT_TYPE.NTP_CLIENT_SIDE_SUGGESTION);
  var data = {};
  if (params.url) {
    // Means that the suggestion data comes from the server. Create data object.
    data.url = params.url;
    data.thumbnailUrl = params.tu || '';
    data.thumbnailUrl2 = params.tu2 || '';
    data.title = params.ti || '';
    data.direction = params.di || '';
    data.domain = params.dom || '';
    data.ping = params.ping || '';
    data.provider = params.pr || SERVER_PROVIDER_NAME;

    // Log the fact that suggestion was obtained from the server.
    var ntpApiHandle = chrome.embeddedSearch.newTabPage;
    ntpApiHandle.logEvent(NTP_LOGGING_EVENT_TYPE.NTP_SERVER_SIDE_SUGGESTION);
  } else {
    var apiHandle = chrome.embeddedSearch.searchBox;
    data = apiHandle.getMostVisitedItemData(params.rid);
    if (!data)
      return;
    data.provider = CLIENT_PROVIDER_NAME;
    delete data.ping;
  }
  if (/^javascript:/i.test(data.url) ||
      /^javascript:/i.test(data.thumbnailUrl) ||
      /^javascript:/i.test(data.thumbnailUrl2) ||
      !/^[a-z0-9]{0,8}$/i.test(data.provider))
    return;
  if (data.direction)
    document.body.dir = data.direction;
  fill(params, data);
}
