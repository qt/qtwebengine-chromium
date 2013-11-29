// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/delay_based_time_source.h"

#include <algorithm>
#include <cmath>

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"

namespace cc {

namespace {

// kDoubleTickThreshold prevents ticks from running within the specified
// fraction of an interval.  This helps account for jitter in the timebase as
// well as quick timer reactivation.
static const double kDoubleTickThreshold = 0.25;

// kIntervalChangeThreshold is the fraction of the interval that will trigger an
// immediate interval change.  kPhaseChangeThreshold is the fraction of the
// interval that will trigger an immediate phase change.  If the changes are
// within the thresholds, the change will take place on the next tick.  If
// either change is outside the thresholds, the next tick will be canceled and
// reissued immediately.
static const double kIntervalChangeThreshold = 0.25;
static const double kPhaseChangeThreshold = 0.25;

}  // namespace

scoped_refptr<DelayBasedTimeSource> DelayBasedTimeSource::Create(
    base::TimeDelta interval,
    base::SingleThreadTaskRunner* task_runner) {
  return make_scoped_refptr(new DelayBasedTimeSource(interval, task_runner));
}

DelayBasedTimeSource::DelayBasedTimeSource(
    base::TimeDelta interval, base::SingleThreadTaskRunner* task_runner)
    : client_(NULL),
      has_tick_target_(false),
      current_parameters_(interval, base::TimeTicks()),
      next_parameters_(interval, base::TimeTicks()),
      state_(STATE_INACTIVE),
      task_runner_(task_runner),
      weak_factory_(this) {}

DelayBasedTimeSource::~DelayBasedTimeSource() {}

void DelayBasedTimeSource::SetActive(bool active) {
  TRACE_EVENT1("cc", "DelayBasedTimeSource::SetActive", "active", active);
  if (!active) {
    state_ = STATE_INACTIVE;
    weak_factory_.InvalidateWeakPtrs();
    return;
  }

  if (state_ == STATE_STARTING || state_ == STATE_ACTIVE)
    return;

  if (!has_tick_target_) {
    // Becoming active the first time is deferred: we post a 0-delay task.
    // When it runs, we use that to establish the timebase, become truly
    // active, and fire the first tick.
    state_ = STATE_STARTING;
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&DelayBasedTimeSource::OnTimerFired,
                                      weak_factory_.GetWeakPtr()));
    return;
  }

  state_ = STATE_ACTIVE;

  PostNextTickTask(Now());
}

bool DelayBasedTimeSource::Active() const { return state_ != STATE_INACTIVE; }

base::TimeTicks DelayBasedTimeSource::LastTickTime() { return last_tick_time_; }

base::TimeTicks DelayBasedTimeSource::NextTickTime() {
  return Active() ? current_parameters_.tick_target : base::TimeTicks();
}

void DelayBasedTimeSource::OnTimerFired() {
  DCHECK(state_ != STATE_INACTIVE);

  base::TimeTicks now = this->Now();
  last_tick_time_ = now;

  if (state_ == STATE_STARTING) {
    SetTimebaseAndInterval(now, current_parameters_.interval);
    state_ = STATE_ACTIVE;
  }

  PostNextTickTask(now);

  // Fire the tick.
  if (client_)
    client_->OnTimerTick();
}

void DelayBasedTimeSource::SetClient(TimeSourceClient* client) {
  client_ = client;
}

