/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import {css, CSSResultGroup, html, LitElement} from 'lit';

/**
 * A chromeOS compliant tab-slider.
 *
 * https://www.figma.com/file/1XsFoZH868xLcLPfPZRxLh/CrOS-Next---Component-Library-%26-Spec?type=design&node-id=2852-19308&mode=design&t=uXwuM7QRyBvvkmwP-0
 */
export class TabSlider extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host {
      background: rgba(var(--cros-sys-surface_variant-rgb), 0.8);
      border-radius: 100vh;
      display: inline-block;
      font: var(--cros-body-0-font);
      padding: 2px;
    }

    #items {
      display: flex;
    }

    /** TODO(b/296808808) - Replace with animation. */
    #items::slotted(cros-tab-slider-item[selected]) {
      background: var(--cros-sys-primary);
    }
  `;

  /** @nocollapse */
  static override properties = {};

  override render() {
    return html`
      <div>
        <slot id="items"></slot>
      </div>
    `;
  }
}

customElements.define('cros-tab-slider', TabSlider);

declare global {
  interface HTMLElementTagNameMap {
    'cros-tab-slider': TabSlider;
  }
}
