// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * TODO(dzvorygin): Here we use this hack, since 'hidden' is standard
 * attribute and we can't use it's setter as usual.
 * @param {boolean} value New value of hidden property.
 */
cr.ui.Command.prototype.setHidden = function(value) {
  this.__lookupSetter__('hidden').call(this, value);
};

/**
 * A command.
 * @interface
 */
var Command = function() {};

/**
 * Handles the execute event.
 * @param {Event} event Command event.
 * @param {FileManager} fileManager FileManager.
 */
Command.prototype.execute = function(event, fileManager) {};

/**
 * Handles the can execute event.
 * @param {Event} event Can execute event.
 * @param {FileManager} fileManager FileManager.
 */
Command.prototype.canExecute = function(event, fileManager) {};

/**
 * Utility for commands.
 */
var CommandUtil = {};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {DirectoryTree|DirectoryItem|NavigationList|HTMLLIElement|cr.ui.List}
 *     element Directory to extract a path from.
 * @return {Entry} Entry of the found node.
 */
CommandUtil.getCommandEntry = function(element) {
  if (element instanceof NavigationList) {
    // element is a NavigationList.

    /** @type {NavigationModelItem} */
    var selectedItem = element.selectedItem;
    return selectedItem && selectedItem.getCachedEntry();
  } else if (element instanceof NavigationListItem) {
    // element is a subitem of NavigationList.
    /** @type {NavigationList} */
    var navigationList = element.parentElement;
    var index = navigationList.getIndexOfListItem(element);
    /** @type {NavigationModelItem} */
    var item = (index != -1) ? navigationList.dataModel.item(index) : null;
    return item && item.getCachedEntry();
  } else if (element instanceof DirectoryTree) {
    // element is a DirectoryTree.
    return element.selectedItem;
  } else if (element instanceof DirectoryItem) {
    // element is a sub item in DirectoryTree.

    // DirectoryItem.fullPath is set on initialization, but entry is lazily.
    // We may use fullPath just in case that the entry has not been set yet.
    return element.entry;
  } else if (element instanceof cr.ui.List) {
    // element is a normal List (eg. the file list on the right panel).
    var entry = element.selectedItem;
    // Check if it is Entry or not by referring the fullPath member variable.
    return entry && entry.fullPath ? entry : null;
  } else {
    console.warn('Unsupported element');
    return null;
  }
};

/**
 * @param {NavigationList} navigationList navigation list to extract root node.
 * @return {?RootType} Type of the found root.
 */
CommandUtil.getCommandRootType = function(navigationList) {
  var root = CommandUtil.getCommandEntry(navigationList);
  return root &&
         PathUtil.isRootPath(root.fullPath) &&
         PathUtil.getRootType(root.fullPath);
};

/**
 * Checks if command can be executed on drive.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteEnabledOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
};

/**
 * Checks if command should be visible on drive.
 * @param {Event} event Command event to mark.
 * @param {FileManager} fileManager FileManager to use.
 */
CommandUtil.canExecuteVisibleOnDriveOnly = function(event, fileManager) {
  event.canExecute = fileManager.isOnDrive();
  event.command.setHidden(!fileManager.isOnDrive());
};

/**
 * Sets as the command as always enabled.
 * @param {Event} event Command event to mark.
 */
CommandUtil.canExecuteAlways = function(event) {
  event.canExecute = true;
};

/**
 * Returns a single selected/passed entry or null.
 * @param {Event} event Command event.
 * @param {FileManager} fileManager FileManager to use.
 * @return {FileEntry} The entry or null.
 */
CommandUtil.getSingleEntry = function(event, fileManager) {
  if (event.target.entry) {
    return event.target.entry;
  }
  var selection = fileManager.getSelection();
  if (selection.totalCount == 1) {
    return selection.entries[0];
  }
  return null;
};

/**
 * Obtains target entries that can be pinned from the selection.
 * If directories are included in the selection, it just returns an empty
 * array to avoid confusing because pinning directory is not supported
 * currently.
 *
 * @return {Array.<Entry>} Target entries.
 */
CommandUtil.getPinTargetEntries = function() {
  var hasDirectory = false;
  var results = fileManager.getSelection().entries.filter(function(entry) {
    hasDirectory = hasDirectory || entry.isDirectory;
    if (!entry || hasDirectory)
      return false;
    var metadata = fileManager.metadataCache_.getCached(entry, 'drive');
    if (!metadata || metadata.hosted)
      return false;
    entry.pinned = metadata.pinned;
    return true;
  });
  return hasDirectory ? [] : results;
};

