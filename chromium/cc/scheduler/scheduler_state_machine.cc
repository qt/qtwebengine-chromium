// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/scheduler_state_machine.h"

#include "base/debug/trace_event.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "ui/gfx/frame_time.h"

namespace cc {

SchedulerStateMachine::SchedulerStateMachine(const SchedulerSettings& settings)
    : settings_(settings),
      output_surface_state_(OUTPUT_SURFACE_LOST),
      begin_impl_frame_state_(BEGIN_IMPL_FRAME_STATE_IDLE),
      commit_state_(COMMIT_STATE_IDLE),
      texture_state_(LAYER_TEXTURE_STATE_UNLOCKED),
      forced_redraw_state_(FORCED_REDRAW_STATE_IDLE),
      readback_state_(READBACK_STATE_IDLE),
      commit_count_(0),
      current_frame_number_(0),
      last_frame_number_swap_performed_(-1),
      last_frame_number_begin_main_frame_sent_(-1),
      last_frame_number_update_visible_tiles_was_called_(-1),
      last_frame_number_manage_tiles_called_(-1),
      consecutive_failed_draws_(0),
      needs_redraw_(false),
      needs_manage_tiles_(false),
      swap_used_incomplete_tile_(false),
      needs_commit_(false),
      main_thread_needs_layer_textures_(false),
      inside_poll_for_anticipated_draw_triggers_(false),
      visible_(false),
      can_start_(false),
      can_draw_(false),
      has_pending_tree_(false),
      pending_tree_is_ready_for_activation_(false),
      active_tree_needs_first_draw_(false),
      draw_if_possible_failed_(false),
      did_create_and_initialize_first_output_surface_(false),
      smoothness_takes_priority_(false),
      skip_begin_main_frame_to_reduce_latency_(false) {}

const char* SchedulerStateMachine::OutputSurfaceStateToString(
    OutputSurfaceState state) {
  switch (state) {
    case OUTPUT_SURFACE_ACTIVE:
      return "OUTPUT_SURFACE_ACTIVE";
    case OUTPUT_SURFACE_LOST:
      return "OUTPUT_SURFACE_LOST";
    case OUTPUT_SURFACE_CREATING:
      return "OUTPUT_SURFACE_CREATING";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT";
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return "OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::BeginImplFrameStateToString(
    BeginImplFrameState state) {
  switch (state) {
    case BEGIN_IMPL_FRAME_STATE_IDLE:
      return "BEGIN_IMPL_FRAME_STATE_IDLE";
    case BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING:
      return "BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING";
    case BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME:
      return "BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME";
    case BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE:
      return "BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::CommitStateToString(CommitState state) {
  switch (state) {
    case COMMIT_STATE_IDLE:
      return "COMMIT_STATE_IDLE";
    case COMMIT_STATE_FRAME_IN_PROGRESS:
      return "COMMIT_STATE_FRAME_IN_PROGRESS";
    case COMMIT_STATE_READY_TO_COMMIT:
      return "COMMIT_STATE_READY_TO_COMMIT";
    case COMMIT_STATE_WAITING_FOR_FIRST_DRAW:
      return "COMMIT_STATE_WAITING_FOR_FIRST_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::TextureStateToString(TextureState state) {
  switch (state) {
    case LAYER_TEXTURE_STATE_UNLOCKED:
      return "LAYER_TEXTURE_STATE_UNLOCKED";
    case LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD:
      return "LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD";
    case LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD:
      return "LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::SynchronousReadbackStateToString(
    SynchronousReadbackState state) {
  switch (state) {
    case READBACK_STATE_IDLE:
      return "READBACK_STATE_IDLE";
    case READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME:
      return "READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME";
    case READBACK_STATE_WAITING_FOR_COMMIT:
      return "READBACK_STATE_WAITING_FOR_COMMIT";
    case READBACK_STATE_WAITING_FOR_ACTIVATION:
      return "READBACK_STATE_WAITING_FOR_ACTIVATION";
    case READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK:
      return "READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK";
    case READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT:
      return "READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT";
    case READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION:
      return "READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ForcedRedrawOnTimeoutStateToString(
    ForcedRedrawOnTimeoutState state) {
  switch (state) {
    case FORCED_REDRAW_STATE_IDLE:
      return "FORCED_REDRAW_STATE_IDLE";
    case FORCED_REDRAW_STATE_WAITING_FOR_COMMIT:
      return "FORCED_REDRAW_STATE_WAITING_FOR_COMMIT";
    case FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION:
      return "FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION";
    case FORCED_REDRAW_STATE_WAITING_FOR_DRAW:
      return "FORCED_REDRAW_STATE_WAITING_FOR_DRAW";
  }
  NOTREACHED();
  return "???";
}

const char* SchedulerStateMachine::ActionToString(Action action) {
  switch (action) {
    case ACTION_NONE:
      return "ACTION_NONE";
    case ACTION_SEND_BEGIN_MAIN_FRAME:
      return "ACTION_SEND_BEGIN_MAIN_FRAME";
    case ACTION_COMMIT:
      return "ACTION_COMMIT";
    case ACTION_UPDATE_VISIBLE_TILES:
      return "ACTION_UPDATE_VISIBLE_TILES";
    case ACTION_ACTIVATE_PENDING_TREE:
      return "ACTION_ACTIVATE_PENDING_TREE";
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE:
      return "ACTION_DRAW_AND_SWAP_IF_POSSIBLE";
    case ACTION_DRAW_AND_SWAP_FORCED:
      return "ACTION_DRAW_AND_SWAP_FORCED";
    case ACTION_DRAW_AND_SWAP_ABORT:
      return "ACTION_DRAW_AND_SWAP_ABORT";
    case ACTION_DRAW_AND_READBACK:
      return "ACTION_DRAW_AND_READBACK";
    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      return "ACTION_BEGIN_OUTPUT_SURFACE_CREATION";
    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      return "ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD";
    case ACTION_MANAGE_TILES:
      return "ACTION_MANAGE_TILES";
  }
  NOTREACHED();
  return "???";
}

scoped_ptr<base::Value> SchedulerStateMachine::AsValue() const  {
  scoped_ptr<base::DictionaryValue> state(new base::DictionaryValue);

  scoped_ptr<base::DictionaryValue> major_state(new base::DictionaryValue);
  major_state->SetString("next_action", ActionToString(NextAction()));
  major_state->SetString("begin_impl_frame_state",
                         BeginImplFrameStateToString(begin_impl_frame_state_));
  major_state->SetString("commit_state", CommitStateToString(commit_state_));
  major_state->SetString("texture_state_",
                         TextureStateToString(texture_state_));
  major_state->SetString("output_surface_state_",
                         OutputSurfaceStateToString(output_surface_state_));
  major_state->SetString(
      "forced_redraw_state",
      ForcedRedrawOnTimeoutStateToString(forced_redraw_state_));
  major_state->SetString("readback_state",
                         SynchronousReadbackStateToString(readback_state_));
  state->Set("major_state", major_state.release());

  scoped_ptr<base::DictionaryValue> timestamps_state(new base::DictionaryValue);
  base::TimeTicks now = gfx::FrameTime::Now();
  timestamps_state->SetDouble(
      "0_interval",
      last_begin_impl_frame_args_.interval.InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "1_now_to_deadline",
      (last_begin_impl_frame_args_.deadline - now).InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "2_frame_time_to_now",
      (now - last_begin_impl_frame_args_.frame_time).InMicroseconds() /
          1000.0L);
  timestamps_state->SetDouble(
      "3_frame_time_to_deadline",
      (last_begin_impl_frame_args_.deadline -
              last_begin_impl_frame_args_.frame_time).InMicroseconds() /
          1000.0L);
  timestamps_state->SetDouble(
      "4_now", (now - base::TimeTicks()).InMicroseconds() / 1000.0L);
  timestamps_state->SetDouble(
      "5_frame_time",
      (last_begin_impl_frame_args_.frame_time - base::TimeTicks())
              .InMicroseconds() /
          1000.0L);
  timestamps_state->SetDouble(
      "6_deadline",
      (last_begin_impl_frame_args_.deadline - base::TimeTicks())
              .InMicroseconds() /
          1000.0L);
  state->Set("major_timestamps_in_ms", timestamps_state.release());

  scoped_ptr<base::DictionaryValue> minor_state(new base::DictionaryValue);
  minor_state->SetInteger("commit_count", commit_count_);
  minor_state->SetInteger("current_frame_number", current_frame_number_);

  minor_state->SetInteger("last_frame_number_swap_performed",
                          last_frame_number_swap_performed_);
  minor_state->SetInteger(
      "last_frame_number_begin_main_frame_sent",
      last_frame_number_begin_main_frame_sent_);
  minor_state->SetInteger(
      "last_frame_number_update_visible_tiles_was_called",
      last_frame_number_update_visible_tiles_was_called_);

  minor_state->SetInteger("consecutive_failed_draws",
                          consecutive_failed_draws_);
  minor_state->SetBoolean("needs_redraw", needs_redraw_);
  minor_state->SetBoolean("needs_manage_tiles", needs_manage_tiles_);
  minor_state->SetBoolean("swap_used_incomplete_tile",
                          swap_used_incomplete_tile_);
  minor_state->SetBoolean("needs_commit", needs_commit_);
  minor_state->SetBoolean("main_thread_needs_layer_textures",
                          main_thread_needs_layer_textures_);
  minor_state->SetBoolean("visible", visible_);
  minor_state->SetBoolean("can_start", can_start_);
  minor_state->SetBoolean("can_draw", can_draw_);
  minor_state->SetBoolean("has_pending_tree", has_pending_tree_);
  minor_state->SetBoolean("pending_tree_is_ready_for_activation",
                          pending_tree_is_ready_for_activation_);
  minor_state->SetBoolean("active_tree_needs_first_draw",
                          active_tree_needs_first_draw_);
  minor_state->SetBoolean("draw_if_possible_failed", draw_if_possible_failed_);
  minor_state->SetBoolean("did_create_and_initialize_first_output_surface",
                          did_create_and_initialize_first_output_surface_);
  minor_state->SetBoolean("smoothness_takes_priority",
                          smoothness_takes_priority_);
  minor_state->SetBoolean("main_thread_is_in_high_latency_mode",
                          MainThreadIsInHighLatencyMode());
  minor_state->SetBoolean("skip_begin_main_frame_to_reduce_latency",
                          skip_begin_main_frame_to_reduce_latency_);
  state->Set("minor_state", minor_state.release());

  return state.PassAs<base::Value>();
}

bool SchedulerStateMachine::HasSentBeginMainFrameThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_begin_main_frame_sent_;
}

bool SchedulerStateMachine::HasUpdatedVisibleTilesThisFrame() const {
  return current_frame_number_ ==
         last_frame_number_update_visible_tiles_was_called_;
}

bool SchedulerStateMachine::HasSwappedThisFrame() const {
  return current_frame_number_ == last_frame_number_swap_performed_;
}

bool SchedulerStateMachine::PendingDrawsShouldBeAborted() const {
  // These are all the cases where we normally cannot or do not want to draw
  // but, if needs_redraw_ is true and we do not draw to make forward progress,
  // we might deadlock with the main thread.
  // This should be a superset of PendingActivationsShouldBeForced() since
  // activation of the pending tree is blocked by drawing of the active tree and
  // the main thread might be blocked on activation of the most recent commit.
  if (PendingActivationsShouldBeForced())
    return true;

  // Additional states where we should abort draws.
  // Note: We don't force activation in these cases because doing so would
  // result in checkerboarding on resize, becoming visible, etc.
  if (!can_draw_)
    return true;
  if (!visible_)
    return true;
  return false;
}

bool SchedulerStateMachine::PendingActivationsShouldBeForced() const {
  // These are all the cases where, if we do not force activations to make
  // forward progress, we might deadlock with the main thread.

  // The impl thread cannot lock layer textures unless the pending
  // tree can be activated to unblock the commit.
  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD)
    return true;

  // There is no output surface to trigger our activations.
  if (output_surface_state_ == OUTPUT_SURFACE_LOST)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldBeginOutputSurfaceCreation() const {
  // Don't try to initialize too early.
  if (!can_start_)
    return false;

  // We only want to start output surface initialization after the
  // previous commit is complete.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // We want to clear the pipline of any pending draws and activations
  // before starting output surface initialization. This allows us to avoid
  // weird corner cases where we abort draws or force activation while we
  // are initializing the output surface and can potentially have a pending
  // readback.
  if (active_tree_needs_first_draw_ || has_pending_tree_)
    return false;

  // We need to create the output surface if we don't have one and we haven't
  // started creating one yet.
  return output_surface_state_ == OUTPUT_SURFACE_LOST;
}

bool SchedulerStateMachine::ShouldDraw() const {
  // After a readback, make sure not to draw again until we've replaced the
  // readback commit with a real one.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT ||
      readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
    return false;

  // Draw immediately for readbacks to unblock the main thread quickly.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    return true;
  }

  // If we need to abort draws, we should do so ASAP since the draw could
  // be blocking other important actions (like output surface initialization),
  // from occuring. If we are waiting for the first draw, then perfom the
  // aborted draw to keep things moving. If we are not waiting for the first
  // draw however, we don't want to abort for no reason.
  if (PendingDrawsShouldBeAborted())
    return active_tree_needs_first_draw_;

  // After this line, we only want to swap once per frame.
  if (HasSwappedThisFrame())
    return false;

  // Except for the cases above, do not draw outside of the BeginImplFrame
  // deadline.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
    return false;

  // Only handle forced redraws due to timeouts on the regular deadline.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    return true;
  }

  return needs_redraw_;
}

bool SchedulerStateMachine::ShouldAcquireLayerTexturesForMainThread() const {
  if (!main_thread_needs_layer_textures_)
    return false;
  if (texture_state_ == LAYER_TEXTURE_STATE_UNLOCKED)
    return true;
  DCHECK_EQ(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD);
  return false;
}

bool SchedulerStateMachine::ShouldActivatePendingTree() const {
  // There is nothing to activate.
  if (!has_pending_tree_)
    return false;

  // We should not activate a second tree before drawing the first one.
  // Even if we need to force activation of the pending tree, we should abort
  // drawing the active tree first.
  if (active_tree_needs_first_draw_)
    return false;

  // If we want to force activation, do so ASAP.
  if (PendingActivationsShouldBeForced())
    return true;

  // At this point, only activate if we are ready to activate.
  return pending_tree_is_ready_for_activation_;
}

bool SchedulerStateMachine::ShouldUpdateVisibleTiles() const {
  if (!settings_.impl_side_painting)
    return false;
  if (HasUpdatedVisibleTilesThisFrame())
    return false;

  // There's no reason to check for tiles if we don't have an output surface.
  if (!HasInitializedOutputSurface())
    return false;

  // We should not check for visible tiles until we've entered the deadline so
  // we check as late as possible and give the tiles more time to initialize.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
    return false;

  // If the last swap drew with checkerboard or missing tiles, we should
  // poll for any new visible tiles so we can be notified to draw again
  // when there are.
  if (swap_used_incomplete_tile_)
    return true;

  return false;
}

bool SchedulerStateMachine::ShouldSendBeginMainFrame() const {
  if (!needs_commit_)
    return false;

  // Only send BeginMainFrame when there isn't another commit pending already.
  if (commit_state_ != COMMIT_STATE_IDLE)
    return false;

  // We can't accept a commit if we have a pending tree.
  if (has_pending_tree_)
    return false;

  // We want to handle readback commits immediately to unblock the main thread.
  // Note: This BeginMainFrame will correspond to the replacement commit that
  // comes after the readback commit itself, so we only send the BeginMainFrame
  // if a commit isn't already pending behind the readback.
  if (readback_state_ == READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME)
    return !CommitPending();

  // We do not need commits if we are not visible, unless there's a
  // request for a readback.
  if (!visible_)
    return false;

  // We want to start the first commit after we get a new output surface ASAP.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT)
    return true;

  // With deadline scheduling enabled, we should not send BeginMainFrame while
  // we are in BEGIN_IMPL_FRAME_STATE_IDLE, since we might have new user input
  // coming in soon.
  // However, if we are not expecting a BeginImplFrame to take us out of idle,
  // we should not early out here to avoid blocking commits forever.
  // This only works well when deadline scheduling is enabled because there is
  // an interval over which to accept the commit and draw. Without deadline
  // scheduling, delaying the commit could prevent us from having something
  // to draw on the next BeginImplFrame.
  // TODO(brianderson): Allow sending BeginMainFrame while idle when the main
  // thread isn't consuming user input.
  if (settings_.deadline_scheduling_enabled &&
      begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_IDLE &&
      BeginImplFrameNeeded())
    return false;

  // We need a new commit for the forced redraw. This honors the
  // single commit per interval because the result will be swapped to screen.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT)
    return true;

  // After this point, we only start a commit once per frame.
  if (HasSentBeginMainFrameThisFrame())
    return false;

  // We shouldn't normally accept commits if there isn't an OutputSurface.
  if (!HasInitializedOutputSurface())
    return false;

  if (skip_begin_main_frame_to_reduce_latency_)
    return false;

  return true;
}

bool SchedulerStateMachine::ShouldCommit() const {
  return commit_state_ == COMMIT_STATE_READY_TO_COMMIT;
}

bool SchedulerStateMachine::IsCommitStateWaiting() const {
  return commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS;
}

bool SchedulerStateMachine::ShouldManageTiles() const {
  // ManageTiles only really needs to be called immediately after commit
  // and then periodically after that.  Limiting to once per frame prevents
  // post-commit and post-draw ManageTiles on the same frame.
  if (last_frame_number_manage_tiles_called_ == current_frame_number_)
    return false;

  // Limiting to once per-frame is not enough, since we only want to
  // manage tiles _after_ draws. Polling for draw triggers and
  // begin-frame are mutually exclusive, so we limit to these two cases.
  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE &&
      !inside_poll_for_anticipated_draw_triggers_)
    return false;
  return needs_manage_tiles_;
}

SchedulerStateMachine::Action SchedulerStateMachine::NextAction() const {
  if (ShouldAcquireLayerTexturesForMainThread())
    return ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD;
  if (ShouldUpdateVisibleTiles())
    return ACTION_UPDATE_VISIBLE_TILES;
  if (ShouldActivatePendingTree())
    return ACTION_ACTIVATE_PENDING_TREE;
  if (ShouldCommit())
    return ACTION_COMMIT;
  if (ShouldDraw()) {
    if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK)
      return ACTION_DRAW_AND_READBACK;
    else if (PendingDrawsShouldBeAborted())
      return ACTION_DRAW_AND_SWAP_ABORT;
    else if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
      return ACTION_DRAW_AND_SWAP_FORCED;
    else
      return ACTION_DRAW_AND_SWAP_IF_POSSIBLE;
  }
  if (ShouldManageTiles())
    return ACTION_MANAGE_TILES;
  if (ShouldSendBeginMainFrame())
    return ACTION_SEND_BEGIN_MAIN_FRAME;
  if (ShouldBeginOutputSurfaceCreation())
    return ACTION_BEGIN_OUTPUT_SURFACE_CREATION;
  return ACTION_NONE;
}

void SchedulerStateMachine::CheckInvariants() {
  // We should never try to perform a draw for readback and forced draw due to
  // timeout simultaneously.
  DCHECK(!(forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW &&
           readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK));
}

void SchedulerStateMachine::UpdateState(Action action) {
  switch (action) {
    case ACTION_NONE:
      return;

    case ACTION_UPDATE_VISIBLE_TILES:
      last_frame_number_update_visible_tiles_was_called_ =
          current_frame_number_;
      return;

    case ACTION_ACTIVATE_PENDING_TREE:
      UpdateStateOnActivation();
      return;

    case ACTION_SEND_BEGIN_MAIN_FRAME:
      DCHECK(!has_pending_tree_);
      DCHECK(visible_ ||
             readback_state_ == READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME);
      commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
      needs_commit_ = false;
      if (readback_state_ == READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME)
        readback_state_ = READBACK_STATE_WAITING_FOR_COMMIT;
      last_frame_number_begin_main_frame_sent_ =
          current_frame_number_;
      return;

    case ACTION_COMMIT: {
      bool commit_was_aborted = false;
      UpdateStateOnCommit(commit_was_aborted);
      return;
    }

    case ACTION_DRAW_AND_SWAP_FORCED:
    case ACTION_DRAW_AND_SWAP_IF_POSSIBLE: {
      bool did_swap = true;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_DRAW_AND_SWAP_ABORT:
    case ACTION_DRAW_AND_READBACK: {
      bool did_swap = false;
      UpdateStateOnDraw(did_swap);
      return;
    }

    case ACTION_BEGIN_OUTPUT_SURFACE_CREATION:
      DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_LOST);
      output_surface_state_ = OUTPUT_SURFACE_CREATING;

      // The following DCHECKs make sure we are in the proper quiescent state.
      // The pipeline should be flushed entirely before we start output
      // surface creation to avoid complicated corner cases.
      DCHECK_EQ(commit_state_, COMMIT_STATE_IDLE);
      DCHECK(!has_pending_tree_);
      DCHECK(!active_tree_needs_first_draw_);
      return;

    case ACTION_ACQUIRE_LAYER_TEXTURES_FOR_MAIN_THREAD:
      texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD;
      main_thread_needs_layer_textures_ = false;
      return;

    case ACTION_MANAGE_TILES:
      UpdateStateOnManageTiles();
      return;
  }
}

void SchedulerStateMachine::UpdateStateOnCommit(bool commit_was_aborted) {
  commit_count_++;

  // If we are impl-side-painting but the commit was aborted, then we behave
  // mostly as if we are not impl-side-painting since there is no pending tree.
  has_pending_tree_ = settings_.impl_side_painting && !commit_was_aborted;

  // Update state related to readbacks.
  if (readback_state_ == READBACK_STATE_WAITING_FOR_COMMIT) {
    // Update the state if this is the readback commit.
    readback_state_ = has_pending_tree_
                          ? READBACK_STATE_WAITING_FOR_ACTIVATION
                          : READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK;
  } else if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT) {
    // Update the state if this is the commit replacing the readback commit.
    readback_state_ = has_pending_tree_
                          ? READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION
                          : READBACK_STATE_IDLE;
  } else {
    DCHECK(readback_state_ == READBACK_STATE_IDLE);
  }

