// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/thread_group_impl.h"

#include "base/auto_reset.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_token.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool/thread_group_worker_delegate.h"
#include "base/task/thread_pool/worker_thread_waitable_event.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/threading/scoped_blocking_call_internal.h"
#include "base/threading/thread_checker.h"
#include "base/time/time_override.h"
#include "base/trace_event/base_tracing.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace internal {

namespace {

constexpr size_t kMaxNumberOfWorkers = 256;

}  // namespace

// Upon destruction, executes actions that control the number of active workers.
// Useful to satisfy locking requirements of these actions.
class ThreadGroupImpl::ScopedCommandsExecutor
    : public ThreadGroup::BaseScopedCommandsExecutor {
 public:
  explicit ScopedCommandsExecutor(ThreadGroupImpl* outer)
      : BaseScopedCommandsExecutor(outer) {}

  ScopedCommandsExecutor(const ScopedCommandsExecutor&) = delete;
  ScopedCommandsExecutor& operator=(const ScopedCommandsExecutor&) = delete;
  ~ScopedCommandsExecutor() override {
    CheckedLock::AssertNoLockHeldOnCurrentThread();

    // Wake up workers.
    for (auto worker : workers_to_wake_up_) {
      worker->WakeUp();
    }
  }

  void ScheduleWakeUp(scoped_refptr<WorkerThreadWaitableEvent> worker) {
    workers_to_wake_up_.emplace_back(std::move(worker));
  }

 private:
  absl::InlinedVector<scoped_refptr<WorkerThreadWaitableEvent>, 2>
      workers_to_wake_up_;
};