/**
 * Sets the default handler for the commandId and prevents handling
 * the keydown events for this command. Not doing that breaks relationship
 * of original keyboard event and the command. WebKit would handle it
 * differently in some cases.
 * @param {Node} node to register command handler on.
 * @param {string} commandId Command id to respond to.
 */
CommandUtil.forceDefaultHandler = function(node, commandId) {
  var doc = node.ownerDocument;
  var command = doc.querySelector('command[id="' + commandId + '"]');
  node.addEventListener('keydown', function(e) {
    if (command.matchesEvent(e)) {
      // Prevent cr.ui.CommandManager of handling it and leave it
      // for the default handler.
      e.stopPropagation();
    }
  });
  node.addEventListener('command', function(event) {
    if (event.command.id !== commandId)
      return;
    document.execCommand(event.command.id);
    event.cancelBubble = true;
  });
  node.addEventListener('canExecute', function(event) {
    if (event.command.id === commandId)
      event.canExecute = document.queryCommandEnabled(event.command.id);
  });
};

/**
 * Default command.
 * @type {Command}
 */
CommandUtil.defaultCommand = {
  execute: function(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  },
  canExecute: function(event, fileManager) {
    event.canExecute = fileManager.document.queryCommandEnabled(
        event.command.id);
  }
};

/**
 * Creates the volume switch command with index.
 * @param {number} index Volume index from 1 to 9.
 * @return {Command} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = function(index) {
  return {
    execute: function(event, fileManager) {
      fileManager.navigationList.selectByIndex(index - 1);
    },
    canExecute: function(event, fileManager) {
      event.canExecute = index > 0 &&
          index <= fileManager.navigationList.dataModel.length;
    }
  };
};

/**
 * Handle of the command events.
 * @param {FileManager} fileManager FileManager.
 * @constructor
 */
var CommandHandler = function(fileManager) {
  /**
   * FileManager.
   * @type {FileManager}
   * @private
   */
  this.fileManager_ = fileManager;

  /**
   * Command elements.
   * @type {Object.<string, cr.ui.Command>}
   * @private
   */
  this.commands_ = {};

  /**
   * Whether the ctrl key is pressed or not.
   * @type {boolean}
   * @private
   */
  this.ctrlKeyPressed_ = false;

  Object.seal(this);

  // Decorate command tags in the document.
  var commands = fileManager.document.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++) {
    cr.ui.Command.decorate(commands[i]);
    this.commands_[commands[i].id] = commands[i];
  }

  // Register events.
  fileManager.document.addEventListener('command', this.onCommand_.bind(this));
  fileManager.document.addEventListener('canExecute',
                                        this.onCanExecute_.bind(this));
  fileManager.document.addEventListener('keydown', this.onKeyDown_.bind(this));
  fileManager.document.addEventListener('keyup', this.onKeyUp_.bind(this));
};

/**
 * Updates the availability of all commands.
 */
CommandHandler.prototype.updateAvailability = function() {
  for (var id in this.commands_) {
    this.commands_[id].canExecuteChange();
  }
};

/**
 * Checks if the handler should ignore the current event, eg. since there is
 * a popup dialog currently opened.
 *
 * @return {boolean} True if the event should be ignored, false otherwise.
 * @private
 */
CommandHandler.prototype.shouldIgnoreEvents_ = function() {
  // Do not handle commands, when a dialog is shown.
  if (this.fileManager_.document.querySelector('.cr-dialog-container.shown'))
    return true;

  return false;  // Do not ignore.
};

/**
 * Handles command events.
 * @param {Event} event Command event.
 * @private
 */
CommandHandler.prototype.onCommand_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.execute.call(this, event, this.fileManager_);
};

/**
 * Handles canExecute events.
 * @param {Event} event Can execute event.
 * @private
 */
CommandHandler.prototype.onCanExecute_ = function(event) {
  if (this.shouldIgnoreEvents_())
    return;
  var handler = CommandHandler.COMMANDS_[event.command.id];
  handler.canExecute.call(this, event, this.fileManager_);
};

/**
 * Handle key down event.
 * @param {Event} event Key down event.
 * @private
 */
CommandHandler.prototype.onKeyDown_ = function(event) {
  // 17 is the keycode of Ctrl key and it means the event is not for other keys
  // with Ctrl modifier but for ctrl key itself.
  if (util.getKeyModifiers(event) + event.keyCode == 'Ctrl-17') {
    this.ctrlKeyPressed_ = true;
    this.updateAvailability();
  }
};

