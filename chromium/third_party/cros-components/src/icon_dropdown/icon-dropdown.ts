/**
 * @license
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import '@material/web/iconbutton/filled-tonal-icon-button.js';
import '@material/web/iconbutton/icon-button.js';
import '../menu/menu';

import {SelectOption} from '@material/web/select/select-option.js';
import {css, CSSResultGroup, html, LitElement, PropertyValues} from 'lit';
import {createRef, ref} from 'lit/directives/ref';
import {html as staticHtml, literal} from 'lit/static-html';

import {type Menu} from '../menu/menu';
import {type MenuItemTriggeredEvent} from '../menu/menu_item';

import {IconDropdownOption} from './icon-dropdown-option';

/** The SVG to use in the trailing icon slot when the dropdown is closed. */
const ARROW_DROP_DOWN_SVG = html`
  <svg xmlns="http://www.w3.org/2000/svg" width="20" height="20" viewBox="0 -960 960 960">
    <path d="M480-376.348 274.848-581.5h410.304L480-376.348Z"/>
  </svg>`;

/** A chromeOS compliant icon-dropdown. */
export class IconDropdown extends LitElement {
  static override shadowRootOptions = {
    ...LitElement.shadowRootOptions,
    delegatesFocus: true,
  };

  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host {
      --_button-container-color: var(--cros-sys-surface_variant);
      --_button-container-height: 40px;
      --_button-container-shape: 12px;
      --_button-container-width: 40px;
      --_button-icon-color: var(--cros-sys-on_surface);
      --_button-icon-focus-color: var(--cros-sys-on_surface);
      --_button-icon-size: 20px;
      display: inline-block;
      font: var(--cros-body-0-font);
    }

    :host([shape="circle"]) {
      --_button-container-shape: 100vmax;
      --_button-container-width: 48px;
    }

    :host([size="large"]) {
      --_button-container-height: 56px;
      --_button-container-width: 72px;
      --_button-icon-size: 24px;
    }

    :host([shape="square"][size="large"]) {
      --_button-container-shape: 16px;
    }

    :host([surface="base"]) md-icon-button {
      --_button-icon-color: var(--cros-sys-on_surface);
      --_button-icon-focus-color: var(--cros-sys-on_surface);;
    }

    :host([surface="prominent"]) md-icon-button {
      --_button-icon-color: var(--cros-sys-on_primary);
      --_button-icon-focus-color: var(--cros-sys-on_primary);
    }

    :host([surface="subtle"]) md-icon-button {
      --_button-icon-color: var(--cros-sys-on_primary_container);
      --_button-icon-focus-color: var(--cros-sys-on_primary_container);
    }

    .button-icon-container {
      display: flex;
      place-content: center;
    }

    .button-icon-container .dropdown-arrow {
      color: var(--_button-icon-color);
      fill: var(--_button-icon-color);
      /* 5px overlap with the button icon. */
      margin-inline: -5px 1px;
    }

    .button-icon-container .dropdown-icon {
      color: var(--_button-icon-color);
      fill: var(--_button-icon-color);
      margin-inline: 4px 0;
    }

    md-filled-tonal-icon-button {
      --md-filled-tonal-icon-button-container-color: var(--_button-container-color);
      --md-filled-tonal-icon-button-container-height: var(--_button-container-height);
      --md-filled-tonal-icon-button-container-shape: var(--_button-container-shape);
      --md-filled-tonal-icon-button-container-width: var(--_button-container-width);
      --md-filled-tonal-icon-button-icon-color: var(--_button-icon-color);
      --md-filled-tonal-icon-button-icon-focus-color: var(--_button-icon-focus-color);
      --md-filled-tonal-icon-button-icon-size: var(--_button-icon-size);
    }

    md-icon-button {
      --md-icon-button-focus-icon-color: var(--_button-icon-focus-color);
      --md-icon-button-icon-color: var(--_button-icon-color);
      --md-icon-button-icon-size: var(--_button-icon-size);
      --md-icon-button-state-layer-height: var(--_button-container-height);
      --md-icon-button-state-layer-shape: var(--_button-container-shape);
      --md-icon-button-state-layer-width: var(--_button-container-width);
    }

    md-filled-tonal-icon-button::part(focus-ring),
    md-icon-button::part(focus-ring) {
      --md-focus-ring-color: var(--cros-sys-focus_ring);
      --md-focus-ring-width: 2px;
    }
    :host([surface="prominent"]) md-filled-tonal-icon-button::part(focus-ring),
    :host([surface="prominent"]) md-icon-button::part(focus-ring) {
      --md-focus-ring-color: var(--cros-sys-inverse_focus_ring);
    }

