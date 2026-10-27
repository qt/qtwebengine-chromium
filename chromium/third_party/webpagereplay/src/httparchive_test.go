// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"bytes"
	"io"
	"os"
	"path/filepath"
	"strings"
	"testing"

	"github.com/urfave/cli/v2"
	"go.chromium.org/webpagereplay/src/webpagereplay"
)

func createTestArchive(t *testing.T, scripts map[string]string) string {
	a := webpagereplay.Archive{InjectedScripts: scripts}
	archivePath := filepath.Join(t.TempDir(), "archive.json.gz")
	f, err := os.Create(archivePath)
	if err != nil {
		t.Fatalf("Failed to create temp file: %v", err)
	}
	defer f.Close()
	if err := a.Serialize(f); err != nil {
		t.Fatalf("Failed to serialize archive: %v", err)
	}
	return archivePath
}

func captureStdout(t *testing.T, action func()) string {
	oldStdout := os.Stdout
	r, w, err := os.Pipe()
	if err != nil {
		t.Fatalf("Failed to create pipe: %v", err)
	}
	os.Stdout = w

	action()

	w.Close()
	os.Stdout = oldStdout

	var buf bytes.Buffer
	if _, err := io.Copy(&buf, r); err != nil {
		t.Fatalf("Failed to copy output: %v", err)
	}
	return buf.String()
}

func TestFlags(t *testing.T) {
	cfg := &webpagereplay.HttpArchiveConfig{}

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
			webpagereplay.AddLegacyAliases(&flags)
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

func TestLsScriptsCommand(t *testing.T) {
	cases := map[string]struct {
		injectedScripts map[string]string
		wantOutput      []string
	}{
		"empty": {nil, []string{}},
		"one":   {map[string]string{"script1.js": "content1"}, []string{"script1.js"}},
		"multiple": {
			map[string]string{"script1.js": "content1", "script2.js": "content2"},
			[]string{"script1.js", "script2.js"},
		},
	}

	for name, tt := range cases {
		t.Run(name, func(t *testing.T) {
			archivePath := createTestArchive(t, tt.injectedScripts)

			oldArgs := os.Args
			defer func() { os.Args = oldArgs }()
			os.Args = []string{"httparchive", "ls-scripts", archivePath}

			output := captureStdout(t, main)

			var lines []string
			trimmed := strings.TrimSpace(output)
			if trimmed != "" {
				lines = strings.Split(trimmed, "\n")
			}

			if len(lines) != len(tt.wantOutput) {
				t.Errorf("Expected %d lines of output, got %d. Output: %q",
					len(tt.wantOutput), len(lines), output)
			}

			for _, want := range tt.wantOutput {
				found := false
				for _, line := range lines {
					if line == want {
						found = true
						break
					}
				}
				if !found {
					t.Errorf("Output missing expected script %q. Got: %v", want, lines)
				}
			}
		})
	}
}
