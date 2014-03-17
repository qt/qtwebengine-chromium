// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a MarginSettings object. This object encapsulates all settings and
   * logic related to the margins mode.
   * @param {!print_preview.ticket_items.MarginsType} marginsTypeTicketItem Used
   *     to read and write the margins type ticket item.
   * @constructor
   * @extends {print_preview.Component}
   */
  function MarginSettings(marginsTypeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write the margins type ticket item.
     * @type {!print_preview.ticket_items.MarginsType}
     * @private
     */
    this.marginsTypeTicketItem_ = marginsTypeTicketItem;
  };

  /**
   * CSS classes used by the margin settings component.
   * @enum {string}
   * @private
   */
  MarginSettings.Classes_ = {
    SELECT: 'margin-settings-select'
  };

  MarginSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.select_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.tracker.add(
          this.select_, 'change', this.onSelectChange_.bind(this));
      this.tracker.add(
          this.marginsTypeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onMarginsTypeTicketItemChange_.bind(this));
    },

    /**
     * @return {HTMLSelectElement} Select element containing the margin options.
     * @private
     */
    get select_() {
      return this.getElement().getElementsByClassName(
          MarginSettings.Classes_.SELECT)[0];
    },

    /**
     * Called when the select element is changed. Updates the print ticket
     * margin type.
     * @private
     */
    onSelectChange_: function() {
      var select = this.select_;
      var marginsType =
          /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
              select.selectedIndex);
      this.marginsTypeTicketItem_.updateValue(marginsType);
    },

    /**
     * Called when the print ticket store changes. Selects the corresponding
     * select option.
     * @private
     */
    onMarginsTypeTicketItemChange_: function() {
      if (this.marginsTypeTicketItem_.isCapabilityAvailable()) {
        var select = this.select_;
        var marginsType = this.marginsTypeTicketItem_.getValue();
        var selectedMarginsType =
            /** @type {!print_preview.ticket_items.MarginsType.Value} */ (
                select.selectedIndex);
        if (marginsType != selectedMarginsType) {
          select.options[selectedMarginsType].selected = false;
          select.options[marginsType].selected = true;
        }
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    MarginSettings: MarginSettings
  };
});
