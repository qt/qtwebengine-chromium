// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * A data store that stores destinations and dispatches events when the data
   * store changes.
   * @param {!print_preview.NativeLayer} nativeLayer Used to fetch local print
   *     destinations.
   * @param {!print_preview.AppState} appState Application state.
   * @param {!print_preview.Metrics} metrics Metrics.
   * @constructor
   * @extends {cr.EventTarget}
   */
  function DestinationStore(nativeLayer, appState, metrics) {
    cr.EventTarget.call(this);

    /**
     * Used to fetch local print destinations.
     * @type {!print_preview.NativeLayer}
     * @private
     */
    this.nativeLayer_ = nativeLayer;

    /**
     * Used to load and persist the selected destination.
     * @type {!print_preview.AppState}
     * @private
     */
    this.appState_ = appState;

    /**
     * Used to track metrics.
     * @type {!print_preview.AppState}
     * @private
     */
    this.metrics_ = metrics;

    /**
     * Internal backing store for the data store.
     * @type {!Array.<!print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Cache used for constant lookup of destinations by origin and id.
     * @type {object.<string, !print_preview.Destination>}
     * @private
     */
    this.destinationMap_ = {};

    /**
     * Currently selected destination.
     * @type {print_preview.Destination}
     * @private
     */
    this.selectedDestination_ = null;

    /**
     * Initial destination ID used to auto-select the first inserted destination
     * that matches. If {@code null}, the first destination inserted into the
     * store will be selected.
     * @type {?string}
     * @private
     */
    this.initialDestinationId_ = null;

    /**
     * Initial origin used to auto-select destination.
     * @type {print_preview.Destination.Origin}
     * @private
     */
    this.initialDestinationOrigin_ = print_preview.Destination.Origin.LOCAL;

    /**
     * Whether the destination store will auto select the destination that
     * matches the initial destination.
     * @type {boolean}
     * @private
     */
    this.isInAutoSelectMode_ = false;

    /**
     * Event tracker used to track event listeners of the destination store.
     * @type {!EventTracker}
     * @private
     */
    this.tracker_ = new EventTracker();

    /**
     * Used to fetch cloud-based print destinations.
     * @type {print_preview.CloudPrintInterface}
     * @private
     */
    this.cloudPrintInterface_ = null;

    /**
     * Whether the destination store has already loaded or is loading all cloud
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllCloudDestinations_ = false;

    /**
     * ID of a timeout after the initial destination ID is set. If no inserted
     * destination matches the initial destination ID after the specified
     * timeout, the first destination in the store will be automatically
     * selected.
     * @type {?number}
     * @private
     */
    this.autoSelectTimeout_ = null;

    /**
     * Whether a search for local destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isLocalDestinationSearchInProgress_ = false;

    /**
     * Whether the destination store has already loaded or is loading all local
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllLocalDestinations_ = false;

    /**
     * Whether a search for privet destinations is in progress.
     * @type {boolean}
     * @private
     */
    this.isPrivetDestinationSearchInProgress_ = false;

    /**
     * Whether the destination store has already loaded or is loading all privet
     * destinations.
     * @type {boolean}
     * @private
     */
    this.hasLoadedAllPrivetDestinations_ = false;

    this.addEventListeners_();
    this.reset_();
  };

  /**
   * Event types dispatched by the data store.
   * @enum {string}
   */
  DestinationStore.EventType = {
    DESTINATION_SEARCH_DONE:
        'print_preview.DestinationStore.DESTINATION_SEARCH_DONE',
    DESTINATION_SEARCH_STARTED:
        'print_preview.DestinationStore.DESTINATION_SEARCH_STARTED',
    DESTINATION_SELECT: 'print_preview.DestinationStore.DESTINATION_SELECT',
    DESTINATIONS_INSERTED:
        'print_preview.DestinationStore.DESTINATIONS_INSERTED',
    CACHED_SELECTED_DESTINATION_INFO_READY:
        'print_preview.DestinationStore.CACHED_SELECTED_DESTINATION_INFO_READY',
    SELECTED_DESTINATION_CAPABILITIES_READY:
        'print_preview.DestinationStore.SELECTED_DESTINATION_CAPABILITIES_READY'
  };

  /**
   * Delay in milliseconds before the destination store ignores the initial
   * destination ID and just selects any printer (since the initial destination
   * was not found).
   * @type {number}
   * @const
   * @private
   */
  DestinationStore.AUTO_SELECT_TIMEOUT_ = 15000;

  /**
   * Creates a local PDF print destination.
   * @return {!print_preview.Destination} Created print destination.
   * @private
   */
  DestinationStore.createLocalPdfPrintDestination_ = function() {
    var dest = new print_preview.Destination(
        print_preview.Destination.GooglePromotedId.SAVE_AS_PDF,
        print_preview.Destination.Type.LOCAL,
        print_preview.Destination.Origin.LOCAL,
        localStrings.getString('printToPDF'),
        false /*isRecent*/,
        print_preview.Destination.ConnectionStatus.ONLINE);
    dest.capabilities = {
      version: '1.0',
      printer: {
        page_orientation: {
          option: [
            {type: 'AUTO', is_default: true},
            {type: 'PORTRAIT'},
            {type: 'LANDSCAPE'}
          ]
        },
        color: { option: [{type: 'STANDARD_COLOR', is_default: true}] }
      }
    };
    return dest;
  };

  DestinationStore.prototype = {
    __proto__: cr.EventTarget.prototype,

    /**
     * @return {!Array.<!print_preview.Destination>} List of destinations in
     *     the store.
     */
    get destinations() {
      return this.destinations_.slice(0);
    },

    /**
     * @return {print_preview.Destination} The currently selected destination or
     *     {@code null} if none is selected.
     */
    get selectedDestination() {
      return this.selectedDestination_;
    },

    /**
     * @return {boolean} Whether a search for local destinations is in progress.
     */
    get isLocalDestinationSearchInProgress() {
      return this.isLocalDestinationSearchInProgress_ ||
        this.isPrivetDestinationSearchInProgress_;
    },

    /**
     * @return {boolean} Whether a search for cloud destinations is in progress.
     */
    get isCloudDestinationSearchInProgress() {
      return this.cloudPrintInterface_ &&
             this.cloudPrintInterface_.isCloudDestinationSearchInProgress;
    },

    /**
     * Initializes the destination store. Sets the initially selected
     * destination. If any inserted destinations match this ID, that destination
     * will be automatically selected. This method must be called after the
     * print_preview.AppState has been initialized.
     * @param {?string} systemDefaultDestinationId ID of the system default
     *     destination.
     * @private
     */
    init: function(systemDefaultDestinationId) {
      if (this.appState_.selectedDestinationId &&
          this.appState_.selectedDestinationOrigin) {
        this.initialDestinationId_ = this.appState_.selectedDestinationId;
        this.initialDestinationOrigin_ =
            this.appState_.selectedDestinationOrigin;
      } else if (systemDefaultDestinationId) {
        this.initialDestinationId_ = systemDefaultDestinationId;
        this.initialDestinationOrigin_ = print_preview.Destination.Origin.LOCAL;
      }
      this.isInAutoSelectMode_ = true;
      if (!this.initialDestinationId_ || !this.initialDestinationOrigin_) {
        this.onAutoSelectFailed_();
      } else {
        var key = this.getDestinationKey_(this.initialDestinationOrigin_,
                                          this.initialDestinationId_);
        var candidate = this.destinationMap_[key];
        if (candidate != null) {
          this.selectDestination(candidate);
        } else if (this.initialDestinationOrigin_ ==
                   print_preview.Destination.Origin.LOCAL) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              this.initialDestinationId_);
        } else if (this.cloudPrintInterface_ &&
                   (this.initialDestinationOrigin_ ==
                    print_preview.Destination.Origin.COOKIES ||
                    this.initialDestinationOrigin_ ==
                    print_preview.Destination.Origin.DEVICE)) {
          this.cloudPrintInterface_.printer(this.initialDestinationId_,
                                            this.initialDestinationOrigin_);
        } else if (this.initialDestinationOrigin_ ==
                   print_preview.Destination.Origin.PRIVET) {
          // TODO(noamsml): Resolve a specific printer instead of listing all
          // privet printers in this case.
          this.nativeLayer_.startGetPrivetDestinations();

          var destinationName = this.appState_.selectedDestinationName || '';

          // Create a fake selectedDestination_ that is not actually in the
          // destination store. When the real destination is created, this
          // destination will be overwritten.
          this.selectedDestination_ = new print_preview.Destination(
              this.initialDestinationId_,
              print_preview.Destination.Type.LOCAL,
              print_preview.Destination.Origin.PRIVET,
              destinationName,
              false /*isRecent*/,
              print_preview.Destination.ConnectionStatus.ONLINE);
          this.selectedDestination_.capabilities =
              this.appState_.selectedDestinationCapabilities;

          cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.CACHED_SELECTED_DESTINATION_INFO_READY);

        } else {
          this.onAutoSelectFailed_();
        }
      }
    },

    /**
     * Sets the destination store's Google Cloud Print interface.
     * @param {!print_preview.CloudPrintInterface} cloudPrintInterface Interface
     *     to set.
     */
    setCloudPrintInterface: function(cloudPrintInterface) {
      this.cloudPrintInterface_ = cloudPrintInterface;
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_DONE,
          this.onCloudPrintSearchDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.SEARCH_FAILED,
          this.onCloudPrintSearchFailed_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_DONE,
          this.onCloudPrintPrinterDone_.bind(this));
      this.tracker_.add(
          this.cloudPrintInterface_,
          cloudprint.CloudPrintInterface.EventType.PRINTER_FAILED,
          this.onCloudPrintPrinterFailed_.bind(this));
    },

    /**
     * @return {boolean} Whether only default cloud destinations have been
     *     loaded.
     */
    hasOnlyDefaultCloudDestinations: function() {
      return this.destinations_.every(function(dest) {
        return dest.isLocal ||
            dest.id == print_preview.Destination.GooglePromotedId.DOCS ||
            dest.id == print_preview.Destination.GooglePromotedId.FEDEX;
      });
    },

    /** @param {!print_preview.Destination} Destination to select. */
    selectDestination: function(destination) {
      this.selectedDestination_ = destination;
      this.selectedDestination_.isRecent = true;
      this.isInAutoSelectMode_ = false;
      if (this.autoSelectTimeout_ != null) {
        clearTimeout(this.autoSelectTimeout_);
        this.autoSelectTimeout_ = null;
      }
      if (destination.id == print_preview.Destination.GooglePromotedId.FEDEX &&
          !destination.isTosAccepted) {
        assert(this.cloudPrintInterface_ != null,
               'Selected FedEx Office destination, but Google Cloud Print is ' +
               'not enabled');
        destination.isTosAccepted = true;
        this.cloudPrintInterface_.updatePrinterTosAcceptance(destination.id,
                                                             destination.origin,
                                                             true);
      }
      this.appState_.persistSelectedDestination(this.selectedDestination_);

      if (destination.cloudID &&
          this.destinations.some(function(otherDestination) {
            return otherDestination.cloudID == destination.cloudID &&
                otherDestination != destination;
            })) {
        if (destination.isPrivet) {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  PRIVET_DUPLICATE_SELECTED);
        } else {
          this.metrics_.incrementDestinationSearchBucket(
              print_preview.Metrics.DestinationSearchBucket.
                  CLOUD_DUPLICATE_SELECTED);
        }
      }

      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SELECT);
      if (destination.capabilities == null) {
        if (destination.isPrivet) {
          this.nativeLayer_.startGetPrivetDestinationCapabilities(
              destination.id);
        }
        else if (destination.isLocal) {
          this.nativeLayer_.startGetLocalDestinationCapabilities(
              destination.id);
        } else {
          assert(this.cloudPrintInterface_ != null,
                 'Selected destination is a cloud destination, but Google ' +
                 'Cloud Print is not enabled');
          this.cloudPrintInterface_.printer(destination.id,
                                            destination.origin);
        }
      } else {
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Inserts a print destination to the data store and dispatches a
     * DESTINATIONS_INSERTED event. If the destination matches the initial
     * destination ID, then the destination will be automatically selected.
     * @param {!print_preview.Destination} destination Print destination to
     *     insert.
     */
    insertDestination: function(destination) {
      if (this.insertDestination_(destination)) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
        if (this.isInAutoSelectMode_ &&
            this.matchInitialDestination_(destination.id, destination.origin)) {
          this.selectDestination(destination);
        }
      }
    },

    /**
     * Inserts multiple print destinations to the data store and dispatches one
     * DESTINATIONS_INSERTED event. If any of the destinations match the initial
     * destination ID, then that destination will be automatically selected.
     * @param {!Array.<print_preview.Destination>} destinations Print
     *     destinations to insert.
     */
    insertDestinations: function(destinations) {
      var insertedDestination = false;
      var destinationToAutoSelect = null;
      destinations.forEach(function(dest) {
        if (this.insertDestination_(dest)) {
          insertedDestination = true;
          if (this.isInAutoSelectMode_ &&
              destinationToAutoSelect == null &&
              this.matchInitialDestination_(dest.id, dest.origin)) {
            destinationToAutoSelect = dest;
          }
        }
      }, this);
      if (insertedDestination) {
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATIONS_INSERTED);
      }
      if (destinationToAutoSelect != null) {
        this.selectDestination(destinationToAutoSelect);
      }
    },

    /**
     * Updates an existing print destination with capabilities and display name
     * information. If the destination doesn't already exist, it will be added.
     * @param {!print_preview.Destination} destination Destination to update.
     * @return {!print_preview.Destination} The existing destination that was
     *     updated or {@code null} if it was the new destination.
     */
    updateDestination: function(destination) {
      assert(destination.constructor !== Array, 'Single printer expected');
      var key = this.getDestinationKey_(destination.origin, destination.id);
      var existingDestination = this.destinationMap_[key];
      if (existingDestination != null) {
        existingDestination.capabilities = destination.capabilities;
      } else {
        this.insertDestination(destination);
      }

      if (existingDestination == this.selectedDestination_ ||
          destination == this.selectedDestination_) {
        this.appState_.persistSelectedDestination(this.selectedDestination_);
        cr.dispatchSimpleEvent(
            this,
            DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY);
      }

      return existingDestination;
    },

    /** Initiates loading of local print destinations. */
    startLoadLocalDestinations: function() {
      if (!this.hasLoadedAllLocalDestinations_) {
        this.hasLoadedAllLocalDestinations_ = true;
        this.nativeLayer_.startGetLocalDestinations();
        this.isLocalDestinationSearchInProgress_ = true;
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      }
    },

    /** Initiates loading of privet print destinations. */
    startLoadPrivetDestinations: function() {
      if (!this.hasLoadedAllPrivetDestinations_) {
        this.isPrivetDestinationSearchInProgress_ = true;
        this.nativeLayer_.startGetPrivetDestinations();
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      }
    },

    /**
     * Initiates loading of cloud destinations.
     */
    startLoadCloudDestinations: function() {
      if (this.cloudPrintInterface_ != null &&
          !this.hasLoadedAllCloudDestinations_) {
        this.hasLoadedAllCloudDestinations_ = true;
        this.cloudPrintInterface_.search(true);
        this.cloudPrintInterface_.search(false);
        cr.dispatchSimpleEvent(
            this, DestinationStore.EventType.DESTINATION_SEARCH_STARTED);
      }
    },

    /**
     * Inserts a destination into the store without dispatching any events.
     * @return {boolean} Whether the inserted destination was not already in the
     *     store.
     * @private
     */
    insertDestination_: function(destination) {
      var key = this.getDestinationKey_(destination.origin, destination.id);
      var existingDestination = this.destinationMap_[key];
      if (existingDestination == null) {
        this.destinations_.push(destination);
        this.destinationMap_[key] = destination;
        return true;
      } else if (existingDestination.connectionStatus ==
                     print_preview.Destination.ConnectionStatus.UNKNOWN &&
                 destination.connectionStatus !=
                     print_preview.Destination.ConnectionStatus.UNKNOWN) {
        existingDestination.connectionStatus = destination.connectionStatus;
        return true;
      } else {
        return false;
      }
    },

    /**
     * Binds handlers to events.
     * @private
     */
    addEventListeners_: function() {
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET,
          this.onLocalDestinationsSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.CAPABILITIES_SET,
          this.onLocalDestinationCapabilitiesSet_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.GET_CAPABILITIES_FAIL,
          this.onGetCapabilitiesFail_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.DESTINATIONS_RELOAD,
          this.onDestinationsReload_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRIVET_PRINTER_CHANGED,
          this.onPrivetPrinterAdded_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRIVET_PRINTER_SEARCH_DONE,
          this.onPrivetPrinterSearchDone_.bind(this));
      this.tracker_.add(
          this.nativeLayer_,
          print_preview.NativeLayer.EventType.PRIVET_CAPABILITIES_SET,
          this.onPrivetCapabilitiesSet_.bind(this));
    },

    /**
     * Resets the state of the destination store to its initial state.
     * @private
     */
    reset_: function() {
      this.destinations_ = [];
      this.destinationMap_ = {};
      this.selectedDestination_ = null;
      this.hasLoadedAllCloudDestinations_ = false;
      this.hasLoadedAllLocalDestinations_ = false;
      this.insertDestination(
          DestinationStore.createLocalPdfPrintDestination_());
      this.autoSelectTimeout_ =
          setTimeout(this.onAutoSelectFailed_.bind(this),
                     DestinationStore.AUTO_SELECT_TIMEOUT_);
    },

    /**
     * Called when the local destinations have been got from the native layer.
     * @param {Event} Contains the local destinations.
     * @private
     */
    onLocalDestinationsSet_: function(event) {
      var localDestinations = event.destinationInfos.map(function(destInfo) {
        return print_preview.LocalDestinationParser.parse(destInfo);
      });
      this.insertDestinations(localDestinations);
      this.isLocalDestinationSearchInProgress_ = false;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when the native layer retrieves the capabilities for the selected
     * local destination. Updates the destination with new capabilities if the
     * destination already exists, otherwise it creates a new destination and
     * then updates its capabilities.
     * @param {Event} event Contains the capabilities of the local print
     *     destination.
     * @private
     */
    onLocalDestinationCapabilitiesSet_: function(event) {
      var destinationId = event.settingsInfo['printerId'];
      var key =
          this.getDestinationKey_(print_preview.Destination.Origin.LOCAL,
                                  destinationId);
      var destination = this.destinationMap_[key];
      var capabilities = print_preview.LocalCapabilitiesParser.parse(
            event.settingsInfo);
      if (destination) {
        // In case there were multiple capabilities request for this local
        // destination, just ignore the later ones.
        if (destination.capabilities != null) {
          return;
        }
        destination.capabilities = capabilities;
      } else {
        // TODO(rltoscano): This makes the assumption that the "deviceName" is
        // the same as "printerName". We should include the "printerName" in the
        // response. See http://crbug.com/132831.
        destination = print_preview.LocalDestinationParser.parse(
            {deviceName: destinationId, printerName: destinationId});
        destination.capabilities = capabilities;
        this.insertDestination(destination);
      }
      if (this.selectedDestination_ &&
          this.selectedDestination_.id == destinationId) {
        cr.dispatchSimpleEvent(this,
                               DestinationStore.EventType.
                                   SELECTED_DESTINATION_CAPABILITIES_READY);
      }
    },

    /**
     * Called when a request to get a local destination's print capabilities
     * fails. If the destination is the initial destination, auto-select another
     * destination instead.
     * @param {Event} event Contains the destination ID that failed.
     * @private
     */
    onGetCapabilitiesFail_: function(event) {
      console.error('Failed to get print capabilities for printer ' +
                    event.destinationId);
      if (this.isInAutoSelectMode_ &&
          this.matchInitialDestinationStrict_(event.destinationId,
                                              event.destinationOrigin)) {
        assert(this.destinations_.length > 0,
               'No destinations were loaded when failed to get initial ' +
               'destination');
        this.selectDestination(this.destinations_[0]);
      }
    },

    /**
     * Called when the /search call completes. Adds the fetched destinations to
     * the destination store.
     * @param {Event} event Contains the fetched destinations.
     * @private
     */
    onCloudPrintSearchDone_: function(event) {
      this.insertDestinations(event.printers);
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when the /search call fails. Updates outstanding request count and
     * dispatches CLOUD_DESTINATIONS_LOADED event.
     * @private
     */
    onCloudPrintSearchFailed_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called when /printer call completes. Updates the specified destination's
     * print capabilities.
     * @param {Event} event Contains detailed information about the
     *     destination.
     * @private
     */
    onCloudPrintPrinterDone_: function(event) {
      this.updateDestination(event.printer);
    },

    /**
     * Called when the Google Cloud Print interface fails to lookup a
     * destination. Selects another destination if the failed destination was
     * the initial destination.
     * @param {object} event Contains the ID of the destination that was failed
     *     to be looked up.
     * @private
     */
    onCloudPrintPrinterFailed_: function(event) {
      if (this.isInAutoSelectMode_ &&
          this.matchInitialDestinationStrict_(event.destinationId,
                                              event.destinationOrigin)) {
        console.error('Could not find initial printer: ' + event.destinationId);
        assert(this.destinations_.length > 0,
               'No destinations were loaded when failed to get initial ' +
               'destination');
        this.selectDestination(this.destinations_[0]);
      }
    },

    /**
     * Called when a Privet printer is added to the local network.
     * @param {object} event Contains information about the added printer.
     * @private
     */
    onPrivetPrinterAdded_: function(event) {
        this.insertDestinations(
            print_preview.PrivetDestinationParser.parse(event.printer));
    },

    /**
     * Called when capabilities for a privet printer are set.
     * @param {object} event Contains the capabilities and printer ID.
     * @private
     */
    onPrivetCapabilitiesSet_: function(event) {
      var destinationId = event.printerId;
      var destinations =
          print_preview.PrivetDestinationParser.parse(event.printer);
      destinations.forEach(function(dest) {
        dest.capabilities = event.capabilities;
        this.updateDestination(dest);
      }, this);
    },

    /**
     * Called when the search for Privet printers is done.
     * @private
     */
    onPrivetPrinterSearchDone_: function() {
      this.isPrivetDestinationSearchInProgress_ = false;
      this.hasLoadedAllPrivetDestinations_ = true;
      cr.dispatchSimpleEvent(
          this, DestinationStore.EventType.DESTINATION_SEARCH_DONE);
    },

    /**
     * Called from native layer after the user was requested to sign in, and did
     * so successfully.
     * @private
     */
    onDestinationsReload_: function() {
      this.reset_();
      this.isInAutoSelectMode_ = true;
      this.startLoadLocalDestinations();
      this.startLoadCloudDestinations();
      this.startLoadPrivetDestinations();
    },

    /**
     * Called when auto-selection fails. Selects the first destination in store.
     * @private
     */
    onAutoSelectFailed_: function() {
      this.autoSelectTimeout_ = null;
      assert(this.destinations_.length > 0,
             'No destinations were loaded before auto-select timeout expired');
      this.selectDestination(this.destinations_[0]);
    },

    // TODO(vitalybuka): Remove three next functions replacing Destination.id
    //    and Destination.origin by complex ID.
    /**
     * Returns key to be used with {@code destinationMap_}.
     * @param {!print_preview.Destination.Origin} origin Destination origin.
     * @return {!string} id Destination id.
     * @private
     */
    getDestinationKey_: function(origin, id) {
      return origin + '/' + id;
    },

    /**
     * @param {?string} id Id of the destination.
     * @param {?string} origin Oring of the destination.
     * @return {boolean} Whether a initial destination matches provided.
     * @private
     */
    matchInitialDestination_: function(id, origin) {
      return this.initialDestinationId_ == null ||
             this.initialDestinationOrigin_ == null ||
             this.matchInitialDestinationStrict_(id, origin);
    },

    /**
     * @param {?string} id Id of the destination.
     * @param {?string} origin Oring of the destination.
     * @return {boolean} Whether destination is the same as initial.
     * @private
     */
    matchInitialDestinationStrict_: function(id, origin) {
      return id == this.initialDestinationId_ &&
             origin == this.initialDestinationOrigin_;
    }
  };

  // Export
  return {
    DestinationStore: DestinationStore
  };
});
