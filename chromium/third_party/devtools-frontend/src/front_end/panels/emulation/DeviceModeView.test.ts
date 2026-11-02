// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';
import sinon from 'sinon';

import * as Common from '../../core/common/common.js';
import * as SDK from '../../core/sdk/sdk.js';
import * as EmulationModel from '../../models/emulation/emulation.js';
import {assertScreenshot, renderElementIntoDOM} from '../../testing/DOMHelpers.js';
import {
  createTarget,
  describeWithEnvironment,
} from '../../testing/EnvironmentHelpers.js';
import {setupLocaleHooks} from '../../testing/LocaleHelpers.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as MobileThrottling from '../mobile_throttling/mobile_throttling.js';

import * as Emulation from './emulation.js';

describeWithEnvironment('DeviceModeView', () => {
  setupLocaleHooks();

  describe('DeviceModeView Tests', () => {
    let deviceModeModel: EmulationModel.DeviceModeModel.DeviceModeModel;
    let view: Emulation.DeviceModeView.DeviceModeView;
    let showRulersSetting: Common.Settings.Setting<boolean>;

    beforeEach(async () => {
      showRulersSetting = Common.Settings.Settings.instance().moduleSetting('emulation.show-rulers');

      SDK.NetworkManager.MultitargetNetworkManager.instance({forceNew: true});

      MobileThrottling.ThrottlingManager.ThrottlingManager.instance({forceNew: true});

      createTarget();
      deviceModeModel = EmulationModel.DeviceModeModel.DeviceModeModel.instance({forceNew: true});
      view = new Emulation.DeviceModeView.DeviceModeView();
    });

    it('renders the view', async () => {
      renderElementIntoDOM(view, {includeCommonStyles: true});
      await UI.Widget.Widget.allUpdatesComplete;
      await assertScreenshot('device_mode_view/base.png');
    });

    it('renders the view with rulers', async () => {
      showRulersSetting.set(true);
      renderElementIntoDOM(view, {includeCommonStyles: true, width: 800, height: 600});
      deviceModeModel.emulate(EmulationModel.DeviceModeModel.Type.Responsive, null, null);
      await UI.Widget.Widget.allUpdatesComplete;
      await assertScreenshot('device_mode_view/rulers.png');
    });

    describe('Logic Tests', () => {
      it('creates preset bars during initialization', () => {
        const presetsContainer = view.contentElement.querySelector('.device-mode-presets-container');
        assert.exists(presetsContainer);
        const presetBars = presetsContainer?.querySelectorAll('.device-mode-preset-bar');
        assert.strictEqual(presetBars?.length, 7);
      });

      it('toggles rulers when setting changes', async () => {
        renderElementIntoDOM(view);
        deviceModeModel.emulate(EmulationModel.DeviceModeModel.Type.Responsive, null, null);

        const contentClip = view.contentElement.querySelector('.device-mode-content-clip');
        assert.isFalse(contentClip?.classList.contains('device-mode-rulers-visible'));

        showRulersSetting.set(true);
        await view.updateComplete;

        assert.isTrue(contentClip?.classList.contains('device-mode-rulers-visible'));
      });

      it('sets correct dimensions on screenArea upon model updates', async () => {
        renderElementIntoDOM(view);
        deviceModeModel.emulate(EmulationModel.DeviceModeModel.Type.Responsive, null, null);

        sinon.stub(deviceModeModel, 'screenRect').returns(new EmulationModel.DeviceModeModel.Rect(0, 0, 800, 600));
        deviceModeModel.dispatchEventToListeners(EmulationModel.DeviceModeModel.Events.UPDATED);
        await view.updateComplete;

        const screenArea = view.contentElement.querySelector<HTMLElement>('.device-mode-screen-area');
        assert.instanceOf(screenArea, HTMLElement);
        assert.strictEqual(screenArea.style.width, '800px');
        assert.strictEqual(screenArea.style.height, '600px');
      });

      it('clicks a preset button and updates model', () => {
        renderElementIntoDOM(view);

        const setWidthAndScaleToFitSpy = sinon.spy(deviceModeModel, 'setWidthAndScaleToFit');

        const presetsContainer = view.contentElement.querySelector('.device-mode-presets-container');
        const preset = presetsContainer?.querySelector<HTMLElement>('.device-mode-preset-bar');

        assert.instanceOf(preset, HTMLElement);
        preset.click();

        sinon.assert.calledWith(setWidthAndScaleToFitSpy, 2560);
      });
    });
  });
});
