// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/child/webkitplatformsupport_impl.h"

#include <math.h>

#include <vector>

#include "base/allocator/allocator_extension.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/metrics/sparse_histogram.h"
#include "base/metrics/stats_counters.h"
#include "base/platform_file.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "grit/blink_resources.h"
#include "grit/webkit_resources.h"
#include "grit/webkit_strings.h"
#include "net/base/data_url.h"
#include "net/base/mime_util.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/public/platform/WebCookie.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebDiscardableMemory.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebPluginListBuilder.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebVector.h"
#include "third_party/WebKit/public/web/WebFrameClient.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/base/layout.h"
#include "webkit/child/webkit_child_helpers.h"
#include "webkit/child/websocketstreamhandle_impl.h"
#include "webkit/child/weburlloader_impl.h"
#include "webkit/common/user_agent/user_agent.h"

#if defined(OS_ANDROID)
#include "base/android/sys_utils.h"
#endif

#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
#include "third_party/tcmalloc/chromium/src/gperftools/heap-profiler.h"
#endif

using blink::WebAudioBus;
using blink::WebCookie;
using blink::WebData;
using blink::WebLocalizedString;
using blink::WebPluginListBuilder;
using blink::WebString;
using blink::WebSocketStreamHandle;
using blink::WebURL;
using blink::WebURLError;
using blink::WebURLLoader;
using blink::WebVector;

namespace {

// A simple class to cache the memory usage for a given amount of time.
class MemoryUsageCache {
 public:
  // Retrieves the Singleton.
  static MemoryUsageCache* GetInstance() {
    return Singleton<MemoryUsageCache>::get();
  }

  MemoryUsageCache() : memory_value_(0) { Init(); }
  ~MemoryUsageCache() {}

  void Init() {
    const unsigned int kCacheSeconds = 1;
    cache_valid_time_ = base::TimeDelta::FromSeconds(kCacheSeconds);
  }

  // Returns true if the cached value is fresh.
  // Returns false if the cached value is stale, or if |cached_value| is NULL.
  bool IsCachedValueValid(size_t* cached_value) {
    base::AutoLock scoped_lock(lock_);
    if (!cached_value)
      return false;
    if (base::Time::Now() - last_updated_time_ > cache_valid_time_)
      return false;
    *cached_value = memory_value_;
    return true;
  };

  // Setter for |memory_value_|, refreshes |last_updated_time_|.
  void SetMemoryValue(const size_t value) {
    base::AutoLock scoped_lock(lock_);
    memory_value_ = value;
    last_updated_time_ = base::Time::Now();
  }

 private:
  // The cached memory value.
  size_t memory_value_;

  // How long the cached value should remain valid.
  base::TimeDelta cache_valid_time_;

  // The last time the cached value was updated.
  base::Time last_updated_time_;

  base::Lock lock_;
};

}  // anonymous namespace