  // Readbacks can interrupt output surface initialization and forced draws,
  // so we do not want to advance those states if we are in the middle of a
  // readback. Note: It is possible for the readback's replacement commit to
  // be the output surface's first commit and/or the forced redraw's commit.
  if (readback_state_ == READBACK_STATE_IDLE ||
      readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION) {
    // Update state related to forced draws.
    if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_COMMIT) {
      forced_redraw_state_ = has_pending_tree_
                                 ? FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION
                                 : FORCED_REDRAW_STATE_WAITING_FOR_DRAW;
    }

    // Update the output surface state.
    DCHECK_NE(output_surface_state_,
              OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION);
    if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT) {
      if (has_pending_tree_) {
        output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION;
      } else {
        output_surface_state_ = OUTPUT_SURFACE_ACTIVE;
        needs_redraw_ = true;
      }
    }
  }

  // Update the commit state. We expect and wait for a draw if the commit
  // was not aborted or if we are in a readback or forced draw.
  if (!commit_was_aborted) {
    DCHECK(commit_state_ == COMMIT_STATE_READY_TO_COMMIT);
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
  } else if (readback_state_ != READBACK_STATE_IDLE ||
             forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE) {
    commit_state_ = COMMIT_STATE_WAITING_FOR_FIRST_DRAW;
  } else {
    commit_state_ = COMMIT_STATE_IDLE;
  }

  // Update state if we have a new active tree to draw, or if the active tree
  // was unchanged but we need to do a readback or forced draw.
  if (!has_pending_tree_ &&
      (!commit_was_aborted ||
       readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK ||
       forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)) {
    needs_redraw_ = true;
    active_tree_needs_first_draw_ = true;
  }

  // This post-commit work is common to both completed and aborted commits.
  pending_tree_is_ready_for_activation_ = false;

  if (draw_if_possible_failed_)
    last_frame_number_swap_performed_ = -1;

  // If we are planing to draw with the new commit, lock the layer textures for
  // use on the impl thread. Otherwise, leave them unlocked.
  if (has_pending_tree_ || needs_redraw_)
    texture_state_ = LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD;
  else
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;
}

