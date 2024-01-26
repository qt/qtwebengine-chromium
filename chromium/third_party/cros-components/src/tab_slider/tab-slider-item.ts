/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import '@material/web/ripple/ripple.js';
import '@material/web/focus/md-focus-ring.js';

import {css, CSSResultGroup, html, LitElement} from 'lit';

/** A chromeOS compliant tab-slider. */
export class TabSliderItem extends LitElement {
  /**
   * TabSliderItem doesn't set its own background, because the parent TabSlider
   * needs to control it to create the sliding animation.
   * @nocollapse
   */
  static override styles: CSSResultGroup = css`
    :host {
      border-radius: 100vh;
      display: inline-block;
      font: var(--cros-button-2-font);
      height: fit-content;
      position: relative;
      width: fit-content;
    }

    #container {
      align-items: center;
      background: transparent;
      border: none;
      border-radius: 100vh;
      color: var(--cros-sys-on_surface_variant);
      font: inherit;
      height: 40px;
      justify-content: center;
      padding-inline-start: 16px;
      padding-inline-end: 16px;
      position: relative;
      text-align: center;
      width: fit-content;
    }

    :host([selected]) #container {
      color: var(--cros-sys-on_primary);
    }

    #container:focus-visible {
      outline: none;
    }

    :host(:not([selected])) #container:hover {
      color: var(--cros-sys-on_surface);
    }

    md-focus-ring {
      animation-duration: 0s;
      --md-focus-ring-color: var(--cros-sys-focus_ring);
      --md-focus-ring-width: 2px;
    }

    md-ripple {
      display: none;
    }

    :host([selected]:active) md-ripple {
      display: unset;
      --md-ripple-hover-color: var(--cros-sys-ripple_primary);
      --md-ripple-hover-opacity: 1;
      --md-ripple-pressed-color: var(--cros-sys-ripple_primary);
      --md-ripple-pressed-opacity: 1;
    }
  `;

  /** @export */
  label = 'Item';

  /** @export */
  selected = false;

  /** @nocollapse */
  static override properties = {
    label: {type: String, reflect: true},
    selected: {type: Boolean, reflect: true},
  };


  override render() {
    return html`
        <div>
          <button id="container">
            <span>${this.label}</span>
            <md-ripple for="container"></md-ripple>
            <md-focus-ring for="container"></md-focus-ring>
          </button>
        </div>
    `;
  }
}

customElements.define('cros-tab-slider-item', TabSliderItem);

declare global {
  interface HTMLElementTagNameMap {
    'cros-tab-slider-item': TabSliderItem;
  }
}