class ThreadGroupImpl::WaitableEventWorkerDelegate
    : public ThreadGroup::ThreadGroupWorkerDelegate,
      public WorkerThreadWaitableEvent::Delegate {
 public:
  // |outer| owns the worker for which this delegate is constructed. If
  // |is_excess| is true, this worker will be eligible for reclaim.
  explicit WaitableEventWorkerDelegate(TrackedRef<ThreadGroup> outer,
                                       bool is_excess);
  WaitableEventWorkerDelegate(const WaitableEventWorkerDelegate&) = delete;
  WaitableEventWorkerDelegate& operator=(const WaitableEventWorkerDelegate&) =
      delete;

  // OnMainExit() handles the thread-affine cleanup;
  // WaitableEventWorkerDelegate can thereafter safely be deleted from any
  // thread.
  ~WaitableEventWorkerDelegate() override = default;

  // ThreadGroup::Delegate:
  void OnMainEntry(WorkerThread* worker) override;
  void OnMainExit(WorkerThread* worker) override;
  RegisteredTaskSource GetWork(WorkerThread* worker) override;
  RegisteredTaskSource SwapProcessedTask(RegisteredTaskSource task_source,
                                         WorkerThread* worker) override;

  // WorkerThreadWaitableEvent::Delegate:
  void RecordUnnecessaryWakeup() override;
  // True if the calling worker is be eligible for reclaim.
  bool IsExcess() const override;
  TimeDelta GetSleepTimeout() override;

 private:
  ThreadGroupImpl* outer() const {
    return static_cast<ThreadGroupImpl*>(outer_.get());
  }

  // ThreadGroup::ThreadGroupWorkerDelegate:
  bool CanGetWorkLockRequired(BaseScopedCommandsExecutor* executor,
                              WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;
  void CleanupLockRequired(BaseScopedCommandsExecutor* executor,
                           WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;
  void OnWorkerBecomesIdleLockRequired(BaseScopedCommandsExecutor* executor,
                                       WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;

  RegisteredTaskSource GetWorkLockRequired(ScopedCommandsExecutor* executor,
                                           WorkerThreadWaitableEvent* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_);

  // Returns true if |worker| is allowed to cleanup and remove itself from the
  // thread group. Called from GetWork() when no work is available.
  bool CanCleanupLockRequired(const WorkerThread* worker)
      EXCLUSIVE_LOCKS_REQUIRED(outer()->lock_) override;

  const bool is_excess_;
};

std::unique_ptr<ThreadGroup::BaseScopedCommandsExecutor>
ThreadGroupImpl::GetExecutor() {
  return static_cast<std::unique_ptr<BaseScopedCommandsExecutor>>(
      std::make_unique<ScopedCommandsExecutor>(this));
}

ThreadGroupImpl::ThreadGroupImpl(StringPiece histogram_label,
                                 StringPiece thread_group_label,
                                 ThreadType thread_type_hint,
                                 TrackedRef<TaskTracker> task_tracker,
                                 TrackedRef<Delegate> delegate)
    : ThreadGroup(histogram_label,
                  thread_group_label,
                  thread_type_hint,
                  std::move(task_tracker),
                  std::move(delegate)),
      idle_workers_set_cv_for_testing_(lock_.CreateConditionVariable()),
      tracked_ref_factory_(this) {
  DCHECK(!thread_group_label_.empty());
}

void ThreadGroupImpl::Start(
    size_t max_tasks,
    size_t max_best_effort_tasks,
    TimeDelta suggested_reclaim_time,
    scoped_refptr<SingleThreadTaskRunner> service_thread_task_runner,
    WorkerThreadObserver* worker_thread_observer,
    WorkerEnvironment worker_environment,
    bool synchronous_thread_start_for_testing,
    absl::optional<TimeDelta> may_block_threshold) {
  ThreadGroup::Start(max_tasks, max_best_effort_tasks, suggested_reclaim_time,
                     service_thread_task_runner, worker_thread_observer,
                     worker_environment, may_block_threshold);

  if (synchronous_thread_start_for_testing) {
    worker_started_for_testing_.emplace(WaitableEvent::ResetPolicy::AUTOMATIC);
    // Don't emit a ScopedBlockingCallWithBaseSyncPrimitives from this
    // WaitableEvent or it defeats the purpose of having threads start without
    // externally visible side-effects.
    worker_started_for_testing_->declare_only_used_while_idle();
  }

  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_.empty());
  EnsureEnoughWorkersLockRequired(&executor);
}

ThreadGroupImpl::~ThreadGroupImpl() {
  // ThreadGroup should only ever be deleted:
  //  1) In tests, after JoinForTesting().
  //  2) In production, iff initialization failed.
  // In both cases |workers_| should be empty.
  DCHECK(workers_.empty());
}

void ThreadGroupImpl::UpdateSortKey(TaskSource::Transaction transaction) {
  ScopedCommandsExecutor executor(this);
  UpdateSortKeyImpl(&executor, std::move(transaction));
}

void ThreadGroupImpl::PushTaskSourceAndWakeUpWorkers(
    RegisteredTaskSourceAndTransaction transaction_with_task_source) {
  ScopedCommandsExecutor executor(this);
  PushTaskSourceAndWakeUpWorkersImpl(&executor,
                                     std::move(transaction_with_task_source));
}

void ThreadGroupImpl::WaitForWorkersIdleLockRequiredForTesting(size_t n) {
  // Make sure workers do not cleanup while watching the idle count.
  AutoReset<bool> ban_cleanups(&worker_cleanup_disallowed_for_testing_, true);

  while (idle_workers_set_.Size() < n) {
    idle_workers_set_cv_for_testing_->Wait();
  }
}

void ThreadGroupImpl::WaitForWorkersIdleForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

#if DCHECK_IS_ON()
  DCHECK(!some_workers_cleaned_up_for_testing_)
      << "Workers detached prior to waiting for a specific number of idle "
         "workers. Doing the wait under such conditions is flaky. Consider "
         "setting the suggested reclaim time to TimeDelta::Max() in Start().";
#endif

  WaitForWorkersIdleLockRequiredForTesting(n);
}

void ThreadGroupImpl::WaitForAllWorkersIdleForTesting() {
  CheckedAutoLock auto_lock(lock_);
  WaitForWorkersIdleLockRequiredForTesting(workers_.size());
}

void ThreadGroupImpl::WaitForWorkersCleanedUpForTesting(size_t n) {
  CheckedAutoLock auto_lock(lock_);

  if (!num_workers_cleaned_up_for_testing_cv_) {
    num_workers_cleaned_up_for_testing_cv_ = lock_.CreateConditionVariable();
  }

  while (num_workers_cleaned_up_for_testing_ < n) {
    num_workers_cleaned_up_for_testing_cv_->Wait();
  }

  num_workers_cleaned_up_for_testing_ = 0;
}

ThreadGroupImpl::WaitableEventWorkerDelegate::WaitableEventWorkerDelegate(
    TrackedRef<ThreadGroup> outer,
    bool is_excess)
    : ThreadGroupWorkerDelegate(std::move(outer)), is_excess_(is_excess) {
  // Bound in OnMainEntry().
  DETACH_FROM_THREAD(worker_thread_checker_);
}

TimeDelta ThreadGroupImpl::WaitableEventWorkerDelegate::GetSleepTimeout() {
  return ThreadPoolSleepTimeout();
}

void ThreadGroupImpl::WaitableEventWorkerDelegate::OnMainEntry(
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  {
#if DCHECK_IS_ON()
    CheckedAutoLock auto_lock(outer()->lock_);
    DCHECK(ContainsWorker(outer()->workers_,
                          static_cast<WorkerThreadWaitableEvent*>(worker)));
#endif
  }

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment = GetScopedWindowsThreadEnvironment(
      outer()->after_start().worker_environment);
#endif  // BUILDFLAG(IS_WIN)

  PlatformThread::SetName(
      StringPrintf("ThreadPool%sWorker", outer()->thread_group_label_.c_str()));

  outer()->BindToCurrentThread();
  worker_only().worker_thread_ =
      static_cast<WorkerThreadWaitableEvent*>(worker);
  SetBlockingObserverForCurrentThread(this);

  if (outer()->worker_started_for_testing_) {
    // When |worker_started_for_testing_| is set, the thread that starts workers
    // should wait for a worker to have started before starting the next one,
    // and there should only be one thread that wakes up workers at a time.
    DCHECK(!outer()->worker_started_for_testing_->IsSignaled());
    outer()->worker_started_for_testing_->Signal();
  }
}

void ThreadGroupImpl::WaitableEventWorkerDelegate::OnMainExit(
    WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

#if DCHECK_IS_ON()
  WorkerThreadWaitableEvent* worker =
      static_cast<WorkerThreadWaitableEvent*>(worker_base);
  {
    bool shutdown_complete = outer()->task_tracker_->IsShutdownComplete();
    CheckedAutoLock auto_lock(outer()->lock_);

    // |worker| should already have been removed from the idle workers set and
    // |workers_| by the time the thread is about to exit. (except in the
    // cases where the thread group is no longer going to be used - in which
    // case, it's fine for there to be invalid workers in the thread group).
    if (!shutdown_complete && !outer()->join_for_testing_started_) {
      DCHECK(!outer()->idle_workers_set_.Contains(worker));
      DCHECK(!ContainsWorker(outer()->workers_, worker));
    }
  }
#endif

#if BUILDFLAG(IS_WIN)
  worker_only().win_thread_environment.reset();
#endif  // BUILDFLAG(IS_WIN)

  // Count cleaned up workers for tests. It's important to do this here
  // instead of at the end of CleanupLockRequired() because some side-effects
  // of cleaning up happen outside the lock (e.g. recording histograms) and
  // resuming from tests must happen-after that point or checks on the main
  // thread will be flaky (crbug.com/1047733).
  CheckedAutoLock auto_lock(outer()->lock_);
  ++outer()->num_workers_cleaned_up_for_testing_;
#if DCHECK_IS_ON()
  outer()->some_workers_cleaned_up_for_testing_ = true;
#endif
  if (outer()->num_workers_cleaned_up_for_testing_cv_) {
    outer()->num_workers_cleaned_up_for_testing_cv_->Signal();
  }
}

bool ThreadGroupImpl::WaitableEventWorkerDelegate::CanGetWorkLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThreadWaitableEvent* worker =
      static_cast<WorkerThreadWaitableEvent*>(worker_base);

  const bool is_on_idle_workers_set = outer()->IsOnIdleSetLockRequired(worker);
  DCHECK_EQ(is_on_idle_workers_set,
            outer()->idle_workers_set_.Contains(worker));

  AnnotateAcquiredLockAlias annotate(outer()->lock_, lock());
  // This occurs when the when WorkerThread::Delegate::WaitForWork() times out
  // (i.e. when the worker's wakes up after GetSleepTimeout()).
  if (is_on_idle_workers_set) {
    if (CanCleanupLockRequired(worker)) {
      CleanupLockRequired(executor, worker);
    }
    return false;
  }

  // If too many workers are running, this worker should not get work, until
  // tasks are no longer in excess (i.e. max tasks increases). This ensures that
  // if this worker is in excess, it gets a chance to being cleaned up.
  if (outer()->GetNumAwakeWorkersLockRequired() > outer()->max_tasks_) {
    OnWorkerBecomesIdleLockRequired(executor, worker);
    return false;
  }

  return true;
}