void SchedulerStateMachine::UpdateStateOnActivation() {
  // Update output surface state.
  if (output_surface_state_ == OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION)
    output_surface_state_ = OUTPUT_SURFACE_ACTIVE;

  // Update readback state
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_ACTIVATION)
    forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_DRAW;

  // Update forced redraw state
  if (readback_state_ == READBACK_STATE_WAITING_FOR_ACTIVATION)
    readback_state_ = READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK;
  else if (readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
    readback_state_ = READBACK_STATE_IDLE;

  has_pending_tree_ = false;
  pending_tree_is_ready_for_activation_ = false;
  active_tree_needs_first_draw_ = true;
  needs_redraw_ = true;
}

void SchedulerStateMachine::UpdateStateOnDraw(bool did_swap) {
  DCHECK(readback_state_ != READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT &&
         readback_state_ != READBACK_STATE_WAITING_FOR_REPLACEMENT_ACTIVATION)
      << *AsValue();

  if (readback_state_ == READBACK_STATE_WAITING_FOR_DRAW_AND_READBACK) {
    // The draw correspons to a readback commit.
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    // We are blocking commits from the main thread until after this draw, so
    // we should not have a pending tree.
    DCHECK(!has_pending_tree_);
    // We transition to COMMIT_STATE_FRAME_IN_PROGRESS because there is a
    // pending BeginMainFrame behind the readback request.
    commit_state_ = COMMIT_STATE_FRAME_IN_PROGRESS;
    readback_state_ = READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT;
  } else if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW) {
    DCHECK_EQ(commit_state_, COMMIT_STATE_WAITING_FOR_FIRST_DRAW);
    commit_state_ = COMMIT_STATE_IDLE;
    forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;
  } else if (commit_state_ == COMMIT_STATE_WAITING_FOR_FIRST_DRAW &&
             !has_pending_tree_) {
    commit_state_ = COMMIT_STATE_IDLE;
  }

  if (texture_state_ == LAYER_TEXTURE_STATE_ACQUIRED_BY_IMPL_THREAD)
    texture_state_ = LAYER_TEXTURE_STATE_UNLOCKED;

  needs_redraw_ = false;
  draw_if_possible_failed_ = false;
  active_tree_needs_first_draw_ = false;

  if (did_swap)
    last_frame_number_swap_performed_ = current_frame_number_;
}

