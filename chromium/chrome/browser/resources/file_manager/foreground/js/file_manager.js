// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application (though the
 * latter is not yet implemented).
 *
 * @constructor
 */
function FileManager() {
  this.initializeQueue_ = new AsyncUtil.Group();

  /**
   * Current list type.
   * @type {ListType}
   * @private
   */
  this.listType_ = null;

  /**
   * Whether to suppress the focus moving or not.
   * This is used to filter out focusing by mouse.
   * @type {boolean}
   * @private
   */
  this.suppressFocus_ = false;

  /**
   * SelectionHandler.
   * @type {SelectionHandler}
   * @private
   */
  this.selectionHandler_ = null;
}

/**
 * Maximum delay in milliseconds for updating thumbnails in the bottom panel
 * to mitigate flickering. If images load faster then the delay they replace
 * old images smoothly. On the other hand we don't want to keep old images
 * too long.
 *
 * @type {number}
 * @const
 */
FileManager.THUMBNAIL_SHOW_DELAY = 100;

FileManager.prototype = {
  __proto__: cr.EventTarget.prototype,
  get directoryModel() {
    return this.directoryModel_;
  },
  get navigationList() {
    return this.navigationList_;
  },
  get document() {
    return this.document_;
  },
  get fileTransferController() {
    return this.fileTransferController_;
  },
  get backgroundPage() {
    return this.backgroundPage_;
  },
  get volumeManager() {
    return this.volumeManager_;
  }
};

/**
 * Unload the file manager.
 * Used by background.js (when running in the packaged mode).
 */
function unload() {
  fileManager.onBeforeUnload_();
  fileManager.onUnload_();
}

/**
 * List of dialog types.
 *
 * Keep this in sync with FileManagerDialog::GetDialogTypeAsString, except
 * FULL_PAGE which is specific to this code.
 *
 * @enum {string}
 */
var DialogType = {
  SELECT_FOLDER: 'folder',
  SELECT_UPLOAD_FOLDER: 'upload-folder',
  SELECT_SAVEAS_FILE: 'saveas-file',
  SELECT_OPEN_FILE: 'open-file',
  SELECT_OPEN_MULTI_FILE: 'open-multi-file',
  FULL_PAGE: 'full-page'
};

/**
 * @param {string} type Dialog type.
 * @return {boolean} Whether the type is modal.
 */
DialogType.isModal = function(type) {
  return type == DialogType.SELECT_FOLDER ||
      type == DialogType.SELECT_UPLOAD_FOLDER ||
      type == DialogType.SELECT_SAVEAS_FILE ||
      type == DialogType.SELECT_OPEN_FILE ||
      type == DialogType.SELECT_OPEN_MULTI_FILE;
};

/**
 * @param {string} type Dialog type.
 * @return {boolean} Whether the type is open dialog.
 */
DialogType.isOpenDialog = function(type) {
  return type == DialogType.SELECT_OPEN_FILE ||
         type == DialogType.SELECT_OPEN_MULTI_FILE;
};

/**
 * @param {string} type Dialog type.
 * @return {boolean} Whether the type is folder selection dialog.
 */
DialogType.isFolderDialog = function(type) {
  return type == DialogType.SELECT_FOLDER ||
         type == DialogType.SELECT_UPLOAD_FOLDER;
};

/**
 * Bottom margin of the list and tree for transparent preview panel.
 * @const
 */
var BOTTOM_MARGIN_FOR_PREVIEW_PANEL_PX = 52;

