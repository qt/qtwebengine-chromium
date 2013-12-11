// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_

#include "content/common/content_export.h"
#include "net/cookies/cookie_monster.h"

namespace base {
class FilePath;
}

namespace quota {
class SpecialStoragePolicy;
}

namespace content {

// All blocking database accesses will be performed on |background_task_runner|.
// If background_task_runner is NULL, then a background task runner will be
// created internally.
CONTENT_EXPORT net::CookieStore* CreatePersistentCookieStore(
    const base::FilePath& path,
    bool restore_old_session_cookies,
    quota::SpecialStoragePolicy* storage_policy,
    net::CookieMonster::Delegate* cookie_monster_delegate,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_COOKIE_STORE_FACTORY_H_