namespace webkit_glue {

static int ToMessageID(WebLocalizedString::Name name) {
  switch (name) {
    case WebLocalizedString::AXAMPMFieldText:
      return IDS_AX_AM_PM_FIELD_TEXT;
    case WebLocalizedString::AXButtonActionVerb:
      return IDS_AX_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXCheckedCheckBoxActionVerb:
      return IDS_AX_CHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXDateTimeFieldEmptyValueText:
      return IDS_AX_DATE_TIME_FIELD_EMPTY_VALUE_TEXT;
    case WebLocalizedString::AXDayOfMonthFieldText:
      return IDS_AX_DAY_OF_MONTH_FIELD_TEXT;
    case WebLocalizedString::AXHeadingText:
      return IDS_AX_ROLE_HEADING;
    case WebLocalizedString::AXHourFieldText:
      return IDS_AX_HOUR_FIELD_TEXT;
    case WebLocalizedString::AXImageMapText:
      return IDS_AX_ROLE_IMAGE_MAP;
    case WebLocalizedString::AXLinkActionVerb:
      return IDS_AX_LINK_ACTION_VERB;
    case WebLocalizedString::AXLinkText:
      return IDS_AX_ROLE_LINK;
    case WebLocalizedString::AXListMarkerText:
      return IDS_AX_ROLE_LIST_MARKER;
    case WebLocalizedString::AXMediaDefault:
      return IDS_AX_MEDIA_DEFAULT;
    case WebLocalizedString::AXMediaAudioElement:
      return IDS_AX_MEDIA_AUDIO_ELEMENT;
    case WebLocalizedString::AXMediaVideoElement:
      return IDS_AX_MEDIA_VIDEO_ELEMENT;
    case WebLocalizedString::AXMediaMuteButton:
      return IDS_AX_MEDIA_MUTE_BUTTON;
    case WebLocalizedString::AXMediaUnMuteButton:
      return IDS_AX_MEDIA_UNMUTE_BUTTON;
    case WebLocalizedString::AXMediaPlayButton:
      return IDS_AX_MEDIA_PLAY_BUTTON;
    case WebLocalizedString::AXMediaPauseButton:
      return IDS_AX_MEDIA_PAUSE_BUTTON;
    case WebLocalizedString::AXMediaSlider:
      return IDS_AX_MEDIA_SLIDER;
    case WebLocalizedString::AXMediaSliderThumb:
      return IDS_AX_MEDIA_SLIDER_THUMB;
    case WebLocalizedString::AXMediaRewindButton:
      return IDS_AX_MEDIA_REWIND_BUTTON;
    case WebLocalizedString::AXMediaReturnToRealTime:
      return IDS_AX_MEDIA_RETURN_TO_REALTIME_BUTTON;
    case WebLocalizedString::AXMediaCurrentTimeDisplay:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY;
    case WebLocalizedString::AXMediaTimeRemainingDisplay:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY;
    case WebLocalizedString::AXMediaStatusDisplay:
      return IDS_AX_MEDIA_STATUS_DISPLAY;
    case WebLocalizedString::AXMediaEnterFullscreenButton:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON;
    case WebLocalizedString::AXMediaExitFullscreenButton:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON;
  case WebLocalizedString::AXMediaSeekForwardButton:
    return IDS_AX_MEDIA_SEEK_FORWARD_BUTTON;
    case WebLocalizedString::AXMediaSeekBackButton:
      return IDS_AX_MEDIA_SEEK_BACK_BUTTON;
    case WebLocalizedString::AXMediaShowClosedCaptionsButton:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::AXMediaHideClosedCaptionsButton:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON;
    case WebLocalizedString::AXMediaAudioElementHelp:
      return IDS_AX_MEDIA_AUDIO_ELEMENT_HELP;
    case WebLocalizedString::AXMediaVideoElementHelp:
      return IDS_AX_MEDIA_VIDEO_ELEMENT_HELP;
    case WebLocalizedString::AXMediaMuteButtonHelp:
      return IDS_AX_MEDIA_MUTE_BUTTON_HELP;
    case WebLocalizedString::AXMediaUnMuteButtonHelp:
      return IDS_AX_MEDIA_UNMUTE_BUTTON_HELP;
    case WebLocalizedString::AXMediaPlayButtonHelp:
      return IDS_AX_MEDIA_PLAY_BUTTON_HELP;
    case WebLocalizedString::AXMediaPauseButtonHelp:
      return IDS_AX_MEDIA_PAUSE_BUTTON_HELP;
    case WebLocalizedString::AXMediaSliderHelp:
      return IDS_AX_MEDIA_SLIDER_HELP;
    case WebLocalizedString::AXMediaSliderThumbHelp:
      return IDS_AX_MEDIA_SLIDER_THUMB_HELP;
    case WebLocalizedString::AXMediaRewindButtonHelp:
      return IDS_AX_MEDIA_REWIND_BUTTON_HELP;
    case WebLocalizedString::AXMediaReturnToRealTimeHelp:
      return IDS_AX_MEDIA_RETURN_TO_REALTIME_BUTTON_HELP;
    case WebLocalizedString::AXMediaCurrentTimeDisplayHelp:
      return IDS_AX_MEDIA_CURRENT_TIME_DISPLAY_HELP;
    case WebLocalizedString::AXMediaTimeRemainingDisplayHelp:
      return IDS_AX_MEDIA_TIME_REMAINING_DISPLAY_HELP;
    case WebLocalizedString::AXMediaStatusDisplayHelp:
      return IDS_AX_MEDIA_STATUS_DISPLAY_HELP;
    case WebLocalizedString::AXMediaEnterFullscreenButtonHelp:
      return IDS_AX_MEDIA_ENTER_FULL_SCREEN_BUTTON_HELP;
    case WebLocalizedString::AXMediaExitFullscreenButtonHelp:
      return IDS_AX_MEDIA_EXIT_FULL_SCREEN_BUTTON_HELP;
  case WebLocalizedString::AXMediaSeekForwardButtonHelp:
    return IDS_AX_MEDIA_SEEK_FORWARD_BUTTON_HELP;
    case WebLocalizedString::AXMediaSeekBackButtonHelp:
      return IDS_AX_MEDIA_SEEK_BACK_BUTTON_HELP;
    case WebLocalizedString::AXMediaShowClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_SHOW_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::AXMediaHideClosedCaptionsButtonHelp:
      return IDS_AX_MEDIA_HIDE_CLOSED_CAPTIONS_BUTTON_HELP;
    case WebLocalizedString::AXMillisecondFieldText:
      return IDS_AX_MILLISECOND_FIELD_TEXT;
    case WebLocalizedString::AXMinuteFieldText:
      return IDS_AX_MINUTE_FIELD_TEXT;
    case WebLocalizedString::AXMonthFieldText:
      return IDS_AX_MONTH_FIELD_TEXT;
    case WebLocalizedString::AXRadioButtonActionVerb:
      return IDS_AX_RADIO_BUTTON_ACTION_VERB;
    case WebLocalizedString::AXSecondFieldText:
      return IDS_AX_SECOND_FIELD_TEXT;
    case WebLocalizedString::AXTextFieldActionVerb:
      return IDS_AX_TEXT_FIELD_ACTION_VERB;
    case WebLocalizedString::AXUncheckedCheckBoxActionVerb:
      return IDS_AX_UNCHECKED_CHECK_BOX_ACTION_VERB;
    case WebLocalizedString::AXWebAreaText:
      return IDS_AX_ROLE_WEB_AREA;
    case WebLocalizedString::AXWeekOfYearFieldText:
      return IDS_AX_WEEK_OF_YEAR_FIELD_TEXT;
    case WebLocalizedString::AXYearFieldText:
      return IDS_AX_YEAR_FIELD_TEXT;
    case WebLocalizedString::CalendarClear:
      return IDS_FORM_CALENDAR_CLEAR;
    case WebLocalizedString::CalendarToday:
      return IDS_FORM_CALENDAR_TODAY;
    case WebLocalizedString::DateFormatDayInMonthLabel:
      return IDS_FORM_DATE_FORMAT_DAY_IN_MONTH;
    case WebLocalizedString::DateFormatMonthLabel:
      return IDS_FORM_DATE_FORMAT_MONTH;
    case WebLocalizedString::DateFormatYearLabel:
      return IDS_FORM_DATE_FORMAT_YEAR;
    case WebLocalizedString::DetailsLabel:
      return IDS_DETAILS_WITHOUT_SUMMARY_LABEL;
    case WebLocalizedString::FileButtonChooseFileLabel:
      return IDS_FORM_FILE_BUTTON_LABEL;
    case WebLocalizedString::FileButtonChooseMultipleFilesLabel:
      return IDS_FORM_MULTIPLE_FILES_BUTTON_LABEL;
    case WebLocalizedString::FileButtonNoFileSelectedLabel:
      return IDS_FORM_FILE_NO_FILE_LABEL;
    case WebLocalizedString::InputElementAltText:
      return IDS_FORM_INPUT_ALT;
    case WebLocalizedString::KeygenMenuHighGradeKeySize:
      return IDS_KEYGEN_HIGH_GRADE_KEY;
    case WebLocalizedString::KeygenMenuMediumGradeKeySize:
      return IDS_KEYGEN_MED_GRADE_KEY;
    case WebLocalizedString::MissingPluginText:
      return IDS_PLUGIN_INITIALIZATION_ERROR;
    case WebLocalizedString::MultipleFileUploadText:
      return IDS_FORM_FILE_MULTIPLE_UPLOAD;
    case WebLocalizedString::OtherColorLabel:
      return IDS_FORM_OTHER_COLOR_LABEL;
    case WebLocalizedString::OtherDateLabel:
        return IDS_FORM_OTHER_DATE_LABEL;
    case WebLocalizedString::OtherMonthLabel:
      return IDS_FORM_OTHER_MONTH_LABEL;
    case WebLocalizedString::OtherTimeLabel:
      return IDS_FORM_OTHER_TIME_LABEL;
    case WebLocalizedString::OtherWeekLabel:
      return IDS_FORM_OTHER_WEEK_LABEL;
    case WebLocalizedString::PlaceholderForDayOfMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_DAY_OF_MONTH_FIELD;
    case WebLocalizedString::PlaceholderForMonthField:
      return IDS_FORM_PLACEHOLDER_FOR_MONTH_FIELD;
    case WebLocalizedString::PlaceholderForYearField:
      return IDS_FORM_PLACEHOLDER_FOR_YEAR_FIELD;
    case WebLocalizedString::ResetButtonDefaultLabel:
      return IDS_FORM_RESET_LABEL;
    case WebLocalizedString::SearchableIndexIntroduction:
      return IDS_SEARCHABLE_INDEX_INTRO;
    case WebLocalizedString::SearchMenuClearRecentSearchesText:
      return IDS_RECENT_SEARCHES_CLEAR;
    case WebLocalizedString::SearchMenuNoRecentSearchesText:
      return IDS_RECENT_SEARCHES_NONE;
    case WebLocalizedString::SearchMenuRecentSearchesText:
      return IDS_RECENT_SEARCHES;
    case WebLocalizedString::SubmitButtonDefaultLabel:
      return IDS_FORM_SUBMIT_LABEL;
    case WebLocalizedString::ThisMonthButtonLabel:
      return IDS_FORM_THIS_MONTH_LABEL;
    case WebLocalizedString::ThisWeekButtonLabel:
      return IDS_FORM_THIS_WEEK_LABEL;
    case WebLocalizedString::ValidationBadInputForDateTime:
      return IDS_FORM_VALIDATION_BAD_INPUT_DATETIME;
    case WebLocalizedString::ValidationBadInputForNumber:
      return IDS_FORM_VALIDATION_BAD_INPUT_NUMBER;
    case WebLocalizedString::ValidationPatternMismatch:
      return IDS_FORM_VALIDATION_PATTERN_MISMATCH;
    case WebLocalizedString::ValidationRangeOverflow:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW;
    case WebLocalizedString::ValidationRangeOverflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_OVERFLOW_DATETIME;
    case WebLocalizedString::ValidationRangeUnderflow:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW;
    case WebLocalizedString::ValidationRangeUnderflowDateTime:
      return IDS_FORM_VALIDATION_RANGE_UNDERFLOW_DATETIME;
    case WebLocalizedString::ValidationStepMismatch:
      return IDS_FORM_VALIDATION_STEP_MISMATCH;
    case WebLocalizedString::ValidationStepMismatchCloseToLimit:
      return IDS_FORM_VALIDATION_STEP_MISMATCH_CLOSE_TO_LIMIT;
    case WebLocalizedString::ValidationTooLong:
      return IDS_FORM_VALIDATION_TOO_LONG;
    case WebLocalizedString::ValidationTypeMismatch:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH;
    case WebLocalizedString::ValidationTypeMismatchForEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmpty:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmptyDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_DOMAIN;
    case WebLocalizedString::ValidationTypeMismatchForEmailEmptyLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_EMPTY_LOCAL;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidDomain:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOMAIN;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidDots:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_DOTS;
    case WebLocalizedString::ValidationTypeMismatchForEmailInvalidLocal:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_INVALID_LOCAL;
    case WebLocalizedString::ValidationTypeMismatchForEmailNoAtSign:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_EMAIL_NO_AT_SIGN;
    case WebLocalizedString::ValidationTypeMismatchForMultipleEmail:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_MULTIPLE_EMAIL;
    case WebLocalizedString::ValidationTypeMismatchForURL:
      return IDS_FORM_VALIDATION_TYPE_MISMATCH_URL;
    case WebLocalizedString::ValidationValueMissing:
      return IDS_FORM_VALIDATION_VALUE_MISSING;
    case WebLocalizedString::ValidationValueMissingForCheckbox:
      return IDS_FORM_VALIDATION_VALUE_MISSING_CHECKBOX;
    case WebLocalizedString::ValidationValueMissingForFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_FILE;
    case WebLocalizedString::ValidationValueMissingForMultipleFile:
      return IDS_FORM_VALIDATION_VALUE_MISSING_MULTIPLE_FILE;
    case WebLocalizedString::ValidationValueMissingForRadio:
      return IDS_FORM_VALIDATION_VALUE_MISSING_RADIO;
    case WebLocalizedString::ValidationValueMissingForSelect:
      return IDS_FORM_VALIDATION_VALUE_MISSING_SELECT;
    case WebLocalizedString::WeekFormatTemplate:
      return IDS_FORM_INPUT_WEEK_TEMPLATE;
    case WebLocalizedString::WeekNumberLabel:
      return IDS_FORM_WEEK_NUMBER_LABEL;
    // This "default:" line exists to avoid compile warnings about enum
    // coverage when we add a new symbol to WebLocalizedString.h in WebKit.
    // After a planned WebKit patch is landed, we need to add a case statement
    // for the added symbol here.
    default:
      break;
  }
  return -1;
}

WebKitPlatformSupportImpl::WebKitPlatformSupportImpl()
    : main_loop_(base::MessageLoop::current()),
      shared_timer_func_(NULL),
      shared_timer_fire_time_(0.0),
      shared_timer_fire_time_was_set_while_suspended_(false),
      shared_timer_suspended_(0) {}

WebKitPlatformSupportImpl::~WebKitPlatformSupportImpl() {
}

WebURLLoader* WebKitPlatformSupportImpl::createURLLoader() {
  return new WebURLLoaderImpl(this);
}

WebSocketStreamHandle* WebKitPlatformSupportImpl::createSocketStreamHandle() {
  return new WebSocketStreamHandleImpl(this);
}

WebString WebKitPlatformSupportImpl::userAgent(const WebURL& url) {
  return WebString::fromUTF8(webkit_glue::GetUserAgent(url));
}

WebData WebKitPlatformSupportImpl::parseDataURL(
    const WebURL& url,
    WebString& mimetype_out,
    WebString& charset_out) {
  std::string mime_type, char_set, data;
  if (net::DataURL::Parse(url, &mime_type, &char_set, &data)
      && net::IsSupportedMimeType(mime_type)) {
    mimetype_out = WebString::fromUTF8(mime_type);
    charset_out = WebString::fromUTF8(char_set);
    return data;
  }
  return WebData();
}

WebURLError WebKitPlatformSupportImpl::cancelledError(
    const WebURL& unreachableURL) const {
  return WebURLLoaderImpl::CreateError(unreachableURL, net::ERR_ABORTED);
}

void WebKitPlatformSupportImpl::decrementStatsCounter(const char* name) {
  base::StatsCounter(name).Decrement();
}

void WebKitPlatformSupportImpl::incrementStatsCounter(const char* name) {
  base::StatsCounter(name).Increment();
}

void WebKitPlatformSupportImpl::histogramCustomCounts(
    const char* name, int sample, int min, int max, int bucket_count) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter =
      base::Histogram::FactoryGet(name, min, max, bucket_count,
          base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

void WebKitPlatformSupportImpl::histogramEnumeration(
    const char* name, int sample, int boundary_value) {
  // Copied from histogram macro, but without the static variable caching
  // the histogram because name is dynamic.
  base::HistogramBase* counter =
      base::LinearHistogram::FactoryGet(name, 1, boundary_value,
          boundary_value + 1, base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK_EQ(name, counter->histogram_name());
  counter->Add(sample);
}

void WebKitPlatformSupportImpl::histogramSparse(const char* name, int sample) {
  // For sparse histograms, we can use the macro, as it does not incorporate a
  // static.
  UMA_HISTOGRAM_SPARSE_SLOWLY(name, sample);
}

const unsigned char* WebKitPlatformSupportImpl::getTraceCategoryEnabledFlag(
    const char* category_group) {
  return TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(category_group);
}

long* WebKitPlatformSupportImpl::getTraceSamplingState(
    const unsigned thread_bucket) {
  switch (thread_bucket) {
    case 0:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(0));
    case 1:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(1));
    case 2:
      return reinterpret_cast<long*>(&TRACE_EVENT_API_THREAD_BUCKET(2));
    default:
      NOTREACHED() << "Unknown thread bucket type.";
  }
  return NULL;
}

COMPILE_ASSERT(
    sizeof(blink::Platform::TraceEventHandle) ==
        sizeof(base::debug::TraceEventHandle),
    TraceEventHandle_types_must_be_same_size);

blink::Platform::TraceEventHandle WebKitPlatformSupportImpl::addTraceEvent(
    char phase,
    const unsigned char* category_group_enabled,
    const char* name,
    unsigned long long id,
    int num_args,
    const char** arg_names,
    const unsigned char* arg_types,
    const unsigned long long* arg_values,
    unsigned char flags) {
  base::debug::TraceEventHandle handle = TRACE_EVENT_API_ADD_TRACE_EVENT(
      phase, category_group_enabled, name, id,
      num_args, arg_names, arg_types, arg_values, NULL, flags);
  blink::Platform::TraceEventHandle result;
  memcpy(&result, &handle, sizeof(result));
  return result;
}

void WebKitPlatformSupportImpl::updateTraceEventDuration(
    const unsigned char* category_group_enabled,
    const char* name,
    TraceEventHandle handle) {
  base::debug::TraceEventHandle traceEventHandle;
  memcpy(&traceEventHandle, &handle, sizeof(handle));
  TRACE_EVENT_API_UPDATE_TRACE_EVENT_DURATION(
      category_group_enabled, name, traceEventHandle);
}

namespace {

WebData loadAudioSpatializationResource(WebKitPlatformSupportImpl* platform,
                                        const char* name) {
#ifdef IDR_AUDIO_SPATIALIZATION_COMPOSITE
  if (!strcmp(name, "Composite")) {
    base::StringPiece resource =
        platform->GetDataResource(IDR_AUDIO_SPATIALIZATION_COMPOSITE,
                                  ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif

#ifdef IDR_AUDIO_SPATIALIZATION_T000_P000
  const size_t kExpectedSpatializationNameLength = 31;
  if (strlen(name) != kExpectedSpatializationNameLength) {
    return WebData();
  }

  // Extract the azimuth and elevation from the resource name.
  int azimuth = 0;
  int elevation = 0;
  int values_parsed =
      sscanf(name, "IRC_Composite_C_R0195_T%3d_P%3d", &azimuth, &elevation);
  if (values_parsed != 2) {
    return WebData();
  }

  // The resource index values go through the elevations first, then azimuths.
  const int kAngleSpacing = 15;

  // 0 <= elevation <= 90 (or 315 <= elevation <= 345)
  // in increments of 15 degrees.
  int elevation_index =
      elevation <= 90 ? elevation / kAngleSpacing :
      7 + (elevation - 315) / kAngleSpacing;
  bool is_elevation_index_good = 0 <= elevation_index && elevation_index < 10;

  // 0 <= azimuth < 360 in increments of 15 degrees.
  int azimuth_index = azimuth / kAngleSpacing;
  bool is_azimuth_index_good = 0 <= azimuth_index && azimuth_index < 24;

  const int kNumberOfElevations = 10;
  const int kNumberOfAudioResources = 240;
  int resource_index = kNumberOfElevations * azimuth_index + elevation_index;
  bool is_resource_index_good = 0 <= resource_index &&
      resource_index < kNumberOfAudioResources;

  if (is_azimuth_index_good && is_elevation_index_good &&
      is_resource_index_good) {
    const int kFirstAudioResourceIndex = IDR_AUDIO_SPATIALIZATION_T000_P000;
    base::StringPiece resource =
        platform->GetDataResource(kFirstAudioResourceIndex + resource_index,
                                  ui::SCALE_FACTOR_NONE);
    return WebData(resource.data(), resource.size());
  }
#endif  // IDR_AUDIO_SPATIALIZATION_T000_P000

  NOTREACHED();
  return WebData();
}

struct DataResource {
  const char* name;
  int id;
  ui::ScaleFactor scale_factor;
};

const DataResource kDataResources[] = {
  { "missingImage", IDR_BROKENIMAGE, ui::SCALE_FACTOR_100P },
  { "missingImage@2x", IDR_BROKENIMAGE, ui::SCALE_FACTOR_200P },
  { "mediaplayerPause", IDR_MEDIAPLAYER_PAUSE_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseHover",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPauseDown",
    IDR_MEDIAPLAYER_PAUSE_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlay", IDR_MEDIAPLAYER_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayHover",
    IDR_MEDIAPLAYER_PLAY_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDown",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerPlayDisabled",
    IDR_MEDIAPLAYER_PLAY_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel3Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL3_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel2Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL2_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel1Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL1_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Hover",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundLevel0Down",
    IDR_MEDIAPLAYER_SOUND_LEVEL0_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerSoundDisabled",
    IDR_MEDIAPLAYER_SOUND_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumb",
    IDR_MEDIAPLAYER_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbHover",
    IDR_MEDIAPLAYER_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerSliderThumbDown",
    IDR_MEDIAPLAYER_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumb",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbHover",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDown",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerVolumeSliderThumbDisabled",
    IDR_MEDIAPLAYER_VOLUME_SLIDER_THUMB_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaption",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionHover",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionDown",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerClosedCaptionDisabled",
    IDR_MEDIAPLAYER_CLOSEDCAPTION_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreen",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenHover",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_HOVER, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDown",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DOWN, ui::SCALE_FACTOR_100P },
  { "mediaplayerFullscreenDisabled",
    IDR_MEDIAPLAYER_FULLSCREEN_BUTTON_DISABLED, ui::SCALE_FACTOR_100P },
#if defined(OS_ANDROID)
  { "mediaplayerOverlayPlay",
    IDR_MEDIAPLAYER_OVERLAY_PLAY_BUTTON, ui::SCALE_FACTOR_100P },
#endif
#if defined(OS_MACOSX)
  { "overhangPattern", IDR_OVERHANG_PATTERN, ui::SCALE_FACTOR_100P },
  { "overhangShadow", IDR_OVERHANG_SHADOW, ui::SCALE_FACTOR_100P },
#endif
  { "panIcon", IDR_PAN_SCROLL_ICON, ui::SCALE_FACTOR_100P },
  { "searchCancel", IDR_SEARCH_CANCEL, ui::SCALE_FACTOR_100P },
  { "searchCancelPressed", IDR_SEARCH_CANCEL_PRESSED, ui::SCALE_FACTOR_100P },
  { "searchMagnifier", IDR_SEARCH_MAGNIFIER, ui::SCALE_FACTOR_100P },
  { "searchMagnifierResults",
    IDR_SEARCH_MAGNIFIER_RESULTS, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_100P },
  { "textAreaResizeCorner@2x", IDR_TEXTAREA_RESIZER, ui::SCALE_FACTOR_200P },
  { "inputSpeech", IDR_INPUT_SPEECH, ui::SCALE_FACTOR_100P },
  { "inputSpeechRecording", IDR_INPUT_SPEECH_RECORDING, ui::SCALE_FACTOR_100P },
  { "inputSpeechWaiting", IDR_INPUT_SPEECH_WAITING, ui::SCALE_FACTOR_100P },
  { "americanExpressCC", IDR_AUTOFILL_CC_AMEX, ui::SCALE_FACTOR_100P },
  { "dinersCC", IDR_AUTOFILL_CC_DINERS, ui::SCALE_FACTOR_100P },
  { "discoverCC", IDR_AUTOFILL_CC_DISCOVER, ui::SCALE_FACTOR_100P },
  { "genericCC", IDR_AUTOFILL_CC_GENERIC, ui::SCALE_FACTOR_100P },
  { "jcbCC", IDR_AUTOFILL_CC_JCB, ui::SCALE_FACTOR_100P },
  { "masterCardCC", IDR_AUTOFILL_CC_MASTERCARD, ui::SCALE_FACTOR_100P },
  { "visaCC", IDR_AUTOFILL_CC_VISA, ui::SCALE_FACTOR_100P },
  { "generatePassword", IDR_PASSWORD_GENERATION_ICON, ui::SCALE_FACTOR_100P },
  { "generatePasswordHover",
    IDR_PASSWORD_GENERATION_ICON_HOVER, ui::SCALE_FACTOR_100P },
  { "syntheticTouchCursor",
    IDR_SYNTHETIC_TOUCH_CURSOR, ui::SCALE_FACTOR_100P },
};

}  // namespace

WebData WebKitPlatformSupportImpl::loadResource(const char* name) {
  // Some clients will call into this method with an empty |name| when they have
  // optional resources.  For example, the PopupMenuChromium code can have icons
  // for some Autofill items but not for others.
  if (!strlen(name))
    return WebData();

  // Check the name prefix to see if it's an audio resource.
  if (StartsWithASCII(name, "IRC_Composite", true) ||
      StartsWithASCII(name, "Composite", true))
    return loadAudioSpatializationResource(this, name);

  // TODO(flackr): We should use a better than linear search here, a trie would
  // be ideal.
  for (size_t i = 0; i < arraysize(kDataResources); ++i) {
    if (!strcmp(name, kDataResources[i].name)) {
      base::StringPiece resource =
          GetDataResource(kDataResources[i].id,
                          kDataResources[i].scale_factor);
      return WebData(resource.data(), resource.size());
    }
  }

  NOTREACHED() << "Unknown image resource " << name;
  return WebData();
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return GetLocalizedString(message_id);
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name, int numeric_value) {
  return queryLocalizedString(name, base::IntToString16(numeric_value));
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name, const WebString& value) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  return ReplaceStringPlaceholders(GetLocalizedString(message_id), value, NULL);
}

WebString WebKitPlatformSupportImpl::queryLocalizedString(
    WebLocalizedString::Name name,
    const WebString& value1,
    const WebString& value2) {
  int message_id = ToMessageID(name);
  if (message_id < 0)
    return WebString();
  std::vector<base::string16> values;
  values.reserve(2);
  values.push_back(value1);
  values.push_back(value2);
  return ReplaceStringPlaceholders(
      GetLocalizedString(message_id), values, NULL);
}

double WebKitPlatformSupportImpl::currentTime() {
  return base::Time::Now().ToDoubleT();
}

double WebKitPlatformSupportImpl::monotonicallyIncreasingTime() {
  return base::TimeTicks::Now().ToInternalValue() /
      static_cast<double>(base::Time::kMicrosecondsPerSecond);
}

void WebKitPlatformSupportImpl::cryptographicallyRandomValues(
    unsigned char* buffer, size_t length) {
  base::RandBytes(buffer, length);
}

void WebKitPlatformSupportImpl::setSharedTimerFiredFunction(void (*func)()) {
  shared_timer_func_ = func;
}

void WebKitPlatformSupportImpl::setSharedTimerFireInterval(
    double interval_seconds) {
  shared_timer_fire_time_ = interval_seconds + monotonicallyIncreasingTime();
  if (shared_timer_suspended_) {
    shared_timer_fire_time_was_set_while_suspended_ = true;
    return;
  }

  // By converting between double and int64 representation, we run the risk
  // of losing precision due to rounding errors. Performing computations in
  // microseconds reduces this risk somewhat. But there still is the potential
  // of us computing a fire time for the timer that is shorter than what we
  // need.
  // As the event loop will check event deadlines prior to actually firing
  // them, there is a risk of needlessly rescheduling events and of
  // needlessly looping if sleep times are too short even by small amounts.
  // This results in measurable performance degradation unless we use ceil() to
  // always round up the sleep times.
  int64 interval = static_cast<int64>(
      ceil(interval_seconds * base::Time::kMillisecondsPerSecond)
      * base::Time::kMicrosecondsPerMillisecond);

  if (interval < 0)
    interval = 0;

  shared_timer_.Stop();
  shared_timer_.Start(FROM_HERE, base::TimeDelta::FromMicroseconds(interval),
                      this, &WebKitPlatformSupportImpl::DoTimeout);
  OnStartSharedTimer(base::TimeDelta::FromMicroseconds(interval));
}

void WebKitPlatformSupportImpl::stopSharedTimer() {
  shared_timer_.Stop();
}

void WebKitPlatformSupportImpl::callOnMainThread(
    void (*func)(void*), void* context) {
  main_loop_->PostTask(FROM_HERE, base::Bind(func, context));
}

base::PlatformFile WebKitPlatformSupportImpl::databaseOpenFile(
    const blink::WebString& vfs_file_name, int desired_flags) {
  return base::kInvalidPlatformFileValue;
}

int WebKitPlatformSupportImpl::databaseDeleteFile(
    const blink::WebString& vfs_file_name, bool sync_dir) {
  return -1;
}

long WebKitPlatformSupportImpl::databaseGetFileAttributes(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long WebKitPlatformSupportImpl::databaseGetFileSize(
    const blink::WebString& vfs_file_name) {
  return 0;
}

long long WebKitPlatformSupportImpl::databaseGetSpaceAvailableForOrigin(
    const blink::WebString& origin_identifier) {
  return 0;
}

blink::WebString WebKitPlatformSupportImpl::signedPublicKeyAndChallengeString(
    unsigned key_size_index,
    const blink::WebString& challenge,
    const blink::WebURL& url) {
  return blink::WebString("");
}

static scoped_ptr<base::ProcessMetrics> CurrentProcessMetrics() {
  using base::ProcessMetrics;
#if defined(OS_MACOSX)
  return scoped_ptr<ProcessMetrics>(
      // The default port provider is sufficient to get data for the current
      // process.
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle(),
                                           NULL));
#else
  return scoped_ptr<ProcessMetrics>(
      ProcessMetrics::CreateProcessMetrics(base::GetCurrentProcessHandle()));
#endif
}

static size_t getMemoryUsageMB(bool bypass_cache) {
  size_t current_mem_usage = 0;
  MemoryUsageCache* mem_usage_cache_singleton = MemoryUsageCache::GetInstance();
  if (!bypass_cache &&
      mem_usage_cache_singleton->IsCachedValueValid(&current_mem_usage))
    return current_mem_usage;

  current_mem_usage = MemoryUsageKB() >> 10;
  mem_usage_cache_singleton->SetMemoryValue(current_mem_usage);
  return current_mem_usage;
}

size_t WebKitPlatformSupportImpl::memoryUsageMB() {
  return getMemoryUsageMB(false);
}

size_t WebKitPlatformSupportImpl::actualMemoryUsageMB() {
  return getMemoryUsageMB(true);
}

size_t WebKitPlatformSupportImpl::physicalMemoryMB() {
  return static_cast<size_t>(base::SysInfo::AmountOfPhysicalMemoryMB());
}

size_t WebKitPlatformSupportImpl::numberOfProcessors() {
  return static_cast<size_t>(base::SysInfo::NumberOfProcessors());
}

void WebKitPlatformSupportImpl::startHeapProfiling(
  const blink::WebString& prefix) {
  // FIXME(morrita): Make this built on windows.
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerStart(prefix.utf8().data());
#endif
}

void WebKitPlatformSupportImpl::stopHeapProfiling() {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerStop();
#endif
}

void WebKitPlatformSupportImpl::dumpHeapProfiling(
  const blink::WebString& reason) {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  HeapProfilerDump(reason.utf8().data());
#endif
}

WebString WebKitPlatformSupportImpl::getHeapProfile() {
#if !defined(NO_TCMALLOC) && defined(USE_TCMALLOC) && !defined(OS_WIN)
  char* data = GetHeapProfile();
  WebString result = WebString::fromUTF8(std::string(data));
  free(data);
  return result;
#else
  return WebString();
#endif
}

bool WebKitPlatformSupportImpl::processMemorySizesInBytes(
    size_t* private_bytes,
    size_t* shared_bytes) {
  return CurrentProcessMetrics()->GetMemoryBytes(private_bytes, shared_bytes);
}

bool WebKitPlatformSupportImpl::memoryAllocatorWasteInBytes(size_t* size) {
  return base::allocator::GetAllocatorWasteSize(size);
}

size_t WebKitPlatformSupportImpl::maxDecodedImageBytes() {
#if defined(OS_ANDROID)
  if (base::android::SysUtils::IsLowEndDevice()) {
    // Limit image decoded size to 3M pixels on low end devices.
    // 4 is maximum number of bytes per pixel.
    return 3 * 1024 * 1024 * 4;
  }
  // For other devices, limit decoded image size based on the amount of physical
  // memory. For a device with 2GB physical memory the limit is 16M pixels.
  return base::SysInfo::AmountOfPhysicalMemory() / 32;
#else
  return noDecodedImageByteLimit;
#endif
}

void WebKitPlatformSupportImpl::SuspendSharedTimer() {
  ++shared_timer_suspended_;
}

void WebKitPlatformSupportImpl::ResumeSharedTimer() {
  // The shared timer may have fired or been adjusted while we were suspended.
  if (--shared_timer_suspended_ == 0 &&
      (!shared_timer_.IsRunning() ||
       shared_timer_fire_time_was_set_while_suspended_)) {
    shared_timer_fire_time_was_set_while_suspended_ = false;
    setSharedTimerFireInterval(
        shared_timer_fire_time_ - monotonicallyIncreasingTime());
  }
}

}  // namespace webkit_glue
