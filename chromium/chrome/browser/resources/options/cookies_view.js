// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /////////////////////////////////////////////////////////////////////////////
  // CookiesView class:

  /**
   * Encapsulated handling of the cookies and other site data page.
   * @constructor
   */
  function CookiesView(model) {
    OptionsPage.call(this, 'cookies',
                     loadTimeData.getString('cookiesViewPageTabTitle'),
                     'cookies-view-page');
  }

  cr.addSingletonGetter(CookiesView);

  CookiesView.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * The timer id of the timer set on search query change events.
     * @type {number}
     * @private
     */
    queryDelayTimerId_: 0,

    /**
     * The most recent search query, empty string if the query is empty.
     * @type {string}
     * @private
     */
    lastQuery_: '',

    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var searchBox = this.pageDiv.querySelector('.cookies-search-box');
      searchBox.addEventListener(
          'search', this.handleSearchQueryChange_.bind(this));
      searchBox.onkeydown = function(e) {
        // Prevent the overlay from handling this event.
        if (e.keyIdentifier == 'Enter')
          e.stopPropagation();
      };

      this.pageDiv.querySelector('.remove-all-cookies-button').onclick =
          function(e) {
            chrome.send('removeAllCookies');
          };

      var cookiesList = this.pageDiv.querySelector('.cookies-list');
      options.CookiesList.decorate(cookiesList);

      this.addEventListener('visibleChange', this.handleVisibleChange_);

      this.pageDiv.querySelector('.cookies-view-overlay-confirm').onclick =
          OptionsPage.closeOverlay.bind(OptionsPage);
    },

    /** @override */
    didShowPage: function() {
      this.pageDiv.querySelector('.cookies-search-box').value = '';
      this.lastQuery_ = '';
    },

    /**
     * Search cookie using text in |cookies-search-box|.
     */
    searchCookie: function() {
      this.queryDelayTimerId_ = 0;
      var filter = this.pageDiv.querySelector('.cookies-search-box').value;
      if (this.lastQuery_ != filter) {
        this.lastQuery_ = filter;
        chrome.send('updateCookieSearchResults', [filter]);
      }
    },

    /**
     * Handles search query changes.
     * @param {!Event} e The event object.
     * @private
     */
    handleSearchQueryChange_: function(e) {
      if (this.queryDelayTimerId_)
        window.clearTimeout(this.queryDelayTimerId_);

      this.queryDelayTimerId_ = window.setTimeout(
          this.searchCookie.bind(this), 500);
    },

    initialized_: false,

    /**
     * Handler for OptionsPage's visible property change event.
     * @param {Event} e Property change event.
     * @private
     */
    handleVisibleChange_: function(e) {
      if (!this.visible)
        return;

      chrome.send('reloadCookies');

      if (!this.initialized_) {
        this.initialized_ = true;
        this.searchCookie();
      } else {
        this.pageDiv.querySelector('.cookies-list').redraw();
      }

      this.pageDiv.querySelector('.cookies-search-box').focus();
    },
  };

  // CookiesViewHandler callbacks.
  CookiesView.onTreeItemAdded = function(args) {
    $('cookies-list').addByParentId(args[0], args[1], args[2]);
  };

  CookiesView.onTreeItemRemoved = function(args) {
    $('cookies-list').removeByParentId(args[0], args[1], args[2]);
  };

  CookiesView.loadChildren = function(args) {
    $('cookies-list').loadChildren(args[0], args[1]);
  };

  // Export
  return {
    CookiesView: CookiesView
  };

});
