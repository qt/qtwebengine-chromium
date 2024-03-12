// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/compose/core/browser/compose_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace compose {

const char kComposeDialogInnerTextShortenedBy[] =
    "Compose.Dialog.InnerTextShortenedBy";
const char kComposeDialogInnerTextSize[] = "Compose.Dialog.InnerTextSize";
const char kComposeDialogOpenLatency[] = "Compose.Dialog.OpenLatency";
const char kComposeDialogSelectionLength[] = "Compose.Dialog.SelectionLength";
const char kComposeRequestReason[] = "Compose.Request.Reason";
const char kComposeRequestDurationOk[] = "Compose.Request.Duration.Ok";
const char kComposeRequestDurationError[] = "Compose.Request.Duration.Error";
const char kComposeRequestStatus[] = "Compose.Request.Status";
const char kComposeSessionComposeCount[] = "Compose.Session.ComposeCount";
const char kComposeSessionCloseReason[] = "Compose.Session.CloseReason";
const char kComposeSessionDialogShownCount[] =
    "Compose.Session.DialogShownCount";
const char kComposeSessionEventCounts[] = "Compose.Session.EventCounts";
const char kComposeSessionDuration[] = "Compose.Session.Duration";
const char kComposeSessionOverOneDay[] = "Compose.Session.Duration.OverOneDay";
const char kComposeSessionUndoCount[] = "Compose.Session.UndoCount";
const char kComposeSessionUpdateInputCount[] =
    "Compose.Session.SubmitEditCount";
const char kComposeShowStatus[] = "Compose.ContextMenu.ShowStatus";
const char kComposeMSBBSessionCloseReason[] =
    "Compose.Session.FRE.MSBB.CloseReason";
const char kComposeMSBBSessionDialogShownCount[] =
    "Compose.Session.FRE.MSBB.DialogShownCount";
const char kComposeFirstRunSessionCloseReason[] =
    "Compose.Session.FRE.Disclaimer.CloseReason";
const char kComposeFirstRunSessionDialogShownCount[] =
    "Compose.Session.FRE.Disclaimer.DialogShownCount";
const char kInnerTextNodeOffsetFound[] =
    "Compose.Dialog.InnerTextNodeOffsetFound";
const char kComposeContextMenuCtr[] = "Compose.ContextMenu.CTR";
const char kOpenComposeDialogResult[] =
    "Compose.ContextMenu.OpenComposeDialogResult";

PageUkmTracker::PageUkmTracker(ukm::SourceId source_id)
    : source_id(source_id) {}

PageUkmTracker::~PageUkmTracker() {
  MaybeLogUkm();
}

void PageUkmTracker::MenuItemShown() {
  event_was_recorded_ = true;
  ++menu_item_shown_count_;
}
void PageUkmTracker::MenuItemClicked() {
  event_was_recorded_ = true;
  ++menu_item_clicked_count_;
}
void PageUkmTracker::ComposeTextInserted() {
  event_was_recorded_ = true;
  ++compose_text_inserted_count_;
}

void PageUkmTracker::ShowDialogAbortedDueToMissingFormData() {
  event_was_recorded_ = true;
  ++missing_form_data_count_;
}

void PageUkmTracker::ShowDialogAbortedDueToMissingFormFieldData() {
  event_was_recorded_ = true;
  ++missing_form_field_data_count_;
}

void PageUkmTracker::MaybeLogUkm() {
  if (!event_was_recorded_) {
    return;
  }

  ukm::builders::Compose_PageEvents(source_id)
      .SetMenuItemShown(
          ukm::GetExponentialBucketMinForCounts1000(menu_item_shown_count_))
      .SetMenuItemClicked(
          ukm::GetExponentialBucketMinForCounts1000(menu_item_clicked_count_))
      .SetComposeTextInserted(ukm::GetExponentialBucketMinForCounts1000(
          compose_text_inserted_count_))
      .SetMissingFormData(
          ukm::GetExponentialBucketMinForCounts1000(missing_form_data_count_))
      .SetMissingFormFieldData(ukm::GetExponentialBucketMinForCounts1000(
          missing_form_field_data_count_))
      .Record(ukm::UkmRecorder::Get());
}

