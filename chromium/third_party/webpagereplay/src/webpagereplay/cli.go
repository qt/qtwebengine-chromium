// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package webpagereplay

import (
	"reflect"
	"strings"

	"github.com/urfave/cli/v2"
)

// AddLegacyAliases modifies the provided slice of flags to include hidden
// copies of any flags that contain dashes in their names, with those dashes
// replaced by underscores. This is done for legacy reasons, as earlier
// versions of this tool used underscores. To avoid confusion and ensure
// a consistent user experience, this treatment is also applied to newly
// introduced flags.
func AddLegacyAliases(flags *[]cli.Flag) {
	n := len(*flags)
	for i := 0; i < n; i++ {
		v := reflect.Indirect(reflect.ValueOf((*flags)[i]))
		name := v.FieldByName("Name").String()
		if legacyName := strings.ReplaceAll(name, "-", "_"); name != legacyName {
			newFlag := reflect.New(v.Type())
			newFlag.Elem().Set(v)
			newFlag.Elem().FieldByName("Name").SetString(legacyName)
			newFlag.Elem().FieldByName("Hidden").SetBool(true)
			*flags = append(*flags, newFlag.Interface().(cli.Flag))
		}
	}
}
