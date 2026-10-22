/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

import {DirectiveResult} from 'lit/directive.js';
import {unsafeHTML, UnsafeHTMLDirective} from 'lit/directives/unsafe-html.js';

/**
 * Util method to hide HTML sanitization logic so we can support both Open
 * Source Lit and Google3 Lit.
 */
export function maybeSafeHTML(
    html: string,
    ): DirectiveResult<typeof UnsafeHTMLDirective> {
  // TODO: b/338151548 - Add sanitizer here.
  return unsafeHTML(html);
}
