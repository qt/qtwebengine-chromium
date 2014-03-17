// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/precache/core/precache_database.h"

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_samples.h"
#include "base/metrics/statistics_recorder.h"
#include "base/time/time.h"
#include "components/precache/core/precache_url_table.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const GURL kURL("http://url.com");
const base::Time kFetchTime = base::Time() + base::TimeDelta::FromHours(1000);
const base::Time kOldFetchTime = kFetchTime - base::TimeDelta::FromDays(1);
const int64 kSize = 5000;

const char* kHistogramNames[] = {"Precache.DownloadedPrecacheMotivated",
                                 "Precache.DownloadedNonPrecache",
                                 "Precache.DownloadedNonPrecache.Cellular",
                                 "Precache.Saved",
                                 "Precache.Saved.Cellular"};

scoped_ptr<base::HistogramSamples> GetHistogramSamples(
    const char* histogram_name) {
  base::HistogramBase* histogram =
      base::StatisticsRecorder::FindHistogram(histogram_name);

  EXPECT_NE(static_cast<base::HistogramBase*>(NULL), histogram);

  return histogram->SnapshotSamples().Pass();
}

std::map<GURL, base::Time> BuildURLTableMap(const GURL& url,
                                            const base::Time& precache_time) {
  std::map<GURL, base::Time> url_table_map;
  url_table_map[url] = precache_time;
  return url_table_map;
}

}  // namespace

namespace precache {

class PrecacheDatabaseTest : public testing::Test {
 public:
  PrecacheDatabaseTest() {}
  virtual ~PrecacheDatabaseTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    base::StatisticsRecorder::Initialize();
    precache_database_ = new PrecacheDatabase();

    ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    base::FilePath db_path = scoped_temp_dir_.path().Append(
        base::FilePath(FILE_PATH_LITERAL("precache_database")));
    precache_database_->Init(db_path);

    // Log a sample for each histogram, to ensure that they are all created.
    // This has to be done here, and not in the for loop below, because of the
    // way that UMA_HISTOGRAM_COUNTS uses static variables.
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedPrecacheMotivated", 0);
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache", 0);
    UMA_HISTOGRAM_COUNTS("Precache.DownloadedNonPrecache.Cellular", 0);
    UMA_HISTOGRAM_COUNTS("Precache.Saved", 0);
    UMA_HISTOGRAM_COUNTS("Precache.Saved.Cellular", 0);

    for (size_t i = 0; i < arraysize(kHistogramNames); i++) {
      initial_histogram_samples_[i] =
          GetHistogramSamples(kHistogramNames[i]).Pass();
      initial_histogram_samples_map_[kHistogramNames[i]] =
          initial_histogram_samples_[i].get();
    }
  }

  std::map<GURL, base::Time> GetActualURLTableMap() {
    std::map<GURL, base::Time> url_table_map;
    precache_url_table()->GetAllDataForTesting(&url_table_map);
    return url_table_map;
  }

  PrecacheURLTable* precache_url_table() {
    return precache_database_->precache_url_table_.get();
  }

  scoped_ptr<base::HistogramSamples> GetHistogramSamplesDelta(
      const char* histogram_name) {
    scoped_ptr<base::HistogramSamples> delta_samples(
        GetHistogramSamples(histogram_name));
    delta_samples->Subtract(*initial_histogram_samples_map_[histogram_name]);

    return delta_samples.Pass();
  }

  void ExpectNewSample(const char* histogram_name,
                       base::HistogramBase::Sample sample) {
    scoped_ptr<base::HistogramSamples> delta_samples(
        GetHistogramSamplesDelta(histogram_name));
    EXPECT_EQ(1, delta_samples->TotalCount());
    EXPECT_EQ(1, delta_samples->GetCount(sample));
  }

  void ExpectNoNewSamples(const char* histogram_name) {
    scoped_ptr<base::HistogramSamples> delta_samples(
        GetHistogramSamplesDelta(histogram_name));
    EXPECT_EQ(0, delta_samples->TotalCount());
  }

  // Convenience methods for recording different types of URL fetches. These
  // exist to improve the readability of the tests.
  void RecordPrecacheFromNetwork(const GURL& url, const base::Time& fetch_time,
                                 int64 size);
  void RecordPrecacheFromCache(const GURL& url, const base::Time& fetch_time,
                               int64 size);
  void RecordFetchFromNetwork(const GURL& url, const base::Time& fetch_time,
                              int64 size);
  void RecordFetchFromNetworkCellular(const GURL& url,
                                      const base::Time& fetch_time, int64 size);
  void RecordFetchFromCache(const GURL& url, const base::Time& fetch_time,
                            int64 size);
  void RecordFetchFromCacheCellular(const GURL& url,
                                    const base::Time& fetch_time, int64 size);

