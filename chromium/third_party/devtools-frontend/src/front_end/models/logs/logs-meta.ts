// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';

const UIStrings = {
  /**
   * @description Text to keep the log after refreshing.
   */
  keepLog: 'Keep log',
  /**
   * @description A term that can be used to search in the command menu, and will find the search
   * result 'Keep log on page reload / navigation'. This is an additional search term to help
   * the user find the setting even when they don't know the exact name of it.
   */
  keep: 'keep',
  /**
   * @description A term that can be used to search in the command menu, and will find the search
   * result 'Keep log on page reload / navigation'. This is an additional search term to help
   * the user find the setting even when they don't know the exact name of it.
   */
  preserve: 'preserve',
  /**
   * @description A term that can be used to search in the command menu, and will find the search
   * result 'Keep log on page reload / navigation'. This is an additional search term to help
   * the user find the setting even when they don't know the exact name of it.
   */
  clear: 'clear',
  /**
   * @description A term that can be used to search in the command menu, and will find the search
   * result 'Keep log on page reload / navigation'. This is an additional search term to help
   * the user find the setting even when they don't know the exact name of it.
   */
  reset: 'reset',
  /**
   * @description Title of a setting under the Network category that can be invoked through the Command Menu.
   */
  keepLogOnPageReload: 'Keep log on page reload / navigation',
  /**
   * @description Title of a setting under the Network category that can be invoked through the Command Menu.
   */
  doNotKeepLogOnPageReload: 'Don’t keep log on page reload / navigation',
  /**
   * @description Title of an action in the network tool to toggle recording.
   */
  recordNetworkLog: 'Record network log',
} as const;
const str_ = i18n.i18n.registerUIStrings('models/logs/logs-meta.ts', UIStrings);
const i18nLazyString = i18n.i18n.getLazilyComputedLocalizedString.bind(undefined, str_);

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.NETWORK,
  title: i18nLazyString(UIStrings.keepLog),
  settingName: 'network-log.preserve-log',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: false,
  tags: [
    i18nLazyString(UIStrings.keep),
    i18nLazyString(UIStrings.preserve),
    i18nLazyString(UIStrings.clear),
    i18nLazyString(UIStrings.reset),
  ],
  options: [
    {
      value: true,
      title: i18nLazyString(UIStrings.keepLogOnPageReload),
    },
    {
      value: false,
      title: i18nLazyString(UIStrings.doNotKeepLogOnPageReload),
    },
  ],
});

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.NETWORK,
  title: i18nLazyString(UIStrings.recordNetworkLog),
  settingName: 'network-log.record-log',
  settingType: Common.Settings.SettingType.BOOLEAN,
  defaultValue: true,
  storageType: Common.Settings.SettingStorageType.SESSION,
});