ComposeSessionEvents::ComposeSessionEvents() {}

void LogComposeContextMenuCtr(ComposeContextMenuCtrEvent event) {
  UMA_HISTOGRAM_ENUMERATION(kComposeContextMenuCtr, event);
}

void LogComposeContextMenuShowStatus(ComposeShowStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kComposeShowStatus, status);
}

void LogOpenComposeDialogResult(OpenComposeDialogResult result) {
  UMA_HISTOGRAM_ENUMERATION(kOpenComposeDialogResult, result);
}

void LogComposeRequestReason(ComposeRequestReason reason) {
  UMA_HISTOGRAM_ENUMERATION(kComposeRequestReason, reason);
}

void LogComposeRequestDuration(base::TimeDelta duration, bool is_valid) {
  base::UmaHistogramMediumTimes(
      is_valid ? kComposeRequestDurationOk : kComposeRequestDurationError,
      duration);
}

void LogComposeFirstRunSessionCloseReason(
    ComposeFirstRunSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeFirstRunSessionCloseReason, reason);
}

void LogComposeFirstRunSessionDialogShownCount(
    ComposeFirstRunSessionCloseReason reason,
    int dialog_shown_count) {
  std::string status;
  switch (reason) {
    case ComposeFirstRunSessionCloseReason::
        kFirstRunDisclaimerAcknowledgedWithoutInsert:
    case ComposeFirstRunSessionCloseReason::
        kFirstRunDisclaimerAcknowledgedWithInsert:
      status = ".Acknowledged";
      break;
    case ComposeFirstRunSessionCloseReason::kCloseButtonPressed:
    case ComposeFirstRunSessionCloseReason::kEndedImplicitly:
    case ComposeFirstRunSessionCloseReason::kNewSessionWithSelectedText:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeFirstRunSessionDialogShownCount + status,
                               dialog_shown_count);
}

void LogComposeMSBBSessionCloseReason(ComposeMSBBSessionCloseReason reason) {
  base::UmaHistogramEnumeration(kComposeMSBBSessionCloseReason, reason);
}

void LogComposeMSBBSessionDialogShownCount(ComposeMSBBSessionCloseReason reason,
                                           int dialog_shown_count) {
  std::string status;
  switch (reason) {
    case ComposeMSBBSessionCloseReason::kMSBBAcceptedWithoutInsert:
    case ComposeMSBBSessionCloseReason::kMSBBAcceptedWithInsert:
      status = ".Accepted";
      break;
    case ComposeMSBBSessionCloseReason::kMSBBEndedImplicitly:
    case ComposeMSBBSessionCloseReason::kMSBBCloseButtonPressed:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeMSBBSessionDialogShownCount + status,
                               dialog_shown_count);
}

