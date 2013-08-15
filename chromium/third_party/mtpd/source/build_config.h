// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_BUILD_CONFIG_H_
#define MTPD_BUILD_CONFIG_H_

#if defined(CHROMIUM_BUILD) || defined(GOOGLE_CHROME_BUILD)
#define CHROMIUM_LINUX_BUILD
#else
#define CROS_BUILD
#endif

#endif  // MTPD_BUILD_CONFIG_H_