// Anonymous "namespace".
(function() {

  // Private variables and helper functions.

  /**
   * Number of milliseconds in a day.
   */
  var MILLISECONDS_IN_DAY = 24 * 60 * 60 * 1000;

  /**
   * Some UI elements react on a single click and standard double click handling
   * leads to confusing results. We ignore a second click if it comes soon
   * after the first.
   */
  var DOUBLE_CLICK_TIMEOUT = 200;

  /**
   * Update the element to display the information about remaining space for
   * the storage.
   * @param {!Element} spaceInnerBar Block element for a percentage bar
   *                                 representing the remaining space.
   * @param {!Element} spaceInfoLabel Inline element to contain the message.
   * @param {!Element} spaceOuterBar Block element around the percentage bar.
   */
   var updateSpaceInfo = function(
      sizeStatsResult, spaceInnerBar, spaceInfoLabel, spaceOuterBar) {
    spaceInnerBar.removeAttribute('pending');
    if (sizeStatsResult) {
      var sizeStr = util.bytesToString(sizeStatsResult.remainingSize);
      spaceInfoLabel.textContent = strf('SPACE_AVAILABLE', sizeStr);

      var usedSpace =
          sizeStatsResult.totalSize - sizeStatsResult.remainingSize;
      spaceInnerBar.style.width =
          (100 * usedSpace / sizeStatsResult.totalSize) + '%';

      spaceOuterBar.hidden = false;
    } else {
      spaceOuterBar.hidden = true;
      spaceInfoLabel.textContent = str('FAILED_SPACE_INFO');
    }
  };

  // Public statics.

  FileManager.ListType = {
    DETAIL: 'detail',
    THUMBNAIL: 'thumb'
  };

  FileManager.prototype.initPreferences_ = function(callback) {
    var group = new AsyncUtil.Group();

    // DRIVE preferences should be initialized before creating DirectoryModel
    // to rebuild the roots list.
    group.add(this.getPreferences_.bind(this));

    // Get startup preferences.
    this.viewOptions_ = {};
    group.add(function(done) {
      util.platform.getPreference(this.startupPrefName_, function(value) {
        // Load the global default options.
        try {
          this.viewOptions_ = JSON.parse(value);
        } catch (ignore) {}
        // Override with window-specific options.
        if (window.appState && window.appState.viewOptions) {
          for (var key in window.appState.viewOptions) {
            if (window.appState.viewOptions.hasOwnProperty(key))
              this.viewOptions_[key] = window.appState.viewOptions[key];
          }
        }
        done();
      }.bind(this));
    }.bind(this));

    // Get the command line option.
    group.add(function(done) {
      chrome.commandLinePrivate.hasSwitch(
          'file-manager-show-checkboxes', function(flag) {
        this.showCheckboxes_ = flag;
        done();
      }.bind(this));
    }.bind(this));

    // TODO(yoshiki): Remove the flag when the feature is launched.
    this.enableExperimentalWebstoreIntegration_ = true;

    group.run(callback);
  };

  /**
   * One time initialization for the file system and related things.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initFileSystemUI_ = function(callback) {
    this.table_.startBatchUpdates();
    this.grid_.startBatchUpdates();

    this.initFileList_();
    this.setupCurrentDirectory_();

    // PyAuto tests monitor this state by polling this variable
    this.__defineGetter__('workerInitialized_', function() {
       return this.metadataCache_.isInitialized();
    }.bind(this));

    this.initDateTimeFormatters_();

    var self = this;

    // Get the 'allowRedeemOffers' preference before launching
    // FileListBannerController.
    this.getPreferences_(function(pref) {
      /** @type {boolean} */
      var showOffers = pref['allowRedeemOffers'];
      self.bannersController_ = new FileListBannerController(
          self.directoryModel_, self.volumeManager_, self.document_,
          showOffers);
      self.bannersController_.addEventListener('relayout',
                                               self.onResize_.bind(self));
    });

    var dm = this.directoryModel_;
    dm.addEventListener('directory-changed',
                        this.onDirectoryChanged_.bind(this));
    dm.addEventListener('begin-update-files', function() {
      self.currentList_.startBatchUpdates();
    });
    dm.addEventListener('end-update-files', function() {
      self.restoreItemBeingRenamed_();
      self.currentList_.endBatchUpdates();
    });
    dm.addEventListener('scan-started', this.onScanStarted_.bind(this));
    dm.addEventListener('scan-completed', this.onScanCompleted_.bind(this));
    dm.addEventListener('scan-failed', this.onScanCancelled_.bind(this));
    dm.addEventListener('scan-cancelled', this.onScanCancelled_.bind(this));
    dm.addEventListener('scan-updated', this.onScanUpdated_.bind(this));
    dm.addEventListener('rescan-completed',
                        this.onRescanCompleted_.bind(this));

    this.directoryTree_.addEventListener('change', function() {
      this.ensureDirectoryTreeItemNotBehindPreviewPanel_();
    }.bind(this));

    var stateChangeHandler =
        this.onPreferencesChanged_.bind(this);
    chrome.fileBrowserPrivate.onPreferencesChanged.addListener(
        stateChangeHandler);
    stateChangeHandler();

    var driveConnectionChangedHandler =
        this.onDriveConnectionChanged_.bind(this);
    this.volumeManager_.addEventListener('drive-connection-changed',
        driveConnectionChangedHandler);
    driveConnectionChangedHandler();

    // Set the initial focus.
    this.refocus();
    // Set it as a fallback when there is no focus.
    this.document_.addEventListener('focusout', function(e) {
      setTimeout(function() {
        // When there is no focus, the active element is the <body>.
        if (this.document_.activeElement == this.document_.body)
          this.refocus();
      }.bind(this), 0);
    }.bind(this));

    this.initDataTransferOperations_();

    this.initContextMenus_();
    this.initCommands_();

    this.updateFileTypeFilter_();

    this.selectionHandler_.onFileSelectionChanged();

    this.table_.endBatchUpdates();
    this.grid_.endBatchUpdates();

    callback();
  };

  /**
   * If |item| in the directory tree is behind the preview panel, scrolls up the
   * parent view and make the item visible. This should be called when:
   *  - the selected item is changed in the directory tree.
   *  - the visibility of the the preview panel is changed.
   *
   * @private
   */
  FileManager.prototype.ensureDirectoryTreeItemNotBehindPreviewPanel_ =
      function() {
    var selectedSubTree = this.directoryTree_.selectedItem;
    if (!selectedSubTree)
      return;
    var item = selectedSubTree.rowElement;
    var parentView = this.directoryTree_;

    var itemRect = item.getBoundingClientRect();
    if (!itemRect)
      return;

    var listRect = parentView.getBoundingClientRect();
    if (!listRect)
      return;

    var previewPanel = this.dialogDom_.querySelector('.preview-panel');
    var previewPanelRect = previewPanel.getBoundingClientRect();
    var panelHeight = previewPanelRect ? previewPanelRect.height : 0;

    var itemBottom = itemRect.bottom;
    var listBottom = listRect.bottom - panelHeight;

    if (itemBottom > listBottom) {
      var scrollOffset = itemBottom - listBottom;
      parentView.scrollTop += scrollOffset;
    }
  };

  /**
   * @private
   */
  FileManager.prototype.initDateTimeFormatters_ = function() {
    var use12hourClock = !this.preferences_['use24hourClock'];
    this.table_.setDateTimeFormat(use12hourClock);
  };

  /**
   * @private
   */
  FileManager.prototype.initDataTransferOperations_ = function() {
    this.fileOperationManager_ = FileOperationManagerWrapper.getInstance(
        this.backgroundPage_);

    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType != DialogType.FULL_PAGE) return;

    // TODO(hidehiko): Extract FileOperationManager related code from
    // FileManager to simplify it.
    this.onCopyProgressBound_ = this.onCopyProgress_.bind(this);
    this.fileOperationManager_.addEventListener(
        'copy-progress', this.onCopyProgressBound_);

    this.onEntryChangedBound_ = this.onEntryChanged_.bind(this);
    this.fileOperationManager_.addEventListener(
        'entry-changed', this.onEntryChangedBound_);

    var controller = this.fileTransferController_ =
        new FileTransferController(this.document_,
                                   this.fileOperationManager_,
                                   this.metadataCache_,
                                   this.directoryModel_);
    controller.attachDragSource(this.table_.list);
    controller.attachFileListDropTarget(this.table_.list);
    controller.attachDragSource(this.grid_);
    controller.attachFileListDropTarget(this.grid_);
    controller.attachTreeDropTarget(this.directoryTree_);
    controller.attachNavigationListDropTarget(this.navigationList_, true);
    controller.attachCopyPasteHandlers();
    controller.addEventListener('selection-copied',
        this.blinkSelection.bind(this));
    controller.addEventListener('selection-cut',
        this.blinkSelection.bind(this));
  };

  /**
   * One-time initialization of context menus.
   * @private
   */
  FileManager.prototype.initContextMenus_ = function() {
    this.fileContextMenu_ = this.dialogDom_.querySelector('#file-context-menu');
    cr.ui.Menu.decorate(this.fileContextMenu_);

    cr.ui.contextMenuHandler.setContextMenu(this.grid_, this.fileContextMenu_);
    cr.ui.contextMenuHandler.setContextMenu(this.table_.querySelector('.list'),
        this.fileContextMenu_);
    cr.ui.contextMenuHandler.setContextMenu(
        this.document_.querySelector('.drive-welcome.page'),
        this.fileContextMenu_);

    this.rootsContextMenu_ =
        this.dialogDom_.querySelector('#roots-context-menu');
    cr.ui.Menu.decorate(this.rootsContextMenu_);
    this.navigationList_.setContextMenu(this.rootsContextMenu_);

    this.directoryTreeContextMenu_ =
        this.dialogDom_.querySelector('#directory-tree-context-menu');
    cr.ui.Menu.decorate(this.directoryTreeContextMenu_);
    this.directoryTree_.contextMenuForSubitems = this.directoryTreeContextMenu_;

    this.textContextMenu_ =
        this.dialogDom_.querySelector('#text-context-menu');
    cr.ui.Menu.decorate(this.textContextMenu_);

    this.gearButton_ = this.dialogDom_.querySelector('#gear-button');
    this.gearButton_.addEventListener('menushow',
        this.refreshRemainingSpace_.bind(this,
                                         false /* Without loading caption. */));
    this.dialogDom_.querySelector('#gear-menu').menuItemSelector =
        'menuitem, hr';
    cr.ui.decorate(this.gearButton_, cr.ui.MenuButton);

    if (this.dialogType == DialogType.FULL_PAGE) {
      // This is to prevent the buttons from stealing focus on mouse down.
      var preventFocus = function(event) {
        event.preventDefault();
      };

      var maximizeButton = this.dialogDom_.querySelector('#maximize-button');
      maximizeButton.addEventListener('click', this.onMaximize.bind(this));
      maximizeButton.addEventListener('mousedown', preventFocus);

      var closeButton = this.dialogDom_.querySelector('#close-button');
      closeButton.addEventListener('click', this.onClose.bind(this));
      closeButton.addEventListener('mousedown', preventFocus);
    }

    this.syncButton.checkable = true;
    this.hostedButton.checkable = true;
    this.detailViewButton_.checkable = true;
    this.thumbnailViewButton_.checkable = true;

    if (util.platform.runningInBrowser()) {
      // Suppresses the default context menu.
      this.dialogDom_.addEventListener('contextmenu', function(e) {
        e.preventDefault();
        e.stopPropagation();
      });
    }
  };

  FileManager.prototype.onMaximize = function() {
    var appWindow = chrome.app.window.current();
    if (appWindow.isMaximized())
      appWindow.restore();
    else
      appWindow.maximize();
  };

  FileManager.prototype.onClose = function() {
    window.close();
  };

  /**
   * One-time initialization of commands.
   * @private
   */
  FileManager.prototype.initCommands_ = function() {
    this.commandHandler = new CommandHandler(this);

    // TODO(hirono): Move the following block to the UI part.
    var commandButtons = this.dialogDom_.querySelectorAll('button[command]');
    for (var j = 0; j < commandButtons.length; j++)
      CommandButton.decorate(commandButtons[j]);

    var inputs = this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea');
    for (var i = 0; i < inputs.length; i++) {
      cr.ui.contextMenuHandler.setContextMenu(inputs[i], this.textContextMenu_);
      this.registerInputCommands_(inputs[i]);
    }

    cr.ui.contextMenuHandler.setContextMenu(this.renameInput_,
                                            this.textContextMenu_);
    this.registerInputCommands_(this.renameInput_);
    this.document_.addEventListener('command',
                                    this.setNoHover_.bind(this, true));
  };

  /**
   * Registers cut, copy, paste and delete commands on input element.
   *
   * @param {Node} node Text input element to register on.
   * @private
   */
  FileManager.prototype.registerInputCommands_ = function(node) {
    CommandUtil.forceDefaultHandler(node, 'cut');
    CommandUtil.forceDefaultHandler(node, 'copy');
    CommandUtil.forceDefaultHandler(node, 'paste');
    CommandUtil.forceDefaultHandler(node, 'delete');
    node.addEventListener('keydown', function(e) {
      var key = util.getKeyModifiers(e) + e.keyCode;
      if (key === '190' /* '/' */ || key === '191' /* '.' */) {
        // If this key event is propagated, this is handled search command,
        // which calls 'preventDefault' method.
        e.stopPropagation();
      }
    });
  };

  /**
   * Entry point of the initialization.
   * This method is called from main.js.
   */
  FileManager.prototype.initializeCore = function() {
    this.initializeQueue_.add(this.initGeneral_.bind(this), [], 'initGeneral');
    this.initializeQueue_.add(this.initBackgroundPage_.bind(this),
                              [], 'initBackgroundPage');
    this.initializeQueue_.add(this.initPreferences_.bind(this),
                              ['initGeneral'], 'initPreferences');
    this.initializeQueue_.add(this.initVolumeManager_.bind(this),
                              ['initGeneral', 'initBackgroundPage'],
                              'initVolumeManager');

    this.initializeQueue_.run();
  };

  FileManager.prototype.initializeUI = function(dialogDom, callback) {
    this.dialogDom_ = dialogDom;
    this.document_ = this.dialogDom_.ownerDocument;

    this.initializeQueue_.add(
        this.initEssentialUI_.bind(this),
        ['initGeneral', 'initBackgroundPage'],
        'initEssentialUI');
    this.initializeQueue_.add(this.initAdditionalUI_.bind(this),
        ['initEssentialUI'], 'initAdditionalUI');
    this.initializeQueue_.add(
        this.initFileSystemUI_.bind(this),
        ['initAdditionalUI', 'initPreferences'], 'initFileSystemUI');

    // Run again just in case if all pending closures have completed and the
    // queue has stopped and monitor the completion.
    this.initializeQueue_.run(callback);
  };

  /**
   * Initializes general purpose basic things, which are used by other
   * initializing methods.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initGeneral_ = function(callback) {
    // Initialize the application state.
    if (window.appState) {
      this.params_ = window.appState.params || {};
      this.defaultPath = window.appState.defaultPath;
    } else {
      this.params_ = location.search ?
                     JSON.parse(decodeURIComponent(location.search.substr(1))) :
                     {};
      this.defaultPath = this.params_.defaultPath;
    }

    // Initialize the member variables that depend this.params_.
    this.dialogType = this.params_.type || DialogType.FULL_PAGE;
    this.startupPrefName_ = 'file-manager-' + this.dialogType;
    this.fileTypes_ = this.params_.typeList || [];

    callback();
  };

  /**
   * Initialize the background page.
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initBackgroundPage_ = function(callback) {
    chrome.runtime.getBackgroundPage(function(backgroundPage) {
      this.backgroundPage_ = backgroundPage;
      this.backgroundPage_.background.ready(function() {
        loadTimeData.data = this.backgroundPage_.background.stringData;
        callback();
      }.bind(this));
    }.bind(this));
  };

  /**
   * Initializes the VolumeManager instance.
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initVolumeManager_ = function(callback) {
    // Auto resolving to local path does not work for folders (e.g., dialog for
    // loading unpacked extensions).
    var noLocalPathResolution = DialogType.isFolderDialog(this.params_.type);

    // If this condition is false, VolumeManagerWrapper hides all drive
    // related event and data, even if Drive is enabled on preference.
    // In other words, even if Drive is disabled on preference but Files.app
    // should show Drive when it is re-enabled, then the value should be set to
    // true.
    // Note that the Drive enabling preference change is listened by
    // DriveIntegrationService, so here we don't need to take care about it.
    var driveEnabled =
        !noLocalPathResolution || !this.params_.shouldReturnLocalPath;
    this.volumeManager_ = new VolumeManagerWrapper(
        driveEnabled, this.backgroundPage_);
    callback();
  };

  /**
   * One time initialization of the Files.app's essential UI elements. These
   * elements will be shown to the user. Only visible elements should be
   * initialized here. Any heavy operation should be avoided. Files.app's
   * window is shown at the end of this routine.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initEssentialUI_ = function(callback) {
    // Optional list of file types.
    metrics.recordEnum('Create', this.dialogType,
        [DialogType.SELECT_FOLDER,
         DialogType.SELECT_UPLOAD_FOLDER,
         DialogType.SELECT_SAVEAS_FILE,
         DialogType.SELECT_OPEN_FILE,
         DialogType.SELECT_OPEN_MULTI_FILE,
         DialogType.FULL_PAGE]);

    // Create the metadata cache.
    this.metadataCache_ = MetadataCache.createFull();

    // Create the root view of FileManager.
    this.ui_ = new FileManagerUI(this.dialogDom_, this.dialogType);
    this.fileTypeSelector_ = this.ui_.fileTypeSelector;
    this.okButton_ = this.ui_.okButton;
    this.cancelButton_ = this.ui_.cancelButton;

    // Show the window as soon as the UI pre-initialization is done.
    if (this.dialogType == DialogType.FULL_PAGE &&
        !util.platform.runningInBrowser()) {
      chrome.app.window.current().show();
      setTimeout(callback, 100);  // Wait until the animation is finished.
    } else {
      callback();
    }
  };

  /**
   * One-time initialization of dialogs.
   * @private
   */
  FileManager.prototype.initDialogs_ = function() {
    // Initialize the dialog.
    this.ui_.initDialogs();
    FileManagerDialogBase.setFileManager(this);

    // Obtains the dialog instances from FileManagerUI.
    // TODO(hirono): Remove the properties from the FileManager class.
    this.error = this.ui_.errorDialog;
    this.alert = this.ui_.alertDialog;
    this.confirm = this.ui_.confirmDialog;
    this.prompt = this.ui_.promptDialog;
    this.shareDialog_ = this.ui_.shareDialog;
    this.defaultTaskPicker = this.ui_.defaultTaskPicker;
    this.suggestAppsDialog = this.ui_.suggestAppsDialog;
  };

  /**
   * One-time initialization of various DOM nodes. Loads the additional DOM
   * elements visible to the user. Initialize here elements, which are expensive
   * or hidden in the beginning.
   *
   * @param {function()} callback Completion callback.
   * @private
   */
  FileManager.prototype.initAdditionalUI_ = function(callback) {
    this.initDialogs_();
    this.ui_.initAdditionalUI();

    this.dialogDom_.addEventListener('drop', function(e) {
      // Prevent opening an URL by dropping it onto the page.
      e.preventDefault();
    });

    this.dialogDom_.addEventListener('click',
                                     this.onExternalLinkClick_.bind(this));
    // Cache nodes we'll be manipulating.
    var dom = this.dialogDom_;

    this.filenameInput_ = dom.querySelector('#filename-input-box input');
    this.taskItems_ = dom.querySelector('#tasks');

    this.table_ = dom.querySelector('.detail-table');
    this.grid_ = dom.querySelector('.thumbnail-grid');
    this.spinner_ = dom.querySelector('#list-container > .spinner-layer');
    this.showSpinner_(true);

    // Check the option to hide the selecting checkboxes.
    this.table_.showCheckboxes = this.showCheckboxes_;

    var fullPage = this.dialogType == DialogType.FULL_PAGE;
    FileTable.decorate(this.table_, this.metadataCache_, fullPage);
    FileGrid.decorate(this.grid_, this.metadataCache_);

    this.previewPanel_ = new PreviewPanel(
        dom.querySelector('.preview-panel'),
        DialogType.isOpenDialog(this.dialogType) ?
            PreviewPanel.VisibilityType.ALWAYS_VISIBLE :
            PreviewPanel.VisibilityType.AUTO,
        this.metadataCache_,
        this.volumeManager_);
    this.previewPanel_.addEventListener(
        PreviewPanel.Event.VISIBILITY_CHANGE,
        this.onPreviewPanelVisibilityChange_.bind(this));
    this.previewPanel_.initialize();

    this.previewPanel_.breadcrumbs.addEventListener(
         'pathclick', this.onBreadcrumbClick_.bind(this));

    // Initialize progress center panel.
    this.progressCenterPanel_ = new ProgressCenterPanel(
        dom.querySelector('#progress-center'));
    this.backgroundPage_.background.progressCenter.addPanel(
        this.progressCenterPanel_);

    this.document_.addEventListener('keydown', this.onKeyDown_.bind(this));

    // This capturing event is only used to distinguish focusing using
    // keyboard from focusing using mouse.
    this.document_.addEventListener('mousedown', function() {
      this.suppressFocus_ = true;
    }.bind(this), true);

    this.renameInput_ = this.document_.createElement('input');
    this.renameInput_.className = 'rename';

    this.renameInput_.addEventListener(
        'keydown', this.onRenameInputKeyDown_.bind(this));
    this.renameInput_.addEventListener(
        'blur', this.onRenameInputBlur_.bind(this));

    // TODO(hirono): Rename the handler after creating the DialogFooter class.
    this.filenameInput_.addEventListener(
        'input', this.onFilenameInputInput_.bind(this));
    this.filenameInput_.addEventListener(
        'keydown', this.onFilenameInputKeyDown_.bind(this));
    this.filenameInput_.addEventListener(
        'focus', this.onFilenameInputFocus_.bind(this));

    this.listContainer_ = this.dialogDom_.querySelector('#list-container');
    this.listContainer_.addEventListener(
        'keydown', this.onListKeyDown_.bind(this));
    this.listContainer_.addEventListener(
        'keypress', this.onListKeyPress_.bind(this));
    this.listContainer_.addEventListener(
        'mousemove', this.onListMouseMove_.bind(this));

    this.okButton_.addEventListener('click', this.onOk_.bind(this));
    this.onCancelBound_ = this.onCancel_.bind(this);
    this.cancelButton_.addEventListener('click', this.onCancelBound_);

    this.decorateSplitter(
        this.dialogDom_.querySelector('#navigation-list-splitter'));
    this.decorateSplitter(
        this.dialogDom_.querySelector('#middlebar-splitter'));

    this.dialogContainer_ = this.dialogDom_.querySelector('.dialog-container');

    this.syncButton = this.dialogDom_.querySelector('#drive-sync-settings');
    this.syncButton.addEventListener('click', this.onDrivePrefClick_.bind(
        this, 'cellularDisabled', false /* not inverted */));

    this.hostedButton = this.dialogDom_.querySelector('#drive-hosted-settings');
    this.hostedButton.addEventListener('click', this.onDrivePrefClick_.bind(
        this, 'hostedFilesDisabled', true /* inverted */));

    this.detailViewButton_ =
        this.dialogDom_.querySelector('#detail-view');
    this.detailViewButton_.addEventListener('activate',
        this.onDetailViewButtonClick_.bind(this));

    this.thumbnailViewButton_ =
        this.dialogDom_.querySelector('#thumbnail-view');
    this.thumbnailViewButton_.addEventListener('activate',
        this.onThumbnailViewButtonClick_.bind(this));

    cr.ui.ComboButton.decorate(this.taskItems_);
    this.taskItems_.showMenu = function(shouldSetFocus) {
      // Prevent the empty menu from opening.
      if (!this.menu.length)
        return;
      cr.ui.ComboButton.prototype.showMenu.call(this, shouldSetFocus);
    };
    this.taskItems_.addEventListener('select',
        this.onTaskItemClicked_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    this.filePopup_ = null;

    this.searchBoxWrapper_ = this.ui_.searchBox.element;
    this.searchBox_ = this.ui_.searchBox.inputElement;
    this.searchBox_.addEventListener(
        'input', this.onSearchBoxUpdate_.bind(this));
    this.ui_.searchBox.clearButton.addEventListener(
        'click', this.onSearchClearButtonClick_.bind(this));

    this.lastSearchQuery_ = '';

    this.autocompleteList_ = this.ui_.searchBox.autocompleteList;
    this.autocompleteList_.requestSuggestions =
        this.requestAutocompleteSuggestions_.bind(this);

    // Instead, open the suggested item when Enter key is pressed or
    // mouse-clicked.
    this.autocompleteList_.handleEnterKeydown = function(event) {
      this.openAutocompleteSuggestion_();
      this.lastAutocompleteQuery_ = '';
      this.autocompleteList_.suggestions = [];
    }.bind(this);
    this.autocompleteList_.addEventListener('mousedown', function(event) {
      this.openAutocompleteSuggestion_();
      this.lastAutocompleteQuery_ = '';
      this.autocompleteList_.suggestions = [];
    }.bind(this));

    this.defaultActionMenuItem_ =
        this.dialogDom_.querySelector('#default-action');

    this.openWithCommand_ =
        this.dialogDom_.querySelector('#open-with');

    this.driveBuyMoreStorageCommand_ =
        this.dialogDom_.querySelector('#drive-buy-more-space');

    this.defaultActionMenuItem_.addEventListener('click',
        this.dispatchSelectionAction_.bind(this));

    this.initFileTypeFilter_();

    util.addIsFocusedMethod();

    // Populate the static localized strings.
    i18nTemplate.process(this.document_, loadTimeData);

    // Arrange the file list.
    this.table_.normalizeColumns();
    this.table_.redraw();

    callback();
  };

  /**
   * @private
   */
  FileManager.prototype.onBreadcrumbClick_ = function(event) {
    // TODO(hirono): Use directoryModel#changeDirectoryEntry after implementing
    // it.
    if (event.entry === RootType.DRIVE_SHARED_WITH_ME)
      this.directoryModel_.changeDirectory(RootDirectory.DRIVE_SHARED_WITH_ME);
    else
      this.directoryModel_.changeDirectory(event.entry.fullPath);
  };

  /**
   * Constructs table and grid (heavy operation).
   * @private
   **/
  FileManager.prototype.initFileList_ = function() {
    // Always sharing the data model between the detail/thumb views confuses
    // them.  Instead we maintain this bogus data model, and hook it up to the
    // view that is not in use.
    this.emptyDataModel_ = new cr.ui.ArrayDataModel([]);
    this.emptySelectionModel_ = new cr.ui.ListSelectionModel();

    var singleSelection =
        this.dialogType == DialogType.SELECT_OPEN_FILE ||
        this.dialogType == DialogType.SELECT_FOLDER ||
        this.dialogType == DialogType.SELECT_UPLOAD_FOLDER ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE;

    this.fileFilter_ = new FileFilter(
        this.metadataCache_,
        false  /* Don't show dot files by default. */);

    this.fileWatcher_ = new FileWatcher(this.metadataCache_);
    this.fileWatcher_.addEventListener(
        'watcher-metadata-changed',
        this.onWatcherMetadataChanged_.bind(this));

    this.directoryModel_ = new DirectoryModel(
        singleSelection,
        this.fileFilter_,
        this.fileWatcher_,
        this.metadataCache_,
        this.volumeManager_);

    this.folderShortcutsModel_ = new FolderShortcutsDataModel();

    this.selectionHandler_ = new FileSelectionHandler(this);

    var dataModel = this.directoryModel_.getFileList();

    this.table_.setupCompareFunctions(dataModel);

    dataModel.addEventListener('permuted',
                               this.updateStartupPrefs_.bind(this));

    this.directoryModel_.getFileListSelection().addEventListener('change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

    this.initList_(this.grid_);
    this.initList_(this.table_.list);

    var fileListFocusBound = this.onFileListFocus_.bind(this);
    var fileListBlurBound = this.onFileListBlur_.bind(this);

    this.table_.list.addEventListener('focus', fileListFocusBound);
    this.grid_.addEventListener('focus', fileListFocusBound);

    this.table_.list.addEventListener('blur', fileListBlurBound);
    this.grid_.addEventListener('blur', fileListBlurBound);

    var dragStartBound = this.onDragStart_.bind(this);
    this.table_.list.addEventListener('dragstart', dragStartBound);
    this.grid_.addEventListener('dragstart', dragStartBound);

    var dragEndBound = this.onDragEnd_.bind(this);
    this.table_.list.addEventListener('dragend', dragEndBound);
    this.grid_.addEventListener('dragend', dragEndBound);
    // This event is published by DragSelector because drag end event is not
    // published at the end of drag selection.
    this.table_.list.addEventListener('dragselectionend', dragEndBound);
    this.grid_.addEventListener('dragselectionend', dragEndBound);

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    this.initNavigationList_();

    this.table_.addEventListener('column-resize-end',
                                 this.updateStartupPrefs_.bind(this));

    // Restore preferences.
    this.directoryModel_.sortFileList(
        this.viewOptions_.sortField || 'modificationTime',
        this.viewOptions_.sortDirection || 'desc');
    if (this.viewOptions_.columns) {
      var cm = this.table_.columnModel;
      for (var i = 0; i < cm.totalSize; i++) {
        if (this.viewOptions_.columns[i] > 0)
          cm.setWidth(i, this.viewOptions_.columns[i]);
      }
    }
    this.setListType(this.viewOptions_.listType || FileManager.ListType.DETAIL);

    this.textSearchState_ = {text: '', date: new Date()};
    this.closeOnUnmount_ = (this.params_.action == 'auto-open');

    if (this.closeOnUnmount_) {
      this.volumeManager_.addEventListener('externally-unmounted',
         this.onExternallyUnmounted_.bind(this));
    }

    // Update metadata to change 'Today' and 'Yesterday' dates.
    var today = new Date();
    today.setHours(0);
    today.setMinutes(0);
    today.setSeconds(0);
    today.setMilliseconds(0);
    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               today.getTime() + MILLISECONDS_IN_DAY - Date.now() + 1000);
  };

  /**
   * @private
   */
  FileManager.prototype.initNavigationList_ = function() {
    this.directoryTree_ = this.dialogDom_.querySelector('#directory-tree');
    DirectoryTree.decorate(this.directoryTree_, this.directoryModel_);

    this.navigationList_ = this.dialogDom_.querySelector('#navigation-list');
    NavigationList.decorate(this.navigationList_,
                            this.volumeManager_,
                            this.directoryModel_);
    this.navigationList_.fileManager = this;
    this.navigationList_.dataModel = new NavigationListModel(
        this.volumeManager_, this.folderShortcutsModel_);
  };

  /**
   * @private
   */
  FileManager.prototype.updateMiddleBarVisibility_ = function() {
    var entry = this.directoryModel_.getCurrentDirEntry();
    if (!entry)
      return;

    var driveVolume = this.volumeManager_.getVolumeInfo(entry);
    var visible =
        DirectoryTreeUtil.isEligiblePathForDirectoryTree(entry.fullPath) &&
        driveVolume && !driveVolume.error;
    this.dialogDom_.
        querySelector('.dialog-middlebar-contents').hidden = !visible;
    this.dialogDom_.querySelector('#middlebar-splitter').hidden = !visible;
    this.onResize_();
  };

  /**
   * @private
   */
  FileManager.prototype.updateStartupPrefs_ = function() {
    var sortStatus = this.directoryModel_.getFileList().sortStatus;
    var prefs = {
      sortField: sortStatus.field,
      sortDirection: sortStatus.direction,
      columns: [],
      listType: this.listType_
    };
    var cm = this.table_.columnModel;
    for (var i = 0; i < cm.totalSize; i++) {
      prefs.columns.push(cm.getWidth(i));
    }
    // Save the global default.
    util.platform.setPreference(this.startupPrefName_, JSON.stringify(prefs));

    // Save the window-specific preference.
    if (window.appState) {
      window.appState.viewOptions = prefs;
      util.saveAppState();
    }
  };

  FileManager.prototype.refocus = function() {
    var targetElement;
    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE)
      targetElement = this.filenameInput_;
    else
      targetElement = this.currentList_;

    // Hack: if the tabIndex is disabled, we can assume a modal dialog is
    // shown. Focus to a button on the dialog instead.
    if (!targetElement.hasAttribute('tabIndex') || targetElement.tabIndex == -1)
      targetElement = document.querySelector('button:not([tabIndex="-1"])');

    if (targetElement)
      targetElement.focus();
  };

  /**
   * File list focus handler. Used to select the top most element on the list
   * if nothing was selected.
   *
   * @private
   */
  FileManager.prototype.onFileListFocus_ = function() {
    // Do not select default item if focused using mouse.
    if (this.suppressFocus_)
      return;

    var selection = this.getSelection();
    if (!selection || selection.totalCount != 0)
      return;

    this.directoryModel_.selectIndex(0);
  };

  /**
   * File list blur handler.
   *
   * @private
   */
  FileManager.prototype.onFileListBlur_ = function() {
    this.suppressFocus_ = false;
  };

  /**
   * Index of selected item in the typeList of the dialog params.
   *
   * @return {number} 1-based index of selected type or 0 if no type selected.
   * @private
   */
  FileManager.prototype.getSelectedFilterIndex_ = function() {
    var index = Number(this.fileTypeSelector_.selectedIndex);
    if (index < 0)  // Nothing selected.
      return 0;
    if (this.params_.includeAllFiles)  // Already 1-based.
      return index;
    return index + 1;  // Convert to 1-based;
  };

  FileManager.prototype.setListType = function(type) {
    if (type && type == this.listType_)
      return;

    this.table_.list.startBatchUpdates();
    this.grid_.startBatchUpdates();

    // TODO(dzvorygin): style.display and dataModel setting order shouldn't
    // cause any UI bugs. Currently, the only right way is first to set display
    // style and only then set dataModel.

    if (type == FileManager.ListType.DETAIL) {
      this.table_.dataModel = this.directoryModel_.getFileList();
      this.table_.selectionModel = this.directoryModel_.getFileListSelection();
      this.table_.hidden = false;
      this.grid_.hidden = true;
      this.grid_.selectionModel = this.emptySelectionModel_;
      this.grid_.dataModel = this.emptyDataModel_;
      this.table_.hidden = false;
      /** @type {cr.ui.List} */
      this.currentList_ = this.table_.list;
      this.detailViewButton_.setAttribute('checked', '');
      this.thumbnailViewButton_.removeAttribute('checked');
      this.detailViewButton_.setAttribute('disabled', '');
      this.thumbnailViewButton_.removeAttribute('disabled');
    } else if (type == FileManager.ListType.THUMBNAIL) {
      this.grid_.dataModel = this.directoryModel_.getFileList();
      this.grid_.selectionModel = this.directoryModel_.getFileListSelection();
      this.grid_.hidden = false;
      this.table_.hidden = true;
      this.table_.selectionModel = this.emptySelectionModel_;
      this.table_.dataModel = this.emptyDataModel_;
      this.grid_.hidden = false;
      /** @type {cr.ui.List} */
      this.currentList_ = this.grid_;
      this.thumbnailViewButton_.setAttribute('checked', '');
      this.detailViewButton_.removeAttribute('checked');
      this.thumbnailViewButton_.setAttribute('disabled', '');
      this.detailViewButton_.removeAttribute('disabled');
    } else {
      throw new Error('Unknown list type: ' + type);
    }

    this.listType_ = type;
    this.updateStartupPrefs_();
    this.onResize_();

    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();
  };

  /**
   * Initialize the file list table or grid.
   *
   * @param {cr.ui.List} list The list.
   * @private
   */
  FileManager.prototype.initList_ = function(list) {
    // Overriding the default role 'list' to 'listbox' for better accessibility
    // on ChromeOS.
    list.setAttribute('role', 'listbox');
    list.addEventListener('click', this.onDetailClick_.bind(this));
    list.id = 'file-list';
  };

  /**
   * @private
   */
  FileManager.prototype.onCopyProgress_ = function(event) {
    if (event.reason == 'ERROR' &&
        event.error.code == util.FileOperationErrorType.FILESYSTEM_ERROR &&
        event.error.data.toDrive &&
        event.error.data.code == FileError.QUOTA_EXCEEDED_ERR) {
      this.alert.showHtml(
          strf('DRIVE_SERVER_OUT_OF_SPACE_HEADER'),
          strf('DRIVE_SERVER_OUT_OF_SPACE_MESSAGE',
              decodeURIComponent(
                  event.error.data.sourceFileUrl.split('/').pop()),
              str('GOOGLE_DRIVE_BUY_STORAGE_URL')));
    }
  };

  /**
   * Handler of file manager operations. Called when an entry has been
   * changed.
   * This updates directory model to reflect operation result immediately (not
   * waiting for directory update event). Also, preloads thumbnails for the
   * images of new entries.
   * See also FileOperationManager.EventRouter.
   *
   * @param {Event} event An event for the entry change.
   * @private
   */
  FileManager.prototype.onEntryChanged_ = function(event) {
    var kind = event.kind;
    var entry = event.entry;
    this.directoryModel_.onEntryChanged(kind, entry);
    this.selectionHandler_.onFileSelectionChanged();

    if (kind == util.EntryChangedKind.CREATE && FileType.isImage(entry)) {
      // Preload a thumbnail if the new copied entry an image.
      var metadata = entry.getMetadata(function(metadata) {
        var url = entry.toURL();
        var thumbnailLoader_ = new ThumbnailLoader(
            url,
            ThumbnailLoader.LoaderType.CANVAS,
            metadata,
            undefined,  // Media type.
            FileType.isOnDrive(url) ?
                ThumbnailLoader.UseEmbedded.USE_EMBEDDED :
                ThumbnailLoader.UseEmbedded.NO_EMBEDDED,
            10);  // Very low priority.
        thumbnailLoader_.loadDetachedImage(function(success) {});
      });
    }
  };

  /**
   * Fills the file type list or hides it.
   * @private
   */
  FileManager.prototype.initFileTypeFilter_ = function() {
    if (this.params_.includeAllFiles) {
      var option = this.document_.createElement('option');
      option.innerText = str('ALL_FILES_FILTER');
      this.fileTypeSelector_.appendChild(option);
      option.value = 0;
    }

    for (var i = 0; i < this.fileTypes_.length; i++) {
      var fileType = this.fileTypes_[i];
      var option = this.document_.createElement('option');
      var description = fileType.description;
      if (!description) {
        // See if all the extensions in the group have the same description.
        for (var j = 0; j != fileType.extensions.length; j++) {
          var currentDescription =
              FileType.getTypeString('.' + fileType.extensions[j]);
          if (!description)  // Set the first time.
            description = currentDescription;
          else if (description != currentDescription) {
            // No single description, fall through to the extension list.
            description = null;
            break;
          }
        }

        if (!description)
          // Convert ['jpg', 'png'] to '*.jpg, *.png'.
          description = fileType.extensions.map(function(s) {
           return '*.' + s;
          }).join(', ');
       }
       option.innerText = description;

       option.value = i + 1;

       if (fileType.selected)
         option.selected = true;

       this.fileTypeSelector_.appendChild(option);
    }

    var options = this.fileTypeSelector_.querySelectorAll('option');
    if (options.length >= 2) {
      // There is in fact no choice, show the selector.
      this.fileTypeSelector_.hidden = false;

      this.fileTypeSelector_.addEventListener('change',
          this.updateFileTypeFilter_.bind(this));
    }
  };

  /**
   * Filters file according to the selected file type.
   * @private
   */
  FileManager.prototype.updateFileTypeFilter_ = function() {
    this.fileFilter_.removeFilter('fileType');
    var selectedIndex = this.getSelectedFilterIndex_();
    if (selectedIndex > 0) { // Specific filter selected.
      var regexp = new RegExp('.*(' +
          this.fileTypes_[selectedIndex - 1].extensions.join('|') + ')$', 'i');
      var filter = function(entry) {
        return entry.isDirectory || regexp.test(entry.name);
      };
      this.fileFilter_.addFilter('fileType', filter);
    }
  };

  /**
   * Resize details and thumb views to fit the new window size.
   * @private
   */
  FileManager.prototype.onResize_ = function() {
    if (this.listType_ == FileManager.ListType.THUMBNAIL)
      this.grid_.relayout();
    else
      this.table_.relayout();

    // May not be available during initialization.
    if (this.directoryTree_)
      this.directoryTree_.relayout();

    // TODO(mtomasz, yoshiki): Initialize navigation list earlier, before
    // file system is available.
    if (this.navigationList_)
      this.navigationList_.redraw();

    this.ui_.searchBox.updateSizeRelatedStyle();

    this.previewPanel_.breadcrumbs.truncate();
  };

  /**
   * Handles local metadata changes in the currect directory.
   * @param {Event} event Change event.
   * @private
   */
  FileManager.prototype.onWatcherMetadataChanged_ = function(event) {
    this.updateMetadataInUI_(
        event.metadataType, event.entries, event.properties);
  };

  /**
   * Resize details and thumb views to fit the new window size.
   * @private
   */
  FileManager.prototype.onPreviewPanelVisibilityChange_ = function() {
    // This method may be called on initialization. Some object may not be
    // initialized.

    var panelHeight = this.previewPanel_.visible ?
        this.previewPanel_.height : 0;
    if (this.grid_)
      this.grid_.setBottomMarginForPanel(panelHeight);
    if (this.table_)
      this.table_.setBottomMarginForPanel(panelHeight);

    if (this.directoryTree_) {
      this.directoryTree_.setBottomMarginForPanel(panelHeight);
      this.ensureDirectoryTreeItemNotBehindPreviewPanel_();
    }
  };

  /**
   * Invoked when the drag is started on the list or the grid.
   * @private
   */
  FileManager.prototype.onDragStart_ = function() {
    // On open file dialog, the preview panel is always shown.
    if (DialogType.isOpenDialog(this.dialogType))
      return;
    this.previewPanel_.visibilityType =
        PreviewPanel.VisibilityType.ALWAYS_HIDDEN;
  };

  /**
   * Invoked when the drag is ended on the list or the grid.
   * @private
   */
  FileManager.prototype.onDragEnd_ = function() {
    // On open file dialog, the preview panel is always shown.
    if (DialogType.isOpenDialog(this.dialogType))
      return;
    this.previewPanel_.visibilityType = PreviewPanel.VisibilityType.AUTO;
  };

  /**
   * Restores current directory and may be a selected item after page load (or
   * reload) or popping a state (after click on back/forward). defaultPath
   * primarily is used with save/open dialogs.
   * Default path may also contain a file name. Freshly opened file manager
   * window has neither.
   *
   * @private
   */
  FileManager.prototype.setupCurrentDirectory_ = function() {
    var tracker = this.directoryModel_.createDirectoryChangeTracker();
    var queue = new AsyncUtil.Queue();

    // Wait until the volume manager is initialized.
    queue.run(function(callback) {
      tracker.start();
      this.volumeManager_.ensureInitialized(callback);
    }.bind(this));

    // Resolve the default path.
    var defaultFullPath;
    var candidateFullPath;
    var candidateEntry;
    queue.run(function(callback) {
      // Cancel this sequence if the current directory has already changed.
      if (tracker.hasChanged) {
        callback();
        return;
      }

      // Resolve the absolute path in case only the file name or an empty string
      // is passed.
      if (!this.defaultPath) {
        defaultFullPath = PathUtil.DEFAULT_MOUNT_POINT;
      } else if (this.defaultPath.indexOf('/') === -1) {
        // Path is a file name.
        defaultFullPath = PathUtil.DEFAULT_MOUNT_POINT + '/' + this.defaultPath;
      } else {
        defaultFullPath = this.defaultPath;
      }

      // If Drive is disabled but the path points to Drive's entry, fallback to
      // DEFAULT_MOUNT_POINT.
      if (PathUtil.isDriveBasedPath(defaultFullPath) &&
          !this.volumeManager_.getVolumeInfo(RootDirectory.DRIVE)) {
        candidateFullPath = PathUtil.DEFAULT_MOUNT_POINT + '/' +
            PathUtil.basename(defaultFullPath);
      } else {
        candidateFullPath = defaultFullPath;
      }

      // If the path points a fake entry, use the entry directly.
      var fakeEntries = DirectoryModel.FAKE_DRIVE_SPECIAL_SEARCH_ENTRIES;
      for (var i = 0; i < fakeEntries.length; i++) {
        if (candidateFullPath === fakeEntries[i].fullPath) {
          candidateEntry = fakeEntries[i];
          callback();
          return;
        }
      }

      // Convert the path to the directory entry and an optional selection
      // entry.
      // TODO(hirono): There may be a race here. The path on Drive, may not
      // be available yet.
      this.volumeManager_.resolveAbsolutePath(candidateFullPath,
                                              function(inEntry) {
        candidateEntry = inEntry;
        callback();
      }, function() {
        callback();
      });
    }.bind(this));

    // Check the obtained entry.
    var nextCurrentDirEntry;
    var selectionEntry = null;
    var suggestedName = null;
    var error = null;
    queue.run(function(callback) {
      // Cancel this sequence if the current directory has already changed.
      if (tracker.hasChanged) {
        callback();
        return;
      }

      if (candidateEntry) {
        // The entry is directory. Use it.
        if (candidateEntry.isDirectory) {
          nextCurrentDirEntry = candidateEntry;
          callback();
          return;
        }
        // The entry exists, but it is not a directory. Therefore use a
        // parent.
        candidateEntry.getParent(function(parentEntry) {
          nextCurrentDirEntry = parentEntry;
          selectionEntry = candidateEntry;
          callback();
        }, function() {
          error = new Error('Unable to resolve parent for: ' +
              candidateEntry.fullPath);
          callback();
        });
        return;
      }

      // If the entry doesn't exist, most probably because the path contains a
      // suggested name. Therefore try to open its parent. However, the parent
      // may also not exist. In such situation, fallback.
      var pathNodes = candidateFullPath.split('/');
      var baseName = pathNodes.pop();
      var parentPath = pathNodes.join('/');
      this.volumeManager_.resolveAbsolutePath(
          parentPath,
          function(parentEntry) {
            nextCurrentDirEntry = parentEntry;
            suggestedName = baseName;
            callback();
          },
          callback);  // In case of an error, continue.
    }.bind(this));

    // If the directory is not set at this stage, fallback to the default
    // mount point.
    queue.run(function(callback) {
      // Cancel this sequence if the current directory has already changed,
      // or the next current directory is already set.
      if (tracker.hasChanged || nextCurrentDirEntry) {
        callback();
        return;
      }
      this.volumeManager_.resolveAbsolutePath(
          PathUtil.DEFAULT_MOUNT_POINT,
          function(fallbackEntry) {
            nextCurrentDirEntry = fallbackEntry;
            callback();
          },
          function() {
            // Fallback directory not available? Throw an error.
            error = new Error('Unable to resolve the fallback directory: ' +
                PathUtil.DEFAULT_MOUNT_POINT);
            callback();
          });
    }.bind(this));

    queue.run(function(callback) {
      // Check error.
      if (error) {
        callback();
        throw error;
      }
      // Check directory change.
      tracker.stop();
      if (tracker.hasChanged) {
        callback();
        return;
      }
      // Finish setup current directory.
      this.finishSetupCurrentDirectory_(
          nextCurrentDirEntry, selectionEntry, suggestedName);
      callback();
    }.bind(this));
  };

  /**
   * @param {DirectoryEntry} directoryEntry Directory to be opened.
   * @param {Entry=} opt_selectionEntry Entry to be selected.
   * @param {string=} opt_suggestedName Suggested name for a non-existing\
   *     selection.
   * @private
   */
  FileManager.prototype.finishSetupCurrentDirectory_ = function(
      directoryEntry, opt_selectionEntry, opt_suggestedName) {
    // Open the directory, and select the selection (if passed).
    if (util.isFakeEntry(directoryEntry)) {
      this.directoryModel_.specialSearch(directoryEntry.fullPath, '');
    } else {
      this.directoryModel_.changeDirectoryEntry(directoryEntry, function() {
        if (opt_selectionEntry)
          this.directoryModel_.selectEntry(opt_selectionEntry);
      }.bind(this));
    }

    if (this.dialogType == DialogType.FULL_PAGE) {
      // In the FULL_PAGE mode if the restored path points to a file we might
      // have to invoke a task after selecting it.
      if (this.params_.action == 'select')
        return;

      var task = null;
      if (opt_suggestedName) {
        // Non-existent file or a directory.
        if (this.params_.gallery) {
          // Reloading while the Gallery is open with empty or multiple
          // selection. Open the Gallery when the directory is scanned.
          task = function() {
            new FileTasks(this, this.params_).openGallery([]);
          }.bind(this);
        }
      } else if (opt_selectionEntry) {
        // There is a file to be selected. It means, that we are recovering
        // the Files app.
        // We call the appropriate methods of FileTasks directly as we do
        // not need any of the preparations that |execute| method does.
        // TODO(mtomasz): Change Entry.fullPath to Entry.
        var mediaType = FileType.getMediaType(opt_selectionEntry.fullPath);
        if (mediaType == 'image' || mediaType == 'video') {
          task = function() {
            // TODO(mtomasz): Replace the url with an entry.
            new FileTasks(this, this.params_).openGallery([opt_selectionEntry]);
          }.bind(this);
        } else if (mediaType == 'archive') {
          task = function() {
            new FileTasks(this, this.params_).mountArchives(
                [opt_selectionEntry]);
          }.bind(this);
        }
      }

      // If there is a task to be run, run it after the scan is completed.
      if (task) {
        var listener = function() {
          this.directoryModel_.removeEventListener(
              'scan-completed', listener);
          task();
        }.bind(this);
        this.directoryModel_.addEventListener('scan-completed', listener);
      }
    } else if (this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
      this.filenameInput_.value = opt_suggestedName || '';
      this.selectDefaultPathInFilenameInput_();
    }
  };

  /**
   * Unmounts device.
   * @param {string} path Path to a volume to unmount.
   */
  FileManager.prototype.unmountVolume = function(path) {
    var onError = function(error) {
      this.alert.showHtml('', str('UNMOUNT_FAILED'));
    };
    this.volumeManager_.unmount(path, function() {}, onError.bind(this));
  };

  /**
   * @private
   */
  FileManager.prototype.refreshCurrentDirectoryMetadata_ = function() {
    var entries = this.directoryModel_.getFileList().slice();
    var directoryEntry = this.directoryModel_.getCurrentDirEntry();
    if (!directoryEntry)
      return;
    // We don't pass callback here. When new metadata arrives, we have an
    // observer registered to update the UI.

    // TODO(dgozman): refresh content metadata only when modificationTime
    // changed.
    var isFakeEntry = util.isFakeEntry(directoryEntry);
    var getEntries = (isFakeEntry ? [] : [directoryEntry]).concat(entries);
    if (!isFakeEntry)
      this.metadataCache_.clearRecursively(directoryEntry, '*');
    this.metadataCache_.get(getEntries, 'filesystem', null);

    if (this.isOnDrive())
      this.metadataCache_.get(getEntries, 'drive', null);

    var visibleItems = this.currentList_.items;
    var visibleEntries = [];
    for (var i = 0; i < visibleItems.length; i++) {
      var index = this.currentList_.getIndexOfListItem(visibleItems[i]);
      var entry = this.directoryModel_.getFileList().item(index);
      // The following check is a workaround for the bug in list: sometimes item
      // does not have listIndex, and therefore is not found in the list.
      if (entry) visibleEntries.push(entry);
    }
    this.metadataCache_.get(visibleEntries, 'thumbnail', null);
  };

  /**
   * @private
   */
  FileManager.prototype.dailyUpdateModificationTime_ = function() {
    var fileList = this.directoryModel_.getFileList();
    var entries = [];
    for (var i = 0; i < fileList.length; i++) {
      entries.push(fileList.item(i));
    }
    this.metadataCache_.get(
        entries,
        'filesystem',
        this.updateMetadataInUI_.bind(this, 'filesystem', entries));

    setTimeout(this.dailyUpdateModificationTime_.bind(this),
               MILLISECONDS_IN_DAY);
  };

  /**
   * @param {string} type Type of metadata changed.
   * @param {Array.<Entry>} entries Array of entries.
   * @param {Object.<string, Object>} props Map from entry URLs to metadata
   *     props.
   * @private
   */
  FileManager.prototype.updateMetadataInUI_ = function(
      type, entries, properties) {
    if (this.listType_ == FileManager.ListType.DETAIL)
      this.table_.updateListItemsMetadata(type, properties);
    else
      this.grid_.updateListItemsMetadata(type, properties);
    // TODO: update bottom panel thumbnails.
  };

  /**
   * Restore the item which is being renamed while refreshing the file list. Do
   * nothing if no item is being renamed or such an item disappeared.
   *
   * While refreshing file list it gets repopulated with new file entries.
   * There is not a big difference whether DOM items stay the same or not.
   * Except for the item that the user is renaming.
   *
   * @private
   */
  FileManager.prototype.restoreItemBeingRenamed_ = function() {
    if (!this.isRenamingInProgress())
      return;

    var dm = this.directoryModel_;
    var leadIndex = dm.getFileListSelection().leadIndex;
    if (leadIndex < 0)
      return;

    var leadEntry = dm.getFileList().item(leadIndex);
    if (this.renameInput_.currentEntry.fullPath != leadEntry.fullPath)
      return;

    var leadListItem = this.findListItemForNode_(this.renameInput_);
    if (this.currentList_ == this.table_.list) {
      this.table_.updateFileMetadata(leadListItem, leadEntry);
    }
    this.currentList_.restoreLeadItem(leadListItem);
  };

  /**
   * @return {boolean} True if the current directory content is from Google
   *     Drive.
   */
  FileManager.prototype.isOnDrive = function() {
    var rootType = this.directoryModel_.getCurrentRootType();
    return rootType === RootType.DRIVE ||
           rootType === RootType.DRIVE_SHARED_WITH_ME ||
           rootType === RootType.DRIVE_RECENT ||
           rootType === RootType.DRIVE_OFFLINE;
  };

  /**
   * Overrides default handling for clicks on hyperlinks.
   * In a packaged apps links with targer='_blank' open in a new tab by
   * default, other links do not open at all.
   *
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onExternalLinkClick_ = function(event) {
    if (event.target.tagName != 'A' || !event.target.href)
      return;

    if (this.dialogType != DialogType.FULL_PAGE)
      this.onCancel_();
  };

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   * @private
   */
  FileManager.prototype.onTaskItemClicked_ = function(event) {
    var selection = this.getSelection();
    if (!selection.tasks) return;

    if (event.item.task) {
      // Task field doesn't exist on change-default dropdown item.
      selection.tasks.execute(event.item.task.taskId);
    } else {
      var extensions = [];

      for (var i = 0; i < selection.entries.length; i++) {
        var match = /\.(\w+)$/g.exec(selection.entries[i].toURL());
        if (match) {
          var ext = match[1].toUpperCase();
          if (extensions.indexOf(ext) == -1) {
            extensions.push(ext);
          }
        }
      }

      var format = '';

      if (extensions.length == 1) {
        format = extensions[0];
      }

      // Change default was clicked. We should open "change default" dialog.
      selection.tasks.showTaskPicker(this.defaultTaskPicker,
          loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
          strf('CHANGE_DEFAULT_CAPTION', format),
          this.onDefaultTaskDone_.bind(this));
    }
  };

  /**
   * Sets the given task as default, when this task is applicable.
   *
   * @param {Object} task Task to set as default.
   * @private
   */
  FileManager.prototype.onDefaultTaskDone_ = function(task) {
    // TODO(dgozman): move this method closer to tasks.
    var selection = this.getSelection();
    chrome.fileBrowserPrivate.setDefaultTask(
        task.taskId,
        util.entriesToURLs(selection.entries),
        selection.mimeTypes);
    selection.tasks = new FileTasks(this);
    selection.tasks.init(selection.entries, selection.mimeTypes);
    selection.tasks.display(this.taskItems_);
    this.refreshCurrentDirectoryMetadata_();
    this.selectionHandler_.onFileSelectionChanged();
  };

  /**
   * @private
   */
  FileManager.prototype.onPreferencesChanged_ = function() {
    var self = this;
    this.getPreferences_(function(prefs) {
      self.initDateTimeFormatters_();
      self.refreshCurrentDirectoryMetadata_();

      if (prefs.cellularDisabled)
        self.syncButton.setAttribute('checked', '');
      else
        self.syncButton.removeAttribute('checked');

      if (self.hostedButton.hasAttribute('checked') !=
          prefs.hostedFilesDisabled && self.isOnDrive()) {
        self.directoryModel_.rescan();
      }

      if (!prefs.hostedFilesDisabled)
        self.hostedButton.setAttribute('checked', '');
      else
        self.hostedButton.removeAttribute('checked');
    },
    true /* refresh */);
  };

  FileManager.prototype.onDriveConnectionChanged_ = function() {
    var connection = this.volumeManager_.getDriveConnectionState();
    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    if (this.dialogContainer_)
      this.dialogContainer_.setAttribute('connection', connection.type);
    this.shareDialog_.hideWithResult(ShareDialog.Result.NETWORK_ERROR);
    this.suggestAppsDialog.onDriveConnectionChanged(connection.type);
  };

  /**
   * Get the metered status of Drive connection.
   *
   * @return {boolean} Returns true if drive should limit the traffic because
   * the connection is metered and the 'disable-sync-on-metered' setting is
   * enabled. Otherwise, returns false.
   */
  FileManager.prototype.isDriveOnMeteredConnection = function() {
    var connection = this.volumeManager_.getDriveConnectionState();
    return connection.type == util.DriveConnectionType.METERED;
  };

  /**
   * Get the online/offline status of drive.
   *
   * @return {boolean} Returns true if the connection is offline. Otherwise,
   * returns false.
   */
  FileManager.prototype.isDriveOffline = function() {
    var connection = this.volumeManager_.getDriveConnectionState();
    return connection.type == util.DriveConnectionType.OFFLINE;
  };

  FileManager.prototype.isOnReadonlyDirectory = function() {
    return this.directoryModel_.isReadOnly();
  };

  /**
   * @param {Event} Unmount event.
   * @private
   */
  FileManager.prototype.onExternallyUnmounted_ = function(event) {
    if (event.mountPath == this.directoryModel_.getCurrentRootPath()) {
      if (this.closeOnUnmount_) {
        // If the file manager opened automatically when a usb drive inserted,
        // user have never changed current volume (that implies the current
        // directory is still on the device) then close this window.
        window.close();
      }
    }
  };

  /**
   * Show a modal-like file viewer/editor on top of the File Manager UI.
   *
   * @param {HTMLElement} popup Popup element.
   * @param {function()} closeCallback Function to call after the popup is
   *     closed.
   */
  FileManager.prototype.openFilePopup = function(popup, closeCallback) {
    this.closeFilePopup();
    this.filePopup_ = popup;
    this.filePopupCloseCallback_ = closeCallback;
    this.dialogDom_.insertBefore(
        this.filePopup_, this.dialogDom_.querySelector('#iframe-drag-area'));
    this.filePopup_.focus();
    this.document_.body.setAttribute('overlay-visible', '');
    this.document_.querySelector('#iframe-drag-area').hidden = false;
  };

  /**
   * Closes the modal-like file viewer/editor popup.
   */
  FileManager.prototype.closeFilePopup = function() {
    if (this.filePopup_) {
      this.document_.body.removeAttribute('overlay-visible');
      this.document_.querySelector('#iframe-drag-area').hidden = true;
      // The window resize would not be processed properly while the relevant
      // divs had 'display:none', force resize after the layout fired.
      setTimeout(this.onResize_.bind(this), 0);
      if (this.filePopup_.contentWindow &&
          this.filePopup_.contentWindow.unload) {
        this.filePopup_.contentWindow.unload();
      }

      if (this.filePopupCloseCallback_) {
        this.filePopupCloseCallback_();
        this.filePopupCloseCallback_ = null;
      }

      // These operations have to be in the end, otherwise v8 crashes on an
      // assert. See: crbug.com/224174.
      this.dialogDom_.removeChild(this.filePopup_);
      this.filePopup_ = null;
    }
  };

  /**
   * Updates visibility of the draggable app region in the modal-like file
   * viewer/editor.
   *
   * @param {boolean} visible True for visible, false otherwise.
   */
  FileManager.prototype.onFilePopupAppRegionChanged = function(visible) {
    if (!this.filePopup_)
      return;

    this.document_.querySelector('#iframe-drag-area').hidden = !visible;
  };

  /**
   * @return {Array.<Entry>} List of all entries in the current directory.
   */
  FileManager.prototype.getAllEntriesInCurrentDirectory = function() {
    return this.directoryModel_.getFileList().slice();
  };

  FileManager.prototype.isRenamingInProgress = function() {
    return !!this.renameInput_.currentEntry;
  };

  /**
   * @private
   */
  FileManager.prototype.focusCurrentList_ = function() {
    if (this.listType_ == FileManager.ListType.DETAIL)
      this.table_.focus();
    else  // this.listType_ == FileManager.ListType.THUMBNAIL)
      this.grid_.focus();
  };

  /**
   * Return full path of the current directory or null.
   * @return {?string} The full path of the current directory.
   */
  FileManager.prototype.getCurrentDirectory = function() {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirPath();
  };

  /**
   * Return URL of the current directory or null.
   * @return {string} URL representing the current directory.
   */
  FileManager.prototype.getCurrentDirectoryURL = function() {
    return this.directoryModel_ &&
           this.directoryModel_.getCurrentDirectoryURL();
  };

  /**
   * Return DirectoryEntry of the current directory or null.
   * @return {DirectoryEntry} DirectoryEntry of the current directory. Returns
   *     null if the directory model is not ready or the current directory is
   *     not set.
   */
  FileManager.prototype.getCurrentDirectoryEntry = function() {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirEntry();
  };

  /**
   * Deletes the selected file and directories recursively.
   */
  FileManager.prototype.deleteSelection = function() {
    // TODO(mtomasz): Remove this temporary dialog. crbug.com/167364
    var entries = this.getSelection().entries;
    var message = entries.length == 1 ?
        strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
        strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);
    this.confirm.show(message, function() {
      this.fileOperationManager_.deleteEntries(entries);
    }.bind(this));
  };

  /**
   * Shows the share dialog for the selected file or directory.
   */
  FileManager.prototype.shareSelection = function() {
    var entries = this.getSelection().entries;
    if (entries.length != 1) {
      console.warn('Unable to share multiple items at once.');
      return;
    }
    // Add the overlapped class to prevent the applicaiton window from
    // captureing mouse events.
    this.shareDialog_.show(entries[0], function(result) {
      if (result == ShareDialog.Result.NETWORK_ERROR)
        this.error.show(str('SHARE_ERROR'));
    }.bind(this));
  };

  /**
   * Creates a folder shortcut.
   * @param {string} path A shortcut which refers to |path| to be created.
   */
  FileManager.prototype.createFolderShortcut = function(path) {
    // Duplicate entry.
    if (this.folderShortcutExists(path))
      return;

    this.folderShortcutsModel_.add(path);
  };

  /**
   * Checkes if the shortcut which refers to the given folder exists or not.
   * @param {string} path Path of the folder to be checked.
   */
  FileManager.prototype.folderShortcutExists = function(path) {
    return this.folderShortcutsModel_.exists(path);
  };

  /**
   * Removes the folder shortcut.
   * @param {string} path The shortcut which refers to |path| is to be removed.
   */
  FileManager.prototype.removeFolderShortcut = function(path) {
    this.folderShortcutsModel_.remove(path);
  };

  /**
   * Blinks the selection. Used to give feedback when copying or cutting the
   * selection.
   */
  FileManager.prototype.blinkSelection = function() {
    var selection = this.getSelection();
    if (!selection || selection.totalCount == 0)
      return;

    for (var i = 0; i < selection.entries.length; i++) {
      var selectedIndex = selection.indexes[i];
      var listItem = this.currentList_.getListItemByIndex(selectedIndex);
      if (listItem)
        this.blinkListItem_(listItem);
    }
  };

  /**
   * @param {Element} listItem List item element.
   * @private
   */
  FileManager.prototype.blinkListItem_ = function(listItem) {
    listItem.classList.add('blink');
    setTimeout(function() {
      listItem.classList.remove('blink');
    }, 100);
  };

  /**
   * @private
   */
  FileManager.prototype.selectDefaultPathInFilenameInput_ = function() {
    var input = this.filenameInput_;
    input.focus();
    var selectionEnd = input.value.lastIndexOf('.');
    if (selectionEnd == -1) {
      input.select();
    } else {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    }
    // Clear, so we never do this again.
    this.defaultPath = '';
  };

  /**
   * Handles mouse click or tap.
   *
   * @param {Event} event The click event.
   * @private
   */
  FileManager.prototype.onDetailClick_ = function(event) {
    if (this.isRenamingInProgress()) {
      // Don't pay attention to clicks during a rename.
      return;
    }

    var listItem = this.findListItemForEvent_(event);
    var selection = this.getSelection();
    if (!listItem || !listItem.selected || selection.totalCount != 1) {
      return;
    }

    // React on double click, but only if both clicks hit the same item.
    // TODO(mtomasz): Simplify it, and use a double click handler if possible.
    var clickNumber = (this.lastClickedItem_ == listItem) ? 2 : undefined;
    this.lastClickedItem_ = listItem;

    if (event.detail != clickNumber)
      return;

    var entry = selection.entries[0];
    if (entry.isDirectory) {
      this.onDirectoryAction_(entry);
    } else {
      this.dispatchSelectionAction_();
    }
  };

  /**
   * @private
   */
  FileManager.prototype.dispatchSelectionAction_ = function() {
    if (this.dialogType == DialogType.FULL_PAGE) {
      var selection = this.getSelection();
      var tasks = selection.tasks;
      var urls = selection.urls;
      var mimeTypes = selection.mimeTypes;
      if (tasks)
        tasks.executeDefault();
      return true;
    }
    if (!this.okButton_.disabled) {
      this.onOk_();
      return true;
    }
    return false;
  };

  /**
   * Opens the suggest file dialog.
   *
   * @param {Entry} entry Entry of the file.
   * @param {function()} onSuccess Success callback.
   * @param {function()} onCancelled User-cancelled callback.
   * @param {function()} onFailure Failure callback.
   * @private
   */
  FileManager.prototype.openSuggestAppsDialog =
      function(entry, onSuccess, onCancelled, onFailure) {
    if (!url) {
      onFailure();
      return;
    }

    this.metadataCache_.get([entry], 'drive', function(props) {
      if (!props || !props[0] || !props[0].contentMimeType) {
        onFailure();
        return;
      }

      var basename = entry.name;
      var splitted = PathUtil.splitExtension(basename);
      var filename = splitted[0];
      var extension = splitted[1];
      var mime = props[0].contentMimeType;

      // Returns with failure if the file has neither extension nor mime.
      if (!extension || !mime) {
        onFailure();
        return;
      }

      var onDialogClosed = function(result) {
        switch (result) {
          case SuggestAppsDialog.Result.INSTALL_SUCCESSFUL:
            onSuccess();
            break;
          case SuggestAppsDialog.Result.FAILED:
            onFailure();
            break;
          default:
            onCancelled();
        }
      };

      if (FileTasks.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1) {
        this.suggestAppsDialog.showByFilename(filename, onDialogClosed);
      } else {
        this.suggestAppsDialog.showByExtensionAndMime(
            extension, mime, onDialogClosed);
      }
    }.bind(this));
  };

  /**
   * Called when a dialog is shown or hidden.
   * @param {boolean} flag True if a dialog is shown, false if hidden.   */
  FileManager.prototype.onDialogShownOrHidden = function(show) {
    // Set/unset a flag to disable dragging on the title area.
    this.dialogContainer_.classList.toggle('disable-header-drag', show);
  };

  /**
   * Executes directory action (i.e. changes directory).
   *
   * @param {DirectoryEntry} entry Directory entry to which directory should be
   *                               changed.
   * @private
   */
  FileManager.prototype.onDirectoryAction_ = function(entry) {
    return this.directoryModel_.changeDirectory(entry.fullPath);
  };

  /**
   * Update the window title.
   * @private
   */
  FileManager.prototype.updateTitle_ = function() {
    if (this.dialogType != DialogType.FULL_PAGE)
      return;

    var path = this.getCurrentDirectory();
    var rootPath = PathUtil.getRootPath(path);
    this.document_.title = PathUtil.getRootLabel(rootPath) +
                           path.substring(rootPath.length);
  };

  /**
   * Update the gear menu.
   * @private
   */
  FileManager.prototype.updateGearMenu_ = function() {
    var hideItemsForDrive = !this.isOnDrive();
    this.syncButton.hidden = hideItemsForDrive;
    this.hostedButton.hidden = hideItemsForDrive;
    this.document_.getElementById('drive-separator').hidden =
        hideItemsForDrive;

    // If volume has changed, then fetch remaining space data.
    if (this.previousRootUrl_ != this.directoryModel_.getCurrentMountPointUrl())
      this.refreshRemainingSpace_(true);  // Show loading caption.

    this.previousRootUrl_ = this.directoryModel_.getCurrentMountPointUrl();
  };

  /**
   * Refreshes space info of the current volume.
   * @param {boolean} showLoadingCaption Whether show loading caption or not.
   * @private
   */
  FileManager.prototype.refreshRemainingSpace_ = function(showLoadingCaption) {
    var volumeSpaceInfoLabel =
        this.dialogDom_.querySelector('#volume-space-info-label');
    var volumeSpaceInnerBar =
        this.dialogDom_.querySelector('#volume-space-info-bar');
    var volumeSpaceOuterBar =
        this.dialogDom_.querySelector('#volume-space-info-bar').parentNode;

    volumeSpaceInnerBar.setAttribute('pending', '');

    if (showLoadingCaption) {
      volumeSpaceInfoLabel.innerText = str('WAITING_FOR_SPACE_INFO');
      volumeSpaceInnerBar.style.width = '100%';
    }

    var currentMountPointUrl = this.directoryModel_.getCurrentMountPointUrl();
    chrome.fileBrowserPrivate.getSizeStats(
        currentMountPointUrl, function(result) {
          if (this.directoryModel_.getCurrentMountPointUrl() !=
              currentMountPointUrl)
            return;
          updateSpaceInfo(result,
                          volumeSpaceInnerBar,
                          volumeSpaceInfoLabel,
                          volumeSpaceOuterBar);
        }.bind(this));
  };

  /**
   * Update the UI when the current directory changes.
   *
   * @param {Event} event The directory-changed event.
   * @private
   */
  FileManager.prototype.onDirectoryChanged_ = function(event) {
    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.searchBox.clear();
    util.updateAppState(this.getCurrentDirectory());

    // If the current directory is moved from the device's volume, do not
    // automatically close the window on device removal.
    if (event.previousDirEntry &&
        PathUtil.getRootPath(event.previousDirEntry.fullPath) !=
            PathUtil.getRootPath(event.newDirEntry.fullPath))
      this.closeOnUnmount_ = false;

    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    this.updateUnformattedVolumeStatus_();
    this.updateTitle_();
    this.updateGearMenu_();
    var currentEntry = this.getCurrentDirectoryEntry();
    this.previewPanel_.currentEntry = util.isFakeEntry(currentEntry) ?
        null : currentEntry;
  };

  FileManager.prototype.updateUnformattedVolumeStatus_ = function() {
    var volumeInfo = this.volumeManager_.getVolumeInfo(
        this.directoryModel_.getCurrentDirEntry());

    if (volumeInfo && volumeInfo.error) {
      this.dialogDom_.setAttribute('unformatted', '');

      var errorNode = this.dialogDom_.querySelector('#format-panel > .error');
      if (volumeInfo.error == util.VolumeError.UNSUPPORTED_FILESYSTEM) {
        errorNode.textContent = str('UNSUPPORTED_FILESYSTEM_WARNING');
      } else {
        errorNode.textContent = str('UNKNOWN_FILESYSTEM_WARNING');
      }

      // Update 'canExecute' for format command so the format button's disabled
      // property is properly set.
      if (this.commandHandler)
        this.commandHandler.updateAvailability();
    } else {
      this.dialogDom_.removeAttribute('unformatted');
    }
  };

  FileManager.prototype.findListItemForEvent_ = function(event) {
    return this.findListItemForNode_(event.touchedElement || event.srcElement);
  };

  FileManager.prototype.findListItemForNode_ = function(node) {
    var item = this.currentList_.getListItemAncestor(node);
    // TODO(serya): list should check that.
    return item && this.currentList_.isItem(item) ? item : null;
  };

  /**
   * Unload handler for the page.  May be called manually for the file picker
   * dialog, because it closes by calling extension API functions that do not
   * return.
   *
   * TODO(hirono): This method is not called when Files.app is opend as a dialog
   *     and is closed by the close button in the dialog frame. crbug.com/309967
   * @private
   */
  FileManager.prototype.onUnload_ = function() {
    if (this.directoryModel_)
      this.directoryModel_.dispose();
    if (this.volumeManager_)
      this.volumeManager_.dispose();
    if (this.filePopup_ &&
        this.filePopup_.contentWindow &&
        this.filePopup_.contentWindow.unload)
      this.filePopup_.contentWindow.unload(true /* exiting */);
    if (this.progressCenterPanel_)
      this.backgroundPage_.background.progressCenter.removePanel(
          this.progressCenterPanel_);
    if (this.fileOperationManager_) {
      if (this.onCopyProgressBound_) {
        this.fileOperationManager_.removeEventListener(
            'copy-progress', this.onCopyProgressBound_);
      }
      if (this.onEntryChangedBound_) {
        this.fileOperationManager_.removeEventListener(
            'entry-changed', this.onEntryChangedBound_);
      }
    }
    window.closing = true;
    if (this.backgroundPage_ && util.platform.runningInBrowser())
      this.backgroundPage_.background.tryClose();
  };

  FileManager.prototype.initiateRename = function() {
    var item = this.currentList_.ensureLeadItemExists();
    if (!item)
      return;
    var label = item.querySelector('.filename-label');
    var input = this.renameInput_;

    input.value = label.textContent;
    label.parentNode.setAttribute('renaming', '');
    label.parentNode.appendChild(input);
    input.focus();
    var selectionEnd = input.value.lastIndexOf('.');
    if (selectionEnd == -1) {
      input.select();
    } else {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    }

    // This has to be set late in the process so we don't handle spurious
    // blur events.
    input.currentEntry = this.currentList_.dataModel.item(item.listIndex);
  };

  /**
   * @type {Event} Key event.
   * @private
   */
  FileManager.prototype.onRenameInputKeyDown_ = function(event) {
    if (!this.isRenamingInProgress())
      return;

    // Do not move selection or lead item in list during rename.
    if (event.keyIdentifier == 'Up' || event.keyIdentifier == 'Down') {
      event.stopPropagation();
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '27':  // Escape
        this.cancelRename_();
        event.preventDefault();
        break;

      case '13':  // Enter
        this.commitRename_();
        event.preventDefault();
        break;
    }
  };

  /**
   * @type {Event} Blur event.
   * @private
   */
  FileManager.prototype.onRenameInputBlur_ = function(event) {
    if (this.isRenamingInProgress() && !this.renameInput_.validation_)
      this.commitRename_();
  };

  /**
   * @private
   */
  FileManager.prototype.commitRename_ = function() {
    var input = this.renameInput_;
    var entry = input.currentEntry;
    var newName = input.value;

    if (newName == entry.name) {
      this.cancelRename_();
      return;
    }

    var nameNode = this.findListItemForNode_(this.renameInput_).
                   querySelector('.filename-label');

    input.validation_ = true;
    var validationDone = function(valid) {
      input.validation_ = false;
      // Alert dialog restores focus unless the item removed from DOM.
      if (this.document_.activeElement != input)
        this.cancelRename_();
      if (!valid)
        return;

      // Validation succeeded. Do renaming.

      this.cancelRename_();
      // Optimistically apply new name immediately to avoid flickering in
      // case of success.
      nameNode.textContent = newName;

      util.rename(
          entry, newName,
          function(newEntry) {
            this.directoryModel_.onRenameEntry(entry, newEntry);
          }.bind(this),
          function(error) {
            // Write back to the old name.
            nameNode.textContent = entry.name;

            // Show error dialog.
            var message;
            if (error.code == FileError.PATH_EXISTS_ERR ||
                error.code == FileError.TYPE_MISMATCH_ERR) {
              // Check the existing entry is file or not.
              // 1) If the entry is a file:
              //   a) If we get PATH_EXISTS_ERR, a file exists.
              //   b) If we get TYPE_MISMATCH_ERR, a directory exists.
              // 2) If the entry is a directory:
              //   a) If we get PATH_EXISTS_ERR, a directory exists.
              //   b) If we get TYPE_MISMATCH_ERR, a file exists.
              message = strf(
                  (entry.isFile && error.code == FileError.PATH_EXISTS_ERR) ||
                  (!entry.isFile && error.code == FileError.TYPE_MISMATCH_ERR) ?
                      'FILE_ALREADY_EXISTS' :
                      'DIRECTORY_ALREADY_EXISTS',
                  newName);
            } else {
              message = strf('ERROR_RENAMING', entry.name,
                             util.getFileErrorString(err.code));
            }

            this.alert.show(message);
          }.bind(this));
    };

    // TODO(haruki): this.getCurrentDirectoryURL() might not return the actual
    // parent if the directory content is a search result. Fix it to do proper
    // validation.
    this.validateFileName_(this.getCurrentDirectoryURL(),
                           newName,
                           validationDone.bind(this));
  };

  /**
   * @private
   */
  FileManager.prototype.cancelRename_ = function() {
    this.renameInput_.currentEntry = null;

    var parent = this.renameInput_.parentNode;
    if (parent) {
      parent.removeAttribute('renaming');
      parent.removeChild(this.renameInput_);
    }
  };

  /**
   * @param {Event} Key event.
   * @private
   */
  FileManager.prototype.onFilenameInputInput_ = function() {
    this.selectionHandler_.updateOkButton();
  };

  /**
   * @param {Event} Key event.
   * @private
   */
  FileManager.prototype.onFilenameInputKeyDown_ = function(event) {
    if ((util.getKeyModifiers(event) + event.keyCode) === '13' /* Enter */)
      this.okButton_.click();
  };

  /**
   * @param {Event} Focus event.
   * @private
   */
  FileManager.prototype.onFilenameInputFocus_ = function(event) {
    var input = this.filenameInput_;

    // On focus we want to select everything but the extension, but
    // Chrome will select-all after the focus event completes.  We
    // schedule a timeout to alter the focus after that happens.
    setTimeout(function() {
        var selectionEnd = input.value.lastIndexOf('.');
        if (selectionEnd == -1) {
          input.select();
        } else {
          input.selectionStart = 0;
          input.selectionEnd = selectionEnd;
        }
    }, 0);
  };

  /**
   * @private
   */
  FileManager.prototype.onScanStarted_ = function() {
    if (this.scanInProgress_) {
      this.table_.list.endBatchUpdates();
      this.grid_.endBatchUpdates();
    }

    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    this.table_.list.startBatchUpdates();
    this.grid_.startBatchUpdates();
    this.scanInProgress_ = true;

    this.scanUpdatedAtLeastOnceOrCompleted_ = false;
    if (this.scanCompletedTimer_) {
      clearTimeout(this.scanCompletedTimer_);
      this.scanCompletedTimer_ = null;
    }

    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = null;
    }

    if (this.spinner_.hidden) {
      this.cancelSpinnerTimeout_();
      this.showSpinnerTimeout_ =
          setTimeout(this.showSpinner_.bind(this, true), 500);
    }
  };

  /**
   * @private
   */
  FileManager.prototype.onScanCompleted_ = function() {
    if (!this.scanInProgress_) {
      console.error('Scan-completed event recieved. But scan is not started.');
      return;
    }

    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    this.hideSpinnerLater_();

    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = null;
    }

    // To avoid flickering postpone updating the ui by a small amount of time.
    // There is a high chance, that metadata will be received within 50 ms.
    this.scanCompletedTimer_ = setTimeout(function() {
      // Check if batch updates are already finished by onScanUpdated_().
      if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
        this.scanUpdatedAtLeastOnceOrCompleted_ = true;
        this.updateMiddleBarVisibility_();
      }

      this.scanInProgress_ = false;
      this.table_.list.endBatchUpdates();
      this.grid_.endBatchUpdates();
      this.scanCompletedTimer_ = null;
    }.bind(this), 50);
  };

  /**
   * @private
   */
  FileManager.prototype.onScanUpdated_ = function() {
    if (!this.scanInProgress_) {
      console.error('Scan-updated event recieved. But scan is not started.');
      return;
    }

    if (this.scanUpdatedTimer_ || this.scanCompletedTimer_)
      return;

    // Show contents incrementally by finishing batch updated, but only after
    // 200ms elapsed, to avoid flickering when it is not necessary.
    this.scanUpdatedTimer_ = setTimeout(function() {
      // We need to hide the spinner only once.
      if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
        this.scanUpdatedAtLeastOnceOrCompleted_ = true;
        this.hideSpinnerLater_();
        this.updateMiddleBarVisibility_();
      }

      // Update the UI.
      if (this.scanInProgress_) {
        this.table_.list.endBatchUpdates();
        this.grid_.endBatchUpdates();
        this.table_.list.startBatchUpdates();
        this.grid_.startBatchUpdates();
      }
      this.scanUpdatedTimer_ = null;
    }.bind(this), 200);
  };

  /**
   * @private
   */
  FileManager.prototype.onScanCancelled_ = function() {
    if (!this.scanInProgress_) {
      console.error('Scan-cancelled event recieved. But scan is not started.');
      return;
    }

    if (this.commandHandler)
      this.commandHandler.updateAvailability();
    this.hideSpinnerLater_();
    if (this.scanCompletedTimer_) {
      clearTimeout(this.scanCompletedTimer_);
      this.scanCompletedTimer_ = null;
    }
    if (this.scanUpdatedTimer_) {
      clearTimeout(this.scanUpdatedTimer_);
      this.scanUpdatedTimer_ = null;
    }
    // Finish unfinished batch updates.
    if (!this.scanUpdatedAtLeastOnceOrCompleted_) {
      this.scanUpdatedAtLeastOnceOrCompleted_ = true;
      this.updateMiddleBarVisibility_();
    }

    this.scanInProgress_ = false;
    this.table_.list.endBatchUpdates();
    this.grid_.endBatchUpdates();
  };

  /**
   * Handle the 'rescan-completed' from the DirectoryModel.
   * @private
   */
  FileManager.prototype.onRescanCompleted_ = function() {
    this.selectionHandler_.onFileSelectionChanged();
  };

  /**
   * @private
   */
  FileManager.prototype.cancelSpinnerTimeout_ = function() {
    if (this.showSpinnerTimeout_) {
      clearTimeout(this.showSpinnerTimeout_);
      this.showSpinnerTimeout_ = null;
    }
  };

  /**
   * @private
   */
  FileManager.prototype.hideSpinnerLater_ = function() {
    this.cancelSpinnerTimeout_();
    this.showSpinner_(false);
  };

  /**
   * @param {boolean} on True to show, false to hide.
   * @private
   */
  FileManager.prototype.showSpinner_ = function(on) {
    if (on && this.directoryModel_ && this.directoryModel_.isScanning())
      this.spinner_.hidden = false;

    if (!on && (!this.directoryModel_ ||
                !this.directoryModel_.isScanning() ||
                this.directoryModel_.getFileList().length != 0)) {
      this.spinner_.hidden = true;
    }
  };

  FileManager.prototype.createNewFolder = function() {
    var defaultName = str('DEFAULT_NEW_FOLDER_NAME');

    // Find a name that doesn't exist in the data model.
    var files = this.directoryModel_.getFileList();
    var hash = {};
    for (var i = 0; i < files.length; i++) {
      var name = files.item(i).name;
      // Filtering names prevents from conflicts with prototype's names
      // and '__proto__'.
      if (name.substring(0, defaultName.length) == defaultName)
        hash[name] = 1;
    }

    var baseName = defaultName;
    var separator = '';
    var suffix = '';
    var index = '';

    var advance = function() {
      separator = ' (';
      suffix = ')';
      index++;
    };

    var current = function() {
      return baseName + separator + index + suffix;
    };

    // Accessing hasOwnProperty is safe since hash properties filtered.
    while (hash.hasOwnProperty(current())) {
      advance();
    }

    var self = this;
    var list = self.currentList_;
    var tryCreate = function() {
      self.directoryModel_.createDirectory(current(),
                                           onSuccess, onError);
    };

    var onSuccess = function(entry) {
      metrics.recordUserAction('CreateNewFolder');
      list.selectedItem = entry;
      self.initiateRename();
    };

    var onError = function(error) {
      self.alert.show(strf('ERROR_CREATING_FOLDER', current(),
                           util.getFileErrorString(error.code)));
    };

    tryCreate();
  };

  /**
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onDetailViewButtonClick_ = function(event) {
    // Stop propagate and hide the menu manually, in order to prevent the focus
    // from being back to the button. (cf. http://crbug.com/248479)
    event.stopPropagation();
    this.gearButton_.hideMenu();

    this.setListType(FileManager.ListType.DETAIL);
    this.currentList_.focus();
  };

  /**
   * @param {Event} event Click event.
   * @private
   */
  FileManager.prototype.onThumbnailViewButtonClick_ = function(event) {
    // Stop propagate and hide the menu manually, in order to prevent the focus
    // from being back to the button. (cf. http://crbug.com/248479)
    event.stopPropagation();
    this.gearButton_.hideMenu();

    this.setListType(FileManager.ListType.THUMBNAIL);
    this.currentList_.focus();
  };

  /**
   * KeyDown event handler for the document.
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onKeyDown_ = function(event) {
    if (event.srcElement === this.renameInput_) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case 'Ctrl-190':  // Ctrl-. => Toggle filter files.
        this.fileFilter_.setFilterHidden(
            !this.fileFilter_.isFilterHiddenOn());
        event.preventDefault();
        return;

      case '27':  // Escape => Cancel dialog.
        if (this.dialogType != DialogType.FULL_PAGE) {
          // If there is nothing else for ESC to do, then cancel the dialog.
          event.preventDefault();
          this.cancelButton_.click();
        }
        break;
    }
  };

  /**
   * KeyDown event handler for the div#list-container element.
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onListKeyDown_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.keyCode) {
      case '8':  // Backspace => Up one directory.
        event.preventDefault();
        var path = this.getCurrentDirectory();
        if (path && !PathUtil.isRootPath(path)) {
          var path = path.replace(/\/[^\/]+$/, '');
          this.directoryModel_.changeDirectory(path);
        }
        break;

      case '13':  // Enter => Change directory or perform default action.
        // TODO(dgozman): move directory action to dispatchSelectionAction.
        var selection = this.getSelection();
        if (selection.totalCount == 1 &&
            selection.entries[0].isDirectory &&
            !DialogType.isFolderDialog(this.dialogType)) {
          event.preventDefault();
          this.onDirectoryAction_(selection.entries[0]);
        } else if (this.dispatchSelectionAction_()) {
          event.preventDefault();
        }
        break;
    }

    switch (event.keyIdentifier) {
      case 'Home':
      case 'End':
      case 'Up':
      case 'Down':
      case 'Left':
      case 'Right':
        // When navigating with keyboard we hide the distracting mouse hover
        // highlighting until the user moves the mouse again.
        this.setNoHover_(true);
        break;
    }
  };

  /**
   * Suppress/restore hover highlighting in the list container.
   * @param {boolean} on True to temporarity hide hover state.
   * @private
   */
  FileManager.prototype.setNoHover_ = function(on) {
    if (on) {
      this.listContainer_.classList.add('nohover');
    } else {
      this.listContainer_.classList.remove('nohover');
    }
  };

  /**
   * KeyPress event handler for the div#list-container element.
   * @param {Event} event Key event.
   * @private
   */
  FileManager.prototype.onListKeyPress_ = function(event) {
    if (event.srcElement.tagName == 'INPUT') {
      // Ignore keypress handler in the rename input box.
      return;
    }

    if (event.ctrlKey || event.metaKey || event.altKey)
      return;

    var now = new Date();
    var char = String.fromCharCode(event.charCode).toLowerCase();
    var text = now - this.textSearchState_.date > 1000 ? '' :
        this.textSearchState_.text;
    this.textSearchState_ = {text: text + char, date: now};

    this.doTextSearch_();
  };

  /**
   * Mousemove event handler for the div#list-container element.
   * @param {Event} event Mouse event.
   * @private
   */
  FileManager.prototype.onListMouseMove_ = function(event) {
    // The user grabbed the mouse, restore the hover highlighting.
    this.setNoHover_(false);
  };

  /**
   * Performs a 'text search' - selects a first list entry with name
   * starting with entered text (case-insensitive).
   * @private
   */
  FileManager.prototype.doTextSearch_ = function() {
    var text = this.textSearchState_.text;
    if (!text)
      return;

    var dm = this.directoryModel_.getFileList();
    for (var index = 0; index < dm.length; ++index) {
      var name = dm.item(index).name;
      if (name.substring(0, text.length).toLowerCase() == text) {
        this.currentList_.selectionModel.selectedIndexes = [index];
        return;
      }
    }

    this.textSearchState_.text = '';
  };

  /**
   * Handle a click of the cancel button.  Closes the window.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   *
   * @param {Event} event The click event.
   * @private
   */
  FileManager.prototype.onCancel_ = function(event) {
    chrome.fileBrowserPrivate.cancelDialog();
    this.onUnload_();
    window.close();
  };

  /**
   * Resolves selected file urls returned from an Open dialog.
   *
   * For drive files this involves some special treatment.
   * Starts getting drive files if needed.
   *
   * @param {Array.<string>} fileUrls Drive URLs.
   * @param {function(Array.<string>)} callback To be called with fixed URLs.
   * @private
   */
  FileManager.prototype.resolveSelectResults_ = function(fileUrls, callback) {
    if (this.isOnDrive()) {
      chrome.fileBrowserPrivate.getDriveFiles(
        fileUrls,
        function(localPaths) {
          callback(fileUrls);
        });
    } else {
      callback(fileUrls);
    }
  };

  /**
   * Closes this modal dialog with some files selected.
   * TODO(jamescook): Make unload handler work automatically, crbug.com/104811
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   * @private
   */
  FileManager.prototype.callSelectFilesApiAndClose_ = function(selection) {
    var self = this;
    function callback() {
      self.onUnload_();
      window.close();
    }
    if (selection.multiple) {
      chrome.fileBrowserPrivate.selectFiles(
          selection.urls, this.params_.shouldReturnLocalPath, callback);
    } else {
      var forOpening = (this.dialogType != DialogType.SELECT_SAVEAS_FILE);
      chrome.fileBrowserPrivate.selectFile(
          selection.urls[0], selection.filterIndex, forOpening,
          this.params_.shouldReturnLocalPath, callback);
    }
  };

  /**
   * Tries to close this modal dialog with some files selected.
   * Performs preprocessing if needed (e.g. for Drive).
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   * @private
   */
  FileManager.prototype.selectFilesAndClose_ = function(selection) {
    if (!this.isOnDrive() ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
      setTimeout(this.callSelectFilesApiAndClose_.bind(this, selection), 0);
      return;
    }

    var shade = this.document_.createElement('div');
    shade.className = 'shade';
    var footer = this.dialogDom_.querySelector('.button-panel');
    var progress = footer.querySelector('.progress-track');
    progress.style.width = '0%';
    var cancelled = false;

    var progressMap = {};
    var filesStarted = 0;
    var filesTotal = selection.urls.length;
    for (var index = 0; index < selection.urls.length; index++) {
      progressMap[selection.urls[index]] = -1;
    }
    var lastPercent = 0;
    var bytesTotal = 0;
    var bytesDone = 0;

    var onFileTransfersUpdated = function(statusList) {
      for (var index = 0; index < statusList.length; index++) {
        var status = statusList[index];
        var escaped = encodeURI(status.fileUrl);
        if (!(escaped in progressMap)) continue;
        if (status.total == -1) continue;

        var old = progressMap[escaped];
        if (old == -1) {
          // -1 means we don't know file size yet.
          bytesTotal += status.total;
          filesStarted++;
          old = 0;
        }
        bytesDone += status.processed - old;
        progressMap[escaped] = status.processed;
      }

      var percent = bytesTotal == 0 ? 0 : bytesDone / bytesTotal;
      // For files we don't have information about, assume the progress is zero.
      percent = percent * filesStarted / filesTotal * 100;
      // Do not decrease the progress. This may happen, if first downloaded
      // file is small, and the second one is large.
      lastPercent = Math.max(lastPercent, percent);
      progress.style.width = lastPercent + '%';
    }.bind(this);

    var setup = function() {
      this.document_.querySelector('.dialog-container').appendChild(shade);
      setTimeout(function() { shade.setAttribute('fadein', 'fadein') }, 100);
      footer.setAttribute('progress', 'progress');
      this.cancelButton_.removeEventListener('click', this.onCancelBound_);
      this.cancelButton_.addEventListener('click', onCancel);
      chrome.fileBrowserPrivate.onFileTransfersUpdated.addListener(
          onFileTransfersUpdated);
    }.bind(this);

    var cleanup = function() {
      shade.parentNode.removeChild(shade);
      footer.removeAttribute('progress');
      this.cancelButton_.removeEventListener('click', onCancel);
      this.cancelButton_.addEventListener('click', this.onCancelBound_);
      chrome.fileBrowserPrivate.onFileTransfersUpdated.removeListener(
          onFileTransfersUpdated);
    }.bind(this);

    var onCancel = function() {
      cancelled = true;
      // According to API cancel may fail, but there is no proper UI to reflect
      // this. So, we just silently assume that everything is cancelled.
      chrome.fileBrowserPrivate.cancelFileTransfers(
          selection.urls, function(response) {});
      cleanup();
    }.bind(this);

    var onResolved = function(resolvedUrls) {
      if (cancelled) return;
      cleanup();
      selection.urls = resolvedUrls;
      // Call next method on a timeout, as it's unsafe to
      // close a window from a callback.
      setTimeout(this.callSelectFilesApiAndClose_.bind(this, selection), 0);
    }.bind(this);

    var onProperties = function(properties) {
      for (var i = 0; i < properties.length; i++) {
        if (!properties[i] || properties[i].present) {
          // For files already in GCache, we don't get any transfer updates.
          filesTotal--;
        }
      }
      this.resolveSelectResults_(selection.urls, onResolved);
    }.bind(this);

    setup();

    // TODO(mtomasz): Use Entry instead of URLs, if possible.
    util.URLsToEntries(selection.urls, function(entries) {
      this.metadataCache_.get(entries, 'drive', onProperties);
    }.bind(this));
  };

  /**
   * Handle a click of the ok button.
   *
   * The ok button has different UI labels depending on the type of dialog, but
   * in code it's always referred to as 'ok'.
   *
   * @param {Event} event The click event.
   * @private
   */
  FileManager.prototype.onOk_ = function(event) {
    if (this.dialogType == DialogType.SELECT_SAVEAS_FILE) {
      // Save-as doesn't require a valid selection from the list, since
      // we're going to take the filename from the text input.
      var filename = this.filenameInput_.value;
      if (!filename)
        throw new Error('Missing filename!');

      var directory = this.getCurrentDirectoryEntry();
      var currentDirUrl = directory.toURL();
      if (currentDirUrl.charAt(currentDirUrl.length - 1) != '/')
        currentDirUrl += '/';
      this.validateFileName_(currentDirUrl, filename, function(isValid) {
        if (!isValid)
          return;

        if (util.isFakeEntry(directory)) {
          // Can't save a file into a fake directory.
          return;
        }

        var selectFileAndClose = function() {
          this.selectFilesAndClose_({
            urls: [currentDirUrl + encodeURIComponent(filename)],
            multiple: false,
            filterIndex: this.getSelectedFilterIndex_(filename)
          });
        }.bind(this);

        directory.getFile(
            filename, {create: false},
            function(entry) {
              // An existing file is found. Show confirmation dialog to
              // overwrite it. If the user select "OK" on the dialog, save it.
              this.confirm.show(strf('CONFIRM_OVERWRITE_FILE', filename),
                                selectFileAndClose);
            }.bind(this),
            function(error) {
              if (error.code == FileError.NOT_FOUND_ERR) {
                // The file does not exist, so it should be ok to create a
                // new file.
                selectFileAndClose();
                return;
              }
              if (error.code == FileError.TYPE_MISMATCH_ERR) {
                // An directory is found.
                // Do not allow to overwrite directory.
                this.alert.show(strf('DIRECTORY_ALREADY_EXISTS', filename));
                return;
              }

              // Unexpected error.
              console.error('File save failed: ' + error.code);
            }.bind(this));
      }.bind(this));
      return;
    }

    var files = [];
    var selectedIndexes = this.currentList_.selectionModel.selectedIndexes;

    if (DialogType.isFolderDialog(this.dialogType) &&
        selectedIndexes.length == 0) {
      var url = this.getCurrentDirectoryURL();
      var singleSelection = {
        urls: [url],
        multiple: false,
        filterIndex: this.getSelectedFilterIndex_()
      };
      this.selectFilesAndClose_(singleSelection);
      return;
    }

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length)
      throw new Error('Nothing selected!');

    var dm = this.directoryModel_.getFileList();
    for (var i = 0; i < selectedIndexes.length; i++) {
      var entry = dm.item(selectedIndexes[i]);
      if (!entry) {
        console.error('Error locating selected file at index: ' + i);
        continue;
      }

      files.push(entry.toURL());
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType == DialogType.SELECT_OPEN_MULTI_FILE) {
      var multipleSelection = {
        urls: files,
        multiple: true
      };
      this.selectFilesAndClose_(multipleSelection);
      return;
    }

    // Everything else must have exactly one.
    if (files.length > 1)
      throw new Error('Too many files selected!');

    var selectedEntry = dm.item(selectedIndexes[0]);

    if (DialogType.isFolderDialog(this.dialogType)) {
      if (!selectedEntry.isDirectory)
        throw new Error('Selected entry is not a folder!');
    } else if (this.dialogType == DialogType.SELECT_OPEN_FILE) {
      if (!selectedEntry.isFile)
        throw new Error('Selected entry is not a file!');
    }

    var singleSelection = {
      urls: [files[0]],
      multiple: false,
      filterIndex: this.getSelectedFilterIndex_()
    };
    this.selectFilesAndClose_(singleSelection);
  };

  /**
   * Verifies the user entered name for file or folder to be created or
   * renamed to. Name restrictions must correspond to File API restrictions
   * (see DOMFilePath::isValidPath). Curernt WebKit implementation is
   * out of date (spec is
   * http://dev.w3.org/2009/dap/file-system/file-dir-sys.html, 8.3) and going to
   * be fixed. Shows message box if the name is invalid.
   *
   * It also verifies if the name length is in the limit of the filesystem.
   *
   * @param {string} parentUrl The URL of the parent directory entry.
   * @param {string} name New file or folder name.
   * @param {function} onDone Function to invoke when user closes the
   *    warning box or immediatelly if file name is correct. If the name was
   *    valid it is passed true, and false otherwise.
   * @private
   */
  FileManager.prototype.validateFileName_ = function(parentUrl, name, onDone) {
    var msg;
    var testResult = /[\/\\\<\>\:\?\*\"\|]/.exec(name);
    if (testResult) {
      msg = strf('ERROR_INVALID_CHARACTER', testResult[0]);
    } else if (/^\s*$/i.test(name)) {
      msg = str('ERROR_WHITESPACE_NAME');
    } else if (/^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])$/i.test(name)) {
      msg = str('ERROR_RESERVED_NAME');
    } else if (this.fileFilter_.isFilterHiddenOn() && name[0] == '.') {
      msg = str('ERROR_HIDDEN_NAME');
    }

    if (msg) {
      this.alert.show(msg, function() {
        onDone(false);
      });
      return;
    }

    var self = this;
    chrome.fileBrowserPrivate.validatePathNameLength(
        parentUrl, name, function(valid) {
          if (!valid) {
            self.alert.show(str('ERROR_LONG_NAME'),
                            function() { onDone(false); });
          } else {
            onDone(true);
          }
        });
  };

  /**
   * Handler invoked on preference setting in drive context menu.
   *
   * @param {string} pref  The preference to alter.
   * @param {boolean} inverted Invert the value if true.
   * @param {Event}  event The click event.
   * @private
   */
  FileManager.prototype.onDrivePrefClick_ = function(pref, inverted, event) {
    var newValue = !event.target.hasAttribute('checked');
    if (newValue)
      event.target.setAttribute('checked', 'checked');
    else
      event.target.removeAttribute('checked');

    var changeInfo = {};
    changeInfo[pref] = inverted ? !newValue : newValue;
    chrome.fileBrowserPrivate.setPreferences(changeInfo);
  };

  /**
   * Invoked when the search box is changed.
   *
   * @param {Event} event The changed event.
   * @private
   */
  FileManager.prototype.onSearchBoxUpdate_ = function(event) {
    var searchString = this.searchBox_.value;

    if (this.isOnDrive()) {
      // When the search text is changed, finishes the search and showes back
      // the last directory by passing an empty string to
      // {@code DirectoryModel.search()}.
      if (this.directoryModel_.isSearching() &&
          this.lastSearchQuery_ != searchString) {
        this.doSearch('');
      }

      // On drive, incremental search is not invoked since we have an auto-
      // complete suggestion instead.
      return;
    }

    this.search_(searchString);
  };

  /**
   * Handle the search clear button click.
   * @private
   */
  FileManager.prototype.onSearchClearButtonClick_ = function() {
    this.ui_.searchBox.clear();
    this.onSearchBoxUpdate_();
  };

  /**
   * Search files and update the list with the search result.
   *
   * @param {string} searchString String to be searched with.
   * @private
   */
  FileManager.prototype.search_ = function(searchString) {
    var noResultsDiv = this.document_.getElementById('no-search-results');

    var reportEmptySearchResults = function() {
      if (this.directoryModel_.getFileList().length === 0) {
        // The string 'SEARCH_NO_MATCHING_FILES_HTML' may contain HTML tags,
        // hence we escapes |searchString| here.
        var html = strf('SEARCH_NO_MATCHING_FILES_HTML',
                        util.htmlEscape(searchString));
        noResultsDiv.innerHTML = html;
        noResultsDiv.setAttribute('show', 'true');
      } else {
        noResultsDiv.removeAttribute('show');
      }
    };

    var hideNoResultsDiv = function() {
      noResultsDiv.removeAttribute('show');
    };

    this.doSearch(searchString,
                  reportEmptySearchResults.bind(this),
                  hideNoResultsDiv.bind(this));
  };

  /**
   * Performs search and displays results.
   *
   * @param {string} query Query that will be searched for.
   * @param {function()=} opt_onSearchRescan Function that will be called when
   *     the search directory is rescanned (i.e. search results are displayed).
   * @param {function()=} opt_onClearSearch Function to be called when search
   *     state gets cleared.
   */
  FileManager.prototype.doSearch = function(
      searchString, opt_onSearchRescan, opt_onClearSearch) {
    var onSearchRescan = opt_onSearchRescan || function() {};
    var onClearSearch = opt_onClearSearch || function() {};

    this.lastSearchQuery_ = searchString;
    this.directoryModel_.search(searchString, onSearchRescan, onClearSearch);
  };

  /**
   * Requests autocomplete suggestions for files on Drive.
   * Once the suggestions are returned, the autocomplete popup will show up.
   *
   * @param {string} query The text to autocomplete from.
   * @private
   */
  FileManager.prototype.requestAutocompleteSuggestions_ = function(query) {
    query = query.trimLeft();

    // Only Drive supports auto-compelete
    if (!this.isOnDrive())
      return;

    // Remember the most recent query. If there is an other request in progress,
    // then it's result will be discarded and it will call a new request for
    // this query.
    this.lastAutocompleteQuery_ = query;
    if (this.autocompleteSuggestionsBusy_)
      return;

    // The autocomplete list should be resized and repositioned here as the
    // search box is resized when it's focused.
    this.autocompleteList_.syncWidthAndPositionToInput();

    if (!query) {
      this.autocompleteList_.suggestions = [];
      return;
    }

    var headerItem = {isHeaderItem: true, searchQuery: query};
    if (!this.autocompleteList_.dataModel ||
        this.autocompleteList_.dataModel.length == 0)
      this.autocompleteList_.suggestions = [headerItem];
    else
      // Updates only the head item to prevent a flickering on typing.
      this.autocompleteList_.dataModel.splice(0, 1, headerItem);

    this.autocompleteSuggestionsBusy_ = true;

    var searchParams = {
      'query': query,
      'types': 'ALL',
      'maxResults': 4
    };
    chrome.fileBrowserPrivate.searchDriveMetadata(
      searchParams,
      function(suggestions) {
        this.autocompleteSuggestionsBusy_ = false;

        // Discard results for previous requests and fire a new search
        // for the most recent query.
        if (query != this.lastAutocompleteQuery_) {
          this.requestAutocompleteSuggestions_(this.lastAutocompleteQuery_);
          return;
        }

        // Keeps the items in the suggestion list.
        this.autocompleteList_.suggestions = [headerItem].concat(suggestions);
      }.bind(this));
  };

  /**
   * Opens the currently selected suggestion item.
   * @private
   */
  FileManager.prototype.openAutocompleteSuggestion_ = function() {
    var selectedItem = this.autocompleteList_.selectedItem;

    // If the entry is the search item or no entry is selected, just change to
    // the search result.
    if (!selectedItem || selectedItem.isHeaderItem) {
      var query = selectedItem ?
          selectedItem.searchQuery : this.searchBox_.value;
      this.search_(query);
      return;
    }

    var entry = selectedItem.entry;
    // If the entry is a directory, just change the directory.
    if (entry.isDirectory) {
      this.onDirectoryAction_(entry);
      return;
    }

    var entries = [entry];
    var self = this;

    // To open a file, first get the mime type.
    this.metadataCache_.get(entries, 'drive', function(props) {
      var mimeType = props[0].contentMimeType || '';
      var mimeTypes = [mimeType];
      var openIt = function() {
        if (self.dialogType == DialogType.FULL_PAGE) {
          var tasks = new FileTasks(self);
          tasks.init(entries, mimeTypes);
          tasks.executeDefault();
        } else {
          self.onOk_();
        }
      };

      // Change the current directory to the directory that contains the
      // selected file. Note that this is necessary for an image or a video,
      // which should be opened in the gallery mode, as the gallery mode
      // requires the entry to be in the current directory model. For
      // consistency, the current directory is always changed regardless of
      // the file type.
      entry.getParent(function(parent) {
        var onDirectoryChanged = function(event) {
          self.directoryModel_.removeEventListener('scan-completed',
                                                   onDirectoryChanged);
          self.directoryModel_.selectEntry(entry);
          openIt();
        };
        // changeDirectory() returns immediately. We should wait until the
        // directory scan is complete.
        self.directoryModel_.addEventListener('scan-completed',
                                              onDirectoryChanged);
        self.directoryModel_.changeDirectory(
          parent.fullPath,
          function() {
            // Remove the listner if the change directory failed.
            self.directoryModel_.removeEventListener('scan-completed',
                                                     onDirectoryChanged);
          });
      });
    });
  };

  FileManager.prototype.decorateSplitter = function(splitterElement) {
    var self = this;

    var Splitter = cr.ui.Splitter;

    var customSplitter = cr.ui.define('div');

    customSplitter.prototype = {
      __proto__: Splitter.prototype,

      handleSplitterDragStart: function(e) {
        Splitter.prototype.handleSplitterDragStart.apply(this, arguments);
        this.ownerDocument.documentElement.classList.add('col-resize');
      },

      handleSplitterDragMove: function(deltaX) {
        Splitter.prototype.handleSplitterDragMove.apply(this, arguments);
        self.onResize_();
      },

      handleSplitterDragEnd: function(e) {
        Splitter.prototype.handleSplitterDragEnd.apply(this, arguments);
        this.ownerDocument.documentElement.classList.remove('col-resize');
      }
    };

    customSplitter.decorate(splitterElement);
  };

  /**
   * Updates default action menu item to match passed taskItem (icon,
   * label and action).
   *
   * @param {Object} defaultItem - taskItem to match.
   * @param {boolean} isMultiple - if multiple tasks available.
   */
  FileManager.prototype.updateContextMenuActionItems = function(defaultItem,
                                                                isMultiple) {
    if (defaultItem) {
      if (defaultItem.iconType) {
        this.defaultActionMenuItem_.style.backgroundImage = '';
        this.defaultActionMenuItem_.setAttribute('file-type-icon',
                                                 defaultItem.iconType);
      } else if (defaultItem.iconUrl) {
        this.defaultActionMenuItem_.style.backgroundImage =
            'url(' + defaultItem.iconUrl + ')';
      } else {
        this.defaultActionMenuItem_.style.backgroundImage = '';
      }

      this.defaultActionMenuItem_.label = defaultItem.title;
      this.defaultActionMenuItem_.disabled = !!defaultItem.disabled;
      this.defaultActionMenuItem_.taskId = defaultItem.taskId;
    }

    var defaultActionSeparator =
        this.dialogDom_.querySelector('#default-action-separator');

    this.openWithCommand_.canExecuteChange();
    this.openWithCommand_.setHidden(!(defaultItem && isMultiple));
    this.openWithCommand_.disabled = defaultItem && !!defaultItem.disabled;

    this.defaultActionMenuItem_.hidden = !defaultItem;
    defaultActionSeparator.hidden = !defaultItem;
  };

  /**
   * Window beforeunload handler.
   * @return {string} Message to show. Ignored when running as a packaged app.
   * @private
   */
  FileManager.prototype.onBeforeUnload_ = function() {
    if (this.filePopup_ &&
        this.filePopup_.contentWindow &&
        this.filePopup_.contentWindow.beforeunload) {
      // The gallery might want to prevent the unload if it is busy.
      return this.filePopup_.contentWindow.beforeunload();
    }
    return null;
  };

  /**
   * @return {FileSelection} Selection object.
   */
  FileManager.prototype.getSelection = function() {
    return this.selectionHandler_.selection;
  };

  /**
   * @return {ArrayDataModel} File list.
   */
  FileManager.prototype.getFileList = function() {
    return this.directoryModel_.getFileList();
  };

  /**
   * @return {cr.ui.List} Current list object.
   */
  FileManager.prototype.getCurrentList = function() {
    return this.currentList_;
  };

  /**
   * Retrieve the preferences of the files.app. This method caches the result
   * and returns it unless opt_update is true.
   * @param {function(Object.<string, *>)} callback Callback to get the
   *     preference.
   * @param {boolean=} opt_update If is's true, don't use the cache and
   *     retrieve latest preference. Default is false.
   * @private
   */
  FileManager.prototype.getPreferences_ = function(callback, opt_update) {
    if (!opt_update && this.preferences_ !== undefined) {
      callback(this.preferences_);
      return;
    }

    chrome.fileBrowserPrivate.getPreferences(function(prefs) {
      this.preferences_ = prefs;
      callback(prefs);
    }.bind(this));
  };
})();
