# fpdfapi/

This subdirectory contains the central implementation of the PDF specification,
ranging from low-level parsing to high-level page representation and rendering.

## Subdirectories

*   **[cmaps/](cmaps/)**: Logic and static data for character mapping, providing
    the necessary infrastructure to support multi-byte character sets (CJK) and
    custom encoding mappings.
*   **[edit/](edit/)**: Tools for modification and creation. Contains the logic
    for document generation, page organization, and font subsetting.
*   **[font/](font/)**: PDF-specific font logic, including Type1, TrueType, and
    CIDFont implementations, as well as glyph mapping and font resource
    management.
*   **[page/](page/)**: Provides a logical representation of PDF objects as
    high-level page content. This includes managing resources like fonts,
    colorspaces, and interpreting content streams into interactive objects.
*   **[parser/](parser/)**: The foundations of PDF reading. Handles low-level
    syntax (objects, arrays, dictionaries), cross-reference tables, and the
    incremental update system.
*   **[render/](render/)**: The bridge between the PDF object graph and the
    graphics engine. It coordinates the translation of page objects into drawing
    calls for the rendering backends.
