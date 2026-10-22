# core/

This directory contains the internal C++ implementation of the PDF
specification. These modules are for internal use and may change without
notice.

## Subdirectories

PDFium's core is divided into several subdirectories, each with a specific
responsibility:

*   **[fdrm/](fdrm/) (Rights Management)**: Provides cryptography primitives
    used for PDF security and encryption handling.
*   **[fpdfapi/](fpdfapi/README.md) (PDF spec implementation)**: The central
    module that understands the PDF format, covering parsing, logical page
    representation, rendering coordination, and document editing.
*   **[fpdfdoc/](fpdfdoc/) (Document Logic)**: Implements higher-level document
    features such as Bookmarks, Annotations, Interactive Forms (AcroForms), and
    Links.
*   **[fpdftext/](fpdftext/) (Text Extraction)**: Specialized logic for
    extracting text, identifying reading order, and performing text searches
    within a page.
*   **[fxcodec/](fxcodec/) (Codecs)**: Responsible for decoding various PDF
    stream formats. It depends on `fxge` for managing target bitmaps.
*   **[fxcrt/](fxcrt/) (Common Runtime)**: The foundational layer providing base
    types, string handling, custom containers, and memory management utilities.
    It has no dependencies on other core modules.
*   **[fxge/](fxge/) (Graphics Engine)**: Handles rendering primitives (paths,
    glyphs, bitmaps), font management abstractions (FreeType, Skia, AGG), and
    device drivers for different platforms.

## Architectural Flow: Life of a Page

Building a rendered page involves several layers working in concert:

1.  **Parsing**: `fpdfapi/parser` reads raw bytes and builds the PDF object
    graph (Dictionaries, Streams, etc.).
2.  **Decoding**: As content is accessed, `fxcodec` decodes compressed data
    streams.
3.  **Logical Representation**: `fpdfapi/page` interprets the objects into
    higher-level constructs like paths, text objects, and images.
4.  **Rendering**: `fpdfapi/render` traverses the page objects and issues
    drawing commands to `fxge`.
5.  **Rasterization**: `fxge` uses its rendering backend (e.g., AGG or Skia) to
    convert these primitives into final pixels in a device buffer.

## Coordinate Systems

PDF documents use **User Space**, a coordinate system where the origin (0,0) is
typically at the bottom-left of the page. Rendering devices (managed by `fxge`)
usually use **Device Space**, where the origin is at the top-left.

`core/` handles the complex affine transformations needed to map between these
spaces, taking into account page rotation, scaling, and the specific
transformation matrices (CTM) defined within content streams.
