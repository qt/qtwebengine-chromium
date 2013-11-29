// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/capture/win/filter_base_win.h"

#pragma comment(lib, "strmiids.lib")

namespace media {

// Implement IEnumPins.
class PinEnumerator
    : public IEnumPins,
      public base::RefCounted<PinEnumerator> {
 public:
  explicit PinEnumerator(FilterBase* filter)
      : filter_(filter),
      index_(0) {
  }

  ~PinEnumerator() {
  }

  // IUnknown implementation.
  STDMETHOD(QueryInterface)(REFIID iid, void** object_ptr) {
    if (iid == IID_IEnumPins || iid == IID_IUnknown) {
      AddRef();
      *object_ptr = static_cast<IEnumPins*>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  STDMETHOD_(ULONG, AddRef)() {
    base::RefCounted<PinEnumerator>::AddRef();
    return 1;
  }

  STDMETHOD_(ULONG, Release)() {
    base::RefCounted<PinEnumerator>::Release();
    return 1;
  }

  // Implement IEnumPins.
  STDMETHOD(Next)(ULONG count, IPin** pins, ULONG* fetched) {
    ULONG pins_fetched = 0;
    while (pins_fetched < count && filter_->NoOfPins() > index_) {
      IPin* pin = filter_->GetPin(index_++);
      pin->AddRef();
      pins[pins_fetched++] = pin;
    }

    if (fetched)
      *fetched =  pins_fetched;

    return pins_fetched == count ? S_OK : S_FALSE;
  }

  STDMETHOD(Skip)(ULONG count) {
    if (filter_->NoOfPins()- index_ > count) {
      index_ += count;
      return S_OK;
    }
    index_ = 0;
    return S_FALSE;
  }

  STDMETHOD(Reset)() {
    index_ = 0;
    return S_OK;
  }

  STDMETHOD(Clone)(IEnumPins** clone) {
    PinEnumerator* pin_enum = new PinEnumerator(filter_);
    if (!pin_enum)
      return E_OUTOFMEMORY;
    pin_enum->AddRef();
    pin_enum->index_ = index_;
    *clone = pin_enum;
    return S_OK;
  }

 private:
  scoped_refptr<FilterBase> filter_;
  size_t index_;
};

FilterBase::FilterBase() : state_(State_Stopped) {
}

FilterBase::~FilterBase() {
}

STDMETHODIMP FilterBase::EnumPins(IEnumPins** enum_pins) {
  *enum_pins = new PinEnumerator(this);
  (*enum_pins)->AddRef();
  return S_OK;
}

STDMETHODIMP FilterBase::FindPin(LPCWSTR id, IPin** pin) {
  return E_NOTIMPL;
}

STDMETHODIMP FilterBase::QueryFilterInfo(FILTER_INFO* info) {
  info->pGraph = owning_graph_;
  info->achName[0] = L'\0';
  if (info->pGraph)
    info->pGraph->AddRef();
  return S_OK;
}

STDMETHODIMP FilterBase::JoinFilterGraph(IFilterGraph* graph, LPCWSTR name) {
  owning_graph_ = graph;
  return S_OK;
}

STDMETHODIMP FilterBase::QueryVendorInfo(LPWSTR *pVendorInfo) {
  return S_OK;
}

// Implement IMediaFilter.
STDMETHODIMP FilterBase::Stop() {
  state_ = State_Stopped;
  return S_OK;
}

STDMETHODIMP FilterBase::Pause() {
  state_ = State_Paused;
  return S_OK;
}

STDMETHODIMP FilterBase::Run(REFERENCE_TIME start) {
  state_ = State_Running;
  return S_OK;
}

STDMETHODIMP FilterBase::GetState(DWORD msec_timeout, FILTER_STATE* state) {
  *state = state_;
  return S_OK;
}

STDMETHODIMP FilterBase::SetSyncSource(IReferenceClock* clock) {
  return S_OK;
}

STDMETHODIMP FilterBase::GetSyncSource(IReferenceClock** clock) {
  return E_NOTIMPL;
}

// Implement from IPersistent.
STDMETHODIMP FilterBase::GetClassID(CLSID* class_id) {
  NOTREACHED();
  return E_NOTIMPL;
}

// Implement IUnknown.
STDMETHODIMP FilterBase::QueryInterface(REFIID id, void** object_ptr) {
  if (id == IID_IMediaFilter || id == IID_IUnknown) {
    *object_ptr = static_cast<IMediaFilter*>(this);
  } else if (id == IID_IPersist) {
    *object_ptr = static_cast<IPersist*>(this);
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG STDMETHODCALLTYPE FilterBase::AddRef() {
  base::RefCounted<FilterBase>::AddRef();
  return 1;
}

ULONG STDMETHODCALLTYPE FilterBase::Release() {
  base::RefCounted<FilterBase>::Release();
  return 1;
}

}  // namespace media
