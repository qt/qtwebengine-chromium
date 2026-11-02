// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../../core/i18n/i18n.js';
import type * as PublicExtensions from '../../models/extensions/extensions.js';
import * as Buttons from '../../ui/components/buttons/buttons.js';
import * as UI from '../../ui/legacy/legacy.js';
import * as Lit from '../../ui/lit/lit.js';
import * as VisualLogging from '../../ui/visual_logging/visual_logging.js';

import * as Extensions from './extensions/extensions.js';
import extensionViewStyles from './extensionView.css.js';

const {html} = Lit;

const UIStrings = {
  /**
   * @description The button label that closes the panel that shows the extension content inside the Recorder panel.
   */
  closeView: 'Close',
  /**
   * @description The label that indicates that the content shown is provided by a browser extension.
   */
  extension: 'Content provided by a browser extension',
} as const;
const str_ = i18n.i18n.registerUIStrings('panels/recorder/ExtensionView.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export interface ViewInput {
  descriptor: PublicExtensions.RecorderPluginManager.ViewDescriptor;
  iframe: HTMLElement;
}

export interface ViewOutput {
  closeView: () => void;
}

export type View = (input: ViewInput, output: ViewOutput, target: HTMLElement) => void;

export const DEFAULT_VIEW: View = (input, output, target) => {
  const {descriptor, iframe} = input;
  // clang-format off
  Lit.render(
    html`
      <style>${extensionViewStyles}</style>
      <div class="extension-view">
        <header>
          <div class="title">
            <devtools-icon
              class="icon"
              title=${i18nString(UIStrings.extension)}
              name="extension">
            </devtools-icon>
            ${descriptor.title}
          </div>
          <devtools-button
            title=${i18nString(UIStrings.closeView)}
            jslog=${VisualLogging.close().track({click: true})}
            .data=${
              {
                variant: Buttons.Button.Variant.ICON,
                size: Buttons.Button.Size.SMALL,
                iconName: 'cross',
              } as Buttons.Button.ButtonData
            }
            @click=${output.closeView}
          ></devtools-button>
        </header>
        <main>
          ${iframe}
        </main>
    </div>
  `, target, {container: {attributes: {jslog: VisualLogging.section('extension-view')}}});
  // clang-format on
};

export class ExtensionView extends UI.Widget.VBox {
  #descriptor?: PublicExtensions.RecorderPluginManager.ViewDescriptor;
  #view: View;
  #onClose?: () => void;
  #viewOutput: ViewOutput = {
    closeView: () => {
      this.#onClose?.();
    },
  };

  set onClose(callback: () => void) {
    this.#onClose = callback;
  }

  constructor(element?: HTMLElement, view: View = DEFAULT_VIEW) {
    super(element, {useShadowDom: true});
    this.#view = view;
  }

  get descriptor(): PublicExtensions.RecorderPluginManager.ViewDescriptor|undefined {
    return this.#descriptor;
  }

  set descriptor(descriptor: PublicExtensions.RecorderPluginManager.ViewDescriptor|undefined) {
    this.#descriptor = descriptor;
    if (descriptor) {
      Extensions.ExtensionManager.ExtensionManager.instance().getView(descriptor.id).show();
    }
    this.requestUpdate();
  }

  override willHide(): void {
    super.willHide();
    if (this.#descriptor) {
      Extensions.ExtensionManager.ExtensionManager.instance().getView(this.#descriptor.id).hide();
    }
  }

  override performUpdate(): void {
    if (!this.#descriptor) {
      return;
    }
    const iframe = Extensions.ExtensionManager.ExtensionManager.instance().getView(this.#descriptor.id).frame();
    this.#view({descriptor: this.#descriptor, iframe}, this.#viewOutput, this.contentElement);
  }
}