RegisteredTaskSource
ThreadGroupImpl::WaitableEventWorkerDelegate::GetWorkLockRequired(
    ScopedCommandsExecutor* executor,
    WorkerThreadWaitableEvent* worker) {
  DCHECK(ContainsWorker(outer()->workers_, worker));

  if (!outer()->after_start().ensure_enough_workers_at_end_of_get_work) {
    // Use this opportunity, before assigning work to this worker, to
    // create/wake additional workers if needed (doing this here allows us to
    // reduce potentially expensive create/wake directly on PostTask()).
    //
    // Note: FlushWorkerCreation() below releases |outer()->lock_|. It is thus
    // important that all other operations come after it to keep this method
    // transactional.
    outer()->EnsureEnoughWorkersLockRequired(executor);
    executor->FlushWorkerCreation(&outer()->lock_);
  }

  if (!CanGetWorkLockRequired(executor, worker)) {
    return nullptr;
  }

  RegisteredTaskSource task_source;
  TaskPriority priority;
  while (!task_source && !outer()->priority_queue_.IsEmpty()) {
    // Enforce the CanRunPolicy and that no more than |max_best_effort_tasks_|
    // BEST_EFFORT tasks run concurrently.
    priority = outer()->priority_queue_.PeekSortKey().priority();
    if (!outer()->task_tracker_->CanRunPriority(priority) ||
        (priority == TaskPriority::BEST_EFFORT &&
         outer()->num_running_best_effort_tasks_ >=
             outer()->max_best_effort_tasks_)) {
      break;
    }

    task_source = outer()->TakeRegisteredTaskSource(executor);
  }
  if (!task_source) {
    OnWorkerBecomesIdleLockRequired(executor, worker);
    return nullptr;
  }

  // Running task bookkeeping.
  outer()->IncrementTasksRunningLockRequired(priority);
  DCHECK(!outer()->idle_workers_set_.Contains(worker));

  AnnotateAcquiredLockAlias annotate(outer()->lock_, lock());
  write_worker().current_task_priority = priority;
  write_worker().current_shutdown_behavior = task_source->shutdown_behavior();

  if (outer()->after_start().ensure_enough_workers_at_end_of_get_work) {
    // Subtle: This must be after the call to WillRunTask() inside
    // TakeRegisteredTaskSource(), so that any state used by WillRunTask() to
    // determine that the task source must remain in the TaskQueue is also used
    // to determine the desired number of workers. Concretely, this wouldn't
    // work:
    //
    //   Thread 1: GetWork() calls EnsureEnoughWorkers(). No worker woken up
    //             because the queue contains a job with max concurrency = 1 and
    //             the current worker is awake.
    //   Thread 2: Increases the job's max concurrency.
    //             ShouldQueueUponCapacityIncrease() returns false because the
    //             job is already queued.
    //   Thread 1: Calls WillRunTask() on the job. It returns
    //             kAllowedNotSaturated because max concurrency is not reached.
    //             But no extra worker is woken up to run the job!
    outer()->EnsureEnoughWorkersLockRequired(executor);
  }

  return task_source;
}