void SchedulerStateMachine::UpdateStateOnManageTiles() {
  needs_manage_tiles_ = false;
}

void SchedulerStateMachine::SetMainThreadNeedsLayerTextures() {
  DCHECK(!main_thread_needs_layer_textures_);
  DCHECK_NE(texture_state_, LAYER_TEXTURE_STATE_ACQUIRED_BY_MAIN_THREAD);
  main_thread_needs_layer_textures_ = true;
}

void SchedulerStateMachine::SetSkipBeginMainFrameToReduceLatency(bool skip) {
  skip_begin_main_frame_to_reduce_latency_ = skip;
}

bool SchedulerStateMachine::BeginImplFrameNeeded() const {
  // Proactive BeginImplFrames are bad for the synchronous compositor because we
  // have to draw when we get the BeginImplFrame and could end up drawing many
  // duplicate frames if our new frame isn't ready in time.
  // To poll for state with the synchronous compositor without having to draw,
  // we rely on ShouldPollForAnticipatedDrawTriggers instead.
  if (!SupportsProactiveBeginImplFrame())
    return BeginImplFrameNeededToDraw();

  return BeginImplFrameNeededToDraw() ||
         ProactiveBeginImplFrameWanted();
}

bool SchedulerStateMachine::ShouldPollForAnticipatedDrawTriggers() const {
  // ShouldPollForAnticipatedDrawTriggers is what we use in place of
  // ProactiveBeginImplFrameWanted when we are using the synchronous
  // compositor.
  if (!SupportsProactiveBeginImplFrame()) {
    return !BeginImplFrameNeededToDraw() &&
           ProactiveBeginImplFrameWanted();
  }

  // Non synchronous compositors should rely on
  // ProactiveBeginImplFrameWanted to poll for state instead.
  return false;
}

