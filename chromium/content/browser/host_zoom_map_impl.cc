// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/host_zoom_map_impl.h"

#include <cmath>

#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/resource_context.h"
#include "content/public/common/page_zoom.h"
#include "net/base/net_util.h"

static const char* kHostZoomMapKeyName = "content_host_zoom_map";

namespace content {

HostZoomMap* HostZoomMap::GetForBrowserContext(BrowserContext* context) {
  HostZoomMapImpl* rv = static_cast<HostZoomMapImpl*>(
      context->GetUserData(kHostZoomMapKeyName));
  if (!rv) {
    rv = new HostZoomMapImpl();
    context->SetUserData(kHostZoomMapKeyName, rv);
  }
  return rv;
}

HostZoomMapImpl::HostZoomMapImpl()
    : default_zoom_level_(0.0) {
  registrar_.Add(
      this, NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW,
      NotificationService::AllSources());
}

void HostZoomMapImpl::CopyFrom(HostZoomMap* copy_interface) {
  // This can only be called on the UI thread to avoid deadlocks, otherwise
  //   UI: a.CopyFrom(b);
  //   IO: b.CopyFrom(a);
  // can deadlock.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  HostZoomMapImpl* copy = static_cast<HostZoomMapImpl*>(copy_interface);
  base::AutoLock auto_lock(lock_);
  base::AutoLock copy_auto_lock(copy->lock_);
  host_zoom_levels_.
      insert(copy->host_zoom_levels_.begin(), copy->host_zoom_levels_.end());
  for (SchemeHostZoomLevels::const_iterator i(copy->
           scheme_host_zoom_levels_.begin());
       i != copy->scheme_host_zoom_levels_.end(); ++i) {
    scheme_host_zoom_levels_[i->first] = HostZoomLevels();
    scheme_host_zoom_levels_[i->first].
        insert(i->second.begin(), i->second.end());
  }
  default_zoom_level_ = copy->default_zoom_level_;
}

double HostZoomMapImpl::GetZoomLevelForHost(const std::string& host) const {
  base::AutoLock auto_lock(lock_);
  HostZoomLevels::const_iterator i(host_zoom_levels_.find(host));
  return (i == host_zoom_levels_.end()) ? default_zoom_level_ : i->second;
}

double HostZoomMapImpl::GetZoomLevelForHostAndScheme(
    const std::string& scheme,
    const std::string& host) const {
  {
    base::AutoLock auto_lock(lock_);
    SchemeHostZoomLevels::const_iterator scheme_iterator(
        scheme_host_zoom_levels_.find(scheme));
    if (scheme_iterator != scheme_host_zoom_levels_.end()) {
      HostZoomLevels::const_iterator i(scheme_iterator->second.find(host));
      if (i != scheme_iterator->second.end())
        return i->second;
    }
  }
  return GetZoomLevelForHost(host);
}

void HostZoomMapImpl::SetZoomLevelForHost(const std::string& host,
                                          double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);

    if (ZoomValuesEqual(level, default_zoom_level_))
      host_zoom_levels_.erase(host);
    else
      host_zoom_levels_[host] = level;
  }

  // Notify renderers from this browser context.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (HostZoomMap::GetForBrowserContext(
            render_process_host->GetBrowserContext()) == this) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentURL(std::string(), host, level));
    }
  }
  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_HOST;
  change.host = host;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::SetZoomLevelForHostAndScheme(const std::string& scheme,
                                                   const std::string& host,
                                                   double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  {
    base::AutoLock auto_lock(lock_);
    scheme_host_zoom_levels_[scheme][host] = level;
  }

  // Notify renderers from this browser context.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* render_process_host = i.GetCurrentValue();
    if (HostZoomMap::GetForBrowserContext(
            render_process_host->GetBrowserContext()) == this) {
      render_process_host->Send(
          new ViewMsg_SetZoomLevelForCurrentURL(scheme, host, level));
    }
  }

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_FOR_SCHEME_AND_HOST;
  change.host = host;
  change.scheme = scheme;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

double HostZoomMapImpl::GetDefaultZoomLevel() const {
  return default_zoom_level_;
}

void HostZoomMapImpl::SetDefaultZoomLevel(double level) {
  default_zoom_level_ = level;
}

scoped_ptr<HostZoomMap::Subscription>
HostZoomMapImpl::AddZoomLevelChangedCallback(
    const ZoomLevelChangedCallback& callback) {
  return zoom_level_changed_callbacks_.Add(callback);
}

double HostZoomMapImpl::GetTemporaryZoomLevel(int render_process_id,
                                              int render_view_id) const {
  base::AutoLock auto_lock(lock_);
  for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
    if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
        temporary_zoom_levels_[i].render_view_id == render_view_id) {
      return temporary_zoom_levels_[i].zoom_level;
    }
  }
  return 0;
}

void HostZoomMapImpl::SetTemporaryZoomLevel(int render_process_id,
                                            int render_view_id,
                                            double level) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  {
    base::AutoLock auto_lock(lock_);
    size_t i;
    for (i = 0; i < temporary_zoom_levels_.size(); ++i) {
      if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
          temporary_zoom_levels_[i].render_view_id == render_view_id) {
        if (level) {
          temporary_zoom_levels_[i].zoom_level = level;
        } else {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
        }
        break;
      }
    }

    if (level && i == temporary_zoom_levels_.size()) {
      TemporaryZoomLevel temp;
      temp.render_process_id = render_process_id;
      temp.render_view_id = render_view_id;
      temp.zoom_level = level;
      temporary_zoom_levels_.push_back(temp);
    }
  }

  HostZoomMap::ZoomLevelChange change;
  change.mode = HostZoomMap::ZOOM_CHANGED_TEMPORARY_ZOOM;
  change.zoom_level = level;

  zoom_level_changed_callbacks_.Notify(change);
}

void HostZoomMapImpl::Observe(int type,
                              const NotificationSource& source,
                              const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDER_VIEW_HOST_WILL_CLOSE_RENDER_VIEW: {
      base::AutoLock auto_lock(lock_);
      int render_view_id = Source<RenderViewHost>(source)->GetRoutingID();
      int render_process_id =
          Source<RenderViewHost>(source)->GetProcess()->GetID();

      for (size_t i = 0; i < temporary_zoom_levels_.size(); ++i) {
        if (temporary_zoom_levels_[i].render_process_id == render_process_id &&
            temporary_zoom_levels_[i].render_view_id == render_view_id) {
          temporary_zoom_levels_.erase(temporary_zoom_levels_.begin() + i);
          break;
        }
      }
      break;
    }
    default:
      NOTREACHED() << "Unexpected preference observed.";
  }
}

HostZoomMapImpl::~HostZoomMapImpl() {
}

}  // namespace content