    slot[name='button-icon']::slotted(*) {
      block-size: var(--_button-icon-size);
      color: var(--_button-icon-color);
      fill: var(--_button-icon-color);
      inline-size: var(--_button-icon-size);
    }
  `;

  /** @nocollapse */
  static override properties = {
    ariaLabel: {type: String, reflect: true, attribute: 'aria-label'},
    value: {type: String},
    shape: {type: String, reflect: true},
    size: {type: String, reflect: true},
    buttonStyle: {type: String},
    surface: {type: String, reflect: true},
  };

  /** @nocollapse */
  static events = {CHANGE: 'change'} as const;

  /**
   * The value of the first selected option in the dropdown or an empty string
   * if no options are selected.
   * @export
   */
  value: string;

  /**
   * The shape of the dropdown icon button. Used to apply CSS.
   * @export
   */
  shape: 'circle'|'square';

  /**
   * The size of the dropdown icon button. Used to apply CSS.
   * @export
   */
  size: 'default'|'large';

  /**
   * Whether the dropdown icon button style is filled or floating button.
   * @export
   */
  buttonStyle: 'filled'|'floating';

  /**
   * @export
   * The background the icon button sits on. Used to apply CSS.
   */
  surface: 'base'|'prominent'|'subtle';

  protected menuRef = createRef<Menu>();

  constructor() {
    super();

    this.value = '';
    this.shape = 'square';
    this.size = 'default';
    this.buttonStyle = 'filled';
    this.surface = 'base';
  }

  get anchor() {
    if (this.buttonStyle === 'floating') {
      return this.renderRoot?.querySelector('md-icon-button');
    }

    return this.renderRoot?.querySelector('md-filled-tonal-icon-button');
  }

  get crosMenu() {
    return this.renderRoot?.querySelector('cros-menu');
  }

  get options(): SelectOption[] {
    return (this.crosMenu?.items ?? []) as SelectOption[];
  }

  override focus() {
    // TODO: b/339905985 - Let the menu handle focus when the menu is open.
    this.anchor?.focus();
  }

  override updated(changedProperties: PropertyValues<IconDropdown>) {
    if (changedProperties.has('value')) {
      this.updateOptionsSelected();
    }
  }

  /**
   * IconDropdown acts as a `select` element thus it requires the option list
   * and "field" to be updated to display and focus correctly.
   */
  override async getUpdateComplete() {
    await this.anchor?.updateComplete;
    await this.crosMenu?.updateComplete;
    return super.getUpdateComplete();
  }

  override render() {
    return html`
      <div @cros-menu-item-triggered=${this.onMenuItemTriggered}>
        ${this.renderButtonContent()}
        ${this.renderMenuContent()}
      </div>
    `;
  }

  private renderButtonContent() {
    const tag = this.buttonStyle === 'floating' ?
        literal`md-icon-button` :
        literal`md-filled-tonal-icon-button`;

    return staticHtml`
      <${tag}
        id="button"
        aria-label=${this.ariaLabel ?? ''}
        @click=${() => this.toggleMenu()}
        @keydown=${(e: KeyboardEvent) => this.onKeydown(e)}
      >
        ${this.getAnchorContent()}
      </${tag}>
    `;
  }

  private getAnchorContent() {
    return html`
        <div class="button-icon-container">
          <span class="dropdown-icon"><slot name="button-icon"></slot></span>
          <span class="dropdown-arrow">${ARROW_DROP_DOWN_SVG}</span>
        </div>
      `;
  }

  private onKeydown(e: KeyboardEvent) {
    if (e.key === 'Enter' || e.key === ' ') {
      this.toggleMenu();
    }
  }

  private toggleMenu() {
    this.menuRef.value?.open ? this.menuRef.value?.close() :
                               this.menuRef.value?.show();
  }

  private renderMenuContent() {
    return html`
      <cros-menu anchor="button" ${ref(this.menuRef)}>
        <slot></slot>
      </cros-menu>
    `;
  }

  /**
   * Handles dropdown option selection. If the selected option is the same as
   * the current value, do nothing. Otherwise, update the value and dispatch a
   * change event.
   *
   * @param e The event object.
   */
  private onMenuItemTriggered(e: MenuItemTriggeredEvent) {
    const menuItem = e.detail.menuItem as IconDropdownOption;
    // If menu item with [itemEnd="switch"] the detail will not be sent.
    // TODO: b/339905985 - Handle menu-item where [itemEnd="switch"]. These
    // menu-items are not standard options and probably need to be handle by the
    // parent component to implement the correct behavior.
    if (!menuItem) {
      return;
    }
    if (menuItem.value === this.value) {
      return;
    }
    this.value = menuItem.value ?? '';
    this.updateOptionsSelected();
    this.dispatchEvent(
        new Event(IconDropdown.events.CHANGE, {bubbles: true, composed: true}));
  }

  private updateOptionsSelected() {
    for (const option of this.options) {
      option.selected = option.value === this.value;
    }
  }
}

customElements.define('cros-icon-dropdown', IconDropdown);

declare global {
  interface HTMLElementEventMap {
    [IconDropdown.events.CHANGE]: Event;
  }

  interface HTMLElementTagNameMap {
    'cros-icon-dropdown': IconDropdown;
  }
}
