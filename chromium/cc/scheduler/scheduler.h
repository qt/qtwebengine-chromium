// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_SCHEDULER_H_
#define CC_SCHEDULER_SCHEDULER_H_

#include <string>

#include "base/basictypes.h"
#include "base/cancelable_callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/time/time.h"
#include "cc/base/cc_export.h"
#include "cc/output/begin_frame_args.h"
#include "cc/scheduler/scheduler_settings.h"
#include "cc/scheduler/scheduler_state_machine.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class Thread;

struct DrawSwapReadbackResult {
  DrawSwapReadbackResult()
      : did_draw(false), did_swap(false), did_readback(false) {}
  DrawSwapReadbackResult(bool did_draw, bool did_swap, bool did_readback)
      : did_draw(did_draw), did_swap(did_swap), did_readback(did_readback) {}
  bool did_draw;
  bool did_swap;
  bool did_readback;
};

class SchedulerClient {
 public:
  virtual void SetNeedsBeginImplFrame(bool enable) = 0;
  virtual void ScheduledActionSendBeginMainFrame() = 0;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapIfPossible() = 0;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndSwapForced() = 0;
  virtual DrawSwapReadbackResult ScheduledActionDrawAndReadback() = 0;
  virtual void ScheduledActionCommit() = 0;
  virtual void ScheduledActionUpdateVisibleTiles() = 0;
  virtual void ScheduledActionActivatePendingTree() = 0;
  virtual void ScheduledActionBeginOutputSurfaceCreation() = 0;
  virtual void ScheduledActionAcquireLayerTexturesForMainThread() = 0;
  virtual void ScheduledActionManageTiles() = 0;
  virtual void DidAnticipatedDrawTimeChange(base::TimeTicks time) = 0;
  virtual base::TimeDelta DrawDurationEstimate() = 0;
  virtual base::TimeDelta BeginMainFrameToCommitDurationEstimate() = 0;
  virtual base::TimeDelta CommitToActivateDurationEstimate() = 0;
  virtual void PostBeginImplFrameDeadline(const base::Closure& closure,
                                          base::TimeTicks deadline) = 0;
  virtual void DidBeginImplFrameDeadline() = 0;

 protected:
  virtual ~SchedulerClient() {}
};

class CC_EXPORT Scheduler {
 public:
  static scoped_ptr<Scheduler> Create(
      SchedulerClient* client,
      const SchedulerSettings& scheduler_settings,
      int layer_tree_host_id) {
    return make_scoped_ptr(
        new Scheduler(client, scheduler_settings, layer_tree_host_id));
  }

  virtual ~Scheduler();

  void SetCanStart();

  void SetVisible(bool visible);
  void SetCanDraw(bool can_draw);
  void NotifyReadyToActivate();

  void SetNeedsCommit();

  // Like SetNeedsCommit(), but ensures a commit will definitely happen even if
  // we are not visible. Will eventually result in a forced draw internally.
  void SetNeedsForcedCommitForReadback();

  void SetNeedsRedraw();

  void SetNeedsManageTiles();

  void SetMainThreadNeedsLayerTextures();

  void SetSwapUsedIncompleteTile(bool used_incomplete_tile);

  void SetSmoothnessTakesPriority(bool smoothness_takes_priority);

  void FinishCommit();
  void BeginMainFrameAborted(bool did_handle);

  void DidManageTiles();
  void DidLoseOutputSurface();
  void DidCreateAndInitializeOutputSurface();
  bool HasInitializedOutputSurface() const {
    return state_machine_.HasInitializedOutputSurface();
  }

  bool CommitPending() const { return state_machine_.CommitPending(); }
  bool RedrawPending() const { return state_machine_.RedrawPending(); }
  bool ManageTilesPending() const {
    return state_machine_.ManageTilesPending();
  }
  bool MainThreadIsInHighLatencyMode() const {
    return state_machine_.MainThreadIsInHighLatencyMode();
  }

  bool WillDrawIfNeeded() const;

  base::TimeTicks AnticipatedDrawTime();

  base::TimeTicks LastBeginImplFrameTime();

  void BeginImplFrame(const BeginFrameArgs& args);
  void OnBeginImplFrameDeadline();
  void PollForAnticipatedDrawTriggers();

  scoped_ptr<base::Value> StateAsValue() {
    return state_machine_.AsValue().Pass();
  }

  bool IsInsideAction(SchedulerStateMachine::Action action) {
    return inside_action_ == action;
  }

 private:
  Scheduler(SchedulerClient* client,
            const SchedulerSettings& scheduler_settings,
            int layer_tree_host_id);

  void PostBeginImplFrameDeadline(base::TimeTicks deadline);
  void SetupNextBeginImplFrameIfNeeded();
  void ActivatePendingTree();
  void DrawAndSwapIfPossible();
  void DrawAndSwapForced();
  void DrawAndReadback();
  void ProcessScheduledActions();

  bool CanCommitAndActivateBeforeDeadline() const;
  void AdvanceCommitStateIfPossible();

  const SchedulerSettings settings_;
  SchedulerClient* client_;
  int layer_tree_host_id_;

  bool last_set_needs_begin_impl_frame_;
  BeginFrameArgs last_begin_impl_frame_args_;
  base::CancelableClosure begin_impl_frame_deadline_closure_;
  base::CancelableClosure poll_for_draw_triggers_closure_;
  base::RepeatingTimer<Scheduler> advance_commit_state_timer_;

  SchedulerStateMachine state_machine_;
  bool inside_process_scheduled_actions_;
  SchedulerStateMachine::Action inside_action_;

  base::WeakPtrFactory<Scheduler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Scheduler);
};

}  // namespace cc

#endif  // CC_SCHEDULER_SCHEDULER_H_
