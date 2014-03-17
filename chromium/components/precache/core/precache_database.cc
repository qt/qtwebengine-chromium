// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_database.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram.h"
#include "base/time/time.h"
#include "components/precache/core/precache_url_table.h"
#include "sql/connection.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace {

// The number of days old that an entry in the precache URL table can be before
// it is considered "old" and is removed from the table.
const int kPrecacheHistoryExpiryPeriodDays = 60;

}  // namespace

namespace precache {

PrecacheDatabase::PrecacheDatabase()
    : precache_url_table_(new PrecacheURLTable()) {
  // A PrecacheDatabase can be constructed on any thread.
  thread_checker_.DetachFromThread();
}

PrecacheDatabase::~PrecacheDatabase() {
  // Since the PrecacheDatabase is refcounted, it will only be deleted if there
  // are no references remaining to it, meaning that it is not in use. Thus, it
  // is safe to delete it, regardless of what thread we are on.
  thread_checker_.DetachFromThread();
}

bool PrecacheDatabase::Init(const base::FilePath& db_path) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!db_);  // Init must only be called once.

  db_.reset(new sql::Connection());
  db_->set_histogram_tag("Precache");

  if (!db_->Open(db_path)) {
    // Don't initialize the URL table if unable to access
    // the database.
    return false;
  }

  if (!precache_url_table_->Init(db_.get())) {
    // Raze and close the database connection to indicate that it's not usable,
    // and so that the database will be created anew next time, in case it's
    // corrupted.
    db_->RazeAndClose();
    return false;
  }
  return true;
}

void PrecacheDatabase::DeleteExpiredPrecacheHistory(
    const base::Time& current_time) {
  if (!IsDatabaseAccessible()) {
    // Do nothing if unable to access the database.
    return;
  }

  // Delete old precache history that has expired.
  precache_url_table_->DeleteAllPrecachedBefore(
      current_time -
      base::TimeDelta::FromDays(kPrecacheHistoryExpiryPeriodDays));
}

void PrecacheDatabase::RecordURLPrecached(const GURL& url,
                                          const base::Time& fetch_time,
                                          int64 size, bool was_cached) {
  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    // Do nothing if unable to begin a transaction.
    return;
  }

  if (was_cached && !precache_url_table_->HasURL(url)) {
    // Since the precache came from the cache, and there's no entry in the URL
    // table for the URL, this means that the resource was already in the cache
    // because of user browsing. Thus, this precache had no effect, so ignore
    // it.
    return;
  }

  if (!was_cached) {
    // The precache only counts as overhead if it was downloaded over the
    // network.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedPrecacheMotivated", size);
  }

  // Use the URL table to keep track of URLs that are in the cache thanks to
  // precaching. If a row for the URL already exists, than update the timestamp
  // to |fetch_time|.
  precache_url_table_->AddURL(url, fetch_time);

  transaction.Commit();
}

void PrecacheDatabase::RecordURLFetched(const GURL& url,
                                        const base::Time& fetch_time,
                                        int64 size, bool was_cached,
                                        bool is_connection_cellular) {
  if (!IsDatabaseAccessible()) {
    // Don't track anything if unable to access the database.
    return;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    // Do nothing if unable to begin a transaction.
    return;
  }

  if (was_cached && !precache_url_table_->HasURL(url)) {
    // Ignore cache hits that precache can't take credit for.
    return;
  }

  if (!was_cached) {
    // The fetch was served over the network during user browsing, so count it
    // as downloaded non-precache bytes.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache", size);
    if (is_connection_cellular) {
      UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache.Cellular", size);
    }
  } else {
    // The fetch was served from the cache, and since there's an entry for this
    // URL in the URL table, this means that the resource was served from the
    // cache only because precaching put it there. Thus, precaching was helpful,
    // so count the fetch as saved bytes.
    UMA_HISTOGRAM_COUNTS("Precache.Saved", size);
    if (is_connection_cellular) {
      UMA_HISTOGRAM_COUNTS("Precache.Saved.Cellular", size);
    }
  }

  // Since the resource has been fetched during user browsing, remove any record
  // of that URL having been precached from the URL table, if any exists.
  // The current fetch would have put this resource in the cache regardless of
  // whether or not it was previously precached, so delete any record of that
  // URL having been precached from the URL table.
  precache_url_table_->DeleteURL(url);

  transaction.Commit();
}

bool PrecacheDatabase::IsDatabaseAccessible() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(db_);

  return db_->is_open();
}

}  // namespace precache