void DelayBasedTimeSource::SetTimebaseAndInterval(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {
  next_parameters_.interval = interval;
  next_parameters_.tick_target = timebase;
  has_tick_target_ = true;

  if (state_ != STATE_ACTIVE) {
    // If we aren't active, there's no need to reset the timer.
    return;
  }

  // If the change in interval is larger than the change threshold,
  // request an immediate reset.
  double interval_delta =
      std::abs((interval - current_parameters_.interval).InSecondsF());
  double interval_change = interval_delta / interval.InSecondsF();
  if (interval_change > kIntervalChangeThreshold) {
    SetActive(false);
    SetActive(true);
    return;
  }

  // If the change in phase is greater than the change threshold in either
  // direction, request an immediate reset. This logic might result in a false
  // negative if there is a simultaneous small change in the interval and the
  // fmod just happens to return something near zero. Assuming the timebase
  // is very recent though, which it should be, we'll still be ok because the
  // old clock and new clock just happen to line up.
  double target_delta =
      std::abs((timebase - current_parameters_.tick_target).InSecondsF());
  double phase_change =
      fmod(target_delta, interval.InSecondsF()) / interval.InSecondsF();
  if (phase_change > kPhaseChangeThreshold &&
      phase_change < (1.0 - kPhaseChangeThreshold)) {
    SetActive(false);
    SetActive(true);
    return;
  }
}

base::TimeTicks DelayBasedTimeSource::Now() const {
  return base::TimeTicks::Now();
}

// This code tries to achieve an average tick rate as close to interval_ as
// possible.  To do this, it has to deal with a few basic issues:
//   1. PostDelayedTask can delay only at a millisecond granularity. So, 16.666
//   has to posted as 16 or 17.
//   2. A delayed task may come back a bit late (a few ms), or really late
//   (frames later)
//
// The basic idea with this scheduler here is to keep track of where we *want*
// to run in tick_target_. We update this with the exact interval.
//
// Then, when we post our task, we take the floor of (tick_target_ and Now()).
// If we started at now=0, and 60FPs (all times in milliseconds):
//      now=0    target=16.667   PostDelayedTask(16)
//
// When our callback runs, we figure out how far off we were from that goal.
// Because of the flooring operation, and assuming our timer runs exactly when
// it should, this yields:
//      now=16   target=16.667
//
// Since we can't post a 0.667 ms task to get to now=16, we just treat this as a
// tick. Then, we update target to be 33.333. We now post another task based on
// the difference between our target and now:
//      now=16   tick_target=16.667  new_target=33.333   -->
//          PostDelayedTask(floor(33.333 - 16)) --> PostDelayedTask(17)
//
// Over time, with no late tasks, this leads to us posting tasks like this:
//      now=0    tick_target=0       new_target=16.667   -->
//          tick(), PostDelayedTask(16)
//      now=16   tick_target=16.667  new_target=33.333   -->
//          tick(), PostDelayedTask(17)
//      now=33   tick_target=33.333  new_target=50.000   -->
//          tick(), PostDelayedTask(17)
//      now=50   tick_target=50.000  new_target=66.667   -->
//          tick(), PostDelayedTask(16)
//
// We treat delays in tasks differently depending on the amount of delay we
// encounter. Suppose we posted a task with a target=16.667:
//   Case 1: late but not unrecoverably-so
//      now=18 tick_target=16.667
//
//   Case 2: so late we obviously missed the tick
//      now=25.0 tick_target=16.667
//
// We treat the first case as a tick anyway, and assume the delay was unusual.
// Thus, we compute the new_target based on the old timebase:
//      now=18   tick_target=16.667  new_target=33.333   -->
//          tick(), PostDelayedTask(floor(33.333-18)) --> PostDelayedTask(15)
// This brings us back to 18+15 = 33, which was where we would have been if the
// task hadn't been late.
//
// For the really late delay, we we move to the next logical tick. The timebase
// is not reset.
//      now=37   tick_target=16.667  new_target=50.000  -->
//          tick(), PostDelayedTask(floor(50.000-37)) --> PostDelayedTask(13)
base::TimeTicks DelayBasedTimeSource::NextTickTarget(base::TimeTicks now) {
  base::TimeDelta new_interval = next_parameters_.interval;
  int intervals_elapsed =
      static_cast<int>(floor((now - next_parameters_.tick_target).InSecondsF() /
                             new_interval.InSecondsF()));
  base::TimeTicks last_effective_tick =
      next_parameters_.tick_target + new_interval * intervals_elapsed;
  base::TimeTicks new_tick_target = last_effective_tick + new_interval;
  DCHECK(now < new_tick_target)
      << "now = " << now.ToInternalValue()
      << "; new_tick_target = " << new_tick_target.ToInternalValue()
      << "; new_interval = " << new_interval.InMicroseconds()
      << "; tick_target = " << next_parameters_.tick_target.ToInternalValue()
      << "; intervals_elapsed = " << intervals_elapsed
      << "; last_effective_tick = " << last_effective_tick.ToInternalValue();

  // Avoid double ticks when:
  // 1) Turning off the timer and turning it right back on.
  // 2) Jittery data is passed to SetTimebaseAndInterval().
  if (new_tick_target - last_tick_time_ <=
      new_interval / static_cast<int>(1.0 / kDoubleTickThreshold))
    new_tick_target += new_interval;

  return new_tick_target;
}

void DelayBasedTimeSource::PostNextTickTask(base::TimeTicks now) {
  base::TimeTicks new_tick_target = NextTickTarget(now);

  // Post another task *before* the tick and update state
  base::TimeDelta delay = new_tick_target - now;
  DCHECK(delay.InMillisecondsF() <=
         next_parameters_.interval.InMillisecondsF() *
         (1.0 + kDoubleTickThreshold));
  task_runner_->PostDelayedTask(FROM_HERE,
                                base::Bind(&DelayBasedTimeSource::OnTimerFired,
                                           weak_factory_.GetWeakPtr()),
                                delay);

  next_parameters_.tick_target = new_tick_target;
  current_parameters_ = next_parameters_;
}

}  // namespace cc
