/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import '../button/button';
import '../chip/chip';

import {Task} from '@lit/task';
import {css, CSSResultGroup, html, LitElement} from 'lit';

import {maybeSafeHTML} from './maybe_safe_html';

interface StringSource {
  getAriaLabelForLink(arg: string): string;
  MSG_FEEDBACK_TITLE: string;
  MSG_FEEDBACK_SUBTITLE: string;
  MSG_FEEDBACK_QUESTION: string;
  MSG_FEEDBACK_QUESTION_PLACEHOLDER: string;
  MSG_OFFENSIVE_OR_UNSAFE: string;
  MSG_FACTUALLY_INCORRECT: string;
  MSG_LEGAL_ISSUE: string;
  getFeedbackDisclaimer(
      linkPolicyBegin: string,
      linkPolicyEnd: string,
      linkTermsOfServiceBegin: string,
      linkTermsOfServiceEnd: string,
      ): string;
  MSG_PRIVACY_POLICY: string;
  MSG_TERMS_OF_SERVICE: string;
  MSG_CANCEL: string;
  MSG_SEND: string;
}

const PRIVACY_POLICY_URL = 'https://policies.google.com/privacy';
const TERMS_OF_SERVICE_URL = 'https://policies.google.com/terms';

/**
 * Component so that users can send feedback about a particular suggestion that
 * we make.
 */
export class OrcaFeedback extends LitElement {
  /** @nocollapse */
  static override styles: CSSResultGroup = css`
    :host {
      --footer-height: 61px;
      --top-padding: 32px;
      background-color: var(--cros-sys-base_elevated);
      display: block;
      padding-top: var(--top-padding);
      width: 512px;
    }

    a.link {
      cursor: pointer;
      color: var(--cros-sys-primary);
      text-decoration: underline;
      font: inherit;
      display: inline;
    }

    textarea {
      background-color: var(--cros-sys-input_field_on_base);
      border-radius: 8px;
      color: var(--cros-sys-secondary);
      margin-block: -16px; /* only 8px gap around textbox */
      flex-shrink: 0;
      font: var(--cros-body-2-font);
      height: 104px;
      padding-inline: 16px;
      padding-block: 8px;
    }

    textarea::placeholder {
      font: var(--cros-body-2-font);
      color: var(--cros-sys-secondary);
    }

    .scrollable {
      box-sizing: border-box;
      display: flex;
      flex-direction: column;
      flex-shrink: 0;
      gap: 24px;
      max-height: calc(600px - var(--footer-height) - var(--top-padding));
      overflow-y: scroll;
      padding-bottom: 24px;
      padding-inline: 32px;

      .heading {
        font: var(--cros-display-6-font);
        color: var(--cros-sys-on_surface);
      }

      .body {
        font: var(--cros-body-2-font);
        color: var(--cros-sys-on_surface_variant);
      }

      .legal-footer-text {
        font: var(--cros-annotation-1-font);
        color: var(--cros-sys-on_surface_variant);
      }

      .question {
        font: var(--cros-headline-1-font);
        color: var(--cros-sys-on_surface);
      }

      .buttons {
        display: flex;
        flex-wrap: wrap;
        gap: 8px;
      }

      .extra-info {
        font: var(--cros-annotation-2-font);
        color: var(--cros-sys-on_surface_variant);
        border: 1px solid var(--cros-sys-separator);
        border-radius: 8px;
        padding: 12px;
        white-space: pre-line;

        .key {
          font-weight: 700;
        }

        .value {
        }
      }
    }

    .scrollable[disablescrollingfortesting] {
      max-height: none;
    }

    .footer {
      position: sticky;
      bottom: 0;
      .footer-buttons {
        align-items: center;
        background-color: var(--cros-sys-base_elevated);
        box-sizing: border-box;
        display: flex;
        flex-shrink: 0;
        gap: 8px;
        padding-inline-end: 16px;
        height: var(--footer-height);
        justify-content: flex-end;
        position: sticky;
        right: 0;
        width: 100%;
      }
    }
    .footer::before {
      content: ' ';
      width: 100%;
      height: 1px;
      flex-shrink: 0;
      background-color: var(--cros-sys-separator);
      display: block;
    }
  `;

  /** @nocollapse */
  static override properties = {
    extraInfoCallback: {
      attribute: false,
    },
    revertToPreviousScreen: {},
    disableScrollingForTesting: {},
    submitFeedback: {
      attribute: false,
    },
    openUrl: {
      attribute: false,
    },
    stringSource: {
      attribute: false,
    },
  };
  /** @export */
  extraInfoCallback = async () => '';
  /** @export */
  revertToPreviousScreen = () => {};
  /** @export */
  disableScrollingForTesting = false;
  private readonly extraInfoTask = new Task(this, {
    task: async ([callback]: [() => Promise<string>]) => await callback(),
    args: () => [this.extraInfoCallback] as const as [() => Promise<string>],
  });
  /** @export */
  submitFeedback: (userDescription: string) => void = () => {};
  /** @export */
  openUrl: (url: string) => void = () => {};
  /**
   * Must be set, to ensure nice user facing strings
   * @export
   **/
  stringSource: StringSource|null = null;
  get userDescription(): HTMLTextAreaElement | undefined {
    return this.shadowRoot!.querySelector('#userDescription') as
      | HTMLTextAreaElement
      | undefined;
  }

  getSelectedChips() {
    const chips = this.shadowRoot!.querySelectorAll('cros-chip');
    return Array.from(chips).filter((chip) => chip.selected);
  }

