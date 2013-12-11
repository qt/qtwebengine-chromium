// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/surface_texture_peer_browser_impl.h"

#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "media/base/android/media_player_android.h"
#include "ui/gl/android/scoped_java_surface.h"

namespace content {

namespace {

// Pass a java surface object to the MediaPlayerAndroid object
// identified by render process handle, render view ID and player ID.
static void SetSurfacePeer(
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    base::ProcessHandle render_process_handle,
    int render_view_id,
    int player_id) {
  int renderer_id = 0;
  RenderProcessHost::iterator it = RenderProcessHost::AllHostsIterator();
  while (!it.IsAtEnd()) {
    if (it.GetCurrentValue()->GetHandle() == render_process_handle) {
      renderer_id = it.GetCurrentValue()->GetID();
      break;
    }
    it.Advance();
  }

  if (renderer_id) {
    RenderViewHostImpl* host = RenderViewHostImpl::FromID(
        renderer_id, render_view_id);
    if (host) {
      media::MediaPlayerAndroid* player =
          host->media_player_manager()->GetPlayer(player_id);
      if (player &&
          player != host->media_player_manager()->GetFullscreenPlayer()) {
        gfx::ScopedJavaSurface surface(surface_texture.get());
        player->SetVideoSurface(surface.Pass());
      }
    }
  }
}

}  // anonymous namespace

SurfaceTexturePeerBrowserImpl::SurfaceTexturePeerBrowserImpl() {
}

SurfaceTexturePeerBrowserImpl::~SurfaceTexturePeerBrowserImpl() {
}

void SurfaceTexturePeerBrowserImpl::EstablishSurfaceTexturePeer(
    base::ProcessHandle render_process_handle,
    scoped_refptr<gfx::SurfaceTexture> surface_texture,
    int render_view_id,
    int player_id) {
  if (!surface_texture.get())
    return;

  BrowserThread::PostTask(BrowserThread::UI, FROM_HERE, base::Bind(
      &SetSurfacePeer, surface_texture, render_process_handle,
      render_view_id, player_id));
}

}  // namespace content