/**
 * Handle key up event.
 * @param {Event} event Key up event.
 * @private
 */
CommandHandler.prototype.onKeyUp_ = function(event) {
  // 17 is the keycode of Ctrl key and it means the event is not for other keys
  // with Ctrl modifier but for ctrl key itself.
  if (util.getKeyModifiers(event) + event.keyCode == '17') {
    this.ctrlKeyPressed_ = false;
    this.updateAvailability();
  }
};

/**
 * Commands.
 * @type {Object.<string, Command>}
 * @const
 * @private
 */
CommandHandler.COMMANDS_ = {};

/**
 * Unmounts external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['unmount'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var root = CommandUtil.getCommandEntry(event.target);
    if (root)
      fileManager.unmountVolume(PathUtil.getRootPath(root.fullPath));
  },
  /**
   * @param {Event} event Command event.
   */
  canExecute: function(event, fileManager) {
    var rootType = CommandUtil.getCommandRootType(event.target);

    event.canExecute = (rootType == RootType.ARCHIVE ||
                        rootType == RootType.REMOVABLE);
    event.command.setHidden(!event.canExecute);
    event.command.label = rootType == RootType.ARCHIVE ?
        str('CLOSE_ARCHIVE_BUTTON_LABEL') :
        str('UNMOUNT_DEVICE_BUTTON_LABEL');
  }
};

/**
 * Formats external drive.
 * @type {Command}
 */
CommandHandler.COMMANDS_['format'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    var root = CommandUtil.getCommandEntry(event.target);
    // If an entry is not found from the event target, use the current
    // directory. This can happen for the format button for unsupported and
    // unrecognized volumes.
    if (!root)
      root = directoryModel.getCurrentDirEntry();

    // TODO(satorux): Stop assuming fullPath to be unique. crbug.com/320967
    var mountPath = root.fullPath;
    var volumeInfo = fileManager.volumeManager.getVolumeInfo(mountPath);
    if (volumeInfo) {
      fileManager.confirm.show(
          loadTimeData.getString('FORMATTING_WARNING'),
          chrome.fileBrowserPrivate.formatVolume.bind(null,
                                                      volumeInfo.volumeId));
    }
  },
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    var root = CommandUtil.getCommandEntry(event.target);
    // See the comment in execute() for why doing this.
    if (!root)
      root = directoryModel.getCurrentDirEntry();
    var removable = root &&
                    PathUtil.getRootType(root.fullPath) == RootType.REMOVABLE;
    // Don't check if the volume is read-only. Unformatted volume is
    // considered read-only per directoryModel.isPathReadOnly(), but can be
    // formatted. An error will be raised if formatting failed anyway.
    event.canExecute = removable;
    event.command.setHidden(!removable);
  }
};

/**
 * Initiates new folder creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-folder'] = {
  execute: function(event, fileManager) {
    fileManager.createNewFolder();
  },
  canExecute: function(event, fileManager) {
    var directoryModel = fileManager.directoryModel;
    event.canExecute = !fileManager.isOnReadonlyDirectory() &&
                       !fileManager.isRenamingInProgress() &&
                       !directoryModel.isSearching() &&
                       !directoryModel.isScanning();
  }
};

/**
 * Initiates new window creation.
 * @type {Command}
 */
