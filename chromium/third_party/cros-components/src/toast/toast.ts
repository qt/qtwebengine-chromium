/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import './toast-item';

import {css, CSSResultGroup, html, LitElement} from 'lit';

/**
 * Event interface for toast toggle events. Closure fires off the wrong event
 * type that is missing these fields.
 * TODO: b/307830635 - Remove once ToggleEvents are supported.
 */
declare interface ToggleEvent extends Event {
  newState: 'open'|'close';
  oldState: 'open'|'close';
}

/** A chromeOS compliant toast. */
export class Toast extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host(:popover-open) {
      background: none;
      border: none;
      width: 100%;
    }

    #container {
      bottom: 0px;
      margin-left: 24px;
      margin-bottom: 24px;
      position: fixed;
    }
  `;

  /**
   * The visible message on the toast surface.
   * @export
   */
  message: string;

  /** @export */
  override role = 'alert';

  /**
   * Timeout between the toast showing and hiding, in milliseconds.
   */
  timeout = -1;

  /** @export */
  override popover = 'manual';

  /** @nocollapse */
  static override properties = {
    message: {type: String, reflect: true},
    timeout: {type: Number, reflect: true},
  };

  /** @nocollapse */
  static events = {
    /** The toast popup timed out and closed itself. */
    TIMEOUT: 'cros-toast-timeout',
  } as const;

  private pendingTimeout: number|null = null;

  constructor() {
    super();
    this.message = '';
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('toggle', (e) => this.onToggle(e as ToggleEvent));
  }

  override render() {
    return html`
        <div id="container">
          <cros-toast-item message=${this.message}>
            <slot
                name="action"
                slot="action"
                @slotchange=${this.handleSlotChanged}>
            </slot>
            <slot
                name="dismiss"
                slot="dismiss"
                @slotchange=${this.handleSlotChanged}>
            </slot>
          </cros-toast-item>
        </div>
    `;
  }

  private onToggle(e: ToggleEvent) {
    if (e.newState === 'open') {
      this.startTimeout();
    }
  }

  private startTimeout() {
    if (this.timeout < 1 || this.pendingTimeout) return;
    this.pendingTimeout = setTimeout(() => {
      this.hidePopover();
      this.dispatchEvent(new CustomEvent('cros-toast-timeout'));
      clearTimeout(this.pendingTimeout!);
      this.pendingTimeout = null;
    }, this.timeout);
  }

  private handleSlotChanged() {
    const hasActionButtons =
        this.querySelectorAll(`*[slot="action"], [slot="dismiss"]`);
    for (const toastItem of this.shadowRoot!.querySelectorAll(
             `cros-toast-item`)) {
      toastItem.classList.toggle(
          'has-action-button', hasActionButtons.length > 0);
    }
  }
}

customElements.define('cros-toast', Toast);

declare global {
  interface HTMLElementTagNameMap {
    'cros-toast': Toast;
  }
  interface HTMLElementEventMap {
    [Toast.events.TIMEOUT]: Event;
  }
}
