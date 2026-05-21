// Copyright 2020 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TESTING_EMBEDDER_TEST_CONSTANTS_H_
#define TESTING_EMBEDDER_TEST_CONSTANTS_H_

namespace pdfium {

// Expectation file basename for rendering annotation_stamp_with_ap.pdf with
// annotations.
inline constexpr char kAnnotationStampWithApPng[] = "annotation_stamp_with_ap";

// Expectation file basename for rendering a 200x200 blank page.
inline constexpr char kBlankPage200x200Png[] = "blank_200x200";

// Expectation file basename for rendering a 612x792 blank page.
inline constexpr char kBlankPage612By792Png[] = "blank_612x792";

// Expectation file basename for rendering bug_890322.pdf.
inline constexpr char kBug890322Png[] = "bug_890322";

// Expectation file basename for rendering hello_world.pdf or bug_455199.pdf.
inline constexpr char kHelloWorldPng[] = "hello_world";

// Expectation file basename for rendering hello_world.pdf after removing
// "Goodbye, world!".
inline constexpr char kHelloWorldRemovedPng[] = "hello_world_removed";

// Expectation file basename for rendering many_rectangles.pdf.
inline constexpr char kManyRectanglesPng[] = "many_rectangles";

// Expectation file basename for rendering rectangles.pdf.
inline constexpr char kRectanglesPng[] = "rectangles";

// Expectation file basename for rendering text_form.pdf.
inline constexpr char kTextFormPng[] = "text_form";

}  // namespace pdfium

#endif  // TESTING_EMBEDDER_TEST_CONSTANTS_H_
