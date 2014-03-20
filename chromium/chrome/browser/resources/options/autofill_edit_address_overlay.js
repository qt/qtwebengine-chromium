// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var OptionsPage = options.OptionsPage;
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;

  // The GUID of the loaded address.
  var guid;

  /**
   * AutofillEditAddressOverlay class
   * Encapsulated handling of the 'Add Page' overlay page.
   * @class
   */
  function AutofillEditAddressOverlay() {
    OptionsPage.call(this, 'autofillEditAddress',
                     loadTimeData.getString('autofillEditAddressTitle'),
                     'autofill-edit-address-overlay');
  }

  cr.addSingletonGetter(AutofillEditAddressOverlay);

  AutofillEditAddressOverlay.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initializes the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.createMultiValueLists_();

      var self = this;
      $('autofill-edit-address-cancel-button').onclick = function(event) {
        self.dismissOverlay_();
      };

      // TODO(jhawkins): Investigate other possible solutions.
      $('autofill-edit-address-apply-button').onclick = function(event) {
        // Blur active element to ensure that pending changes are committed.
        if (document.activeElement)
          document.activeElement.blur();
        // Blurring is delayed for list elements.  Queue save and close to
        // ensure that pending changes have been applied.
        setTimeout(function() {
          self.saveAddress_();
          self.dismissOverlay_();
        }, 0);
      };

      // Prevent 'blur' events on the OK and cancel buttons, which can trigger
      // insertion of new placeholder elements.  The addition of placeholders
      // affects layout, which interferes with being able to click on the
      // buttons.
      $('autofill-edit-address-apply-button').onmousedown = function(event) {
        event.preventDefault();
      };
      $('autofill-edit-address-cancel-button').onmousedown = function(event) {
        event.preventDefault();
      };

      self.guid = '';
      self.populateCountryList_();
      self.clearInputFields_();
      self.connectInputEvents_();
    },

    /**
    * Specifically catch the situations in which the overlay is cancelled
    * externally (e.g. by pressing <Esc>), so that the input fields and
    * GUID can be properly cleared.
    * @override
    */
    handleCancel: function() {
      this.dismissOverlay_();
    },

    /**
     * Creates, decorates and initializes the multi-value lists for full name,
     * phone, and email.
     * @private
     */
    createMultiValueLists_: function() {
      var list = $('full-name-list');
      options.autofillOptions.AutofillNameValuesList.decorate(list);
      list.autoExpands = true;

      list = $('phone-list');
      options.autofillOptions.AutofillPhoneValuesList.decorate(list);
      list.autoExpands = true;

      list = $('email-list');
      options.autofillOptions.AutofillValuesList.decorate(list);
      list.autoExpands = true;
    },

    /**
     * Updates the data model for the list named |listName| with the values from
     * |entries|.
     * @param {string} listName The id of the list.
     * @param {Array} entries The list of items to be added to the list.
     */
    setMultiValueList_: function(listName, entries) {
      // Add data entries.
      var list = $(listName);

      // Add special entry for adding new values.
      var augmentedList = entries.slice();
      augmentedList.push(null);
      list.dataModel = new ArrayDataModel(augmentedList);

      // Update the status of the 'OK' button.
      this.inputFieldChanged_();

      list.dataModel.addEventListener('splice',
                                      this.inputFieldChanged_.bind(this));
      list.dataModel.addEventListener('change',
                                      this.inputFieldChanged_.bind(this));
    },

    /**
     * Clears any uncommitted input, resets the stored GUID and dismisses the
     * overlay.
     * @private
     */
    dismissOverlay_: function() {
      this.clearInputFields_();
      this.guid = '';
      OptionsPage.closeOverlay();
    },

    /**
     * Aggregates the values in the input fields into an array and sends the
     * array to the Autofill handler.
     * @private
     */
    saveAddress_: function() {
      var address = new Array();
      address[0] = this.guid;
      var list = $('full-name-list');
      address[1] = list.dataModel.slice(0, list.dataModel.length - 1);
      address[2] = $('company-name').value;
      address[3] = $('addr-line-1').value;
      address[4] = $('addr-line-2').value;
      address[5] = $('city').value;
      address[6] = $('state').value;
      address[7] = $('postal-code').value;
      address[8] = $('country').value;
      list = $('phone-list');
      address[9] = list.dataModel.slice(0, list.dataModel.length - 1);
      list = $('email-list');
      address[10] = list.dataModel.slice(0, list.dataModel.length - 1);

      chrome.send('setAddress', address);
    },

    /**
     * Connects each input field to the inputFieldChanged_() method that enables
     * or disables the 'Ok' button based on whether all the fields are empty or
     * not.
     * @private
     */
    connectInputEvents_: function() {
      var self = this;
      $('company-name').oninput = $('addr-line-1').oninput =
          $('addr-line-2').oninput = $('city').oninput = $('state').oninput =
          $('postal-code').oninput = function(event) {
        self.inputFieldChanged_();
      };

      $('country').onchange = function(event) {
        self.countryChanged_();
      };
    },

    /**
     * Checks the values of each of the input fields and disables the 'Ok'
     * button if all of the fields are empty.
     * @private
     */
    inputFieldChanged_: function() {
      // Length of lists are tested for <= 1 due to the "add" placeholder item
      // in the list.
      var disabled =
          $('full-name-list').items.length <= 1 &&
          !$('company-name').value &&
          !$('addr-line-1').value && !$('addr-line-2').value &&
          !$('city').value && !$('state').value && !$('postal-code').value &&
          !$('country').value && $('phone-list').items.length <= 1 &&
          $('email-list').items.length <= 1;
      $('autofill-edit-address-apply-button').disabled = disabled;
    },

    /**
     * Updates the postal code and state field labels appropriately for the
     * selected country.
     * @private
     */
    countryChanged_: function() {
      var countryCode = $('country').value ||
          loadTimeData.getString('defaultCountryCode');

      var details = loadTimeData.getValue('autofillCountryData')[countryCode];
      var postal = $('postal-code-label');
      postal.textContent = details.postalCodeLabel;
      $('state-label').textContent = details.stateLabel;

      // Also update the 'Ok' button as needed.
      this.inputFieldChanged_();
    },

    /**
     * Populates the country <select> list.
     * @private
     */
    populateCountryList_: function() {
      var countryList = loadTimeData.getValue('autofillCountrySelectList');

      // Add the countries to the country <select> list.
      var countrySelect = $('country');
      // Add an empty option.
      countrySelect.appendChild(new Option('', ''));
      for (var i = 0; i < countryList.length; i++) {
        var option = new Option(countryList[i].name,
                                countryList[i].value);
        option.disabled = countryList[i].value == 'separator';
        countrySelect.appendChild(option);
      }
    },

    /**
     * Clears the value of each input field.
     * @private
     */
    clearInputFields_: function() {
      this.setMultiValueList_('full-name-list', []);
      $('company-name').value = '';
      $('addr-line-1').value = '';
      $('addr-line-2').value = '';
      $('city').value = '';
      $('state').value = '';
      $('postal-code').value = '';
      $('country').value = '';
      this.setMultiValueList_('phone-list', []);
      this.setMultiValueList_('email-list', []);

      this.countryChanged_();
    },

    /**
     * Loads the address data from |address|, sets the input fields based on
     * this data and stores the GUID of the address.
     * @private
     */
    loadAddress_: function(address) {
      this.setInputFields_(address);
      this.inputFieldChanged_();
      this.guid = address.guid;
    },

    /**
     * Sets the value of each input field according to |address|
     * @private
     */
    setInputFields_: function(address) {
      this.setMultiValueList_('full-name-list', address.fullName);
      $('company-name').value = address.companyName;
      $('addr-line-1').value = address.addrLine1;
      $('addr-line-2').value = address.addrLine2;
      $('city').value = address.city;
      $('state').value = address.state;
      $('postal-code').value = address.postalCode;
      $('country').value = address.country;
      this.setMultiValueList_('phone-list', address.phone);
      this.setMultiValueList_('email-list', address.email);

      this.countryChanged_();
    },
  };

  AutofillEditAddressOverlay.loadAddress = function(address) {
    AutofillEditAddressOverlay.getInstance().loadAddress_(address);
  };

  AutofillEditAddressOverlay.setTitle = function(title) {
    $('autofill-address-title').textContent = title;
  };

  AutofillEditAddressOverlay.setValidatedPhoneNumbers = function(numbers) {
    AutofillEditAddressOverlay.getInstance().setMultiValueList_('phone-list',
                                                                numbers);
  };

  // Export
  return {
    AutofillEditAddressOverlay: AutofillEditAddressOverlay
  };
});