bool SchedulerStateMachine::SupportsProactiveBeginImplFrame() const {
  // Both the synchronous compositor and disabled vsync settings
  // make it undesirable to proactively request BeginImplFrames.
  // If this is true, the scheduler should poll.
  return !settings_.using_synchronous_renderer_compositor &&
         settings_.throttle_frame_production;
}

// These are the cases where we definitely (or almost definitely) have a
// new frame to draw and can draw.
bool SchedulerStateMachine::BeginImplFrameNeededToDraw() const {
  // The output surface is the provider of BeginImplFrames, so we are not going
  // to get them even if we ask for them.
  if (!HasInitializedOutputSurface())
    return false;

  // If we can't draw, don't tick until we are notified that we can draw again.
  if (!can_draw_)
    return false;

  // The forced draw respects our normal draw scheduling, so we need to
  // request a BeginImplFrame for it.
  if (forced_redraw_state_ == FORCED_REDRAW_STATE_WAITING_FOR_DRAW)
    return true;

  // There's no need to produce frames if we are not visible.
  if (!visible_)
    return false;

  // We need to draw a more complete frame than we did the last BeginImplFrame,
  // so request another BeginImplFrame in anticipation that we will have
  // additional visible tiles.
  if (swap_used_incomplete_tile_)
    return true;

  return needs_redraw_;
}

