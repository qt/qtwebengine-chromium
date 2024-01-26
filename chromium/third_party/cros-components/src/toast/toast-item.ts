/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import {css, CSSResultGroup, html, LitElement} from 'lit';

// TODO: b/301335548 - Remove these once we have elevation tokens.
const AMBIENT_SHADOW_COLOR = css`rgba(var(--cros-sys-shadow-rgb), 0.2)`;
const KEY_SHADOW_COLOR = css`rgba(var(--cros-sys-shadow-rgb), 0.1)`;

/** The visible rendered part of a cros-toast. */
export class ToastItem extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host {
      background-color: var(--cros-sys-inverse_surface);
      border-radius: 12px;
      display: inline-block;
      background-color: var(--cros-sys-inverse_surface);
      border-radius: 12px;
      font: var(--cros-body-2-font);
      box-shadow:
        0px 4px 4px 0px ${AMBIENT_SHADOW_COLOR},
        0px 0px 4px 0px ${KEY_SHADOW_COLOR};
    }

    #container {
      align-items: center;
      column-gap: 8px;
      display: flex;
      justify-content: space-between;
      padding-bottom: 16px;
      padding-inline: 20px;
      padding-top: 16px;
      width: 336px;
    }

    :host(.has-action-button) #container {
      padding-top: 8px;
      padding-bottom: 8px;
    }

    #message {
      color: var(--cros-sys-surface);
    }

    #action-container {
      display: flex;
    }
  `;

  /**
   * The visible message on the toast surface.
   * @export
   */
  message: string;

  /** @nocollapse */
  static override properties = {
    message: {type: String, reflect: true},
  };

  constructor() {
    super();
    this.message = '';
  }

  override render() {
    return html`
      <div id="container">
        <div id="message">${this.message}</div>
        <div id="action-container">
          <slot name="action"></slot>
          <slot name="dismiss"></slot>
        </div>
      </div>
    `;
  }
}

customElements.define('cros-toast-item', ToastItem);

declare global {
  interface HTMLElementTagNameMap {
    'cros-toast-item': ToastItem;
  }
}