RegisteredTaskSource ThreadGroupImpl::WaitableEventWorkerDelegate::GetWork(
    WorkerThread* worker_base) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!read_worker().current_task_priority);
  DCHECK(!read_worker().current_shutdown_behavior);
  WorkerThreadWaitableEvent* worker =
      static_cast<WorkerThreadWaitableEvent*>(worker_base);

  ScopedCommandsExecutor executor(outer());
  CheckedAutoLock auto_lock(outer()->lock_);

  return GetWorkLockRequired(&executor, worker);
}

RegisteredTaskSource
ThreadGroupImpl::WaitableEventWorkerDelegate::SwapProcessedTask(
    RegisteredTaskSource task_source,
    WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(read_worker().current_task_priority);
  DCHECK(read_worker().current_shutdown_behavior);

  // A transaction to the TaskSource to reenqueue, if any. Instantiated here as
  // |TaskSource::lock_| is a UniversalPredecessor and must always be acquired
  // prior to acquiring a second lock
  absl::optional<RegisteredTaskSourceAndTransaction>
      transaction_with_task_source;
  if (task_source) {
    transaction_with_task_source.emplace(
        RegisteredTaskSourceAndTransaction::FromTaskSource(
            std::move(task_source)));
  }

  // Calling WakeUp() guarantees that this WorkerThread will run Tasks from
  // TaskSources returned by the GetWork() method of |delegate_| until it
  // returns nullptr. Resetting |wake_up_event_| here doesn't break this
  // invariant and avoids a useless loop iteration before going to sleep if
  // WakeUp() is called while this WorkerThread is awake.
  wake_up_event_.Reset();

  ScopedCommandsExecutor workers_executor(outer());
  ScopedReenqueueExecutor reenqueue_executor;
  CheckedAutoLock auto_lock(outer()->lock_);
  AnnotateAcquiredLockAlias annotate(outer()->lock_, lock());

  // During shutdown, max_tasks may have been incremented in
  // OnShutdownStartedLockRequired().
  if (incremented_max_tasks_for_shutdown_) {
    DCHECK(outer()->shutdown_started_);
    outer()->DecrementMaxTasksLockRequired();
    if (*read_worker().current_task_priority == TaskPriority::BEST_EFFORT) {
      outer()->DecrementMaxBestEffortTasksLockRequired();
    }
    incremented_max_tasks_since_blocked_ = false;
    incremented_max_best_effort_tasks_since_blocked_ = false;
    incremented_max_tasks_for_shutdown_ = false;
  }

  DCHECK(read_worker().blocking_start_time.is_null());
  DCHECK(!incremented_max_tasks_since_blocked_);
  DCHECK(!incremented_max_best_effort_tasks_since_blocked_);

  // Running task bookkeeping.
  outer()->DecrementTasksRunningLockRequired(
      *read_worker().current_task_priority);
  write_worker().current_shutdown_behavior = absl::nullopt;
  write_worker().current_task_priority = absl::nullopt;

  if (transaction_with_task_source) {
    outer()->ReEnqueueTaskSourceLockRequired(
        &workers_executor, &reenqueue_executor,
        std::move(transaction_with_task_source.value()));
  }

  return GetWorkLockRequired(&workers_executor,
                             static_cast<WorkerThreadWaitableEvent*>(worker));
}