// These are cases where we are very likely to draw soon, but might not
// actually have a new frame to draw when we receive the next BeginImplFrame.
// Proactively requesting the BeginImplFrame helps hide the round trip latency
// of the SetNeedsBeginImplFrame request that has to go to the Browser.
bool SchedulerStateMachine::ProactiveBeginImplFrameWanted() const {
  // The output surface is the provider of BeginImplFrames,
  // so we are not going to get them even if we ask for them.
  if (!HasInitializedOutputSurface())
    return false;

  // Do not be proactive when invisible.
  if (!visible_)
    return false;

  // We should proactively request a BeginImplFrame if a commit is pending
  // because we will want to draw if the commit completes quickly.
  if (needs_commit_ || commit_state_ != COMMIT_STATE_IDLE)
    return true;

  // If the pending tree activates quickly, we'll want a BeginImplFrame soon
  // to draw the new active tree.
  if (has_pending_tree_)
    return true;

  // Changing priorities may allow us to activate (given the new priorities),
  // which may result in a new frame.
  if (needs_manage_tiles_)
    return true;

  // If we just swapped, it's likely that we are going to produce another
  // frame soon. This helps avoid negative glitches in our
  // SetNeedsBeginImplFrame requests, which may propagate to the BeginImplFrame
  // provider and get sampled at an inopportune time, delaying the next
  // BeginImplFrame.
  if (last_frame_number_swap_performed_ == current_frame_number_)
    return true;

  return false;
}

