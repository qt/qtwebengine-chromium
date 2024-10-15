/**
 * @license
 * Copyright 2024 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import '../card/card';

import {css, CSSResultGroup, html, LitElement, PropertyValues} from 'lit';

import {AccordionItem, type AccordionItemExpandedEvent} from './accordion_item';

/** A chromeOS compliant accordion. */
export class Accordion extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
      cros-card {
        --cros-card-padding: 0;
      }

      ::slotted(cros-accordion-item:last-of-type) {
        --cros-accordion-item-separator-display: none;
      }
  `;

  /** @nocollapse */
  static override properties = {
    autoCollapse: {type: Boolean, attribute: 'auto-collapse'},
    quick: {type: Boolean},
  };

  /**
   * When true, opening a child `<cros-accordion-item>` in this component will
   * close any other open `<cros-accordion-item>`s. When false, allow multiple
   * `<cros-accordion-item>`s to be open at once.
   * @export
   */
  autoCollapse: boolean;

  /**
   * When true, all child `<cros-accordion-item>`s within this
   * `<cros-accordion>` will skip animations. This overrides the `quick`
   * property set on any child `<cros-accordion-item>`s.
   * @export
   */
  quick: boolean;

  constructor() {
    super();
    this.autoCollapse = false;
    this.quick = false;

    this.addEventListener(
        AccordionItem.events.ACCORDION_ITEM_EXPANDED,
        (e: AccordionItemExpandedEvent) => this.onAccordionItemExpanded(e));
  }

  override updated(changedProperties: PropertyValues<this>) {
    if (changedProperties.has('quick')) {
      // Set the `quick` property on all child `<cros-accordion-item>`s.
      for (const child of this.children) {
        if (child instanceof AccordionItem) {
          child.quick = this.quick;
        }
      }
    }
  }

  override render() {
    return html`
      <cros-card cardStyle="outline">
        <slot></slot>
      </cros-card>
    `;
  }

  private onAccordionItemExpanded(e: AccordionItemExpandedEvent) {
    if (this.autoCollapse) {
      this.closeOtherAccordionItems(e.detail.accordionItem);
    }
  }

  private closeOtherAccordionItems(accordionItem: AccordionItem) {
    for (const child of this.children) {
      if (child instanceof AccordionItem && child !== accordionItem) {
        child.expanded = false;
      }
    }
  }
}

customElements.define('cros-accordion', Accordion);

declare global {
  interface HTMLElementTagNameMap {
    'cros-accordion': Accordion;
  }
}
