---
name: devtools-unicode-escaping
description: Guidelines for escaping strings and handling user-controlled data in DevTools to prevent layout bleed-through, XSS, and security issues.
---

# Unicode Escaping and User-Controlled Data Guidelines

When displaying inspected data, user-controlled strings, or untrusted input in the DevTools UI (e.g., console messages, object properties, DOM tree nodes, or UI titles/descriptions), you must ensure they are properly escaped to prevent layout bleed-through (such as Right-to-Left leaks) or security vulnerabilities.

DevTools provides two primary Unicode escaping functions in `Platform.StringUtilities`:

## 1. Escaping for Text Representation (`escapeUnicodeAsText`)

Use `Platform.StringUtilities.escapeUnicodeAsText(content)` when rendering values that developers need to inspect (e.g. string values inside the Console or the Object properties view) where hidden, invisible, or formatting characters should be made explicitly visible.

* **Behavior**: Escapes all formatting and surrogate characters into literal Unicode escape sequences (e.g., `\u202E`, `\u200B`).
* **Example Usage**:
  ```typescript
  const text = Platform.StringUtilities.escapeUnicodeAsText(JSON.stringify(description));
  ```

## 2. Escaping for UI Rendering (`safeEscapeUnicode`)

Use `Platform.StringUtilities.safeEscapeUnicode(content)` when rendering content inside templates or HTML markup where you want safe layout-critical zero-width formatting characters to function normally for word wrapping or rendering layout, but want to escape dangerous layout-disrupting characters (like bidi overrides).

* **Behavior**: Escapes dangerous formatting and surrogate characters (like `\u202E`), but leaves safe formatting characters untouched:
  * Zero Width Space (`\u200B`)
  * Zero Width Non-Joiner (`\u200C`)
  * Zero Width Joiner (`\u200D`)
* **Example Usage**:
  Used automatically by the global Lit template wrapper.

---

## 3. Global Lit Template Wrapper

DevTools wraps Lit's default `html` function inside `front_end/ui/lit/strip-whitespace.ts` (re-exported via `ui/lit/lit.js`). This wrapper automatically intercepts and escapes standard string values using `Platform.StringUtilities.safeEscapeUnicode(val)` at runtime.

* **What is escaped automatically**: Any standard string interpolated directly in a template (e.g., `html`<span>${myString}</span>``).
* **Array items**: Array values are traversed recursively, and string elements inside arrays are escaped automatically.
* **Lit directives**: Directives (like `ifDefined`, `live`, `repeat`, `classMap`, etc.) are automatically traversed, and any string arguments inside their `values` array are escaped recursively.

### Directives Rule:
Because the wrapper automatically processes directive arguments, you **do not** need to manually escape strings passed to standard Lit directives. They will be handled safely at runtime.

### Correct Example:
```typescript
title=${ifDefined(tooLong ? undefined : description)} // Automatically escaped!
```

---

## 4. Manual DOM Assignments & Non-Lit Rendering

Any manual assignments that bypass Lit entirely (e.g., setting `element.textContent`, `element.title`, or constructing DOM elements imperatively) will also bypass the Lit wrapper.

* Always wrap user-controlled strings in these contexts with the appropriate escaping helper:
  * Use `escapeUnicodeAsText` if you want hidden characters to display as text (e.g., showing `\\u202E`).
  * Use `safeEscapeUnicode` if you want zero-width spaces to function but other dangerous characters to be escaped.

### Correct Example:
```typescript
nameElement.textContent = Platform.StringUtilities.escapeUnicodeAsText(name);
```
