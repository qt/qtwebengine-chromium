// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/glue/thread_wrapper.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/threading/thread_local.h"
#include "third_party/libjingle/source/talk/base/nullsocketserver.h"

namespace jingle_glue {

struct JingleThreadWrapper::PendingSend {
  PendingSend(const talk_base::Message& message_value)
      : sending_thread(JingleThreadWrapper::current()),
        message(message_value),
        done_event(true, false) {
    DCHECK(sending_thread);
  }

  JingleThreadWrapper* sending_thread;
  talk_base::Message message;
  base::WaitableEvent done_event;
};

base::LazyInstance<base::ThreadLocalPointer<JingleThreadWrapper> >
    g_jingle_thread_wrapper = LAZY_INSTANCE_INITIALIZER;

// static
void JingleThreadWrapper::EnsureForCurrentMessageLoop() {
  if (JingleThreadWrapper::current() == NULL) {
    base::MessageLoop* message_loop = base::MessageLoop::current();
    g_jingle_thread_wrapper.Get()
        .Set(new JingleThreadWrapper(message_loop->message_loop_proxy()));
    message_loop->AddDestructionObserver(current());
  }

  DCHECK_EQ(talk_base::Thread::Current(), current());
}

// static
JingleThreadWrapper* JingleThreadWrapper::current() {
  return g_jingle_thread_wrapper.Get().Get();
}

JingleThreadWrapper::JingleThreadWrapper(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : talk_base::Thread(new talk_base::NullSocketServer()),
      task_runner_(task_runner),
      send_allowed_(false),
      last_task_id_(0),
      pending_send_event_(true, false),
      weak_ptr_factory_(this),
      weak_ptr_(weak_ptr_factory_.GetWeakPtr()) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(!talk_base::Thread::Current());
  talk_base::MessageQueueManager::Add(this);
  WrapCurrent();
}

JingleThreadWrapper::~JingleThreadWrapper() {
  Clear(NULL, talk_base::MQID_ANY, NULL);
}

void JingleThreadWrapper::WillDestroyCurrentMessageLoop() {
  DCHECK_EQ(talk_base::Thread::Current(), current());
  UnwrapCurrent();
  g_jingle_thread_wrapper.Get().Set(NULL);
  talk_base::ThreadManager::Instance()->SetCurrentThread(NULL);
  talk_base::MessageQueueManager::Remove(this);
  talk_base::SocketServer* ss = socketserver();
  delete this;
  delete ss;
}

void JingleThreadWrapper::Post(
    talk_base::MessageHandler* handler, uint32 message_id,
    talk_base::MessageData* data, bool time_sensitive) {
  PostTaskInternal(0, handler, message_id, data);
}

void JingleThreadWrapper::PostDelayed(
    int delay_ms, talk_base::MessageHandler* handler,
    uint32 message_id, talk_base::MessageData* data) {
  PostTaskInternal(delay_ms, handler, message_id, data);
}

void JingleThreadWrapper::Clear(talk_base::MessageHandler* handler, uint32 id,
                                talk_base::MessageList* removed) {
  base::AutoLock auto_lock(lock_);

  for (MessagesQueue::iterator it = messages_.begin();
       it != messages_.end();) {
    MessagesQueue::iterator next = it;
    ++next;

    if (it->second.Match(handler, id)) {
      if (removed) {
        removed->push_back(it->second);
      } else {
        delete it->second.pdata;
      }
      messages_.erase(it);
    }

    it = next;
  }

  for (std::list<PendingSend*>::iterator it = pending_send_messages_.begin();
       it != pending_send_messages_.end();) {
    std::list<PendingSend*>::iterator next = it;
    ++next;

    if ((*it)->message.Match(handler, id)) {
      if (removed) {
        removed ->push_back((*it)->message);
      } else {
        delete (*it)->message.pdata;
      }
      (*it)->done_event.Signal();
      pending_send_messages_.erase(it);
    }

    it = next;
  }
}

void JingleThreadWrapper::Send(talk_base::MessageHandler *handler, uint32 id,
                               talk_base::MessageData *data) {
  if (fStop_)
    return;

  JingleThreadWrapper* current_thread = JingleThreadWrapper::current();
  DCHECK(current_thread != NULL) << "Send() can be called only from a "
      "thread that has JingleThreadWrapper.";

  talk_base::Message message;
  message.phandler = handler;
  message.message_id = id;
  message.pdata = data;

  if (current_thread == this) {
    handler->OnMessage(&message);
    return;
  }

  // Send message from a thread different than |this|.

  // Allow inter-thread send only from threads that have
  // |send_allowed_| flag set.
  DCHECK(current_thread->send_allowed_) << "Send()'ing synchronous "
      "messages is not allowed from the current thread.";

  PendingSend pending_send(message);
  {
    base::AutoLock auto_lock(lock_);
    pending_send_messages_.push_back(&pending_send);
  }

  // Need to signal |pending_send_event_| here in case the thread is
  // sending message to another thread.
  pending_send_event_.Signal();
  task_runner_->PostTask(FROM_HERE,
                         base::Bind(&JingleThreadWrapper::ProcessPendingSends,
                                    weak_ptr_));


  while (!pending_send.done_event.IsSignaled()) {
    base::WaitableEvent* events[] = {&pending_send.done_event,
                                     &current_thread->pending_send_event_};
    size_t event = base::WaitableEvent::WaitMany(events, arraysize(events));
    DCHECK(event == 0 || event == 1);

    if (event == 1)
      current_thread->ProcessPendingSends();
  }
}

void JingleThreadWrapper::ProcessPendingSends() {
  while (true) {
    PendingSend* pending_send = NULL;
    {
      base::AutoLock auto_lock(lock_);
      if (!pending_send_messages_.empty()) {
        pending_send = pending_send_messages_.front();
        pending_send_messages_.pop_front();
      } else {
        // Reset the event while |lock_| is still locked.
        pending_send_event_.Reset();
        break;
      }
    }
    if (pending_send) {
      pending_send->message.phandler->OnMessage(&pending_send->message);
      pending_send->done_event.Signal();
    }
  }
}

void JingleThreadWrapper::PostTaskInternal(
    int delay_ms, talk_base::MessageHandler* handler,
    uint32 message_id, talk_base::MessageData* data) {
  int task_id;
  talk_base::Message message;
  message.phandler = handler;
  message.message_id = message_id;
  message.pdata = data;
  {
    base::AutoLock auto_lock(lock_);
    task_id = ++last_task_id_;
    messages_.insert(std::pair<int, talk_base::Message>(task_id, message));
  }

  if (delay_ms <= 0) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&JingleThreadWrapper::RunTask,
                                      weak_ptr_, task_id));
  } else {
    task_runner_->PostDelayedTask(FROM_HERE,
                                  base::Bind(&JingleThreadWrapper::RunTask,
                                             weak_ptr_, task_id),
                                  base::TimeDelta::FromMilliseconds(delay_ms));
  }
}

