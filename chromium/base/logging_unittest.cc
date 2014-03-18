// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/logging.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace logging {

namespace {

using ::testing::Return;

// Needs to be global since log assert handlers can't maintain state.
int log_sink_call_count = 0;

#if !LOGGING_IS_OFFICIAL_BUILD
void LogSink(const std::string& str) {
  ++log_sink_call_count;
}
#endif  // !LOGGING_IS_OFFICIAL_BUILD

// Class to make sure any manipulations we do to the min log level are
// contained (i.e., do not affect other unit tests).
class LogStateSaver {
 public:
  LogStateSaver() : old_min_log_level_(GetMinLogLevel()) {}

  ~LogStateSaver() {
    SetMinLogLevel(old_min_log_level_);
    SetLogAssertHandler(NULL);
    SetLogReportHandler(NULL);
    log_sink_call_count = 0;
  }

 private:
  int old_min_log_level_;

  DISALLOW_COPY_AND_ASSIGN(LogStateSaver);
};

class LoggingTest : public testing::Test {
 private:
  LogStateSaver log_state_saver_;
};

class MockLogSource {
 public:
  MOCK_METHOD0(Log, const char*());
};

TEST_F(LoggingTest, BasicLogging) {
  MockLogSource mock_log_source;
  const int kExpectedDebugOrReleaseCalls = 6;
  const int kExpectedDebugCalls = 6;
  const int kExpectedCalls =
      kExpectedDebugOrReleaseCalls + (DEBUG_MODE ? kExpectedDebugCalls : 0);
  EXPECT_CALL(mock_log_source, Log()).Times(kExpectedCalls).
      WillRepeatedly(Return("log message"));

  SetMinLogLevel(LOG_INFO);

  EXPECT_TRUE(LOG_IS_ON(INFO));
  // As of g++-4.5, the first argument to EXPECT_EQ cannot be a
  // constant expression.
  const bool kIsDebugMode = (DEBUG_MODE != 0);
  EXPECT_TRUE(kIsDebugMode == DLOG_IS_ON(INFO));
  EXPECT_TRUE(VLOG_IS_ON(0));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, true) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, true) << mock_log_source.Log();
  VLOG(0) << mock_log_source.Log();
  VLOG_IF(0, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(0) << mock_log_source.Log();
  DVLOG_IF(0, true) << mock_log_source.Log();
}

TEST_F(LoggingTest, LogIsOn) {
#if defined(NDEBUG)
  const bool kDfatalIsFatal = false;
#else  // defined(NDEBUG)
  const bool kDfatalIsFatal = true;
#endif  // defined(NDEBUG)

  SetMinLogLevel(LOG_INFO);
  EXPECT_TRUE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOG_WARNING);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_TRUE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOG_ERROR);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_TRUE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(LOG_IS_ON(DFATAL));

  SetMinLogLevel(LOG_ERROR_REPORT);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_FALSE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(kDfatalIsFatal == LOG_IS_ON(DFATAL));

  // LOG_IS_ON(ERROR_REPORT) should always be true.
  SetMinLogLevel(LOG_FATAL);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_FALSE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(kDfatalIsFatal == LOG_IS_ON(DFATAL));

  // So should LOG_IS_ON(FATAL).
  SetMinLogLevel(LOG_FATAL + 1);
  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(LOG_IS_ON(WARNING));
  EXPECT_FALSE(LOG_IS_ON(ERROR));
  EXPECT_TRUE(LOG_IS_ON(ERROR_REPORT));
  EXPECT_TRUE(LOG_IS_ON(FATAL));
  EXPECT_TRUE(kDfatalIsFatal == LOG_IS_ON(DFATAL));
}

TEST_F(LoggingTest, LoggingIsLazy) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);

  SetMinLogLevel(LOG_WARNING);

  EXPECT_FALSE(LOG_IS_ON(INFO));
  EXPECT_FALSE(DLOG_IS_ON(INFO));
  EXPECT_FALSE(VLOG_IS_ON(1));

  LOG(INFO) << mock_log_source.Log();
  LOG_IF(INFO, false) << mock_log_source.Log();
  PLOG(INFO) << mock_log_source.Log();
  PLOG_IF(INFO, false) << mock_log_source.Log();
  VLOG(1) << mock_log_source.Log();
  VLOG_IF(1, true) << mock_log_source.Log();

  DLOG(INFO) << mock_log_source.Log();
  DLOG_IF(INFO, true) << mock_log_source.Log();
  DPLOG(INFO) << mock_log_source.Log();
  DPLOG_IF(INFO, true) << mock_log_source.Log();
  DVLOG(1) << mock_log_source.Log();
  DVLOG_IF(1, true) << mock_log_source.Log();
}