void SchedulerStateMachine::OnBeginImplFrame(const BeginFrameArgs& args) {
  current_frame_number_++;
  last_begin_impl_frame_args_ = args;
  DCHECK_EQ(begin_impl_frame_state_, BEGIN_IMPL_FRAME_STATE_IDLE) << *AsValue();
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING;
}

void SchedulerStateMachine::OnBeginImplFrameDeadlinePending() {
  DCHECK_EQ(begin_impl_frame_state_,
            BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING)
      << *AsValue();
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME;
}

void SchedulerStateMachine::OnBeginImplFrameDeadline() {
  DCHECK_EQ(begin_impl_frame_state_, BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
      << *AsValue();
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE;
}

void SchedulerStateMachine::OnBeginImplFrameIdle() {
  DCHECK_EQ(begin_impl_frame_state_, BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE)
      << *AsValue();
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_IDLE;
}

bool SchedulerStateMachine::ShouldTriggerBeginImplFrameDeadlineEarly() const {
  // TODO(brianderson): This should take into account multiple commit sources.

  // If we are in the middle of the readback, we won't swap, so there is
  // no reason to trigger the deadline early.
  if (readback_state_ != READBACK_STATE_IDLE)
    return false;

  if (begin_impl_frame_state_ != BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME)
    return false;

  if (active_tree_needs_first_draw_)
    return true;

  if (!needs_redraw_)
    return false;

  // This is used to prioritize impl-thread draws when the main thread isn't
  // producing anything, e.g., after an aborted commit. We also check that we
  // don't have a pending tree -- otherwise we should give it a chance to
  // activate.
  // TODO(skyostil): Revisit this when we have more accurate deadline estimates.
  if (commit_state_ == COMMIT_STATE_IDLE && !has_pending_tree_)
    return true;

  // Prioritize impl-thread draws in smoothness mode.
  if (smoothness_takes_priority_)
    return true;

  return false;
}

bool SchedulerStateMachine::MainThreadIsInHighLatencyMode() const {
  // If we just sent a BeginMainFrame and haven't hit the deadline yet, the main
  // thread is in a low latency mode.
  if (last_frame_number_begin_main_frame_sent_ == current_frame_number_ &&
      (begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_BEGIN_FRAME_STARTING ||
       begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_INSIDE_BEGIN_FRAME))
    return false;

  // If there's a commit in progress it must either be from the previous frame
  // or it started after the impl thread's deadline. In either case the main
  // thread is in high latency mode.
  if (commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS ||
      commit_state_ == COMMIT_STATE_READY_TO_COMMIT)
    return true;

  // Similarly, if there's a pending tree the main thread is in high latency
  // mode, because either
  //   it's from the previous frame
  // or
  //   we're currently drawing the active tree and the pending tree will thus
  //   only be drawn in the next frame.
  if (has_pending_tree_)
    return true;

  if (begin_impl_frame_state_ == BEGIN_IMPL_FRAME_STATE_INSIDE_DEADLINE) {
    // Even if there's a new active tree to draw at the deadline or we've just
    // drawn it, it may have been triggered by a previous BeginImplFrame, in
    // which case the main thread is in a high latency mode.
    return (active_tree_needs_first_draw_ ||
            last_frame_number_swap_performed_ == current_frame_number_) &&
           last_frame_number_begin_main_frame_sent_ != current_frame_number_;
  }

  // If the active tree needs its first draw in any other state, we know the
  // main thread is in a high latency mode.
  return active_tree_needs_first_draw_;
}

void SchedulerStateMachine::DidEnterPollForAnticipatedDrawTriggers() {
  current_frame_number_++;
  inside_poll_for_anticipated_draw_triggers_ = true;
}

void SchedulerStateMachine::DidLeavePollForAnticipatedDrawTriggers() {
  inside_poll_for_anticipated_draw_triggers_ = false;
}

void SchedulerStateMachine::SetVisible(bool visible) { visible_ = visible; }

void SchedulerStateMachine::SetCanDraw(bool can_draw) { can_draw_ = can_draw; }

void SchedulerStateMachine::SetNeedsRedraw() { needs_redraw_ = true; }

void SchedulerStateMachine::SetNeedsManageTiles() {
  if (!needs_manage_tiles_) {
    TRACE_EVENT0("cc",
                 "SchedulerStateMachine::SetNeedsManageTiles");
    needs_manage_tiles_ = true;
  }
}

void SchedulerStateMachine::SetSwapUsedIncompleteTile(
    bool used_incomplete_tile) {
  swap_used_incomplete_tile_ = used_incomplete_tile;
}

void SchedulerStateMachine::SetSmoothnessTakesPriority(
    bool smoothness_takes_priority) {
  smoothness_takes_priority_ = smoothness_takes_priority;
}

void SchedulerStateMachine::DidDrawIfPossibleCompleted(bool success) {
  draw_if_possible_failed_ = !success;
  if (draw_if_possible_failed_) {
    needs_redraw_ = true;

    // If we're already in the middle of a redraw, we don't need to
    // restart it.
    if (forced_redraw_state_ != FORCED_REDRAW_STATE_IDLE)
      return;

    needs_commit_ = true;
    consecutive_failed_draws_++;
    if (settings_.timeout_and_draw_when_animation_checkerboards &&
        consecutive_failed_draws_ >=
            settings_.maximum_number_of_failed_draws_before_draw_is_forced_) {
      consecutive_failed_draws_ = 0;
      // We need to force a draw, but it doesn't make sense to do this until
      // we've committed and have new textures.
      forced_redraw_state_ = FORCED_REDRAW_STATE_WAITING_FOR_COMMIT;
    }
  } else {
    consecutive_failed_draws_ = 0;
    forced_redraw_state_ = FORCED_REDRAW_STATE_IDLE;
  }
}

void SchedulerStateMachine::SetNeedsCommit() { needs_commit_ = true; }

void SchedulerStateMachine::SetNeedsForcedCommitForReadback() {
  // If this is called in READBACK_STATE_IDLE, this is a "first" readback
  // request.
  // If this is called in READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT, this
  // is a back-to-back readback request that started before the replacement
  // commit had a chance to land.
  DCHECK(readback_state_ == READBACK_STATE_IDLE ||
         readback_state_ == READBACK_STATE_WAITING_FOR_REPLACEMENT_COMMIT);

  // If there is already a commit in progress when we get the readback request
  // (we are in COMMIT_STATE_FRAME_IN_PROGRESS), then we don't need to send a
  // BeginMainFrame for the replacement commit, since there's already a
  // BeginMainFrame behind the readback request. In that case, we can skip
  // READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME and go directly to
  // READBACK_STATE_WAITING_FOR_COMMIT
  if (commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS)
    readback_state_ = READBACK_STATE_WAITING_FOR_COMMIT;
  else
    readback_state_ = READBACK_STATE_NEEDS_BEGIN_MAIN_FRAME;
}

void SchedulerStateMachine::FinishCommit() {
  DCHECK(commit_state_ == COMMIT_STATE_FRAME_IN_PROGRESS) << *AsValue();
  commit_state_ = COMMIT_STATE_READY_TO_COMMIT;
}

void SchedulerStateMachine::BeginMainFrameAborted(bool did_handle) {
  DCHECK_EQ(commit_state_, COMMIT_STATE_FRAME_IN_PROGRESS);
  if (did_handle) {
    bool commit_was_aborted = true;
    UpdateStateOnCommit(commit_was_aborted);
  } else {
    DCHECK_NE(readback_state_, READBACK_STATE_WAITING_FOR_COMMIT);
    commit_state_ = COMMIT_STATE_IDLE;
    SetNeedsCommit();
  }
}

void SchedulerStateMachine::DidManageTiles() {
  needs_manage_tiles_ = false;
  last_frame_number_manage_tiles_called_ = current_frame_number_;
}

void SchedulerStateMachine::DidLoseOutputSurface() {
  if (output_surface_state_ == OUTPUT_SURFACE_LOST ||
      output_surface_state_ == OUTPUT_SURFACE_CREATING)
    return;
  output_surface_state_ = OUTPUT_SURFACE_LOST;
  needs_redraw_ = false;
  begin_impl_frame_state_ = BEGIN_IMPL_FRAME_STATE_IDLE;
}

void SchedulerStateMachine::NotifyReadyToActivate() {
  if (has_pending_tree_)
    pending_tree_is_ready_for_activation_ = true;
}

void SchedulerStateMachine::DidCreateAndInitializeOutputSurface() {
  DCHECK_EQ(output_surface_state_, OUTPUT_SURFACE_CREATING);
  output_surface_state_ = OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT;

  if (did_create_and_initialize_first_output_surface_) {
    // TODO(boliu): See if we can remove this when impl-side painting is always
    // on. Does anything on the main thread need to update after recreate?
    needs_commit_ = true;
  }
  did_create_and_initialize_first_output_surface_ = true;
}

bool SchedulerStateMachine::HasInitializedOutputSurface() const {
  switch (output_surface_state_) {
    case OUTPUT_SURFACE_LOST:
    case OUTPUT_SURFACE_CREATING:
      return false;

    case OUTPUT_SURFACE_ACTIVE:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_COMMIT:
    case OUTPUT_SURFACE_WAITING_FOR_FIRST_ACTIVATION:
      return true;
  }
  NOTREACHED();
  return false;
}

}  // namespace cc