bool ThreadGroupImpl::WaitableEventWorkerDelegate::IsExcess() const {
  return is_excess_;
}

bool ThreadGroupImpl::WaitableEventWorkerDelegate::CanCleanupLockRequired(
    const WorkerThread* worker) {
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  if (!IsExcess()) {
    return false;
  }

  const TimeTicks last_used_time = worker->GetLastUsedTime();
  return !last_used_time.is_null() &&
         subtle::TimeTicksNowIgnoringOverride() - last_used_time >=
             outer()->after_start().suggested_reclaim_time &&
         LIKELY(!outer()->worker_cleanup_disallowed_for_testing_);
}

void ThreadGroupImpl::WaitableEventWorkerDelegate::CleanupLockRequired(
    BaseScopedCommandsExecutor* executor,
    WorkerThread* worker_base) {
  WorkerThreadWaitableEvent* worker =
      static_cast<WorkerThreadWaitableEvent*>(worker_base);
  DCHECK(!outer()->join_for_testing_started_);
  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);

  worker->Cleanup();

  if (outer()->IsOnIdleSetLockRequired(worker)) {
    outer()->idle_workers_set_.Remove(worker);
  }

  // Remove the worker from |workers_|.
  auto worker_iter = ranges::find(outer()->workers_, worker);
  DCHECK(worker_iter != outer()->workers_.end());
  outer()->workers_.erase(worker_iter);
}

void ThreadGroupImpl::WaitableEventWorkerDelegate::
    OnWorkerBecomesIdleLockRequired(BaseScopedCommandsExecutor* executor,
                                    WorkerThread* worker_base) {
  WorkerThreadWaitableEvent* worker =
      static_cast<WorkerThreadWaitableEvent*>(worker_base);

  DCHECK_CALLED_ON_VALID_THREAD(worker_thread_checker_);
  DCHECK(!outer()->idle_workers_set_.Contains(worker));

  // Add the worker to the idle set.
  outer()->idle_workers_set_.Insert(worker);
  DCHECK_LE(outer()->idle_workers_set_.Size(), outer()->workers_.size());
  outer()->idle_workers_set_cv_for_testing_->Broadcast();
}

