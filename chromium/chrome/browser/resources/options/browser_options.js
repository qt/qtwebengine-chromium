// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;
  var ArrayDataModel = cr.ui.ArrayDataModel;
  var RepeatingButton = cr.ui.RepeatingButton;

  //
  // BrowserOptions class
  // Encapsulated handling of browser options page.
  //
  function BrowserOptions() {
    OptionsPage.call(this, 'settings', loadTimeData.getString('settingsTitle'),
                     'settings');
  }

  cr.addSingletonGetter(BrowserOptions);

  BrowserOptions.prototype = {
    __proto__: options.OptionsPage.prototype,

    /**
     * Keeps track of whether the user is signed in or not.
     * @type {boolean}
     * @private
     */
    signedIn_: false,

    /**
     * Keeps track of whether |onShowHomeButtonChanged_| has been called. See
     * |onShowHomeButtonChanged_|.
     * @type {boolean}
     * @private
     */
    onShowHomeButtonChangedCalled_: false,

    /**
     * Track if page initialization is complete.  All C++ UI handlers have the
     * chance to manipulate page content within their InitializePage methods.
     * This flag is set to true after all initializers have been called.
     * @type {boolean}
     * @private
     */
    initializationComplete_: false,

    /** @override */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);
      var self = this;

      // Ensure that navigation events are unblocked on uber page. A reload of
      // the settings page while an overlay is open would otherwise leave uber
      // page in a blocked state, where tab switching is not possible.
      uber.invokeMethodOnParent('stopInterceptingEvents');

      window.addEventListener('message', this.handleWindowMessage_.bind(this));

      $('advanced-settings-expander').onclick = function() {
        self.toggleSectionWithAnimation_(
            $('advanced-settings'),
            $('advanced-settings-container'));

        // If the link was focused (i.e., it was activated using the keyboard)
        // and it was used to show the section (rather than hiding it), focus
        // the first element in the container.
        if (document.activeElement === $('advanced-settings-expander') &&
                $('advanced-settings').style.height === '') {
          var focusElement = $('advanced-settings-container').querySelector(
              'button, input, list, select, a[href]');
          if (focusElement)
            focusElement.focus();
        }
      }

      $('advanced-settings').addEventListener('webkitTransitionEnd',
          this.updateAdvancedSettingsExpander_.bind(this));

      if (cr.isChromeOS)
        UIAccountTweaks.applyGuestModeVisibility(document);

      // Sync (Sign in) section.
      this.updateSyncState_(loadTimeData.getValue('syncData'));

      $('start-stop-sync').onclick = function(event) {
        if (self.signedIn_)
          SyncSetupOverlay.showStopSyncingUI();
        else if (cr.isChromeOS)
          SyncSetupOverlay.showSetupUI();
        else
          SyncSetupOverlay.startSignIn();
      };
      $('customize-sync').onclick = function(event) {
        SyncSetupOverlay.showSetupUI();
      };

      // Internet connection section (ChromeOS only).
      if (cr.isChromeOS) {
        options.network.NetworkList.decorate($('network-list'));
        options.network.NetworkList.refreshNetworkData(
            loadTimeData.getValue('networkData'));
      }

      // On Startup section.
      Preferences.getInstance().addEventListener('session.restore_on_startup',
          this.onRestoreOnStartupChanged_.bind(this));
      Preferences.getInstance().addEventListener(
          'session.startup_urls',
          function(event) {
            $('startup-set-pages').disabled = event.value.disabled;
          });

      $('startup-set-pages').onclick = function() {
        OptionsPage.navigateToPage('startup');
      };

      // Appearance section.
      Preferences.getInstance().addEventListener('browser.show_home_button',
          this.onShowHomeButtonChanged_.bind(this));

      Preferences.getInstance().addEventListener('homepage',
          this.onHomePageChanged_.bind(this));
      Preferences.getInstance().addEventListener('homepage_is_newtabpage',
          this.onHomePageIsNtpChanged_.bind(this));

      $('change-home-page').onclick = function(event) {
        OptionsPage.navigateToPage('homePageOverlay');
      };

      if ($('set-wallpaper')) {
        $('set-wallpaper').onclick = function(event) {
          chrome.send('openWallpaperManager');
        };
      }

      $('themes-gallery').onclick = function(event) {
        window.open(loadTimeData.getString('themesGalleryURL'));
      };
      $('themes-reset').onclick = function(event) {
        chrome.send('themesReset');
      };

      if (loadTimeData.getBoolean('profileIsManaged')) {
        if ($('themes-native-button')) {
          $('themes-native-button').disabled = true;
          $('themes-native-button').hidden = true;
        }
        // Supervised users have just one default theme, even on Linux. So use
        // the same button for Linux as for the other platforms.
        $('themes-reset').textContent = loadTimeData.getString('themesReset');
      }

      // Device section (ChromeOS only).
      if (cr.isChromeOS) {
        $('keyboard-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('keyboard-overlay');
        };
        $('pointer-settings-button').onclick = function(evt) {
          OptionsPage.navigateToPage('pointer-overlay');
        };
      }

      // Search section.
      $('manage-default-search-engines').onclick = function(event) {
        OptionsPage.navigateToPage('searchEngines');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_ManageSearchEngines']);
      };
      $('default-search-engine').addEventListener('change',
          this.setDefaultSearchEngine_);
      // Without this, the bubble would overlap the uber frame navigation pane
      // and would not get mouse event as explained in crbug.com/311421.
      document.querySelector(
          '#default-search-engine + .controlled-setting-indicator').location =
              cr.ui.ArrowLocation.TOP_START;

      // Users section.
      if (loadTimeData.valueExists('profilesInfo')) {
        $('profiles-section').hidden = false;

        var profilesList = $('profiles-list');
        options.browser_options.ProfileList.decorate(profilesList);
        profilesList.autoExpands = true;

        // The profiles info data in |loadTimeData| might be stale.
        this.setProfilesInfo_(loadTimeData.getValue('profilesInfo'));
        chrome.send('requestProfilesInfo');

        profilesList.addEventListener('change',
            this.setProfileViewButtonsStatus_);
        $('profiles-create').onclick = function(event) {
          ManageProfileOverlay.showCreateDialog();
        };
        if (OptionsPage.isSettingsApp()) {
          $('profiles-app-list-switch').onclick = function(event) {
            var selectedProfile = self.getSelectedProfileItem_();
            chrome.send('switchAppListProfile', [selectedProfile.filePath]);
          };
        }
        $('profiles-manage').onclick = function(event) {
          ManageProfileOverlay.showManageDialog();
        };
        $('profiles-delete').onclick = function(event) {
          var selectedProfile = self.getSelectedProfileItem_();
          if (selectedProfile)
            ManageProfileOverlay.showDeleteDialog(selectedProfile);
        };
        if (loadTimeData.getBoolean('profileIsManaged')) {
          $('profiles-create').disabled = true;
          $('profiles-delete').disabled = true;
          $('profiles-list').canDeleteItems = false;
        }
      }

      if (cr.isChromeOS) {
        // Username (canonical email) of the currently logged in user or
        // |kGuestUser| if a guest session is active.
        this.username_ = loadTimeData.getString('username');

        this.updateAccountPicture_();

        $('account-picture').onclick = this.showImagerPickerOverlay_;
        $('change-picture-caption').onclick = this.showImagerPickerOverlay_;

        $('manage-accounts-button').onclick = function(event) {
          OptionsPage.navigateToPage('accounts');
          chrome.send('coreOptionsUserMetricsAction',
              ['Options_ManageAccounts']);
        };
      } else {
        $('import-data').onclick = function(event) {
          ImportDataOverlay.show();
          chrome.send('coreOptionsUserMetricsAction', ['Import_ShowDlg']);
        };

        if ($('themes-native-button')) {
          $('themes-native-button').onclick = function(event) {
            chrome.send('themesSetNative');
          };
        }
      }

      // Default browser section.
      if (!cr.isChromeOS) {
        $('set-as-default-browser').onclick = function(event) {
          chrome.send('becomeDefaultBrowser');
        };

        $('auto-launch').onclick = this.handleAutoLaunchChanged_;
      }

      // Privacy section.
      $('privacyContentSettingsButton').onclick = function(event) {
        OptionsPage.navigateToPage('content');
        OptionsPage.showTab($('cookies-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ContentSettings']);
      };
      $('privacyClearDataButton').onclick = function(event) {
        OptionsPage.navigateToPage('clearBrowserData');
        chrome.send('coreOptionsUserMetricsAction', ['Options_ClearData']);
      };
      $('privacyClearDataButton').hidden = OptionsPage.isSettingsApp();
      // 'metricsReportingEnabled' element is only present on Chrome branded
      // builds, and the 'metricsReportingCheckboxAction' message is only
      // handled on ChromeOS.
      if ($('metricsReportingEnabled') && cr.isChromeOS) {
        $('metricsReportingEnabled').onclick = function(event) {
          chrome.send('metricsReportingCheckboxAction',
              [String(event.currentTarget.checked)]);
        };
      }

      // Bluetooth (CrOS only).
      if (cr.isChromeOS) {
        options.system.bluetooth.BluetoothDeviceList.decorate(
            $('bluetooth-paired-devices-list'));

        $('bluetooth-add-device').onclick =
            this.handleAddBluetoothDevice_.bind(this);

        $('enable-bluetooth').onchange = function(event) {
          var state = $('enable-bluetooth').checked;
          chrome.send('bluetoothEnableChange', [Boolean(state)]);
        };

        $('bluetooth-reconnect-device').onclick = function(event) {
          var device = $('bluetooth-paired-devices-list').selectedItem;
          var address = device.address;
          chrome.send('updateBluetoothDevice', [address, 'connect']);
          OptionsPage.closeOverlay();
        };

        $('bluetooth-paired-devices-list').addEventListener('change',
            function() {
          var item = $('bluetooth-paired-devices-list').selectedItem;
          var disabled = !item || item.connected || !item.connectable;
          $('bluetooth-reconnect-device').disabled = disabled;
        });
      }

      // Passwords and Forms section.
      $('autofill-settings').onclick = function(event) {
        OptionsPage.navigateToPage('autofill');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowAutofillSettings']);
      };
      $('manage-passwords').onclick = function(event) {
        OptionsPage.navigateToPage('passwords');
        OptionsPage.showTab($('passwords-nav-tab'));
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_ShowPasswordManager']);
      };
      if (cr.isChromeOS && UIAccountTweaks.loggedInAsGuest()) {
        // Disable and turn off Autofill in guest mode.
        var autofillEnabled = $('autofill-enabled');
        autofillEnabled.disabled = true;
        autofillEnabled.checked = false;
        cr.dispatchSimpleEvent(autofillEnabled, 'change');
        $('autofill-settings').disabled = true;

        // Disable and turn off Password Manager in guest mode.
        var passwordManagerEnabled = $('password-manager-enabled');
        passwordManagerEnabled.disabled = true;
        passwordManagerEnabled.checked = false;
        cr.dispatchSimpleEvent(passwordManagerEnabled, 'change');
        $('manage-passwords').disabled = true;
      }

      if (cr.isMac) {
        $('mac-passwords-warning').hidden =
            !loadTimeData.getBoolean('multiple_profiles');
      }

      // Network section.
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').onclick = function(event) {
          chrome.send('showNetworkProxySettings');
        };
      }

      // Web Content section.
      $('fontSettingsCustomizeFontsButton').onclick = function(event) {
        OptionsPage.navigateToPage('fonts');
        chrome.send('coreOptionsUserMetricsAction', ['Options_FontSettings']);
      };
      $('defaultFontSize').onchange = function(event) {
        var value = event.target.options[event.target.selectedIndex].value;
        Preferences.setIntegerPref(
             'webkit.webprefs.default_fixed_font_size',
             value - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD, true);
        chrome.send('defaultFontSizeAction', [String(value)]);
      };
      $('defaultZoomFactor').onchange = function(event) {
        chrome.send('defaultZoomFactorAction',
            [String(event.target.options[event.target.selectedIndex].value)]);
      };

      // Languages section.
      var showLanguageOptions = function(event) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
            ['Options_LanuageAndSpellCheckSettings']);
      };
      $('language-button').onclick = showLanguageOptions;
      $('manage-languages').onclick = showLanguageOptions;

      // Downloads section.
      Preferences.getInstance().addEventListener('download.default_directory',
          this.onDefaultDownloadDirectoryChanged_.bind(this));
      $('downloadLocationChangeButton').onclick = function(event) {
        chrome.send('selectDownloadLocation');
      };
      if (!cr.isChromeOS) {
        $('autoOpenFileTypesResetToDefault').onclick = function(event) {
          chrome.send('autoOpenFileTypesAction');
        };
      } else {
        $('disable-drive-row').hidden =
            UIAccountTweaks.loggedInAsLocallyManagedUser();
      }

      // HTTPS/SSL section.
      if (cr.isWindows || cr.isMac) {
        $('certificatesManageButton').onclick = function(event) {
          chrome.send('showManageSSLCertificates');
        };
      } else {
        $('certificatesManageButton').onclick = function(event) {
          OptionsPage.navigateToPage('certificates');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_ManageSSLCertificates']);
        };
      }

      // Cloud Print section.
      // 'cloudPrintProxyEnabled' is true for Chrome branded builds on
      // certain platforms, or could be enabled by a lab.
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorSetupButton').onclick = function(event) {
          if ($('cloudPrintManageButton').style.display == 'none') {
            // Disable the button, set its text to the intermediate state.
            $('cloudPrintConnectorSetupButton').textContent =
              loadTimeData.getString('cloudPrintConnectorEnablingButton');
            $('cloudPrintConnectorSetupButton').disabled = true;
            chrome.send('showCloudPrintSetupDialog');
          } else {
            chrome.send('disableCloudPrintConnector');
          }
        };
      }
      $('cloudPrintManageButton').onclick = function(event) {
        chrome.send('showCloudPrintManagePage');
      };

      if (loadTimeData.getBoolean('cloudPrintShowMDnsOptions')) {
        $('cloudprint-options-mdns').hidden = false;
        $('cloudprint-options-nomdns').hidden = true;
        $('cloudPrintDevicesPageButton').onclick = function() {
          chrome.send('showCloudPrintDevicesPage');
        };
      }

      // Accessibility section (CrOS only).
      if (cr.isChromeOS) {
        var updateAccessibilitySettingsButton = function() {
          $('accessibility-settings').hidden =
              !($('accessibility-spoken-feedback-check').checked);
        };
        Preferences.getInstance().addEventListener(
            'settings.accessibility',
            updateAccessibilitySettingsButton);
        $('accessibility-settings-button').onclick = function(event) {
          window.open(loadTimeData.getString('accessibilitySettingsURL'));
        };
        $('accessibility-spoken-feedback-check').onchange = function(event) {
          chrome.send('spokenFeedbackChange',
                      [$('accessibility-spoken-feedback-check').checked]);
          updateAccessibilitySettingsButton();
        };
        updateAccessibilitySettingsButton();

        $('accessibility-high-contrast-check').onchange = function(event) {
          chrome.send('highContrastChange',
                      [$('accessibility-high-contrast-check').checked]);
        };

        var updateDelayDropdown = function() {
          $('accessibility-autoclick-dropdown').disabled =
              !$('accessibility-autoclick-check').checked;
        };
        Preferences.getInstance().addEventListener(
            $('accessibility-autoclick-check').getAttribute('pref'),
            updateDelayDropdown);

        $('accessibility-sticky-keys').hidden =
            !loadTimeData.getBoolean('enableStickyKeys');
      }

      // Display management section (CrOS only).
      if (cr.isChromeOS) {
        $('display-options').onclick = function(event) {
          OptionsPage.navigateToPage('display');
          chrome.send('coreOptionsUserMetricsAction',
                      ['Options_Display']);
        };
      }

      // Factory reset section (CrOS only).
      if (cr.isChromeOS) {
        $('factory-reset-restart').onclick = function(event) {
          OptionsPage.navigateToPage('factoryResetData');
        };
      }

      // System section.
      if (!cr.isChromeOS) {
        var updateGpuRestartButton = function() {
          $('gpu-mode-reset-restart').hidden =
              loadTimeData.getBoolean('gpuEnabledAtStart') ==
              $('gpu-mode-checkbox').checked;
        };
        Preferences.getInstance().addEventListener(
            $('gpu-mode-checkbox').getAttribute('pref'),
            updateGpuRestartButton);
        $('gpu-mode-reset-restart-button').onclick = function(event) {
          chrome.send('restartBrowser');
        };
        updateGpuRestartButton();
      }

      // Reset profile settings section.
      $('reset-profile-settings').onclick = function(event) {
        OptionsPage.navigateToPage('resetProfileSettings');
      };
      $('reset-profile-settings-section').hidden =
          !loadTimeData.getBoolean('enableResetProfileSettings');
    },

    /** @override */
    didShowPage: function() {
      $('search-field').focus();
    },

   /**
    * Called after all C++ UI handlers have called InitializePage to notify
    * that initialization is complete.
    * @private
    */
    notifyInitializationComplete_: function() {
      this.initializationComplete_ = true;
      cr.dispatchSimpleEvent(document, 'initializationComplete');
    },

    /**
     * Event listener for the 'session.restore_on_startup' pref.
     * @param {Event} event The preference change event.
     * @private
     */
    onRestoreOnStartupChanged_: function(event) {
      /** @const */ var showHomePageValue = 0;

      if (event.value.value == showHomePageValue) {
        // If the user previously selected "Show the homepage", the
        // preference will already be migrated to "Open a specific page". So
        // the only way to reach this code is if the 'restore on startup'
        // preference is managed.
        assert(event.value.controlledBy);

        // Select "open the following pages" and lock down the list of URLs
        // to reflect the intention of the policy.
        $('startup-show-pages').checked = true;
        StartupOverlay.getInstance().setControlsDisabled(true);
      } else {
        // Re-enable the controls in the startup overlay if necessary.
        StartupOverlay.getInstance().updateControlStates();
      }
    },

    /**
     * Handler for messages sent from the main uber page.
     * @param {Event} e The 'message' event from the uber page.
     * @private
     */
    handleWindowMessage_: function(e) {
      if (e.data.method == 'frameSelected')
        $('search-field').focus();
    },

    /**
     * Shows the given section.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @param {boolean} animate Indicate if the expansion should be animated.
     * @private
     */
    showSection_: function(section, container, animate) {
      if (animate)
        this.addTransitionEndListener_(section);

      // Unhide
      section.hidden = false;
      section.style.height = '0px';

      var expander = function() {
        // Reveal the section using a WebKit transition if animating.
        if (animate) {
          section.classList.add('sliding');
          section.style.height = container.offsetHeight + 'px';
        } else {
          section.style.height = 'auto';
        }
      };

      // Delay starting the transition if animating so that hidden change will
      // be processed.
      if (animate)
        setTimeout(expander, 0);
      else
        expander();
      },

    /**
     * Shows the given section, with animation.
     * @param {HTMLElement} section The section to be shown.
     * @param {HTMLElement} container The container for the section. Must be
     *     inside of |section|.
     * @private
     */
    showSectionWithAnimation_: function(section, container) {
      this.showSection_(section, container, /*animate */ true);
    },

    /**
     * See showSectionWithAnimation_.
     */
    hideSectionWithAnimation_: function(section, container) {
      this.addTransitionEndListener_(section);

      // Before we start hiding the section, we need to set
      // the height to a pixel value.
      section.style.height = container.offsetHeight + 'px';

      // Delay starting the transition so that the height change will be
      // processed.
      setTimeout(function() {
        // Hide the section using a WebKit transition.
        section.classList.add('sliding');
        section.style.height = '0px';
      }, 0);
    },

    /**
     * See showSectionWithAnimation_.
     */
    toggleSectionWithAnimation_: function(section, container) {
      if (section.style.height == '')
        this.showSectionWithAnimation_(section, container);
      else
        this.hideSectionWithAnimation_(section, container);
    },

    /**
     * Scrolls the settings page to make the section visible auto-expanding
     * advanced settings if required.  The transition is not animated.  This
     * method is used to ensure that a section associated with an overlay
     * is visible when the overlay is closed.
     * @param {!Element} section  The section to make visible.
     * @private
     */
    scrollToSection_: function(section) {
      var advancedSettings = $('advanced-settings');
      var container = $('advanced-settings-container');
      if (advancedSettings.hidden && section.parentNode == container) {
        this.showSection_($('advanced-settings'),
                          $('advanced-settings-container'),
                          /* animate */ false);
        this.updateAdvancedSettingsExpander_();
      }

      if (!this.initializationComplete_) {
        var self = this;
        var callback = function() {
           document.removeEventListener('initializationComplete', callback);
           self.scrollToSection_(section);
        };
        document.addEventListener('initializationComplete', callback);
        return;
      }

      var pageContainer = $('page-container');
      // pageContainer.offsetTop is relative to the screen.
      var pageTop = pageContainer.offsetTop;
      var sectionBottom = section.offsetTop + section.offsetHeight;
      // section.offsetTop is relative to the 'page-container'.
      var sectionTop = section.offsetTop;
      if (pageTop + sectionBottom > document.body.scrollHeight ||
          pageTop + sectionTop < 0) {
        // Currently not all layout updates are guaranteed to precede the
        // initializationComplete event (for example 'set-as-default-browser'
        // button) leaving some uncertainty in the optimal scroll position.
        // The section is placed approximately in the middle of the screen.
        var top = Math.min(0, document.body.scrollHeight / 2 - sectionBottom);
        pageContainer.style.top = top + 'px';
        pageContainer.oldScrollTop = -top;
      }
    },

    /**
     * Adds a |webkitTransitionEnd| listener to the given section so that
     * it can be animated. The listener will only be added to a given section
     * once, so this can be called as multiple times.
     * @param {HTMLElement} section The section to be animated.
     * @private
     */
    addTransitionEndListener_: function(section) {
      if (section.hasTransitionEndListener_)
        return;

      section.addEventListener('webkitTransitionEnd',
          this.onTransitionEnd_.bind(this));
      section.hasTransitionEndListener_ = true;
    },

    /**
     * Called after an animation transition has ended.
     * @private
     */
    onTransitionEnd_: function(event) {
      if (event.propertyName != 'height')
        return;

      var section = event.target;

      // Disable WebKit transitions.
      section.classList.remove('sliding');

      if (section.style.height == '0px') {
        // Hide the content so it can't get tab focus.
        section.hidden = true;
        section.style.height = '';
      } else {
        // Set the section height to 'auto' to allow for size changes
        // (due to font change or dynamic content).
        section.style.height = 'auto';
      }
    },

    updateAdvancedSettingsExpander_: function() {
      var expander = $('advanced-settings-expander');
      if ($('advanced-settings').style.height == '')
        expander.textContent = loadTimeData.getString('showAdvancedSettings');
      else
        expander.textContent = loadTimeData.getString('hideAdvancedSettings');
    },

    /**
     * Updates the sync section with the given state.
     * @param {Object} syncData A bunch of data records that describe the status
     *     of the sync system.
     * @private
     */
    updateSyncState_: function(syncData) {
      if (!syncData.signinAllowed &&
          (!syncData.supervisedUser || !cr.isChromeOS)) {
        $('sync-section').hidden = true;
        return;
      }

      $('sync-section').hidden = false;

      var subSection = $('sync-section').firstChild;
      while (subSection) {
        if (subSection.nodeType == Node.ELEMENT_NODE)
          subSection.hidden = syncData.supervisedUser;
        subSection = subSection.nextSibling;
      }

      if (syncData.supervisedUser) {
        $('account-picture-wrapper').hidden = false;
        $('sync-general').hidden = false;
        $('sync-status').hidden = true;
        return;
      }

      // If the user gets signed out while the advanced sync settings dialog is
      // visible, say, due to a dashboard clear, close the dialog.
      // However, if the user gets signed out as a result of abandoning first
      // time sync setup, do not call closeOverlay as it will redirect the
      // browser to the main settings page and override any in-progress
      // user-initiated navigation. See crbug.com/278030.
      // Note: SyncSetupOverlay.closeOverlay is a no-op if the overlay is
      // already hidden.
      if (this.signedIn_ && !syncData.signedIn && !syncData.setupInProgress)
        SyncSetupOverlay.closeOverlay();

      this.signedIn_ = syncData.signedIn;

      // Display the "advanced settings" button if we're signed in and sync is
      // not managed/disabled. If the user is signed in, but sync is disabled,
      // this button is used to re-enable sync.
      var customizeSyncButton = $('customize-sync');
      customizeSyncButton.hidden = !this.signedIn_ ||
                                   syncData.managed ||
                                   !syncData.syncSystemEnabled;

      // Only modify the customize button's text if the new text is different.
      // Otherwise, it can affect search-highlighting in the settings page.
      // See http://crbug.com/268265.
      var customizeSyncButtonNewText = syncData.setupCompleted ?
          loadTimeData.getString('customizeSync') :
          loadTimeData.getString('syncButtonTextStart');
      if (customizeSyncButton.textContent != customizeSyncButtonNewText)
        customizeSyncButton.textContent = customizeSyncButtonNewText;

      // Disable the "sign in" button if we're currently signing in, or if we're
      // already signed in and signout is not allowed.
      var signInButton = $('start-stop-sync');
      signInButton.disabled = syncData.setupInProgress ||
                              !syncData.signoutAllowed;
      if (!syncData.signoutAllowed)
        $('start-stop-sync-indicator').setAttribute('controlled-by', 'policy');
      else
        $('start-stop-sync-indicator').removeAttribute('controlled-by');

      // Hide the "sign in" button on Chrome OS, and show it on desktop Chrome.
      signInButton.hidden = cr.isChromeOS;

      signInButton.textContent =
          this.signedIn_ ?
              loadTimeData.getString('syncButtonTextStop') :
              syncData.setupInProgress ?
                  loadTimeData.getString('syncButtonTextInProgress') :
                  loadTimeData.getString('syncButtonTextSignIn');
      $('start-stop-sync-indicator').hidden = signInButton.hidden;

      // TODO(estade): can this just be textContent?
      $('sync-status-text').innerHTML = syncData.statusText;
      var statusSet = syncData.statusText.length != 0;
      $('sync-overview').hidden = statusSet;
      $('sync-status').hidden = !statusSet;

      $('sync-action-link').textContent = syncData.actionLinkText;
      // Don't show the action link if it is empty or undefined.
      $('sync-action-link').hidden = syncData.actionLinkText.length == 0;
      $('sync-action-link').disabled = syncData.managed ||
                                       !syncData.syncSystemEnabled;

      // On Chrome OS, sign out the user and sign in again to get fresh
      // credentials on auth errors.
      $('sync-action-link').onclick = function(event) {
        if (cr.isChromeOS && syncData.hasError)
          SyncSetupOverlay.doSignOutOnAuthError();
        else
          SyncSetupOverlay.showSetupUI();
      };

      if (syncData.hasError)
        $('sync-status').classList.add('sync-error');
      else
        $('sync-status').classList.remove('sync-error');

      // Disable the "customize / set up sync" button if sync has an
      // unrecoverable error. Also disable the button if sync has not been set
      // up and the user is being presented with a link to re-auth.
      // See crbug.com/289791.
      customizeSyncButton.disabled =
          syncData.hasUnrecoverableError ||
          (!syncData.setupCompleted && !$('sync-action-link').hidden);

      // Move #enable-auto-login-checkbox to a different location on CrOS.
      if (cr.isChromeOs) {
        $('sync-general').insertBefore($('sync-status').nextSibling,
                                       $('enable-auto-login-checkbox'));
      }
      $('enable-auto-login-checkbox').hidden = !syncData.autoLoginVisible;
    },

    /**
     * Update the UI depending on whether the current profile manages any
     * supervised users.
     * @param {boolean} value True if the current profile manages any supervised
     *     users.
     */
    updateManagesSupervisedUsers_: function(value) {
      $('profiles-supervised-dashboard-tip').hidden = !value;
    },

    /**
     * Get the start/stop sync button DOM element. Used for testing.
     * @return {DOMElement} The start/stop sync button.
     * @private
     */
    getStartStopSyncButton_: function() {
      return $('start-stop-sync');
    },

    /**
     * Event listener for the 'show home button' preference. Shows/hides the
     * UI for changing the home page with animation, unless this is the first
     * time this function is called, in which case there is no animation.
     * @param {Event} event The preference change event.
     */
    onShowHomeButtonChanged_: function(event) {
      var section = $('change-home-page-section');
      if (this.onShowHomeButtonChangedCalled_) {
        var container = $('change-home-page-section-container');
        if (event.value.value)
          this.showSectionWithAnimation_(section, container);
        else
          this.hideSectionWithAnimation_(section, container);
      } else {
        section.hidden = !event.value.value;
        this.onShowHomeButtonChangedCalled_ = true;
      }
    },

    /**
     * Event listener for the 'homepage is NTP' preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageIsNtpChanged_: function(event) {
      if (!event.value.uncommitted) {
        $('home-page-url').hidden = event.value.value;
        $('home-page-ntp').hidden = !event.value.value;
      }
    },

    /**
     * Event listener for changes to the homepage preference. Updates the label
     * next to the 'Change' button.
     * @param {Event} event The preference change event.
     */
    onHomePageChanged_: function(event) {
      if (!event.value.uncommitted)
        $('home-page-url').textContent = this.stripHttp_(event.value.value);
    },

    /**
     * Removes the 'http://' from a URL, like the omnibox does. If the string
     * doesn't start with 'http://' it is returned unchanged.
     * @param {string} url The url to be processed
     * @return {string} The url with the 'http://' removed.
     */
    stripHttp_: function(url) {
      return url.replace(/^http:\/\//, '');
    },

   /**
    * Shows the autoLaunch preference and initializes its checkbox value.
    * @param {bool} enabled Whether autolaunch is enabled or or not.
    * @private
    */
    updateAutoLaunchState_: function(enabled) {
      $('auto-launch-option').hidden = false;
      $('auto-launch').checked = enabled;
    },

    /**
     * Called when the value of the download.default_directory preference
     * changes.
     * @param {Event} event Change event.
     * @private
     */
    onDefaultDownloadDirectoryChanged_: function(event) {
      $('downloadLocationPath').value = event.value.value;
      if (cr.isChromeOS) {
        // On ChromeOS, replace /special/drive/root with Drive for drive paths,
        // /home/chronos/user/Downloads or /home/chronos/u-<hash>/Downloads
        // with Downloads for local paths, and '/' with ' \u203a ' (angled quote
        // sign) everywhere. The modified path is used only for display purpose.
        var path = $('downloadLocationPath').value;
        path = path.replace(/^\/special\/drive\/root/, 'Google Drive');
        path = path.replace(/^\/home\/chronos\/(user|u-[^\/]*)\//, '');
        path = path.replace(/\//g, ' \u203a ');
        $('downloadLocationPath').value = path;
      }
      $('download-location-label').classList.toggle('disabled',
                                                    event.value.disabled);
      $('downloadLocationChangeButton').disabled = event.value.disabled;
    },

    /**
     * Update the Default Browsers section based on the current state.
     * @param {string} statusString Description of the current default state.
     * @param {boolean} isDefault Whether or not the browser is currently
     *     default.
     * @param {boolean} canBeDefault Whether or not the browser can be default.
     * @private
     */
    updateDefaultBrowserState_: function(statusString, isDefault,
                                         canBeDefault) {
      if (!cr.isChromeOS) {
        var label = $('default-browser-state');
        label.textContent = statusString;

        $('set-as-default-browser').hidden = !canBeDefault || isDefault;
      }
    },

    /**
     * Clears the search engine popup.
     * @private
     */
    clearSearchEngines_: function() {
      $('default-search-engine').textContent = '';
    },

    /**
     * Updates the search engine popup with the given entries.
     * @param {Array} engines List of available search engines.
     * @param {number} defaultValue The value of the current default engine.
     * @param {boolean} defaultManaged Whether the default search provider is
     *     managed. If true, the default search provider can't be changed.
     * @private
     */
    updateSearchEngines_: function(engines, defaultValue, defaultManaged) {
      this.clearSearchEngines_();
      engineSelect = $('default-search-engine');
      engineSelect.disabled = defaultManaged;
      if (defaultManaged && defaultValue == -1)
        return;
      engineCount = engines.length;
      var defaultIndex = -1;
      for (var i = 0; i < engineCount; i++) {
        var engine = engines[i];
        var option = new Option(engine.name, engine.index);
        if (defaultValue == option.value)
          defaultIndex = i;
        engineSelect.appendChild(option);
      }
      if (defaultIndex >= 0)
        engineSelect.selectedIndex = defaultIndex;
    },

    /**
     * Set the default search engine based on the popup selection.
     * @private
     */
    setDefaultSearchEngine_: function() {
      var engineSelect = $('default-search-engine');
      var selectedIndex = engineSelect.selectedIndex;
      if (selectedIndex >= 0) {
        var selection = engineSelect.options[selectedIndex];
        chrome.send('setDefaultSearchEngine', [String(selection.value)]);
      }
    },

   /**
     * Sets or clear whether Chrome should Auto-launch on computer startup.
     * @private
     */
    handleAutoLaunchChanged_: function() {
      chrome.send('toggleAutoLaunch', [$('auto-launch').checked]);
    },

    /**
     * Get the selected profile item from the profile list. This also works
     * correctly if the list is not displayed.
     * @return {Object} the profile item object, or null if nothing is selected.
     * @private
     */
    getSelectedProfileItem_: function() {
      var profilesList = $('profiles-list');
      if (profilesList.hidden) {
        if (profilesList.dataModel.length > 0)
          return profilesList.dataModel.item(0);
      } else {
        return profilesList.selectedItem;
      }
      return null;
    },

    /**
     * Helper function to set the status of profile view buttons to disabled or
     * enabled, depending on the number of profiles and selection status of the
     * profiles list.
     * @private
     */
    setProfileViewButtonsStatus_: function() {
      var profilesList = $('profiles-list');
      var selectedProfile = profilesList.selectedItem;
      var hasSelection = selectedProfile != null;
      var hasSingleProfile = profilesList.dataModel.length == 1;
      var isManaged = loadTimeData.getBoolean('profileIsManaged');
      $('profiles-manage').disabled = !hasSelection ||
          !selectedProfile.isCurrentProfile;
      if (hasSelection && !selectedProfile.isCurrentProfile)
        $('profiles-manage').title = loadTimeData.getString('currentUserOnly');
      else
        $('profiles-manage').title = '';
      $('profiles-delete').disabled = isManaged ||
                                      (!hasSelection && !hasSingleProfile);
      if (OptionsPage.isSettingsApp()) {
        $('profiles-app-list-switch').disabled = !hasSelection ||
            selectedProfile.isCurrentProfile;
      }
      var importData = $('import-data');
      if (importData) {
        importData.disabled = $('import-data').disabled = hasSelection &&
          !selectedProfile.isCurrentProfile;
      }
    },

    /**
     * Display the correct dialog layout, depending on how many profiles are
     * available.
     * @param {number} numProfiles The number of profiles to display.
     * @private
     */
    setProfileViewSingle_: function(numProfiles) {
      var hasSingleProfile = numProfiles == 1;
      $('profiles-list').hidden = hasSingleProfile;
      $('profiles-single-message').hidden = !hasSingleProfile;
      $('profiles-manage').hidden =
          hasSingleProfile || OptionsPage.isSettingsApp();
      $('profiles-delete').textContent = hasSingleProfile ?
          loadTimeData.getString('profilesDeleteSingle') :
          loadTimeData.getString('profilesDelete');
      if (OptionsPage.isSettingsApp())
        $('profiles-app-list-switch').hidden = hasSingleProfile;
    },

    /**
     * Adds all |profiles| to the list.
     * @param {Array.<Object>} profiles An array of profile info objects.
     *     each object is of the form:
     *       profileInfo = {
     *         name: "Profile Name",
     *         iconURL: "chrome://path/to/icon/image",
     *         filePath: "/path/to/profile/data/on/disk",
     *         isCurrentProfile: false
     *       };
     * @private
     */
    setProfilesInfo_: function(profiles) {
      this.setProfileViewSingle_(profiles.length);
      // add it to the list, even if the list is hidden so we can access it
      // later.
      $('profiles-list').dataModel = new ArrayDataModel(profiles);

      // Received new data. If showing the "manage" overlay, keep it up to
      // date. If showing the "delete" overlay, close it.
      if (ManageProfileOverlay.getInstance().visible &&
          !$('manage-profile-overlay-manage').hidden) {
        ManageProfileOverlay.showManageDialog();
      } else {
        ManageProfileOverlay.getInstance().visible = false;
      }

      this.setProfileViewButtonsStatus_();
    },

    /**
     * Reports managed user import errors to the ManagedUserImportOverlay.
     * @param {string} error The error message to display.
     * @private
     */
    showManagedUserImportError_: function(error) {
      ManagedUserImportOverlay.onError(error);
    },

    /**
     * Reports successful importing of a managed user to
     * the ManagedUserImportOverlay.
     * @private
     */
    showManagedUserImportSuccess_: function() {
      ManagedUserImportOverlay.onSuccess();
    },

    /**
     * Reports an error to the "create" overlay during profile creation.
     * @param {string} error The error message to display.
     * @private
     */
    showCreateProfileError_: function(error) {
      CreateProfileOverlay.onError(error);
    },

    /**
    * Sends a warning message to the "create" overlay during profile creation.
    * @param {string} warning The warning message to display.
    * @private
    */
    showCreateProfileWarning_: function(warning) {
      CreateProfileOverlay.onWarning(warning);
    },

    /**
    * Reports successful profile creation to the "create" overlay.
     * @param {Object} profileInfo An object of the form:
     *     profileInfo = {
     *       name: "Profile Name",
     *       filePath: "/path/to/profile/data/on/disk"
     *       isManaged: (true|false),
     *     };
    * @private
    */
    showCreateProfileSuccess_: function(profileInfo) {
      CreateProfileOverlay.onSuccess(profileInfo);
    },

    /**
     * Returns the currently active profile for this browser window.
     * @return {Object} A profile info object.
     * @private
     */
    getCurrentProfile_: function() {
      for (var i = 0; i < $('profiles-list').dataModel.length; i++) {
        var profile = $('profiles-list').dataModel.item(i);
        if (profile.isCurrentProfile)
          return profile;
      }

      assert(false,
             'There should always be a current profile, but none found.');
    },

    setNativeThemeButtonEnabled_: function(enabled) {
      var button = $('themes-native-button');
      if (button)
        button.disabled = !enabled;
    },

    setThemesResetButtonEnabled_: function(enabled) {
      $('themes-reset').disabled = !enabled;
    },

    setAccountPictureManaged_: function(managed) {
      var picture = $('account-picture');
      if (managed || UIAccountTweaks.loggedInAsGuest()) {
        picture.disabled = true;
        ChangePictureOptions.closeOverlay();
      } else {
        picture.disabled = false;
      }

      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event('account-picture');
      if (managed)
        event.value = { controlledBy: 'policy' };
      else
        event.value = {};
      $('account-picture-indicator').handlePrefChange(event);
    },

    /**
     * (Re)loads IMG element with current user account picture.
     * @private
     */
    updateAccountPicture_: function() {
      var picture = $('account-picture');
      if (picture) {
        picture.src = 'chrome://userimage/' + this.username_ + '?id=' +
            Date.now();
      }
    },

    /**
     * Handle the 'add device' button click.
     * @private
     */
    handleAddBluetoothDevice_: function() {
      chrome.send('findBluetoothDevices');
      OptionsPage.showPageByName('bluetooth', false);
    },

    /**
     * Enables or disables the Manage SSL Certificates button.
     * @private
     */
    enableCertificateButton_: function(enabled) {
      $('certificatesManageButton').disabled = !enabled;
    },

    /**
     * Enables factory reset section.
     * @private
     */
    enableFactoryResetSection_: function() {
      $('factory-reset-section').hidden = false;
    },

    /**
     * Set the checked state of the metrics reporting checkbox.
     * @private
     */
    setMetricsReportingCheckboxState_: function(checked, disabled) {
      $('metricsReportingEnabled').checked = checked;
      $('metricsReportingEnabled').disabled = disabled;
    },

    /**
     * @private
     */
    setMetricsReportingSettingVisibility_: function(visible) {
      if (visible)
        $('metricsReportingSetting').style.display = 'block';
      else
        $('metricsReportingSetting').style.display = 'none';
    },

    /**
     * Set the visibility of the password generation checkbox.
     * @private
     */
    setPasswordGenerationSettingVisibility_: function(visible) {
      if (visible)
        $('password-generation-checkbox').style.display = 'block';
      else
        $('password-generation-checkbox').style.display = 'none';
    },

    /**
     * Set the font size selected item. This item actually reflects two
     * preferences: the default font size and the default fixed font size.
     *
     * @param {Object} pref Information about the font size preferences.
     * @param {number} pref.value The value of the default font size pref.
     * @param {boolean} pref.disabled True if either pref not user modifiable.
     * @param {string} pref.controlledBy The source of the pref value(s) if
     *     either pref is currently not controlled by the user.
     * @private
     */
    setFontSize_: function(pref) {
      var selectCtl = $('defaultFontSize');
      selectCtl.disabled = pref.disabled;
      // Create a synthetic pref change event decorated as
      // CoreOptionsHandler::CreateValueForPref() does.
      var event = new Event('synthetic-font-size');
      event.value = {
        value: pref.value,
        controlledBy: pref.controlledBy,
        disabled: pref.disabled
      };
      $('font-size-indicator').handlePrefChange(event);

      for (var i = 0; i < selectCtl.options.length; i++) {
        if (selectCtl.options[i].value == pref.value) {
          selectCtl.selectedIndex = i;
          if ($('Custom'))
            selectCtl.remove($('Custom').index);
          return;
        }
      }

      // Add/Select Custom Option in the font size label list.
      if (!$('Custom')) {
        var option = new Option(loadTimeData.getString('fontSizeLabelCustom'),
                                -1, false, true);
        option.setAttribute('id', 'Custom');
        selectCtl.add(option);
      }
      $('Custom').selected = true;
    },

    /**
     * Populate the page zoom selector with values received from the caller.
     * @param {Array} items An array of items to populate the selector.
     *     each object is an array with three elements as follows:
     *       0: The title of the item (string).
     *       1: The value of the item (number).
     *       2: Whether the item should be selected (boolean).
     * @private
     */
    setupPageZoomSelector_: function(items) {
      var element = $('defaultZoomFactor');

      // Remove any existing content.
      element.textContent = '';

      // Insert new child nodes into select element.
      var value, title, selected;
      for (var i = 0; i < items.length; i++) {
        title = items[i][0];
        value = items[i][1];
        selected = items[i][2];
        element.appendChild(new Option(title, value, false, selected));
      }
    },

    /**
     * Shows/hides the autoOpenFileTypesResetToDefault button and label, with
     * animation.
     * @param {boolean} display Whether to show the button and label or not.
     * @private
     */
    setAutoOpenFileTypesDisplayed_: function(display) {
      if (cr.isChromeOS)
        return;

      if ($('advanced-settings').hidden) {
        // If the Advanced section is hidden, don't animate the transition.
        $('auto-open-file-types-section').hidden = !display;
      } else {
        if (display) {
          this.showSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        } else {
          this.hideSectionWithAnimation_(
              $('auto-open-file-types-section'),
              $('auto-open-file-types-container'));
        }
      }
    },

    /**
     * Set the enabled state for the proxy settings button.
     * @private
     */
    setupProxySettingsSection_: function(disabled, extensionControlled) {
      if (!cr.isChromeOS) {
        $('proxiesConfigureButton').disabled = disabled;
        $('proxiesLabel').textContent =
            loadTimeData.getString(extensionControlled ?
                'proxiesLabelExtension' : 'proxiesLabelSystem');
      }
    },

    /**
     * Set the Cloud Print proxy UI to enabled, disabled, or processing.
     * @private
     */
    setupCloudPrintConnectorSection_: function(disabled, label, allowed) {
      if (!cr.isChromeOS) {
        $('cloudPrintConnectorLabel').textContent = label;
        if (disabled || !allowed) {
          $('cloudPrintConnectorSetupButton').textContent =
            loadTimeData.getString('cloudPrintConnectorDisabledButton');
          $('cloudPrintManageButton').style.display = 'none';
        } else {
          $('cloudPrintConnectorSetupButton').textContent =
            loadTimeData.getString('cloudPrintConnectorEnabledButton');
          $('cloudPrintManageButton').style.display = 'inline';
        }
        $('cloudPrintConnectorSetupButton').disabled = !allowed;
      }
    },

    /**
     * @private
     */
    removeCloudPrintConnectorSection_: function() {
     if (!cr.isChromeOS) {
        var connectorSectionElm = $('cloud-print-connector-section');
        if (connectorSectionElm)
          connectorSectionElm.parentNode.removeChild(connectorSectionElm);
      }
    },

    /**
     * Set the initial state of the spoken feedback checkbox.
     * @private
     */
    setSpokenFeedbackCheckboxState_: function(checked) {
      $('accessibility-spoken-feedback-check').checked = checked;
    },

    /**
     * Set the initial state of the high contrast checkbox.
     * @private
     */
    setHighContrastCheckboxState_: function(checked) {
      $('accessibility-high-contrast-check').checked = checked;
    },

    /**
     * Set the initial state of the virtual keyboard checkbox.
     * @private
     */
    setVirtualKeyboardCheckboxState_: function(checked) {
      // TODO(zork): Update UI
    },

    /**
     * Show/hide mouse settings slider.
     * @private
     */
    showMouseControls_: function(show) {
      $('mouse-settings').hidden = !show;
    },

    /**
     * Show/hide touchpad-related settings.
     * @private
     */
    showTouchpadControls_: function(show) {
      $('touchpad-settings').hidden = !show;
      $('accessibility-tap-dragging').hidden = !show;
    },

    /**
     * Activate the Bluetooth settings section on the System settings page.
     * @private
     */
    showBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = false;
    },

    /**
     * Dectivates the Bluetooth settings section from the System settings page.
     * @private
     */
    hideBluetoothSettings_: function() {
      $('bluetooth-devices').hidden = true;
    },

    /**
     * Sets the state of the checkbox indicating if Bluetooth is turned on. The
     * state of the "Find devices" button and the list of discovered devices may
     * also be affected by a change to the state.
     * @param {boolean} checked Flag Indicating if Bluetooth is turned on.
     * @private
     */
    setBluetoothState_: function(checked) {
      $('enable-bluetooth').checked = checked;
      $('bluetooth-paired-devices-list').parentNode.hidden = !checked;
      $('bluetooth-add-device').hidden = !checked;
      $('bluetooth-reconnect-device').hidden = !checked;
      // Flush list of previously discovered devices if bluetooth is turned off.
      if (!checked) {
        $('bluetooth-paired-devices-list').clear();
        $('bluetooth-unpaired-devices-list').clear();
      } else {
        chrome.send('getPairedBluetoothDevices');
      }
    },

    /**
     * Adds an element to the list of available Bluetooth devices. If an element
     * with a matching address is found, the existing element is updated.
     * @param {{name: string,
     *          address: string,
     *          paired: boolean,
     *          connected: boolean}} device
     *     Decription of the Bluetooth device.
     * @private
     */
    addBluetoothDevice_: function(device) {
      var list = $('bluetooth-unpaired-devices-list');
      // Display the "connecting" (already paired or not yet paired) and the
      // paired devices in the same list.
      if (device.paired || device.connecting) {
        // Test to see if the device is currently in the unpaired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-unpaired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
        list = $('bluetooth-paired-devices-list');
      } else {
        // Test to see if the device is currently in the paired list, in which
        // case it should be removed from that list.
        var index = $('bluetooth-paired-devices-list').find(device.address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
      }
      list.appendDevice(device);

      // One device can be in the process of pairing.  If found, display
      // the Bluetooth pairing overlay.
      if (device.pairing)
        BluetoothPairing.showDialog(device);
    },

    /**
     * Removes an element from the list of available devices.
     * @param {string} address Unique address of the device.
     * @private
     */
    removeBluetoothDevice_: function(address) {
      var index = $('bluetooth-unpaired-devices-list').find(address);
      if (index != undefined) {
        $('bluetooth-unpaired-devices-list').deleteItemAtIndex(index);
      } else {
        index = $('bluetooth-paired-devices-list').find(address);
        if (index != undefined)
          $('bluetooth-paired-devices-list').deleteItemAtIndex(index);
      }
    },

    /**
     * Shows the overlay dialog for changing the user avatar image.
     * @private
     */
    showImagerPickerOverlay_: function() {
      OptionsPage.navigateToPage('changePicture');
    }
  };

  //Forward public APIs to private implementations.
  [
    'addBluetoothDevice',
    'enableCertificateButton',
    'enableFactoryResetSection',
    'getCurrentProfile',
    'getStartStopSyncButton',
    'hideBluetoothSettings',
    'notifyInitializationComplete',
    'removeBluetoothDevice',
    'removeCloudPrintConnectorSection',
    'scrollToSection',
    'setAccountPictureManaged',
    'setAutoOpenFileTypesDisplayed',
    'setBluetoothState',
    'setFontSize',
    'setNativeThemeButtonEnabled',
    'setHighContrastCheckboxState',
    'setMetricsReportingCheckboxState',
    'setMetricsReportingSettingVisibility',
    'setPasswordGenerationSettingVisibility',
    'setProfilesInfo',
    'setSpokenFeedbackCheckboxState',
    'setThemesResetButtonEnabled',
    'setVirtualKeyboardCheckboxState',
    'setupCloudPrintConnectorSection',
    'setupPageZoomSelector',
    'setupProxySettingsSection',
    'showBluetoothSettings',
    'showCreateProfileError',
    'showCreateProfileSuccess',
    'showCreateProfileWarning',
    'showManagedUserImportError',
    'showManagedUserImportSuccess',
    'showMouseControls',
    'showTouchpadControls',
    'updateAccountPicture',
    'updateAutoLaunchState',
    'updateDefaultBrowserState',
    'updateManagesSupervisedUsers',
    'updateSearchEngines',
    'updateStartupPages',
    'updateSyncState',
  ].forEach(function(name) {
    BrowserOptions[name] = function() {
      var instance = BrowserOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  if (cr.isChromeOS) {
    /**
     * Returns username (canonical email) of the user logged in (ChromeOS only).
     * @return {string} user email.
     */
    // TODO(jhawkins): Investigate the use case for this method.
    BrowserOptions.getLoggedInUsername = function() {
      return BrowserOptions.getInstance().username_;
    };
  }

  // Export
  return {
    BrowserOptions: BrowserOptions
  };
});