CommandHandler.COMMANDS_['new-window'] = {
  execute: function(event, fileManager) {
    fileManager.backgroundPage.launchFileManager({
      defaultPath: fileManager.getCurrentDirectory()
    });
  },
  canExecute: function(event, fileManager) {
    event.canExecute =
        fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
};

/**
 * Deletes selected files.
 * @type {Command}
 */
CommandHandler.COMMANDS_['delete'] = {
  execute: function(event, fileManager) {
    fileManager.deleteSelection();
  },
  canExecute: function(event, fileManager) {
    var allowDeletingWhileOffline =
        fileManager.directoryModel.getCurrentRootType() === RootType.DRIVE;
    var selection = fileManager.getSelection();
    event.canExecute = (!fileManager.isOnReadonlyDirectory() ||
                        allowDeletingWhileOffline) &&
                       selection &&
                       selection.totalCount > 0;
  }
};

/**
 * Pastes files from clipboard.
 * @type {Command}
 */
CommandHandler.COMMANDS_['paste'] = {
  execute: function() {
    document.execCommand(event.command.id);
  },
  canExecute: function(event, fileManager) {
    var document = fileManager.document;
    var fileTransferController = fileManager.fileTransferController;
    event.canExecute = (fileTransferController &&
        fileTransferController.queryPasteCommandEnabled());
  }
};

CommandHandler.COMMANDS_['cut'] = CommandUtil.defaultCommand;
CommandHandler.COMMANDS_['copy'] = CommandUtil.defaultCommand;

/**
 * Initiates file renaming.
 * @type {Command}
 */
CommandHandler.COMMANDS_['rename'] = {
  execute: function(event, fileManager) {
    fileManager.initiateRename();
  },
  canExecute: function(event, fileManager) {
    var allowRenamingWhileOffline =
        fileManager.directoryModel.getCurrentRootType() === RootType.DRIVE;
    var selection = fileManager.getSelection();
    event.canExecute =
        !fileManager.isRenamingInProgress() &&
        (!fileManager.isOnReadonlyDirectory() || allowRenamingWhileOffline) &&
        selection &&
        selection.totalCount == 1;
  }
};

/**
 * Opens drive help.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-help'] = {
  execute: function(event, fileManager) {
    if (fileManager.isOnDrive())
      util.visitURL(str('GOOGLE_DRIVE_HELP_URL'));
    else
      util.visitURL(str('FILES_APP_HELP_URL'));
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Opens drive buy-more-space url.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] = {
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Opens drive.google.com.
 * @type {Command}
 */
CommandHandler.COMMANDS_['drive-go-to-drive'] = {
  execute: function(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
  },
  canExecute: CommandUtil.canExecuteVisibleOnDriveOnly
};

/**
 * Displays open with dialog for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['open-with'] = {
  execute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    if (tasks) {
      tasks.showTaskPicker(fileManager.defaultTaskPicker,
          str('OPEN_WITH_BUTTON_LABEL'),
          null,
          function(task) {
            tasks.execute(task.taskId);
          });
    }
  },
  canExecute: function(event, fileManager) {
    var tasks = fileManager.getSelection().tasks;
    event.canExecute = tasks && tasks.size() > 1;
  }
};

/**
 * Focuses search input box.
 * @type {Command}
 */
CommandHandler.COMMANDS_['search'] = {
  execute: function(event, fileManager) {
    var element = fileManager.document.querySelector('#search-box input');
    element.focus();
    element.select();
  },
  canExecute: function(event, fileManager) {
    event.canExecute = !fileManager.isRenamingInProgress();
  }
};

/**
 * Activates the n-th volume.
 * @type {Command}
 */
CommandHandler.COMMANDS_['volume-switch-1'] =
    CommandUtil.createVolumeSwitchCommand(1);
CommandHandler.COMMANDS_['volume-switch-2'] =
    CommandUtil.createVolumeSwitchCommand(2);
CommandHandler.COMMANDS_['volume-switch-3'] =
    CommandUtil.createVolumeSwitchCommand(3);
CommandHandler.COMMANDS_['volume-switch-4'] =
    CommandUtil.createVolumeSwitchCommand(4);
CommandHandler.COMMANDS_['volume-switch-5'] =
    CommandUtil.createVolumeSwitchCommand(5);
CommandHandler.COMMANDS_['volume-switch-6'] =
    CommandUtil.createVolumeSwitchCommand(6);
CommandHandler.COMMANDS_['volume-switch-7'] =
    CommandUtil.createVolumeSwitchCommand(7);
CommandHandler.COMMANDS_['volume-switch-8'] =
    CommandUtil.createVolumeSwitchCommand(8);
CommandHandler.COMMANDS_['volume-switch-9'] =
    CommandUtil.createVolumeSwitchCommand(9);

/**
 * Flips 'available offline' flag on the file.
 * @type {Command}
 */
CommandHandler.COMMANDS_['toggle-pinned'] = {
  execute: function(event, fileManager) {
    var pin = !event.command.checked;
    event.command.checked = pin;
    var entries = CommandUtil.getPinTargetEntries();
    var currentEntry;
    var error = false;
    var steps = {
      // Pick an entry and pin it.
      start: function() {
        // Check if all the entries are pinned or not.
        if (entries.length == 0)
          return;
        currentEntry = entries.shift();
        chrome.fileBrowserPrivate.pinDriveFile(
            currentEntry.toURL(),
            pin,
            steps.entryPinned);
      },

      // Check the result of pinning
      entryPinned: function() {
        // Convert to boolean.
        error = !!chrome.runtime.lastError;
        if (error && pin) {
          fileManager.metadataCache_.get(
              currentEntry, 'filesystem', steps.showError);
        }
        fileManager.metadataCache_.clear(currentEntry, 'drive');
        fileManager.metadataCache_.get(
            currentEntry, 'drive', steps.updateUI.bind(this));
      },

      // Update the user interface accoding to the cache state.
      updateUI: function(drive) {
        fileManager.updateMetadataInUI_(
            'drive', [currentEntry.toURL()], [drive]);
        if (!error)
          steps.start();
      },

      // Show the error
      showError: function(filesystem) {
        fileManager.alert.showHtml(str('DRIVE_OUT_OF_SPACE_HEADER'),
                                   strf('DRIVE_OUT_OF_SPACE_MESSAGE',
                                        unescape(currentEntry.name),
                                        util.bytesToString(filesystem.size)));
      }
    };
    steps.start();
  },

  canExecute: function(event, fileManager) {
    var entries = CommandUtil.getPinTargetEntries();
    var checked = true;
    for (var i = 0; i < entries.length; i++) {
      checked = checked && entries[i].pinned;
    }
    if (entries.length > 0) {
      event.canExecute = true;
      event.command.setHidden(false);
      event.command.checked = checked;
    } else {
      event.canExecute = false;
      event.command.setHidden(true);
    }
  }
};

/**
 * Creates zip file for current selection.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zip-selection'] = {
  execute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    var selectionEntries = fileManager.getSelection().entries;
    fileManager.fileOperationManager_.zipSelection(dirEntry, selectionEntries);
  },
  canExecute: function(event, fileManager) {
    var dirEntry = fileManager.getCurrentDirectoryEntry();
    var selection = fileManager.getSelection();
    event.canExecute =
        dirEntry &&
        !fileManager.isOnReadonlyDirectory() &&
        !fileManager.isOnDrive() &&
        selection && selection.totalCount > 0;
  }
};

/**
 * Shows the share dialog for the current selection (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['share'] = {
  execute: function(event, fileManager) {
    fileManager.shareSelection();
  },
  canExecute: function(event, fileManager) {
    var selection = fileManager.getSelection();
    event.canExecute = fileManager.isOnDrive() &&
        !fileManager.isDriveOffline() &&
        selection && selection.totalCount == 1;
    event.command.setHidden(!fileManager.isOnDrive());
  }
};

/**
 * Creates a shortcut of the selected folder (single only).
 * @type {Command}
 */
CommandHandler.COMMANDS_['create-folder-shortcut'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry)
      fileManager.createFolderShortcut(entry.fullPath);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    var folderShortcutExists = entry &&
                               fileManager.folderShortcutExists(entry.fullPath);

    var onlyOneFolderSelected = true;
    // Only on list, user can select multiple files. The command is enabled only
    // when a single file is selected.
    if (event.target instanceof cr.ui.List &&
        !(event.target instanceof NavigationList)) {
      var items = event.target.selectedItems;
      onlyOneFolderSelected = (items.length == 1 && items[0].isDirectory);
    }

    var eligible = entry &&
                   PathUtil.isEligibleForFolderShortcut(entry.fullPath);
    event.canExecute =
        eligible && onlyOneFolderSelected && !folderShortcutExists;
    event.command.setHidden(!eligible || !onlyOneFolderSelected);
  }
};

/**
 * Removes the folder shortcut.
 * @type {Command}
 */
CommandHandler.COMMANDS_['remove-folder-shortcut'] = {
  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  execute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    if (entry && entry.fullPath)
      fileManager.removeFolderShortcut(entry.fullPath);
  },

  /**
   * @param {Event} event Command event.
   * @param {FileManager} fileManager The file manager instance.
   */
  canExecute: function(event, fileManager) {
    var entry = CommandUtil.getCommandEntry(event.target);
    var path = entry && entry.fullPath;

    var eligible = path && PathUtil.isEligibleForFolderShortcut(path);
    var isShortcut = path && fileManager.folderShortcutExists(path);
    event.canExecute = isShortcut && eligible;
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Zoom in to the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-in'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('in');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Zoom out from the Files.app.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-out'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('out');
  },
  canExecute: CommandUtil.canExecuteAlways
};

/**
 * Reset the zoom factor.
 * @type {Command}
 */
CommandHandler.COMMANDS_['zoom-reset'] = {
  execute: function(event, fileManager) {
    chrome.fileBrowserPrivate.zoom('reset');
  },
  canExecute: CommandUtil.canExecuteAlways
};
