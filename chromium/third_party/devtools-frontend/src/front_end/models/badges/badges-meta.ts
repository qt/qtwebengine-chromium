// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../../core/common/common.js';
import * as i18n from '../../core/i18n/i18n.js';

const UIStrings = {
  /**
   * @description Label for a checkbox in the settings UI. Allows developers to opt-in/opt-out
   * of receiving Google Developer Program (GDP) badges based on their activity in Chrome DevTools.
   */
  earnBadges: 'Earn badges',
} as const;

const str_ = i18n.i18n.registerUIStrings('models/badges/badges-meta.ts', UIStrings);
const i18nLazyString = i18n.i18n.getLazilyComputedLocalizedString.bind(undefined, str_);

Common.Settings.registerSettingExtension({
  category: Common.Settings.SettingCategory.ACCOUNT,
  settingName: 'receive-gdp-badges',
  settingType: Common.Settings.SettingType.BOOLEAN,
  storageType: Common.Settings.SettingStorageType.SYNCED,
  title: i18nLazyString(UIStrings.earnBadges),
  defaultValue: false,
  reloadRequired: true,
});
