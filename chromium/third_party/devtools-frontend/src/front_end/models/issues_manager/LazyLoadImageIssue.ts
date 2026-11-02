// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as i18n from '../../core/i18n/i18n.js';
import type * as SDK from '../../core/sdk/sdk.js';
import * as Protocol from '../../generated/protocol.js';

import {type AffectedElement, Issue, IssueCategory, IssueKind} from './Issue.js';
import type {MarkdownIssueDescription} from './MarkdownIssueDescription.js';

const UIStrings = {
  /**
   * @description Link title for the lazy-loaded image with zero size issue in the Issues panel.
   */
  lazyLoadImageZeroSize: 'Lazy-loaded images should have explicit dimensions',
} as const;
const str_ = i18n.i18n.registerUIStrings('models/issues_manager/LazyLoadImageIssue.ts', UIStrings);
const i18nString = i18n.i18n.getLocalizedString.bind(undefined, str_);

export class LazyLoadImageIssue extends Issue<Protocol.Audits.LazyLoadImageIssueDetails> {
  constructor(issueDetails: Protocol.Audits.LazyLoadImageIssueDetails, issuesModel: SDK.IssuesModel.IssuesModel|null) {
    const umaCode = [Protocol.Audits.InspectorIssueCode.LazyLoadImageIssue, 'ZeroSize'].join('::');
    super({code: Protocol.Audits.InspectorIssueCode.LazyLoadImageIssue, umaCode}, issueDetails, issuesModel);
  }

  primaryKey(): string {
    return `${this.code()}-(${this.details().nodeId})-(${this.details().url})`;
  }

  getCategory(): IssueCategory {
    return IssueCategory.OTHER;
  }

  getDescription(): MarkdownIssueDescription {
    return {
      file: 'lazyLoadImageZeroSize.md',
      links: [
        {
          link: 'https://web.dev/articles/browser-level-image-lazy-loading/#dimension-attributes',
          linkTitle: i18nString(UIStrings.lazyLoadImageZeroSize),
        },
      ],
    };
  }

  elementCount(): number {
    return this.details().nodeId ? 1 : 0;
  }

  override elements(): Iterable<AffectedElement> {
    if (this.details().nodeId) {
      const target = this.model()?.target();
      return [{
        backendNodeId: this.details().nodeId,
        nodeName: 'img',
        target: target || null,
      }];
    }
    return [];
  }

  getKind(): IssueKind {
    return IssueKind.IMPROVEMENT;
  }

  static fromInspectorIssue(issuesModel: SDK.IssuesModel.IssuesModel|null,
                            inspectorIssue: Protocol.Audits.InspectorIssue): LazyLoadImageIssue[] {
    const details = inspectorIssue.details.lazyLoadImageIssueDetails;
    if (!details) {
      console.warn('Lazy-loaded image issue without details received.');
      return [];
    }
    return [new LazyLoadImageIssue(details, issuesModel)];
  }
}