  isFeedbackEnabled() {
    if (this.userDescription?.value !== '') {
      return true;
    }
    return this.getSelectedChips().length > 0;
  }

  // Feedback user description includes both the freeform textarea and the selected chips.
  getFeedbackUserDescription() {
    // The labels on chips are internationalized; we should send `data-label`
    // (which is in English) to make Listenr report easier to understand.
    const chipsDescription = this.getSelectedChips()
                                 .map((chip) => `#${chip.dataset['label']}`)
                                 .join(' ');
    return `${this.userDescription?.value ?? ''}\n${chipsDescription}`;
  }

  constructor() {
    super();
    document.title = this.stringSource?.MSG_FEEDBACK_TITLE ?? '';
  }

  private privacyLinkClickListener: ((e: Event) => void)|null = null;
  private termsOfServiceLinkClickListener: ((e: Event) => void)|null = null;

  override updated() {
    const privacyPolicyLink = this.shadowRoot?.querySelector('#privacy-policy');
    if (privacyPolicyLink) {
      if (this.privacyLinkClickListener) {
        privacyPolicyLink.removeEventListener(
            'click', this.privacyLinkClickListener);
      }
      this.privacyLinkClickListener = (e: Event) => {
        e.preventDefault();
        this.openPrivacyPolicy();
      };
      privacyPolicyLink.addEventListener(
          'click', this.privacyLinkClickListener);
    }

    const termsOfServiceLink =
        this.shadowRoot?.querySelector('#terms-of-service');
    if (termsOfServiceLink) {
      if (this.termsOfServiceLinkClickListener) {
        termsOfServiceLink.removeEventListener(
            'click', this.termsOfServiceLinkClickListener);
      }
      this.termsOfServiceLinkClickListener = (e: Event) => {
        e.preventDefault();
        this.openTermsOfService();
      };
      termsOfServiceLink.addEventListener(
          'click', this.termsOfServiceLinkClickListener);
    }
  }

  private openPrivacyPolicy() {
    this.openUrl(PRIVACY_POLICY_URL);
  }

  private openTermsOfService() {
    this.openUrl(TERMS_OF_SERVICE_URL);
  }

  override render() {
    return html`
      <div class="scrollable" ?disablescrollingfortesting="${
        this.disableScrollingForTesting}">
      <div class="heading">${this.stringSource?.MSG_FEEDBACK_TITLE}</div>
      <div class="body">
        ${this.stringSource?.MSG_FEEDBACK_SUBTITLE}
      </div>
      <div class="question">${this.stringSource?.MSG_FEEDBACK_QUESTION}</div>
      <textarea
        id="userDescription"
        placeholder=${
        this.stringSource?.MSG_FEEDBACK_QUESTION_PLACEHOLDER ?? ''}
        @input=${() => void this.requestUpdate()}
        ></textarea>
      <div class="buttons">
        <cros-chip
          data-label="Offensive/Unsafe"
          label=${this.stringSource?.MSG_OFFENSIVE_OR_UNSAFE ?? ''}
          @click=${() => void this.requestUpdate()}
        >
        </cros-chip>
        <cros-chip
          data-label="Factually incorrect"
          label=${this.stringSource?.MSG_FACTUALLY_INCORRECT ?? ''}
          @click=${() => void this.requestUpdate()}
        >
        </cros-chip>
        <cros-chip
          data-label="Legal issue"
          label=${this.stringSource?.MSG_LEGAL_ISSUE ?? ''}
          @click=${() => void this.requestUpdate()}
        >
        </cros-chip>
      </div>
      <div class="legal-footer-text">
        ${
        /**
         * There is no way to pass event handlers directly to safeHTML. Thus we
         * expose anchor elements by id and add event listeners in firstUpdated.
         */
        maybeSafeHTML(
            this.stringSource?.getFeedbackDisclaimer(
                /*linkPolicyBegin=*/
                `<a id="privacy-policy" class="link" role="link" aria-label="${
                    this.stringSource?.getAriaLabelForLink(
                        this.stringSource.MSG_PRIVACY_POLICY,
                        ) ??
                    ''}" href="${PRIVACY_POLICY_URL}">`,
                /*linkPolicyEnd=*/ '</a>',
                /*linkTermsOfServiceBegin=*/
                `<a id="terms-of-service" class="link" role="link" aria-label="${
                    this.stringSource?.getAriaLabelForLink(
                        this.stringSource.MSG_TERMS_OF_SERVICE,
                        ) ??
                    ''}" href="${TERMS_OF_SERVICE_URL}">`,
                /*linkTermsOfServiceEnd=*/ '</a>',
                ) ??
            '')}
      </div>
      <div class="extra-info">${this.extraInfoTask.render({
      complete: (extraInfo: string) => extraInfo,
    })}</div>
  </div>
      <div class="footer">
        <div class="footer-buttons">
        <cros-button
          label=${this.stringSource?.MSG_CANCEL ?? ''}
          button-style="secondary"
          @click=${() => void this.revertToPreviousScreen()}>
        </cros-button>
        <cros-button
          label=${this.stringSource?.MSG_SEND ?? ''}
          button-style="primary"
          ?disabled=${!this.isFeedbackEnabled()}
          @click=${() => {
      this.submitFeedback(this.getFeedbackUserDescription());
      this.revertToPreviousScreen();
    }}
        ></cros-button>
        </div class="footer-buttons">
      </div>
    `;
  }
}

customElements.define('mako-orca-feedback', OrcaFeedback);

declare global {
  interface HTMLElementTagNameMap {
    'mako-orca-feedback': OrcaFeedback;
  }
}