void ThreadGroupImpl::WaitableEventWorkerDelegate::RecordUnnecessaryWakeup() {
  base::BooleanHistogram::FactoryGet(
      std::string("ThreadPool.UnnecessaryWakeup.") + outer()->histogram_label_,
      base::Histogram::kUmaTargetedHistogramFlag)
      ->Add(true);

  TRACE_EVENT_INSTANT("wakeup.flow", "ThreadPool.UnnecessaryWakeup");
}

void ThreadGroupImpl::JoinForTesting() {
  decltype(workers_) workers_copy;
  {
    CheckedAutoLock auto_lock(lock_);
    priority_queue_.EnableFlushTaskSourcesOnDestroyForTesting();

    DCHECK_GT(workers_.size(), size_t(0))
        << "Joined an unstarted thread group.";

    join_for_testing_started_ = true;

    // Ensure WorkerThreads in |workers_| do not attempt to cleanup while
    // being joined.
    worker_cleanup_disallowed_for_testing_ = true;

    // Make a copy of the WorkerThreads so that we can call
    // WorkerThread::JoinForTesting() without holding |lock_| since
    // WorkerThreads may need to access |workers_|.
    workers_copy = workers_;
  }
  for (const auto& worker : workers_copy) {
    worker->JoinForTesting();
  }

  CheckedAutoLock auto_lock(lock_);
  DCHECK(workers_ == workers_copy);
  // Release |workers_| to clear their TrackedRef against |this|.
  workers_.clear();
}

size_t ThreadGroupImpl::NumberOfWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return workers_.size();
}

size_t ThreadGroupImpl::NumberOfIdleWorkersForTesting() const {
  CheckedAutoLock auto_lock(lock_);
  return idle_workers_set_.Size();
}

void ThreadGroupImpl::MaintainAtLeastOneIdleWorkerLockRequired(
    ScopedCommandsExecutor* executor) {
  if (workers_.size() == kMaxNumberOfWorkers) {
    return;
  }
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);

  if (!idle_workers_set_.IsEmpty()) {
    return;
  }

  if (workers_.size() >= max_tasks_) {
    return;
  }

  scoped_refptr<WorkerThreadWaitableEvent> new_worker =
      CreateAndRegisterWorkerLockRequired(executor);
  DCHECK(new_worker);
  idle_workers_set_.Insert(new_worker.get());
}

scoped_refptr<WorkerThreadWaitableEvent>
ThreadGroupImpl::CreateAndRegisterWorkerLockRequired(
    ScopedCommandsExecutor* executor) {
  DCHECK(!join_for_testing_started_);
  DCHECK_LT(workers_.size(), max_tasks_);
  DCHECK_LT(workers_.size(), kMaxNumberOfWorkers);
  DCHECK(idle_workers_set_.IsEmpty());

  // WorkerThread needs |lock_| as a predecessor for its thread lock because in
  // GetWork(), |lock_| is first acquired and then the thread lock is acquired
  // when GetLastUsedTime() is called on the worker by CanGetWorkLockRequired().
  scoped_refptr<WorkerThreadWaitableEvent> worker =
      MakeRefCounted<WorkerThreadWaitableEvent>(
          thread_type_hint_,
          std::make_unique<WaitableEventWorkerDelegate>(
              tracked_ref_factory_.GetTrackedRef(),
              /* is_excess=*/after_start().no_worker_reclaim
                  ? workers_.size() >= after_start().initial_max_tasks
                  : true),
          task_tracker_, worker_sequence_num_++, &lock_);

  workers_.push_back(worker);
  executor->ScheduleStart(worker);
  DCHECK_LE(workers_.size(), max_tasks_);

  return worker;
}

size_t ThreadGroupImpl::GetNumAwakeWorkersLockRequired() const {
  DCHECK_GE(workers_.size(), idle_workers_set_.Size());
  size_t num_awake_workers = workers_.size() - idle_workers_set_.Size();
  DCHECK_GE(num_awake_workers, num_running_tasks_);
  return num_awake_workers;
}