  scoped_refptr<PrecacheDatabase> precache_database_;

  base::ScopedTempDir scoped_temp_dir_;

  scoped_ptr<base::HistogramSamples> initial_histogram_samples_
      [arraysize(kHistogramNames)];
  std::map<std::string, base::HistogramSamples*> initial_histogram_samples_map_;
};

void PrecacheDatabaseTest::RecordPrecacheFromNetwork(
    const GURL& url, const base::Time& fetch_time, int64 size) {
  precache_database_->RecordURLPrecached(url, fetch_time, size,
                                         false /* was_cached */);
}

void PrecacheDatabaseTest::RecordPrecacheFromCache(const GURL& url,
                                                   const base::Time& fetch_time,
                                                   int64 size) {
  precache_database_->RecordURLPrecached(url, fetch_time, size,
                                         true /* was_cached */);
}

void PrecacheDatabaseTest::RecordFetchFromNetwork(const GURL& url,
                                                  const base::Time& fetch_time,
                                                  int64 size) {
  precache_database_->RecordURLFetched(url, fetch_time, size,
                                       false /* was_cached */,
                                       false /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromNetworkCellular(
    const GURL& url, const base::Time& fetch_time, int64 size) {
  precache_database_->RecordURLFetched(url, fetch_time, size,
                                       false /* was_cached */,
                                       true /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromCache(const GURL& url,
                                                const base::Time& fetch_time,
                                                int64 size) {
  precache_database_->RecordURLFetched(url, fetch_time, size,
                                       true /* was_cached */,
                                       false /* is_connection_cellular */);
}

void PrecacheDatabaseTest::RecordFetchFromCacheCellular(
    const GURL& url, const base::Time& fetch_time, int64 size) {
  precache_database_->RecordURLFetched(url, fetch_time, size,
                                       true /* was_cached */,
                                       true /* is_connection_cellular */);
}

namespace {

TEST_F(PrecacheDatabaseTest, PrecacheOverNetwork) {
  RecordPrecacheFromNetwork(kURL, kFetchTime, kSize);

  EXPECT_EQ(BuildURLTableMap(kURL, kFetchTime), GetActualURLTableMap());

  ExpectNewSample("Precache.DownloadedPrecacheMotivated", kSize);
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, PrecacheFromCacheWithURLTableEntry) {
  precache_url_table()->AddURL(kURL, kOldFetchTime);
  RecordPrecacheFromCache(kURL, kFetchTime, kSize);

  // The URL table entry should have been updated to have |kFetchTime| as the
  // timestamp.
  EXPECT_EQ(BuildURLTableMap(kURL, kFetchTime), GetActualURLTableMap());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, PrecacheFromCacheWithoutURLTableEntry) {
  RecordPrecacheFromCache(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, FetchOverNetwork_NonCellular) {
  RecordFetchFromNetwork(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, FetchOverNetwork_Cellular) {
  RecordFetchFromNetworkCellular(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNewSample("Precache.DownloadedNonPrecache.Cellular", kSize);
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, FetchOverNetworkWithURLTableEntry) {
  precache_url_table()->AddURL(kURL, kOldFetchTime);
  RecordFetchFromNetwork(kURL, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNewSample("Precache.DownloadedNonPrecache", kSize);
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithURLTableEntry_NonCellular) {
  precache_url_table()->AddURL(kURL, kOldFetchTime);
  RecordFetchFromCache(kURL, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNewSample("Precache.Saved", kSize);
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithURLTableEntry_Cellular) {
  precache_url_table()->AddURL(kURL, kOldFetchTime);
  RecordFetchFromCacheCellular(kURL, kFetchTime, kSize);

  // The URL table entry should have been deleted.
  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNewSample("Precache.Saved", kSize);
  ExpectNewSample("Precache.Saved.Cellular", kSize);
}

TEST_F(PrecacheDatabaseTest, FetchFromCacheWithoutURLTableEntry) {
  RecordFetchFromCache(kURL, kFetchTime, kSize);

  EXPECT_TRUE(GetActualURLTableMap().empty());

  ExpectNoNewSamples("Precache.DownloadedPrecacheMotivated");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache");
  ExpectNoNewSamples("Precache.DownloadedNonPrecache.Cellular");
  ExpectNoNewSamples("Precache.Saved");
  ExpectNoNewSamples("Precache.Saved.Cellular");
}

TEST_F(PrecacheDatabaseTest, DeleteExpiredPrecacheHistory) {
  const base::Time kToday = base::Time() + base::TimeDelta::FromDays(1000);
  const base::Time k59DaysAgo = kToday - base::TimeDelta::FromDays(59);
  const base::Time k61DaysAgo = kToday - base::TimeDelta::FromDays(61);

  precache_url_table()->AddURL(GURL("http://expired-precache.com"), k61DaysAgo);
  precache_url_table()->AddURL(GURL("http://old-precache.com"), k59DaysAgo);

  precache_database_->DeleteExpiredPrecacheHistory(kToday);

  EXPECT_EQ(BuildURLTableMap(GURL("http://old-precache.com"), k59DaysAgo),
            GetActualURLTableMap());
}

TEST_F(PrecacheDatabaseTest, SampleInteraction) {
  const GURL kURL1("http://url1.com");
  const int64 kSize1 = 1000;
  const GURL kURL2("http://url2.com");
  const int64 kSize2 = 2000;
  const GURL kURL3("http://url3.com");
  const int64 kSize3 = 3000;
  const GURL kURL4("http://url4.com");
  const int64 kSize4 = 4000;
  const GURL kURL5("http://url5.com");
  const int64 kSize5 = 5000;

  RecordPrecacheFromNetwork(kURL1, kFetchTime, kSize1);
  RecordPrecacheFromNetwork(kURL2, kFetchTime, kSize2);
  RecordPrecacheFromNetwork(kURL3, kFetchTime, kSize3);
  RecordPrecacheFromNetwork(kURL4, kFetchTime, kSize4);

  RecordFetchFromCacheCellular(kURL1, kFetchTime, kSize1);
  RecordFetchFromCacheCellular(kURL1, kFetchTime, kSize1);
  RecordFetchFromNetworkCellular(kURL2, kFetchTime, kSize2);
  RecordFetchFromNetworkCellular(kURL5, kFetchTime, kSize5);
  RecordFetchFromCacheCellular(kURL5, kFetchTime, kSize5);

  RecordPrecacheFromCache(kURL1, kFetchTime, kSize1);
  RecordPrecacheFromNetwork(kURL2, kFetchTime, kSize2);
  RecordPrecacheFromCache(kURL3, kFetchTime, kSize3);
  RecordPrecacheFromCache(kURL4, kFetchTime, kSize4);

  RecordFetchFromCache(kURL1, kFetchTime, kSize1);
  RecordFetchFromNetwork(kURL2, kFetchTime, kSize2);
  RecordFetchFromCache(kURL3, kFetchTime, kSize3);
  RecordFetchFromCache(kURL5, kFetchTime, kSize5);

  scoped_ptr<base::HistogramSamples> downloaded_precache_motivated_bytes(
      GetHistogramSamplesDelta("Precache.DownloadedPrecacheMotivated"));
  EXPECT_EQ(5, downloaded_precache_motivated_bytes->TotalCount());
  EXPECT_EQ(1, downloaded_precache_motivated_bytes->GetCount(kSize1));
  EXPECT_EQ(2, downloaded_precache_motivated_bytes->GetCount(kSize2));
  EXPECT_EQ(1, downloaded_precache_motivated_bytes->GetCount(kSize3));
  EXPECT_EQ(1, downloaded_precache_motivated_bytes->GetCount(kSize4));

  scoped_ptr<base::HistogramSamples> downloaded_non_precache_bytes(
      GetHistogramSamplesDelta("Precache.DownloadedNonPrecache"));
  EXPECT_EQ(3, downloaded_non_precache_bytes->TotalCount());
  EXPECT_EQ(2, downloaded_non_precache_bytes->GetCount(kSize2));
  EXPECT_EQ(1, downloaded_non_precache_bytes->GetCount(kSize5));

  scoped_ptr<base::HistogramSamples> downloaded_non_precache_bytes_cellular(
      GetHistogramSamplesDelta("Precache.DownloadedNonPrecache.Cellular"));
  EXPECT_EQ(2, downloaded_non_precache_bytes_cellular->TotalCount());
  EXPECT_EQ(1, downloaded_non_precache_bytes_cellular->GetCount(kSize2));
  EXPECT_EQ(1, downloaded_non_precache_bytes_cellular->GetCount(kSize5));

  scoped_ptr<base::HistogramSamples> saved_bytes(
      GetHistogramSamplesDelta("Precache.Saved"));
  EXPECT_EQ(2, saved_bytes->TotalCount());
  EXPECT_EQ(1, saved_bytes->GetCount(kSize1));
  EXPECT_EQ(1, saved_bytes->GetCount(kSize3));

  scoped_ptr<base::HistogramSamples> saved_bytes_cellular(
      GetHistogramSamplesDelta("Precache.Saved.Cellular"));
  EXPECT_EQ(1, saved_bytes_cellular->TotalCount());
  EXPECT_EQ(1, saved_bytes_cellular->GetCount(kSize1));
}

}  // namespace

}  // namespace precache
