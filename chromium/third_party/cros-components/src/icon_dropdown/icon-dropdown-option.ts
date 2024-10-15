/**
 * @license
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import {SelectOption} from '@material/web/select/select-option.js';
import {css} from 'lit';

import {MenuItem, MenuItemTriggeredEvent} from '../menu/menu_item';

/** A chromeOS compliant option to be used in cros-icon-dropdown. */
export class IconDropdownOption extends MenuItem implements SelectOption {
  /** @nocollapse */
  static override styles = [
    MenuItem.styles, css`
      md-menu-item {
        --md-menu-item-label-text-font: var(--cros-dropdown-option-text-font, var(--cros-button-2-font-family));
      }
    `
  ];

  /** @nocollapse */
  static override properties = {
    ...MenuItem.properties,
    value: {type: String, reflect: true},
  };

  /**
   * Internal value associated with the option.
   */
  private internalValue: string;

  get value() {
    if (this.internalValue === '') {
      return this.headline;
    }

    return this.internalValue;
  }

  set value(value: string) {
    this.internalValue = value;
  }

  // When extending a lit element, it seems that getters and setters get
  // clobbered. To avoid this we specifically reimplement needed getters/setters
  // below to ensure they function correctly on cros-icon-dropdown-option. These
  // should be identical to the functions they override in cros-menu-item.
  override get selected() {
    return this.renderRoot?.querySelector('md-menu-item')?.selected ?? false;
  }

  override set selected(selected: boolean) {
    const item = this.renderRoot?.querySelector('md-menu-item');
    if (!item) {
      this.missedPropertySets.selected = selected;
    } else {
      item.selected = selected;
    }
  }

  // SelectOption implementation:
  get displayText() {
    return this.headline;
  }

  constructor() {
    super();

    this.type = 'option';
    this.internalValue = '';
    this.keepOpen = false;
    this.selected = false;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.addEventListener('click', this.onClickHandler);
    this.addEventListener('keydown', this.onKeyDownHandler);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    this.removeEventListener('click', this.onClickHandler);
    this.removeEventListener('keydown', this.onKeyDownHandler);
  }

  private onClickHandler = () => {
    this.onItemTriggered();
  };

  private onKeyDownHandler = (e: KeyboardEvent) => {
    if (e.key === 'Enter' || e.key === ' ') {
      this.onItemTriggered();
    }
  };

  // Notifies the parent icon-dropdown that this option was triggered.
  protected onItemTriggered() {
    this.selected = true;
    this.dispatchEvent(
        new CustomEvent(IconDropdownOption.events.MENU_ITEM_TRIGGERED, {
          bubbles: true,
          composed: true,
          detail: {menuItem: this},
        }));
  }
}

customElements.define('cros-icon-dropdown-option', IconDropdownOption);

declare global {
  interface HTMLElementEventMap {
    [MenuItem.events.MENU_ITEM_TRIGGERED]: MenuItemTriggeredEvent;
  }

  interface HTMLElementTagNameMap {
    'cros-icon-dropdown-option': IconDropdownOption;
  }
}