void ThreadGroupImpl::DidUpdateCanRunPolicy() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  EnsureEnoughWorkersLockRequired(&executor);
}

void ThreadGroupImpl::OnShutdownStarted() {
  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);

  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0 || UNLIKELY(join_for_testing_started_)) {
    return;
  }

  // Start a MAY_BLOCK scope on each worker that is already running a task.
  for (scoped_refptr<WorkerThreadWaitableEvent>& worker : workers_) {
    // The delegates of workers inside a ThreadGroup should be
    // WorkerThreadDelegateImpls.
    WaitableEventWorkerDelegate* delegate =
        static_cast<WaitableEventWorkerDelegate*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->OnShutdownStartedLockRequired(&executor);
  }
  EnsureEnoughWorkersLockRequired(&executor);

  shutdown_started_ = true;
}

void ThreadGroupImpl::EnsureEnoughWorkersLockRequired(
    BaseScopedCommandsExecutor* base_executor) {
  // Don't do anything if the thread group isn't started.
  if (max_tasks_ == 0 || UNLIKELY(join_for_testing_started_)) {
    return;
  }

  ScopedCommandsExecutor* executor =
      static_cast<ScopedCommandsExecutor*>(base_executor);

  const size_t desired_num_awake_workers =
      GetDesiredNumAwakeWorkersLockRequired();
  const size_t num_awake_workers = GetNumAwakeWorkersLockRequired();

  size_t num_workers_to_wake_up =
      ClampSub(desired_num_awake_workers, num_awake_workers);
  num_workers_to_wake_up = std::min(num_workers_to_wake_up, size_t(2U));

  // Wake up the appropriate number of workers.
  for (size_t i = 0; i < num_workers_to_wake_up; ++i) {
    MaintainAtLeastOneIdleWorkerLockRequired(executor);
    WorkerThreadWaitableEvent* worker_to_wakeup = idle_workers_set_.Take();
    DCHECK(worker_to_wakeup);
    executor->ScheduleWakeUp(worker_to_wakeup);
  }

  // In the case where the loop above didn't wake up any worker and we don't
  // have excess workers, the idle worker should be maintained. This happens
  // when called from the last worker awake, or a recent increase in |max_tasks|
  // now makes it possible to keep an idle worker.
  if (desired_num_awake_workers == num_awake_workers) {
    MaintainAtLeastOneIdleWorkerLockRequired(executor);
  }

  // This function is called every time a task source is (re-)enqueued,
  // hence the minimum priority needs to be updated.
  UpdateMinAllowedPriorityLockRequired();

  // Ensure that the number of workers is periodically adjusted if needed.
  MaybeScheduleAdjustMaxTasksLockRequired(executor);
}

void ThreadGroupImpl::AdjustMaxTasks() {
  DCHECK(
      after_start().service_thread_task_runner->RunsTasksInCurrentSequence());

  ScopedCommandsExecutor executor(this);
  CheckedAutoLock auto_lock(lock_);
  DCHECK(adjust_max_tasks_posted_);
  adjust_max_tasks_posted_ = false;

  // Increment max tasks for each worker that has been within a MAY_BLOCK
  // ScopedBlockingCall for more than may_block_threshold.
  for (scoped_refptr<WorkerThreadWaitableEvent> worker : workers_) {
    // The delegates of workers inside a ThreadGroup should be
    // WaitableEventWorkerDelegates.
    WaitableEventWorkerDelegate* delegate =
        static_cast<WaitableEventWorkerDelegate*>(worker->delegate());
    AnnotateAcquiredLockAlias annotate(lock_, delegate->lock());
    delegate->MaybeIncrementMaxTasksLockRequired();
  }

  // Wake up workers according to the updated |max_tasks_|. This will also
  // reschedule AdjustMaxTasks() if necessary.
  EnsureEnoughWorkersLockRequired(&executor);
}

bool ThreadGroupImpl::IsOnIdleSetLockRequired(
    WorkerThreadWaitableEvent* worker) const {
  // To avoid searching through the idle set : use GetLastUsedTime() not being
  // null (or being directly on top of the idle set) as a proxy for being on
  // the idle set.
  return idle_workers_set_.Peek() == worker ||
         !worker->GetLastUsedTime().is_null();
}

}  // namespace internal
}  // namespace base
