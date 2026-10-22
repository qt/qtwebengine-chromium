// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"strings"
	"testing"

	"github.com/urfave/cli/v2"
)

func TestFlags(t *testing.T) {
	cfg := &HttpArchiveConfig{}

	// Primary flags (dashes).
	baseFlags := []string{"decode-response-body", "command", "host", "full-path",
		"status-code", "log-level", "relative-timestamps"}
	addFlags := []string{"skip-existing", "overwrite-existing"}
	trimFlags := append([]string{"invert-match"}, baseFlags...)

	// Add legacy flags (underscores).
	addLegacy := func(flags []string) []string {
		var legacyFlags []string
		for _, flag := range flags {
			legacyFlag := strings.ReplaceAll(flag, "-", "_")
			if flag != legacyFlag {
				legacyFlags = append(legacyFlags, legacyFlag)
			}
		}
		return append(flags, legacyFlags...)
	}
	baseFlags = addLegacy(baseFlags)
	addFlags = addLegacy(addFlags)
	trimFlags = addLegacy(trimFlags)

	cases := map[string]struct {
		command   string
		flags     []cli.Flag
		wantFlags []string
	}{
		"ls": {
			command:   "ls",
			flags:     cfg.DefaultFlags(),
			wantFlags: baseFlags,
		},
		"cat": {
			command:   "cat",
			flags:     cfg.DefaultFlags(),
			wantFlags: baseFlags,
		},
		"edit": {
			command:   "edit",
			flags:     cfg.DefaultFlags(),
			wantFlags: baseFlags,
		},
		"merge": {
			command:   "merge",
			flags:     cfg.MergeFlags(),
			wantFlags: []string{"keep-duplicates", "keep_duplicates"},
		},
		"add": {
			command:   "add",
			flags:     cfg.AddFlags(),
			wantFlags: addFlags,
		},
		"add-all": {
			command:   "add-all",
			flags:     cfg.AddFlags(),
			wantFlags: addFlags,
		},
		"trim": {
			command:   "trim",
			flags:     cfg.TrimFlags(),
			wantFlags: trimFlags,
		},
	}

	for name, tt := range cases {
		t.Run(name, func(t *testing.T) {
			flags := append([]cli.Flag{}, tt.flags...)
			AddLegacyAliases(&flags)
			if len(tt.wantFlags) != len(flags) {
				t.Fatalf("Incorrect '%s' flags returned, wanted:%d, actual:%d",
					name, len(tt.wantFlags), len(flags))
			}
			for i, f := range flags {
				actualFlagName := f.Names()[0]
				t.Logf("%s[%d] = %s", name, i, actualFlagName)
				if actualFlagName != tt.wantFlags[i] {
					t.Fatalf("Incorrect flag for '%s' in position %d. wanted:%s, actual:%s",
						name, i, tt.wantFlags[i], actualFlagName)
				}
			}
		})
	}
}
