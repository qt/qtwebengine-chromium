/* Copyright (C) 2025 The Qt Company Ltd.
 * SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only
*/

import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './extensions_list.html.js';
import {ExtensionsUIBrowserProxy} from './extensions_ui_browser_proxy.js'
import type {ExtensionInfo} from './extensions_ui_qt.mojom-webui.js';

export class ExtensionsInfoList extends PolymerElement {
  static get is() {
    return 'extensions-info-list';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      extensionsInfo: {
        type: Array,  // Correctly define the type as Array
      },
      isLoading: {
        type: Boolean,  // Correctly define the type as Boolean
      }
    };
  }

  private extensionUiBrowserProxy: ExtensionsUIBrowserProxy =
      ExtensionsUIBrowserProxy.getInstance();
  private extensionsInfo: ExtensionInfo[] = [];
  private isLoading: boolean = false;

  override connectedCallback(): void {
    super.connectedCallback();
    this.initializeList(true);
  }

  private async initializeList(hasLoading: boolean = false): Promise<void> {
    this.isLoading = hasLoading;
    const {reports} =
        await this.extensionUiBrowserProxy.handler.getAllExtensionInfo();
    if (reports) {
      this.extensionsInfo = reports;
    }
    this.isLoading = false;
  }

  private isEnabled_(isEnabled: boolean): string {
    return isEnabled ? "Enabled" : "Disabled";
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'extensions-info-list': ExtensionsInfoList;
  }
}

customElements.define(ExtensionsInfoList.is, ExtensionsInfoList);
