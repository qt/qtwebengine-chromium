// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"reflect"
	"testing"

	"github.com/urfave/cli/v2"
)

func runTest(t *testing.T, input []cli.Flag, expected map[string]bool) {
	if len(input) != 1 {
		t.Fatalf("runTest only supports exactly one input flag")
	}

	allFlags := append([]cli.Flag{}, input...)
	AddLegacyAliases(&allFlags)

	if len(allFlags) != len(expected) {
		t.Fatalf("Flag count mismatch: expected %d, got %d",
			len(expected), len(allFlags))
	}

	for _, f := range allFlags {
		name := f.Names()[0]
		shouldBeVisible, ok := expected[name]
		if !ok {
			t.Fatalf("Unexpected flag: %s", name)
		}

		v := reflect.Indirect(reflect.ValueOf(f))
		if v.FieldByName("Hidden").Bool() == shouldBeVisible {
			t.Fatalf("Flag %s: visibility mismatch", name)
		}

		// Verify that the destination pointer is preserved from the source flag.
		actualDest := v.FieldByName("Destination")
		sourceDest := reflect.Indirect(
			reflect.ValueOf(input[0])).FieldByName("Destination")
		if sourceDest.IsValid() && !sourceDest.IsNil() &&
			actualDest.Pointer() != sourceDest.Pointer() {
			t.Fatalf("Flag %s: destination pointer mismatch", name)
		}
	}
}

func TestSingleDash(t *testing.T) {
	var destination string
	runTest(t,
		[]cli.Flag{
			&cli.StringFlag{Name: "my-flag", Destination: &destination},
		},
		map[string]bool{
			"my-flag": true,
			"my_flag": false,
		})
}

func TestMultipleDashes(t *testing.T) {
	var destination string
	runTest(t,
		[]cli.Flag{
			&cli.StringFlag{Name: "multi-part-flag", Destination: &destination},
		},
		map[string]bool{
			"multi-part-flag": true,
			"multi_part_flag": false,
		})
}

func TestNoDashes(t *testing.T) {
	var destination string
	runTest(t,
		[]cli.Flag{
			&cli.StringFlag{Name: "simple", Destination: &destination},
		},
		map[string]bool{
			"simple": true,
		})
}

func TestAlreadyHasUnderscores(t *testing.T) {
	runTest(t,
		[]cli.Flag{
			&cli.StringFlag{Name: "foo_bar"},
		},
		map[string]bool{
			"foo_bar": true,
		})
}

func TestMixedUnderscoresAndDashes(t *testing.T) {
	runTest(t,
		[]cli.Flag{
			&cli.StringFlag{Name: "mixed_and-dashed"},
		},
		map[string]bool{
			"mixed_and-dashed": true,
			"mixed_and_dashed": false,
		})
}