void JingleThreadWrapper::RunTask(int task_id) {
  bool have_message = false;
  talk_base::Message message;
  {
    base::AutoLock auto_lock(lock_);
    MessagesQueue::iterator it = messages_.find(task_id);
    if (it != messages_.end()) {
      have_message = true;
      message = it->second;
      messages_.erase(it);
    }
  }

  if (have_message) {
    if (message.message_id == talk_base::MQID_DISPOSE) {
      DCHECK(message.phandler == NULL);
      delete message.pdata;
    } else {
      message.phandler->OnMessage(&message);
    }
  }
}

// All methods below are marked as not reached. See comments in the
// header for more details.
void JingleThreadWrapper::Quit() {
  NOTREACHED();
}

bool JingleThreadWrapper::IsQuitting() {
  NOTREACHED();
  return false;
}

void JingleThreadWrapper::Restart() {
  NOTREACHED();
}

bool JingleThreadWrapper::Get(talk_base::Message*, int, bool) {
  NOTREACHED();
  return false;
}

bool JingleThreadWrapper::Peek(talk_base::Message*, int) {
  NOTREACHED();
  return false;
}

void JingleThreadWrapper::PostAt(uint32, talk_base::MessageHandler*,
                                 uint32, talk_base::MessageData*) {
  NOTREACHED();
}

void JingleThreadWrapper::Dispatch(talk_base::Message* message) {
  NOTREACHED();
}

void JingleThreadWrapper::ReceiveSends() {
  NOTREACHED();
}

int JingleThreadWrapper::GetDelay() {
  NOTREACHED();
  return 0;
}

void JingleThreadWrapper::Stop() {
  NOTREACHED();
}

void JingleThreadWrapper::Run() {
  NOTREACHED();
}

}  // namespace jingle_glue