void LogComposeSessionCloseMetrics(ComposeSessionCloseReason reason,
                                   const ComposeSessionEvents& session_events) {
  base::UmaHistogramEnumeration(kComposeSessionCloseReason, reason);

  std::string status;
  switch (reason) {
    case ComposeSessionCloseReason::kAcceptedSuggestion:
      status = ".Accepted";
      break;
    case ComposeSessionCloseReason::kCloseButtonPressed:
    case ComposeSessionCloseReason::kEndedImplicitly:
    case ComposeSessionCloseReason::kNewSessionWithSelectedText:
    case ComposeSessionCloseReason::kCanceledBeforeResponseReceived:
      status = ".Ignored";
  }
  base::UmaHistogramCounts1000(kComposeSessionComposeCount + status,
                               session_events.compose_count);
  base::UmaHistogramCounts1000(kComposeSessionDialogShownCount + status,
                               session_events.dialog_shown_count);
  base::UmaHistogramCounts1000(kComposeSessionUndoCount + status,
                               session_events.undo_count);
  base::UmaHistogramCounts1000(kComposeSessionUpdateInputCount + status,
                               session_events.update_input_count);

  // Log all events that occurred during the session. Each event type can be
  // logged at most once per session.
  if (session_events.dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kDialogShown);
  }
  if (session_events.fre_dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kFREShown);
  }
  if (session_events.fre_completed_in_session) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kFREAccepted);
  }
  if (session_events.msbb_dialog_shown_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kMSBBShown);
  }
  if (session_events.msbb_settings_opened) {
    base::UmaHistogramEnumeration(
        kComposeSessionEventCounts,
        ComposeSessionEventTypes::kMSBBSettingsOpened);
  }
  if (session_events.msbb_enabled_in_session) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kMSBBEnabled);
  }
  if (session_events.has_initial_text) {
    base::UmaHistogramEnumeration(
        kComposeSessionEventCounts,
        ComposeSessionEventTypes::kStartedWithSelection);
  }
  if (session_events.compose_count > 0) {
    // The first Compose event has to be "Create".
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kCreateClicked);
  }
  if (session_events.update_input_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kUpdateClicked);
  }
  if (session_events.regenerate_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kRetryClicked);
  }
  if (session_events.undo_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kUndoClicked);
  }
  if (session_events.shorten_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kShortenClicked);
  }
  if (session_events.lengthen_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kElaborateClicked);
  }
  if (session_events.casual_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kCasualClicked);
  }
  if (session_events.formal_count > 0) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kFormalClicked);
  }
  if (session_events.has_thumbs_down) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kThumbsDown);
  }
  if (session_events.has_thumbs_up) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kThumbsUp);
  }
  if (session_events.inserted_results) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kInsertClicked);
  }
  if (session_events.close_clicked) {
    base::UmaHistogramEnumeration(kComposeSessionEventCounts,
                                  ComposeSessionEventTypes::kCloseClicked);
  }
}

void LogComposeSessionCloseUkmMetrics(
    ukm::SourceId source_id,
    const ComposeSessionEvents& session_events) {
  // Log the UKM metrics for this session.
  ukm::builders::Compose_SessionProgress(source_id)
      .SetDialogShownCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.dialog_shown_count))
      .SetComposeCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.compose_count))
      .SetShortenCount(session_events.shorten_count)
      .SetLengthenCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.lengthen_count))
      .SetFormalCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.formal_count))
      .SetCasualCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.casual_count))
      .SetRegenerateCount(ukm::GetExponentialBucketMinForCounts1000(
          session_events.regenerate_count))
      .SetUndoCount(
          ukm::GetExponentialBucketMinForCounts1000(session_events.undo_count))
      .SetInsertedResults(session_events.inserted_results)
      .SetCanceled(session_events.close_clicked)
      .Record(ukm::UkmRecorder::Get());
}

void LogComposeDialogInnerTextShortenedBy(int shortened_by) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextShortenedBy, shortened_by);
}

void LogComposeDialogInnerTextSize(int size) {
  base::UmaHistogramCounts10M(kComposeDialogInnerTextSize, size);
}

void LogComposeDialogInnerTextOffsetFound(bool inner_offset_found) {
  UMA_HISTOGRAM_ENUMERATION(kInnerTextNodeOffsetFound,
                            inner_offset_found
                                ? ComposeInnerTextNodeOffset::kOffsetFound
                                : ComposeInnerTextNodeOffset::kNoOffsetFound);
}

void LogComposeDialogOpenLatency(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes(kComposeDialogOpenLatency, duration);
}

void LogComposeDialogSelectionLength(int length) {
  // The autofill::kMaxSelectedTextLength is in UTF16 bytes so divide by 2 for
  // the maximum number of unicode code points.
  const int max_selection_size = 51200 / 2;
  base::UmaHistogramCustomCounts(kComposeDialogSelectionLength, length, 1,
                                 max_selection_size + 1, 100);
}

void LogComposeSessionDuration(base::TimeDelta session_duration,
                               std::string session_suffix) {
  base::UmaHistogramLongTimes100(kComposeSessionDuration + session_suffix,
                                 session_duration);

  if (session_duration.InDays() > 1) {
    base::UmaHistogramBoolean(kComposeSessionOverOneDay, true);
  } else {
    base::UmaHistogramBoolean(kComposeSessionOverOneDay, false);
  }
}

}  // namespace compose