// Official builds have CHECKs directly call BreakDebugger.
#if !LOGGING_IS_OFFICIAL_BUILD

TEST_F(LoggingTest, CheckStreamsAreLazy) {
  MockLogSource mock_log_source, uncalled_mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(8).
      WillRepeatedly(Return("check message"));
  EXPECT_CALL(uncalled_mock_log_source, Log()).Times(0);

  SetLogAssertHandler(&LogSink);

  CHECK(mock_log_source.Log()) << uncalled_mock_log_source.Log();
  PCHECK(!mock_log_source.Log()) << mock_log_source.Log();
  CHECK_EQ(mock_log_source.Log(), mock_log_source.Log())
      << uncalled_mock_log_source.Log();
  CHECK_NE(mock_log_source.Log(), mock_log_source.Log())
      << mock_log_source.Log();
}

#endif

TEST_F(LoggingTest, DebugLoggingReleaseBehavior) {
#if !defined(NDEBUG)
  int debug_only_variable = 1;
#endif
  // These should avoid emitting references to |debug_only_variable|
  // in release mode.
  DLOG_IF(INFO, debug_only_variable) << "test";
  DLOG_ASSERT(debug_only_variable) << "test";
  DPLOG_IF(INFO, debug_only_variable) << "test";
  DVLOG_IF(1, debug_only_variable) << "test";
}

TEST_F(LoggingTest, DcheckStreamsAreLazy) {
  MockLogSource mock_log_source;
  EXPECT_CALL(mock_log_source, Log()).Times(0);
#if !LOGGING_IS_OFFICIAL_BUILD && defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // Unofficial release build without dcheck enabled.
  set_dcheck_state(DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  DCHECK(mock_log_source.Log()) << mock_log_source.Log();
  DPCHECK(mock_log_source.Log()) << mock_log_source.Log();
  DCHECK_EQ(0, 0) << mock_log_source.Log();
  DCHECK_EQ(mock_log_source.Log(), static_cast<const char*>(NULL))
      << mock_log_source.Log();
#endif
}

TEST_F(LoggingTest, Dcheck) {
#if LOGGING_IS_OFFICIAL_BUILD
  // Official build.
  EXPECT_FALSE(DCHECK_IS_ON());
  EXPECT_FALSE(DLOG_IS_ON(DCHECK));
#elif defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
  // Unofficial release build.
  set_dcheck_state(ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  SetLogReportHandler(&LogSink);
  EXPECT_TRUE(DCHECK_IS_ON());
  EXPECT_FALSE(DLOG_IS_ON(DCHECK));
#elif defined(NDEBUG) && defined(DCHECK_ALWAYS_ON)
  // Unofficial release build with real DCHECKS.
  set_dcheck_state(ENABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
  SetLogAssertHandler(&LogSink);
  EXPECT_TRUE(DCHECK_IS_ON());
  EXPECT_FALSE(DLOG_IS_ON(DCHECK));
#else
  // Unofficial debug build.
  SetLogAssertHandler(&LogSink);
  EXPECT_TRUE(DCHECK_IS_ON());
  EXPECT_TRUE(DLOG_IS_ON(DCHECK));
#endif  // defined(LOGGING_IS_OFFICIAL_BUILD)

  EXPECT_EQ(0, log_sink_call_count);
  DCHECK(false);
  EXPECT_EQ(DCHECK_IS_ON() ? 1 : 0, log_sink_call_count);
  DPCHECK(false);
  EXPECT_EQ(DCHECK_IS_ON() ? 2 : 0, log_sink_call_count);
  DCHECK_EQ(0, 1);
  EXPECT_EQ(DCHECK_IS_ON() ? 3 : 0, log_sink_call_count);
}

TEST_F(LoggingTest, DcheckReleaseBehavior) {
  int some_variable = 1;
  // These should still reference |some_variable| so we don't get
  // unused variable warnings.
  DCHECK(some_variable) << "test";
  DPCHECK(some_variable) << "test";
  DCHECK_EQ(some_variable, 1) << "test";
}

}  // namespace

}  // namespace logging
