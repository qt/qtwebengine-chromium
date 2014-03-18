// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Search box.
 *
 * @param {element} element Root element of the search box.
 * @constructor
 */
function SearchBox(element) {
  /**
   * Autocomplete List.
   * @type {AutocompleteList}
   */
  this.autocompleteList = new SearchBox.AutocompleteList(element.ownerDocument);

  /**
   * Root element of the search box.
   * @type {HTMLElement}
   */
  this.element = element;

  /**
   * Text input of the search box.
   * @type {HTMLElement}
   */
  this.inputElement = element.querySelector('input');

  /**
   * Clear button of the search box.
   * @type {HTMLElement}
   */
  this.clearButton = element.querySelector('.clear');

  /**
   * Text measure.
   * @type {TextMeasure}
   * @private
   */
  this.textMeasure_ = new TextMeasure(this.inputElement);

  Object.freeze(this);

  // Register events.
  this.inputElement.addEventListener('input', this.updateStyles_.bind(this));
  this.inputElement.addEventListener('keydown', this.onKeyDown_.bind(this));
  this.inputElement.addEventListener('focus', this.onFocus_.bind(this));
  this.inputElement.addEventListener('blur', this.onBlur_.bind(this));
  element.querySelector('.icon').addEventListener(
      'click', this.onIconClick_.bind(this));
  element.parentNode.appendChild(this.autocompleteList);
}

/**
 * Autocomplete list for search box.
 * @param {HTMLDocument} document Document.
 * @constructor
 */
SearchBox.AutocompleteList = function(document) {
  var self = cr.ui.AutocompleteList.call(this);
  self.__proto__ = SearchBox.AutocompleteList.prototype;
  self.id = 'autocomplete-list';
  self.autoExpands = true;
  self.itemConstructor = SearchBox.AutocompleteListItem_.bind(null, document);
  self.addEventListener('mouseover', self.onMouseOver_.bind(self));
  return self;
};

SearchBox.AutocompleteList.prototype = {
  __proto__: cr.ui.AutocompleteList.prototype
};

/**
 * Do nothing when a suggestion is selected.
 * @override
 */
SearchBox.AutocompleteList.prototype.handleSelectedSuggestion = function() {};

/**
 * Change the selection by a mouse over instead of just changing the
 * color of moused over element with :hover in CSS. Here's why:
 *
 * 1) The user selects an item A with up/down keys (item A is highlighted)
 * 2) Then the user moves the cursor to another item B
 *
 * If we just change the color of moused over element (item B), both
 * the item A and B are highlighted. This is bad. We should change the
 * selection so only the item B is highlighted.
 *
 * @param {Event} event Event.
 * @private
 */
SearchBox.AutocompleteList.prototype.onMouseOver_ = function(event) {
  if (event.target.itemInfo)
    this.selectedItem = event.target.itemInfo;
};

/**
 * ListItem element for autocomple.
 *
 * @param {HTMLDocument} document Document.
 * @param {Object} item An object representing a suggestion.
 * @constructor
 * @private
 */
SearchBox.AutocompleteListItem_ = function(document, item) {
  var li = new cr.ui.ListItem();
  li.itemInfo = item;

  var icon = document.createElement('div');
  icon.className = 'detail-icon';

  var text = document.createElement('div');
  text.className = 'detail-text';

  if (item.isHeaderItem) {
    icon.setAttribute('search-icon', '');
    text.innerHTML =
        strf('SEARCH_DRIVE_HTML', util.htmlEscape(item.searchQuery));
  } else {
    var iconType = FileType.getIcon(item.entry);
    icon.setAttribute('file-type-icon', iconType);
    // highlightedBaseName is a piece of HTML with meta characters properly
    // escaped. See the comment at fileBrowserPrivate.searchDriveMetadata().
    text.innerHTML = item.highlightedBaseName;
  }
  li.appendChild(icon);
  li.appendChild(text);
  return li;
};

/**
 * Updates the size related style.
 */
SearchBox.prototype.updateSizeRelatedStyle = function() {
  // Hide the search box if there is not enough space.
  this.element.classList.toggle(
      'too-short',
      this.element.clientWidth < 100);
};

/**
 * Clears the search query.
 */
SearchBox.prototype.clear = function() {
  this.inputElement.value = '';
  this.updateStyles_();
};

/**
 * Handles a focus event of the search box.
 * @private
 */
SearchBox.prototype.onFocus_ = function() {
  this.element.classList.toggle('has-cursor', true);
  this.inputElement.tabIndex = '99';  // See: go/filesapp-tabindex.
  this.autocompleteList.attachToInput(this.inputElement);
};

/**
 * Handles a blur event of the search box.
 * @private
 */
SearchBox.prototype.onBlur_ = function() {
  this.element.classList.toggle('has-cursor', false);
  this.inputElement.tabIndex = '-1';
  this.autocompleteList.detach();
};

/**
 * Handles a keydown event of the search box.
 * @private
 */
SearchBox.prototype.onKeyDown_ = function() {
  // Handle only Esc key now.
  if (event.keyCode != 27 || this.inputElement.value)
    return;
  this.inputElement.blur();
};

/**
 * Handles a click event of the search icon.
 * @private
 */
SearchBox.prototype.onIconClick_ = function() {
  this.inputElement.focus();
};

/**
 * Updates styles of the search box.
 * @private
 */
SearchBox.prototype.updateStyles_ = function() {
  this.element.classList.toggle('has-text',
                                 !!this.inputElement.value);
  var width = this.textMeasure_.getWidth(this.inputElement.value) +
      16 /* Extra space to allow leeway. */;
  this.inputElement.style.width = width + 'px';
};
