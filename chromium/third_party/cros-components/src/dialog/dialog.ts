/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import {css, CSSResultGroup, html, LitElement} from 'lit';

/**
 * A chromeOS compliant dialog.
 *
 * https://www.figma.com/file/1XsFoZH868xLcLPfPZRxLh/CrOS-Next---Component-Library-%26-Spec?node-id=4733%3A70425&t=sT1EGkVc7oaEEHMC-0
 */
export class Dialog extends LitElement {
  static override styles: CSSResultGroup = css`
    :host {
      display: inline-block;
    }
  `;

  override render() {
    return html``;
  }
}

customElements.define('cros-dialog', Dialog);

declare global {
  interface HTMLElementTagNameMap {
    'cros-dialog': Dialog;
  }
}
