// Copyright 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"github.com/urfave/cli/v2"
)

type HttpArchiveConfig struct {
	Method, Host, FullPath                                           string
	StatusCode                                                       int
	LogLevel                                                         string
	RelativeTimestamps                                               bool
	DecodeResponseBody, SkipExisting, OverwriteExisting, InvertMatch bool
	KeepDuplicates                                                   bool
}

func (cfg *HttpArchiveConfig) RequestFilterFlags() []cli.Flag {
	return []cli.Flag{
		&cli.StringFlag{
			Name:        "command",
			Value:       "",
			Usage:       "Only include URLs matching this HTTP method.",
			Destination: &cfg.Method,
		},
		&cli.StringFlag{
			Name:        "host",
			Value:       "",
			Usage:       "Only include URLs matching this host.",
			Destination: &cfg.Host,
		},
		&cli.StringFlag{
			Name:        "full-path",
			Value:       "",
			Usage:       "Only include URLs matching this full path.",
			Destination: &cfg.FullPath,
		},
		&cli.IntFlag{
			Name:        "status-code",
			Value:       0,
			Usage:       "Only include URLs matching this response status code.",
			Destination: &cfg.StatusCode,
		},
		&cli.StringFlag{
			Name:        "log-level",
			Value:       "INFO",
			Usage:       "Logging level (DEBUG, INFO, WARN, ERROR).",
			Destination: &cfg.LogLevel,
		},
		&cli.BoolFlag{
			Name:        "relative-timestamps",
			Usage:       "Display relative timestamps in logs.",
			Destination: &cfg.RelativeTimestamps,
		},
	}
}

func (cfg *HttpArchiveConfig) DefaultFlags() []cli.Flag {
	return append([]cli.Flag{
		&cli.BoolFlag{
			Name:        "decode-response-body",
			Usage:       "Decode/encode response body according to Content-Encoding header.",
			Destination: &cfg.DecodeResponseBody,
		},
	}, cfg.RequestFilterFlags()...)
}

func (cfg *HttpArchiveConfig) AddFlags() []cli.Flag {
	return []cli.Flag{
		&cli.BoolFlag{
			Name:        "skip-existing",
			Usage:       "Skip over existing urls in the archive",
			Destination: &cfg.SkipExisting,
		},
		&cli.BoolFlag{
			Name:        "overwrite-existing",
			Usage:       "Overwrite existing urls in the archive",
			Destination: &cfg.OverwriteExisting,
		},
	}
}

func (cfg *HttpArchiveConfig) TrimFlags() []cli.Flag {
	return append([]cli.Flag{
		&cli.BoolFlag{
			Name:        "invert-match",
			Usage:       "Trim away any urls that DON'T match in the archive",
			Destination: &cfg.InvertMatch,
		},
	}, cfg.DefaultFlags()...)
}

func (cfg *HttpArchiveConfig) MergeFlags() []cli.Flag {
	return []cli.Flag{
		&cli.BoolFlag{
			Name: "keep-duplicates",
			Usage: "By default, if the archives specify different responses for the same request, " +
				"the response from the first archive will be kept. If this flag is set, both responses " +
				"will be kept, which can be useful if the merged archive is replayed with " +
				"--serve-response-in-chronological-sequence (iterates through the duplicated responses).",
			Destination: &cfg.KeepDuplicates,
		},
	}
}
