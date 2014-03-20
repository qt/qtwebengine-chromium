// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_X_X11_ATOM_CACHE_H_
#define UI_GFX_X_X11_ATOM_CACHE_H_

#include "base/basictypes.h"
#include "ui/gfx/gfx_export.h"

#include <map>
#include <string>

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

namespace ui {

// Pre-caches all Atoms on first use to minimize roundtrips to the X11
// server. By default, GetAtom() will CHECK() that atoms accessed through
// GetAtom() were passed to the constructor, but this behaviour can be changed
// with allow_uncached_atoms().
class GFX_EXPORT X11AtomCache {
 public:
  // Preinterns the NULL terminated list of string |to_cache_ on |xdisplay|.
  X11AtomCache(Display* xdisplay, const char** to_cache);
  ~X11AtomCache();

  // Returns the pre-interned Atom without having to go to the x server.
  ::Atom GetAtom(const char*) const;

  // When an Atom isn't in the list of items we've cached, we should look it
  // up, cache it locally, and then return the result.
  void allow_uncached_atoms() { uncached_atoms_allowed_ = true; }

 private:
  Display* xdisplay_;

  bool uncached_atoms_allowed_;

  mutable std::map<std::string, ::Atom> cached_atoms_;

  DISALLOW_COPY_AND_ASSIGN(X11AtomCache);
};

}  // namespace ui

#endif  // UI_GFX_X_X11_ATOM_CACHE_H_
