/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import '@material/web/tabs/tabs';

import {css, CSSResultGroup, html, LitElement} from 'lit';

/** The default gap value between each tab in a tab group. */
export const TABS_GAP = '8px';

/**
 * A chromeOS compliant tabs component.
 * See spec
 * https://www.figma.com/file/1XsFoZH868xLcLPfPZRxLh/CrOS-Next---Component-Library-%26-Spec?type=design&node-id=4020-63202
 */
export class Tabs extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host {
      display: inline-block;
    }

    md-tabs {
      --md-divider-color: transparent;
    }

    slot {
      display: flex;
      gap: var(--cros-tabs-gap, 8px);
    }
  `;

  /**
   * The index of the active tab.
   * @export
   */
  activeTabIndex: number;

  /** @nocollapse */
  static override properties = {
    activeTabIndex: {type: Number, reflect: true},
  };

  constructor() {
    super();
    this.activeTabIndex = 0;
  }

  override render() {
    return html`
      <md-tabs
        .activeTabIndex=${this.activeTabIndex}>
        <slot></slot>
      </md-tabs>
    `;
  }
}

customElements.define('cros-tabs', Tabs);

declare global {
  interface HTMLElementTagNameMap {
    'cros-tabs': Tabs;
  }
}
