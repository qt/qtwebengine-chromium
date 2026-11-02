// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import * as Common from '../../core/common/common.js';

import * as SettingsUI from './settings.js';

describe('SettingUIRegistration', () => {
  beforeEach(() => {
    SettingsUI.SettingUIRegistration.resetSettings();
    Common.SettingRegistration.resetSettings();
  });
  afterEach(() => {
    SettingsUI.SettingUIRegistration.resetSettings();
    Common.SettingRegistration.resetSettings();
  });

  const settingDescriptor: Common.Settings.SettingDescriptor<boolean> = {
    name: 'mock-setting',
    type: Common.Settings.SettingType.BOOLEAN,
    defaultValue: false,
  };

  it('registers a setting UI descriptor', () => {
    SettingsUI.SettingUIRegistration.register(settingDescriptor, {
      category: Common.SettingRegistration.SettingCategory.GLOBAL,
    });

    const registered = SettingsUI.SettingUIRegistration.getRegisteredSettings();
    assert.lengthOf(registered, 1);
    assert.strictEqual(registered[0].descriptor.name, 'mock-setting');
    assert.strictEqual(registered[0].uiDescriptor.category, Common.SettingRegistration.SettingCategory.GLOBAL);
  });

  it('includes synthesized legacy settings in getRegisteredSettings', () => {
    Common.SettingRegistration.registerSettingExtension({
      settingName: 'legacy-setting',
      settingType: Common.SettingRegistration.SettingType.BOOLEAN,
      defaultValue: true,
      category: Common.SettingRegistration.SettingCategory.CONSOLE,
    });

    const registered = SettingsUI.SettingUIRegistration.getRegisteredSettings();
    const legacyReg = registered.find(r => r.descriptor.name === 'legacy-setting');
    assert.exists(legacyReg);
    assert.strictEqual(legacyReg?.descriptor.name, 'legacy-setting');
    assert.strictEqual(legacyReg?.descriptor.type, Common.SettingRegistration.SettingType.BOOLEAN);
    assert.strictEqual(legacyReg?.uiDescriptor.category, Common.SettingRegistration.SettingCategory.CONSOLE);
  });

  it('throws an error when trying to register a duplicate setting name', () => {
    SettingsUI.SettingUIRegistration.register(settingDescriptor, {});

    assert.throws(() => {
      SettingsUI.SettingUIRegistration.register(settingDescriptor, {});
    }, 'Duplicate setting name \'mock-setting\'');
  });

  it('resolves a registered setting UI descriptor', () => {
    const uiDescriptor: SettingsUI.SettingUIRegistration.SettingUIDescriptor = {
      category: Common.SettingRegistration.SettingCategory.GLOBAL,
    };
    SettingsUI.SettingUIRegistration.register(settingDescriptor, uiDescriptor);

    const resolved = SettingsUI.SettingUIRegistration.resolve(settingDescriptor);
    assert.strictEqual(resolved, uiDescriptor);
  });

  it('resolves legacy setting registrations', () => {
    Common.SettingRegistration.registerSettingExtension({
      settingName: 'legacy-setting',
      settingType: Common.SettingRegistration.SettingType.BOOLEAN,
      defaultValue: true,
      category: Common.SettingRegistration.SettingCategory.CONSOLE,
    });

    const resolved = SettingsUI.SettingUIRegistration.resolve({
      name: 'legacy-setting',
      type: Common.Settings.SettingType.BOOLEAN,
      defaultValue: true,
    });
    assert.strictEqual(resolved.category, Common.SettingRegistration.SettingCategory.CONSOLE);
  });

  it('throws an error when resolving an unregistered setting descriptor', () => {
    assert.throws(() => {
      SettingsUI.SettingUIRegistration.resolve(settingDescriptor);
    }, 'No UI descriptor registered for setting \'mock-setting\'');
  });

  it('returns null when maybeResolve is called for an unregistered setting descriptor', () => {
    assert.isNull(SettingsUI.SettingUIRegistration.maybeResolve(settingDescriptor));
  });
});
