// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/browser_plugin/browser_plugin.h"

#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/browser_plugin/browser_plugin_constants.h"
#include "content/common/browser_plugin/browser_plugin_messages.h"
#include "content/common/view_messages.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/content_renderer_client.h"
#include "content/renderer/browser_plugin/browser_plugin_bindings.h"
#include "content/renderer/browser_plugin/browser_plugin_compositing_helper.h"
#include "content/renderer/browser_plugin/browser_plugin_manager.h"
#include "content/renderer/cursor_utils.h"
#include "content/renderer/drop_data_builder.h"
#include "content/renderer/render_process_impl.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/sad_plugin.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/web/WebBindings.h"
#include "third_party/WebKit/public/web/WebDOMCustomEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebPluginParams.h"
#include "third_party/WebKit/public/web/WebScriptSource.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "ui/base/keycodes/keyboard_codes.h"

#if defined (OS_WIN)
#include "base/sys_info.h"
#endif

using WebKit::WebCanvas;
using WebKit::WebPluginContainer;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebRect;
using WebKit::WebURL;
using WebKit::WebVector;

namespace content {

namespace {

static std::string GetInternalEventName(const char* event_name) {
  return base::StringPrintf("-internal-%s", event_name);
}

typedef std::map<WebKit::WebPluginContainer*,
                 BrowserPlugin*> PluginContainerMap;
static base::LazyInstance<PluginContainerMap> g_plugin_container_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

BrowserPlugin::BrowserPlugin(
    RenderViewImpl* render_view,
    WebKit::WebFrame* frame,
    const WebPluginParams& params)
    : guest_instance_id_(browser_plugin::kInstanceIDNone),
      render_view_(render_view->AsWeakPtr()),
      render_view_routing_id_(render_view->GetRoutingID()),
      container_(NULL),
      damage_buffer_sequence_id_(0),
      resize_ack_received_(true),
      last_device_scale_factor_(1.0f),
      sad_guest_(NULL),
      guest_crashed_(false),
      auto_size_ack_pending_(false),
      persist_storage_(false),
      valid_partition_id_(true),
      content_window_routing_id_(MSG_ROUTING_NONE),
      plugin_focused_(false),
      visible_(true),
      size_changed_in_flight_(false),
      before_first_navigation_(true),
      mouse_locked_(false),
      browser_plugin_manager_(render_view->GetBrowserPluginManager()),
      compositing_enabled_(false),
      weak_ptr_factory_(this) {
}

BrowserPlugin::~BrowserPlugin() {
  // If the BrowserPlugin has never navigated then the browser process and
  // BrowserPluginManager don't know about it and so there is nothing to do
  // here.
  if (!HasGuestInstanceID())
    return;
  browser_plugin_manager()->RemoveBrowserPlugin(guest_instance_id_);
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_PluginDestroyed(render_view_routing_id_,
                                               guest_instance_id_));
}

/*static*/
BrowserPlugin* BrowserPlugin::FromContainer(
    WebKit::WebPluginContainer* container) {
  PluginContainerMap* browser_plugins = g_plugin_container_map.Pointer();
  PluginContainerMap::iterator it = browser_plugins->find(container);
  return it == browser_plugins->end() ? NULL : it->second;
}

bool BrowserPlugin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPlugin, message)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_AdvanceFocus, OnAdvanceFocus)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_Attach_ACK, OnAttachACK)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_BuffersSwapped, OnBuffersSwapped)
    IPC_MESSAGE_HANDLER_GENERIC(BrowserPluginMsg_CompositorFrameSwapped,
                                OnCompositorFrameSwapped(message))
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestContentWindowReady,
                        OnGuestContentWindowReady)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_GuestGone, OnGuestGone)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetCursor, OnSetCursor)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_SetMouseLock, OnSetMouseLock)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_ShouldAcceptTouchEvents,
                        OnShouldAcceptTouchEvents)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_UpdatedName, OnUpdatedName)
    IPC_MESSAGE_HANDLER(BrowserPluginMsg_UpdateRect, OnUpdateRect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void BrowserPlugin::UpdateDOMAttribute(const std::string& attribute_name,
                                       const std::string& attribute_value) {
  if (!container())
    return;

  WebKit::WebElement element = container()->element();
  WebKit::WebString web_attribute_name =
      WebKit::WebString::fromUTF8(attribute_name);
  if (!HasDOMAttribute(attribute_name) ||
      (std::string(element.getAttribute(web_attribute_name).utf8()) !=
          attribute_value)) {
    element.setAttribute(web_attribute_name,
        WebKit::WebString::fromUTF8(attribute_value));
  }
}

void BrowserPlugin::RemoveDOMAttribute(const std::string& attribute_name) {
  if (!container())
    return;

  container()->element().removeAttribute(
      WebKit::WebString::fromUTF8(attribute_name));
}

std::string BrowserPlugin::GetDOMAttributeValue(
    const std::string& attribute_name) const {
  if (!container())
    return std::string();

  return container()->element().getAttribute(
      WebKit::WebString::fromUTF8(attribute_name)).utf8();
}

bool BrowserPlugin::HasDOMAttribute(const std::string& attribute_name) const {
  if (!container())
    return false;

  return container()->element().hasAttribute(
      WebKit::WebString::fromUTF8(attribute_name));
}

std::string BrowserPlugin::GetNameAttribute() const {
  return GetDOMAttributeValue(browser_plugin::kAttributeName);
}

std::string BrowserPlugin::GetSrcAttribute() const {
  return GetDOMAttributeValue(browser_plugin::kAttributeSrc);
}

bool BrowserPlugin::GetAutoSizeAttribute() const {
  return HasDOMAttribute(browser_plugin::kAttributeAutoSize);
}

int BrowserPlugin::GetMaxHeightAttribute() const {
  int max_height;
  base::StringToInt(GetDOMAttributeValue(browser_plugin::kAttributeMaxHeight),
                    &max_height);
  return max_height;
}

int BrowserPlugin::GetMaxWidthAttribute() const {
  int max_width;
  base::StringToInt(GetDOMAttributeValue(browser_plugin::kAttributeMaxWidth),
                    &max_width);
  return max_width;
}

int BrowserPlugin::GetMinHeightAttribute() const {
  int min_height;
  base::StringToInt(GetDOMAttributeValue(browser_plugin::kAttributeMinHeight),
                    &min_height);
  return min_height;
}

int BrowserPlugin::GetMinWidthAttribute() const {
  int min_width;
  base::StringToInt(GetDOMAttributeValue(browser_plugin::kAttributeMinWidth),
                    &min_width);
  return min_width;
}

int BrowserPlugin::GetAdjustedMaxHeight() const {
  int max_height = GetMaxHeightAttribute();
  return max_height ? max_height : height();
}

int BrowserPlugin::GetAdjustedMaxWidth() const {
  int max_width = GetMaxWidthAttribute();
  return max_width ? max_width : width();
}

int BrowserPlugin::GetAdjustedMinHeight() const {
  int min_height = GetMinHeightAttribute();
  // FrameView.cpp does not allow this value to be <= 0, so when the value is
  // unset (or set to 0), we set it to the container size.
  min_height = min_height ? min_height : height();
  // For autosize, minHeight should not be bigger than maxHeight.
  return std::min(min_height, GetAdjustedMaxHeight());
}

int BrowserPlugin::GetAdjustedMinWidth() const {
  int min_width = GetMinWidthAttribute();
  // FrameView.cpp does not allow this value to be <= 0, so when the value is
  // unset (or set to 0), we set it to the container size.
  min_width = min_width ? min_width : width();
  // For autosize, minWidth should not be bigger than maxWidth.
  return std::min(min_width, GetAdjustedMaxWidth());
}

std::string BrowserPlugin::GetPartitionAttribute() const {
  return GetDOMAttributeValue(browser_plugin::kAttributePartition);
}

void BrowserPlugin::ParseNameAttribute() {
  if (!HasGuestInstanceID())
    return;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_SetName(render_view_routing_id_,
                                       guest_instance_id_,
                                       GetNameAttribute()));
}

bool BrowserPlugin::ParseSrcAttribute(std::string* error_message) {
  if (!valid_partition_id_) {
    *error_message = browser_plugin::kErrorInvalidPartition;
    return false;
  }
  std::string src = GetSrcAttribute();
  if (src.empty())
    return true;

  // If we haven't created the guest yet, do so now. We will navigate it right
  // after creation. If |src| is empty, we can delay the creation until we
  // actually need it.
  if (!HasGuestInstanceID()) {
    // On initial navigation, we request an instance ID from the browser
    // process. We essentially ignore all subsequent calls to SetSrcAttribute
    // until we receive an instance ID. |before_first_navigation_|
    // prevents BrowserPlugin from allocating more than one instance ID.
    // Upon receiving an instance ID from the browser process, we continue
    // the process of navigation by populating the
    // BrowserPluginHostMsg_Attach_Params with the current state of
    // BrowserPlugin and sending a BrowserPluginHostMsg_CreateGuest to the
    // browser process in order to create a new guest.
    if (before_first_navigation_) {
      browser_plugin_manager()->AllocateInstanceID(this);
      before_first_navigation_ = false;
    }
    return true;
  }

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_NavigateGuest(render_view_routing_id_,
                                             guest_instance_id_,
                                             src));
  return true;
}

void BrowserPlugin::ParseAutoSizeAttribute() {
  auto_size_ack_pending_ = true;
  last_view_size_ = plugin_rect_.size();
  UpdateGuestAutoSizeState(GetAutoSizeAttribute());
}

void BrowserPlugin::PopulateAutoSizeParameters(
    BrowserPluginHostMsg_AutoSize_Params* params, bool current_auto_size) {
  params->enable = current_auto_size;
  // No need to populate the params if autosize is off.
  if (current_auto_size) {
    params->max_size = gfx::Size(GetAdjustedMaxWidth(), GetAdjustedMaxHeight());
    params->min_size = gfx::Size(GetAdjustedMinWidth(), GetAdjustedMinHeight());
  }
}

void BrowserPlugin::UpdateGuestAutoSizeState(bool current_auto_size) {
  // If we haven't yet heard back from the guest about the last resize request,
  // then we don't issue another request until we do in
  // BrowserPlugin::UpdateRect.
  if (!HasGuestInstanceID() || !resize_ack_received_)
    return;
  BrowserPluginHostMsg_AutoSize_Params auto_size_params;
  BrowserPluginHostMsg_ResizeGuest_Params resize_guest_params;
  if (current_auto_size) {
    GetDamageBufferWithSizeParams(&auto_size_params, &resize_guest_params);
  } else {
    GetDamageBufferWithSizeParams(NULL, &resize_guest_params);
  }
  resize_ack_received_ = false;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_SetAutoSize(render_view_routing_id_,
                                           guest_instance_id_,
                                           auto_size_params,
                                           resize_guest_params));
}

void BrowserPlugin::SizeChangedDueToAutoSize(const gfx::Size& old_view_size) {
  size_changed_in_flight_ = false;

  std::map<std::string, base::Value*> props;
  props[browser_plugin::kOldHeight] =
      new base::FundamentalValue(old_view_size.height());
  props[browser_plugin::kOldWidth] =
      new base::FundamentalValue(old_view_size.width());
  props[browser_plugin::kNewHeight] =
      new base::FundamentalValue(last_view_size_.height());
  props[browser_plugin::kNewWidth] =
      new base::FundamentalValue(last_view_size_.width());
  TriggerEvent(browser_plugin::kEventSizeChanged, &props);
}

// static
bool BrowserPlugin::UsesDamageBuffer(
    const BrowserPluginMsg_UpdateRect_Params& params) {
  return params.damage_buffer_sequence_id != 0 || params.needs_ack;
}

bool BrowserPlugin::UsesPendingDamageBuffer(
    const BrowserPluginMsg_UpdateRect_Params& params) {
  if (!pending_damage_buffer_)
    return false;
  return damage_buffer_sequence_id_ == params.damage_buffer_sequence_id;
}

void BrowserPlugin::OnInstanceIDAllocated(int guest_instance_id) {
  CHECK(guest_instance_id != browser_plugin::kInstanceIDNone);
  before_first_navigation_ = false;
  guest_instance_id_ = guest_instance_id;
  browser_plugin_manager()->AddBrowserPlugin(guest_instance_id, this);

  std::map<std::string, base::Value*> props;
  props[browser_plugin::kWindowID] =
      new base::FundamentalValue(guest_instance_id);
  TriggerEvent(browser_plugin::kEventInternalInstanceIDAllocated, &props);
}

void BrowserPlugin::Attach(scoped_ptr<base::DictionaryValue> extra_params) {
  BrowserPluginHostMsg_Attach_Params attach_params;
  attach_params.focused = ShouldGuestBeFocused();
  attach_params.visible = visible_;
  attach_params.name = GetNameAttribute();
  attach_params.storage_partition_id = storage_partition_id_;
  attach_params.persist_storage = persist_storage_;
  attach_params.src = GetSrcAttribute();
  GetDamageBufferWithSizeParams(&attach_params.auto_size_params,
                                &attach_params.resize_guest_params);

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_Attach(render_view_routing_id_,
                                      guest_instance_id_, attach_params,
                                      *extra_params));
}

void BrowserPlugin::DidCommitCompositorFrame() {
  if (compositing_helper_.get())
    compositing_helper_->DidCommitCompositorFrame();
}

void BrowserPlugin::OnAdvanceFocus(int guest_instance_id, bool reverse) {
  DCHECK(render_view_.get());
  render_view_->GetWebView()->advanceFocus(reverse);
}

void BrowserPlugin::OnAttachACK(
    int guest_instance_id,
    const BrowserPluginMsg_Attach_ACK_Params& params) {
  // Update BrowserPlugin attributes to match the state of the guest.
  if (!params.name.empty())
    OnUpdatedName(guest_instance_id, params.name);
  if (!params.storage_partition_id.empty()) {
    std::string partition_name =
        (params.persist_storage ? browser_plugin::kPersistPrefix : "") +
            params.storage_partition_id;
    UpdateDOMAttribute(browser_plugin::kAttributePartition, partition_name);
  }
}

void BrowserPlugin::OnBuffersSwapped(
    int guest_instance_id,
    const BrowserPluginMsg_BuffersSwapped_Params& params) {
  DCHECK(guest_instance_id == guest_instance_id_);
  EnableCompositing(true);

  compositing_helper_->OnBuffersSwapped(params.size,
                                        params.mailbox_name,
                                        params.route_id,
                                        params.host_id,
                                        GetDeviceScaleFactor());
}

void BrowserPlugin::OnCompositorFrameSwapped(const IPC::Message& message) {
  BrowserPluginMsg_CompositorFrameSwapped::Param param;
  if (!BrowserPluginMsg_CompositorFrameSwapped::Read(&message, &param))
    return;
  scoped_ptr<cc::CompositorFrame> frame(new cc::CompositorFrame);
  param.b.AssignTo(frame.get());

  EnableCompositing(true);
  compositing_helper_->OnCompositorFrameSwapped(frame.Pass(),
                                                param.c /* route_id */,
                                                param.d /* output_surface_id */,
                                                param.e /* host_id */);
}

void BrowserPlugin::OnGuestContentWindowReady(int guest_instance_id,
                                              int content_window_routing_id) {
  DCHECK(content_window_routing_id != MSG_ROUTING_NONE);
  content_window_routing_id_ = content_window_routing_id;
}

void BrowserPlugin::OnGuestGone(int guest_instance_id) {
  guest_crashed_ = true;

  // Queue up showing the sad graphic to give content embedders an opportunity
  // to fire their listeners and potentially overlay the webview with custom
  // behavior. If the BrowserPlugin is destroyed in the meantime, then the
  // task will not be executed.
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&BrowserPlugin::ShowSadGraphic,
                 weak_ptr_factory_.GetWeakPtr()));
}

void BrowserPlugin::OnSetCursor(int guest_instance_id,
                                const WebCursor& cursor) {
  cursor_ = cursor;
}

void BrowserPlugin::OnSetMouseLock(int guest_instance_id,
                                   bool enable) {
  if (enable) {
    if (mouse_locked_)
      return;
    render_view_->mouse_lock_dispatcher()->LockMouse(this);
  } else {
    if (!mouse_locked_) {
      OnLockMouseACK(false);
      return;
    }
    render_view_->mouse_lock_dispatcher()->UnlockMouse(this);
  }
}

void BrowserPlugin::OnShouldAcceptTouchEvents(int guest_instance_id,
                                              bool accept) {
  if (container()) {
    container()->requestTouchEventType(accept ?
        WebKit::WebPluginContainer::TouchEventRequestTypeRaw :
        WebKit::WebPluginContainer::TouchEventRequestTypeNone);
  }
}

void BrowserPlugin::OnUpdatedName(int guest_instance_id,
                                  const std::string& name) {
  UpdateDOMAttribute(browser_plugin::kAttributeName, name);
}

void BrowserPlugin::OnUpdateRect(
    int guest_instance_id,
    const BrowserPluginMsg_UpdateRect_Params& params) {
  // If the guest has updated pixels then it is no longer crashed.
  guest_crashed_ = false;

  bool use_new_damage_buffer = !backing_store_;
  BrowserPluginHostMsg_AutoSize_Params auto_size_params;
  BrowserPluginHostMsg_ResizeGuest_Params resize_guest_params;
  // If we have a pending damage buffer, and the guest has begun to use the
  // damage buffer then we know the guest will no longer use the current
  // damage buffer. At this point, we drop the current damage buffer, and
  // mark the pending damage buffer as the current damage buffer.
  if (UsesPendingDamageBuffer(params)) {
    SwapDamageBuffers();
    use_new_damage_buffer = true;
  }

  bool auto_size = GetAutoSizeAttribute();
  // We receive a resize ACK in regular mode, but not in autosize.
  // In SW, |resize_ack_received_| is reset in SwapDamageBuffers().
  // In HW mode, we need to do it here so we can continue sending
  // resize messages when needed.
  if (params.is_resize_ack ||
      (!params.needs_ack && (auto_size || auto_size_ack_pending_)))
    resize_ack_received_ = true;

  auto_size_ack_pending_ = false;

  if ((!auto_size && (width() != params.view_size.width() ||
                      height() != params.view_size.height())) ||
      (auto_size && (!InAutoSizeBounds(params.view_size))) ||
      GetDeviceScaleFactor() != params.scale_factor) {
    // We are HW accelerated, render widget does not expect an ack,
    // but we still need to update the size.
    if (!params.needs_ack) {
      UpdateGuestAutoSizeState(auto_size);
      return;
    }

    if (!resize_ack_received_) {
      // The guest has not yet responded to the last resize request, and
      // so we don't want to do anything at this point other than ACK the guest.
      if (auto_size)
        PopulateAutoSizeParameters(&auto_size_params, auto_size);
    } else {
      // If we have no pending damage buffer, then the guest has not caught up
      // with the BrowserPlugin container. We now tell the guest about the new
      // container size.
      if (auto_size) {
        GetDamageBufferWithSizeParams(&auto_size_params,
                                      &resize_guest_params);
      } else {
        GetDamageBufferWithSizeParams(NULL, &resize_guest_params);
      }
    }
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
        render_view_routing_id_,
        guest_instance_id_,
        true,
        auto_size_params,
        resize_guest_params));
    return;
  }

  if (auto_size && (params.view_size != last_view_size_)) {
    if (backing_store_)
      backing_store_->Clear(SK_ColorWHITE);
    gfx::Size old_view_size = last_view_size_;
    last_view_size_ = params.view_size;
    // Schedule a SizeChanged instead of calling it directly to ensure that
    // the backing store has been updated before the developer attempts to
    // resize to avoid flicker. |size_changed_in_flight_| acts as a form of
    // flow control for SizeChanged events. If the guest's view size is changing
    // rapidly before a SizeChanged event fires, then we avoid scheduling
    // another SizeChanged event. SizeChanged reads the new size from
    // |last_view_size_| so we can be sure that it always fires an event
    // with the last seen view size.
    if (container_ && !size_changed_in_flight_) {
      size_changed_in_flight_ = true;
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&BrowserPlugin::SizeChangedDueToAutoSize,
                     base::Unretained(this),
                     old_view_size));
    }
  }

  if (UsesDamageBuffer(params)) {

    // If we are seeing damage buffers, HW compositing should be turned off.
    EnableCompositing(false);

    // If we are now using a new damage buffer, then that means that the guest
    // has updated its size state in response to a resize request. We change
    // the backing store's size to accomodate the new damage buffer size.
    if (use_new_damage_buffer) {
      int backing_store_width = auto_size ? GetAdjustedMaxWidth() : width();
      int backing_store_height = auto_size ? GetAdjustedMaxHeight(): height();
      backing_store_.reset(
          new BrowserPluginBackingStore(
              gfx::Size(backing_store_width, backing_store_height),
              params.scale_factor));
    }

    // If we just transitioned from the compositing path to the software path
    // then we might not yet have a damage buffer.
    if (current_damage_buffer_) {
      // Update the backing store.
      if (!params.scroll_rect.IsEmpty()) {
        backing_store_->ScrollBackingStore(params.scroll_delta,
                                          params.scroll_rect,
                                          params.view_size);
      }
      backing_store_->PaintToBackingStore(params.bitmap_rect,
                                          params.copy_rects,
                                          current_damage_buffer_->memory());
      // Invalidate the container.
      // If the BrowserPlugin is scheduled to be deleted, then container_ will
      // be NULL so we shouldn't attempt to access it.
      if (container_)
        container_->invalidate();
    }
  }

  // BrowserPluginHostMsg_UpdateRect_ACK is used by both the compositing and
  // software paths to piggyback updated autosize parameters.
  if (auto_size)
    PopulateAutoSizeParameters(&auto_size_params, auto_size);
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateRect_ACK(
      render_view_routing_id_,
      guest_instance_id_,
      UsesDamageBuffer(params),
      auto_size_params,
      resize_guest_params));
}

void BrowserPlugin::ParseSizeContraintsChanged() {
  bool auto_size = GetAutoSizeAttribute();
  if (auto_size)
    UpdateGuestAutoSizeState(true);
}

bool BrowserPlugin::InAutoSizeBounds(const gfx::Size& size) const {
  return size.width() <= GetAdjustedMaxWidth() &&
      size.height() <= GetAdjustedMaxHeight();
}

NPObject* BrowserPlugin::GetContentWindow() const {
  if (content_window_routing_id_ == MSG_ROUTING_NONE)
    return NULL;
  RenderViewImpl* guest_render_view = RenderViewImpl::FromRoutingID(
      content_window_routing_id_);
  if (!guest_render_view)
    return NULL;
  WebKit::WebFrame* guest_frame = guest_render_view->GetWebView()->mainFrame();
  return guest_frame->windowObject();
}

// static
bool BrowserPlugin::AttachWindowTo(const WebKit::WebNode& node, int window_id) {
  if (node.isNull())
    return false;

  if (!node.isElementNode())
    return false;

  WebKit::WebElement shim_element = node.toConst<WebKit::WebElement>();
  // The shim containing the BrowserPlugin must be attached to a document.
  if (shim_element.document().isNull())
    return false;

  WebKit::WebNode shadow_root = shim_element.shadowRoot();
  if (shadow_root.isNull() || !shadow_root.hasChildNodes())
    return false;

  WebKit::WebNode plugin_element = shadow_root.firstChild();
  WebKit::WebPluginContainer* plugin_container =
      plugin_element.pluginContainer();
  if (!plugin_container)
    return false;

  BrowserPlugin* browser_plugin =
      BrowserPlugin::FromContainer(plugin_container);
  if (!browser_plugin)
    return false;

  // If the BrowserPlugin has already begun to navigate then we shouldn't allow
  // attaching a different guest.
  //
  // Navigation happens in two stages.
  // 1. BrowserPlugin requests an instance ID from the browser process.
  // 2. The browser process returns an instance ID and BrowserPlugin is
  //    "Attach"ed to that instance ID.
  // If the instance ID is new then a new guest will be created.
  // If the instance ID corresponds to an unattached guest then BrowserPlugin
  // is attached to that guest.
  //
  // Between step 1, and step 2, BrowserPlugin::AttachWindowTo may be called.
  // The check below ensures that BrowserPlugin:Attach does not get called with
  // a different instance ID after step 1 has happened.
  // TODO(fsamuel): We may wish to support reattaching guests in the future:
  // http://crbug.com/156219.
  if (browser_plugin->HasNavigated())
    return false;

  browser_plugin->OnInstanceIDAllocated(window_id);
  return true;
}

bool BrowserPlugin::HasNavigated() const {
  return !before_first_navigation_;
}

bool BrowserPlugin::HasGuestInstanceID() const {
  return guest_instance_id_ != browser_plugin::kInstanceIDNone;
}

bool BrowserPlugin::ParsePartitionAttribute(std::string* error_message) {
  if (HasNavigated()) {
    *error_message = browser_plugin::kErrorAlreadyNavigated;
    return false;
  }

  std::string input = GetPartitionAttribute();

  // Since the "persist:" prefix is in ASCII, StartsWith will work fine on
  // UTF-8 encoded |partition_id|. If the prefix is a match, we can safely
  // remove the prefix without splicing in the middle of a multi-byte codepoint.
  // We can use the rest of the string as UTF-8 encoded one.
  if (StartsWithASCII(input, browser_plugin::kPersistPrefix, true)) {
    size_t index = input.find(":");
    CHECK(index != std::string::npos);
    // It is safe to do index + 1, since we tested for the full prefix above.
    input = input.substr(index + 1);
    if (input.empty()) {
      valid_partition_id_ = false;
      *error_message = browser_plugin::kErrorInvalidPartition;
      return false;
    }
    persist_storage_ = true;
  } else {
    persist_storage_ = false;
  }

  valid_partition_id_ = true;
  storage_partition_id_ = input;
  return true;
}

bool BrowserPlugin::CanRemovePartitionAttribute(std::string* error_message) {
  if (HasGuestInstanceID())
    *error_message = browser_plugin::kErrorCannotRemovePartition;
  return !HasGuestInstanceID();
}

void BrowserPlugin::ShowSadGraphic() {
  // We won't paint the contents of the current backing store again so we might
  // as well toss it out and save memory.
  backing_store_.reset();
  // If the BrowserPlugin is scheduled to be deleted, then container_ will be
  // NULL so we shouldn't attempt to access it.
  if (container_)
    container_->invalidate();
  // Turn off compositing so we can display the sad graphic.
  EnableCompositing(false);
}

void BrowserPlugin::ParseAttributes() {
  // TODO(mthiesse): Handle errors here?
  std::string error;
  ParsePartitionAttribute(&error);

  // Parse the 'src' attribute last, as it will set the has_navigated_ flag to
  // true, which prevents changing the 'partition' attribute.
  ParseSrcAttribute(&error);
}

float BrowserPlugin::GetDeviceScaleFactor() const {
  if (!render_view_.get())
    return 1.0f;
  return render_view_->GetWebView()->deviceScaleFactor();
}

void BrowserPlugin::UpdateDeviceScaleFactor(float device_scale_factor) {
  if (last_device_scale_factor_ == device_scale_factor || !resize_ack_received_)
    return;

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(&params, plugin_rect());
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      guest_instance_id_,
      params));
}

void BrowserPlugin::TriggerEvent(const std::string& event_name,
                                 std::map<std::string, base::Value*>* props) {
  if (!container())
    return;

  WebKit::WebFrame* frame = container()->element().document().frame();
  if (!frame)
    return;

  v8::HandleScope handle_scope;
  v8::Local<v8::Context> context = frame->mainWorldScriptContext();
  v8::Context::Scope context_scope(context);

  std::string json_string;
  if (props) {
    base::DictionaryValue dict;
    for (std::map<std::string, base::Value*>::iterator iter = props->begin(),
             end = props->end(); iter != end; ++iter) {
      dict.Set(iter->first, iter->second);
    }

    JSONStringValueSerializer serializer(&json_string);
    if (!serializer.Serialize(dict))
      return;
  }

  WebKit::WebDOMEvent dom_event = frame->document().createEvent("CustomEvent");
  WebKit::WebDOMCustomEvent event = dom_event.to<WebKit::WebDOMCustomEvent>();

  // The events triggered directly from the plugin <object> are internal events
  // whose implementation details can (and likely will) change over time. The
  // wrapper/shim (e.g. <webview> tag) should receive these events, and expose a
  // more appropriate (and stable) event to the consumers as part of the API.
  event.initCustomEvent(
      WebKit::WebString::fromUTF8(GetInternalEventName(event_name.c_str())),
      false, false,
      WebKit::WebSerializedScriptValue::serialize(
          v8::String::New(json_string.c_str(), json_string.size())));
  container()->element().dispatchEvent(event);
}

void BrowserPlugin::OnTrackedObjectGarbageCollected(int id) {
  // Remove from alive objects.
  std::map<int, TrackedV8ObjectID*>::iterator iter =
      tracked_v8_objects_.find(id);
  if (iter != tracked_v8_objects_.end())
    tracked_v8_objects_.erase(iter);

  std::map<std::string, base::Value*> props;
  props[browser_plugin::kId] = new base::FundamentalValue(id);
  TriggerEvent(browser_plugin::kEventInternalTrackedObjectGone, &props);
}

void BrowserPlugin::TrackObjectLifetime(const NPVariant* request, int id) {
  // An object of a given ID can only be tracked once.
  if (tracked_v8_objects_.find(id) != tracked_v8_objects_.end())
    return;

  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  v8::Persistent<v8::Value> weak_request(
      isolate, WebKit::WebBindings::toV8Value(request));

  TrackedV8ObjectID* new_item =
      new std::pair<int, base::WeakPtr<BrowserPlugin> >(
          id, weak_ptr_factory_.GetWeakPtr());

  std::pair<std::map<int, TrackedV8ObjectID*>::iterator, bool>
      result = tracked_v8_objects_.insert(
          std::make_pair(id, new_item));
  CHECK(result.second);  // Inserted in the map.
  TrackedV8ObjectID* request_item = result.first->second;
  weak_request.MakeWeak(static_cast<void*>(request_item),
                        WeakCallbackForTrackedObject);
}

// static
void BrowserPlugin::WeakCallbackForTrackedObject(
    v8::Isolate* isolate, v8::Persistent<v8::Value>* object, void* param) {

  TrackedV8ObjectID* item_ptr = static_cast<TrackedV8ObjectID*>(param);
  int object_id = item_ptr->first;
  base::WeakPtr<BrowserPlugin> plugin = item_ptr->second;
  delete item_ptr;

  object->Dispose();
  if (plugin.get()) {
    // Asynchronously remove item from |tracked_v8_objects_|.
    // Note that we are using weak pointer for the following PostTask, so we
    // don't need to worry about BrowserPlugin going away.
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&BrowserPlugin::OnTrackedObjectGarbageCollected,
                   plugin,
                   object_id));
  }
}

void BrowserPlugin::UpdateGuestFocusState() {
  if (!HasGuestInstanceID())
    return;
  bool should_be_focused = ShouldGuestBeFocused();
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetFocus(
      render_view_routing_id_,
      guest_instance_id_,
      should_be_focused));
}

bool BrowserPlugin::ShouldGuestBeFocused() const {
  bool embedder_focused = false;
  if (render_view_.get())
    embedder_focused = render_view_->has_focus();
  return plugin_focused_ && embedder_focused;
}

WebKit::WebPluginContainer* BrowserPlugin::container() const {
  return container_;
}

bool BrowserPlugin::initialize(WebPluginContainer* container) {
  if (!container)
    return false;

  if (!GetContentClient()->renderer()->AllowBrowserPlugin(container))
    return false;

  // Tell |container| to allow this plugin to use script objects.
  npp_.reset(new NPP_t);
  container->allowScriptObjects();

  bindings_.reset(new BrowserPluginBindings(this));
  container_ = container;
  container_->setWantsWheelEvents(true);
  ParseAttributes();
  g_plugin_container_map.Get().insert(std::make_pair(container_, this));
  return true;
}

void BrowserPlugin::EnableCompositing(bool enable) {
  if (compositing_enabled_ == enable)
    return;

  compositing_enabled_ = enable;
  if (enable) {
    // No need to keep the backing store and damage buffer around if we're now
    // compositing.
    backing_store_.reset();
    current_damage_buffer_.reset();
    if (!compositing_helper_.get()) {
      compositing_helper_ =
          new BrowserPluginCompositingHelper(container_,
                                             browser_plugin_manager(),
                                             guest_instance_id_,
                                             render_view_routing_id_);
    }
  } else {
    // We're switching back to the software path. We create a new damage
    // buffer that can accommodate the current size of the container.
    BrowserPluginHostMsg_ResizeGuest_Params params;
    PopulateResizeGuestParameters(&params, plugin_rect());
    // Request a full repaint from the guest even if its size is not actually
    // changing.
    params.repaint = true;
    resize_ack_received_ = false;
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
        render_view_routing_id_,
        guest_instance_id_,
        params));
  }
  compositing_helper_->EnableCompositing(enable);
}

void BrowserPlugin::destroy() {
  // If the plugin was initialized then it has a valid |npp_| identifier, and
  // the |container_| must clear references to the plugin's script objects.
  DCHECK(!npp_ || container_);
  if (container_)
    container_->clearScriptObjects();

  // The BrowserPlugin's WebPluginContainer is deleted immediately after this
  // call returns, so let's not keep a reference to it around.
  g_plugin_container_map.Get().erase(container_);
  container_ = NULL;
  if (compositing_helper_.get())
    compositing_helper_->OnContainerDestroy();
  // Will be a no-op if the mouse is not currently locked.
  if (render_view_.get())
    render_view_->mouse_lock_dispatcher()->OnLockTargetDestroyed(this);
  base::MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

NPObject* BrowserPlugin::scriptableObject() {
  if (!bindings_)
    return NULL;

  NPObject* browser_plugin_np_object(bindings_->np_object());
  // The object is expected to be retained before it is returned.
  WebKit::WebBindings::retainObject(browser_plugin_np_object);
  return browser_plugin_np_object;
}

NPP BrowserPlugin::pluginNPP() {
  return npp_.get();
}

bool BrowserPlugin::supportsKeyboardFocus() const {
  return true;
}

bool BrowserPlugin::supportsEditCommands() const {
  return true;
}

bool BrowserPlugin::canProcessDrag() const {
  return true;
}

void BrowserPlugin::paint(WebCanvas* canvas, const WebRect& rect) {
  if (guest_crashed_) {
    if (!sad_guest_)  // Lazily initialize bitmap.
      sad_guest_ = content::GetContentClient()->renderer()->
          GetSadWebViewBitmap();
    // content_shell does not have the sad plugin bitmap, so we'll paint black
    // instead to make it clear that something went wrong.
    if (sad_guest_) {
      PaintSadPlugin(canvas, plugin_rect_, *sad_guest_);
      return;
    }
  }
  SkAutoCanvasRestore auto_restore(canvas, true);
  canvas->translate(plugin_rect_.x(), plugin_rect_.y());
  SkRect image_data_rect = SkRect::MakeXYWH(
      SkIntToScalar(0),
      SkIntToScalar(0),
      SkIntToScalar(plugin_rect_.width()),
      SkIntToScalar(plugin_rect_.height()));
  canvas->clipRect(image_data_rect);
  // Paint black or white in case we have nothing in our backing store or we
  // need to show a gutter.
  SkPaint paint;
  paint.setStyle(SkPaint::kFill_Style);
  paint.setColor(guest_crashed_ ? SK_ColorBLACK : SK_ColorWHITE);
  canvas->drawRect(image_data_rect, paint);
  // Stay a solid color if we have never set a non-empty src, or we don't have a
  // backing store.
  if (!backing_store_.get() || !HasGuestInstanceID())
    return;
  float inverse_scale_factor =  1.0f / backing_store_->GetScaleFactor();
  canvas->scale(inverse_scale_factor, inverse_scale_factor);
  canvas->drawBitmap(backing_store_->GetBitmap(), 0, 0);
}

bool BrowserPlugin::InBounds(const gfx::Point& position) const {
  // Note that even for plugins that are rotated using rotate transformations,
  // we use the the |plugin_rect_| provided by updateGeometry, which means we
  // will be off if |position| is within the plugin rect but does not fall
  // within the actual plugin boundary. Not supporting such edge case is OK
  // since this function should not be used for making security-sensitive
  // decisions.
  // This also does not take overlapping plugins into account.
  bool result = position.x() >= plugin_rect_.x() &&
      position.x() < plugin_rect_.x() + plugin_rect_.width() &&
      position.y() >= plugin_rect_.y() &&
      position.y() < plugin_rect_.y() + plugin_rect_.height();
  return result;
}

gfx::Point BrowserPlugin::ToLocalCoordinates(const gfx::Point& point) const {
  if (container_)
    return container_->windowToLocalPoint(WebKit::WebPoint(point));
  return gfx::Point(point.x() - plugin_rect_.x(), point.y() - plugin_rect_.y());
}

// static
bool BrowserPlugin::ShouldForwardToBrowserPlugin(
    const IPC::Message& message) {
  switch (message.type()) {
    case BrowserPluginMsg_AdvanceFocus::ID:
    case BrowserPluginMsg_Attach_ACK::ID:
    case BrowserPluginMsg_BuffersSwapped::ID:
    case BrowserPluginMsg_CompositorFrameSwapped::ID:
    case BrowserPluginMsg_GuestContentWindowReady::ID:
    case BrowserPluginMsg_GuestGone::ID:
    case BrowserPluginMsg_SetCursor::ID:
    case BrowserPluginMsg_SetMouseLock::ID:
    case BrowserPluginMsg_ShouldAcceptTouchEvents::ID:
    case BrowserPluginMsg_UpdatedName::ID:
    case BrowserPluginMsg_UpdateRect::ID:
      return true;
    default:
      break;
  }
  return false;
}

void BrowserPlugin::updateGeometry(
    const WebRect& window_rect,
    const WebRect& clip_rect,
    const WebVector<WebRect>& cut_outs_rects,
    bool is_visible) {
  int old_width = width();
  int old_height = height();
  plugin_rect_ = window_rect;
  if (!HasGuestInstanceID())
    return;

  // In AutoSize mode, guests don't care when the BrowserPlugin container is
  // resized. If |!resize_ack_received_|, then we are still waiting on a
  // previous resize to be ACK'ed and so we don't issue additional resizes
  // until the previous one is ACK'ed.
  // TODO(mthiesse): Assess the performance of calling GetAutoSizeAttribute() on
  // resize.
  if (!resize_ack_received_ ||
      (old_width == window_rect.width && old_height == window_rect.height) ||
      GetAutoSizeAttribute()) {
    // Let the browser know about the updated view rect.
    browser_plugin_manager()->Send(new BrowserPluginHostMsg_UpdateGeometry(
        render_view_routing_id_, guest_instance_id_, plugin_rect_));
    return;
  }

  BrowserPluginHostMsg_ResizeGuest_Params params;
  PopulateResizeGuestParameters(&params, plugin_rect());
  resize_ack_received_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ResizeGuest(
      render_view_routing_id_,
      guest_instance_id_,
      params));
}

void BrowserPlugin::SwapDamageBuffers() {
  current_damage_buffer_.reset(pending_damage_buffer_.release());
  resize_ack_received_ = true;
}

void BrowserPlugin::PopulateResizeGuestParameters(
    BrowserPluginHostMsg_ResizeGuest_Params* params,
    const gfx::Rect& view_rect) {
  params->size_changed = true;
  params->view_rect = view_rect;
  params->scale_factor = GetDeviceScaleFactor();
  if (last_device_scale_factor_ != params->scale_factor){
    params->repaint = true;
    last_device_scale_factor_ = params->scale_factor;
  }

  // In HW compositing mode, we do not need a damage buffer.
  if (compositing_enabled_)
    return;

  const size_t stride = skia::PlatformCanvasStrideForWidth(view_rect.width());
  // Make sure the size of the damage buffer is at least four bytes so that we
  // can fit in a magic word to verify that the memory is shared correctly.
  size_t size =
      std::max(sizeof(unsigned int),
               static_cast<size_t>(view_rect.height() *
                                   stride *
                                   GetDeviceScaleFactor() *
                                   GetDeviceScaleFactor()));

  params->damage_buffer_size = size;
  pending_damage_buffer_.reset(
      CreateDamageBuffer(size, &params->damage_buffer_handle));
  if (!pending_damage_buffer_)
    NOTREACHED();
  params->damage_buffer_sequence_id = ++damage_buffer_sequence_id_;
}

void BrowserPlugin::GetDamageBufferWithSizeParams(
    BrowserPluginHostMsg_AutoSize_Params* auto_size_params,
    BrowserPluginHostMsg_ResizeGuest_Params* resize_guest_params) {
  if (auto_size_params)
    PopulateAutoSizeParameters(auto_size_params, GetAutoSizeAttribute());
  gfx::Size view_size = (auto_size_params && auto_size_params->enable) ?
      auto_size_params->max_size : gfx::Size(width(), height());
  if (view_size.IsEmpty())
    return;
  resize_ack_received_ = false;
  gfx::Rect view_rect = gfx::Rect(plugin_rect_.origin(), view_size);
  PopulateResizeGuestParameters(resize_guest_params, view_rect);
}

#if defined(OS_POSIX)
base::SharedMemory* BrowserPlugin::CreateDamageBuffer(
    const size_t size,
    base::SharedMemoryHandle* damage_buffer_handle) {
  scoped_ptr<base::SharedMemory> shared_buf(
      content::RenderThread::Get()->HostAllocateSharedMemoryBuffer(
          size).release());

  if (shared_buf) {
    if (shared_buf->Map(size)) {
      // Insert the magic word.
      *static_cast<unsigned int*>(shared_buf->memory()) = 0xdeadbeef;
      shared_buf->ShareToProcess(base::GetCurrentProcessHandle(),
                                 damage_buffer_handle);
      return shared_buf.release();
    }
  }
  NOTREACHED();
  return NULL;
}
#elif defined(OS_WIN)
base::SharedMemory* BrowserPlugin::CreateDamageBuffer(
    const size_t size,
    base::SharedMemoryHandle* damage_buffer_handle) {
  scoped_ptr<base::SharedMemory> shared_buf(new base::SharedMemory());

  if (!shared_buf->CreateAndMapAnonymous(size)) {
    NOTREACHED() << "Buffer allocation failed";
    return NULL;
  }

  // Insert the magic word.
  *static_cast<unsigned int*>(shared_buf->memory()) = 0xdeadbeef;
  if (shared_buf->ShareToProcess(base::GetCurrentProcessHandle(),
                                 damage_buffer_handle))
    return shared_buf.release();
  NOTREACHED();
  return NULL;
}
#endif

void BrowserPlugin::updateFocus(bool focused) {
  if (plugin_focused_ == focused)
    return;

  bool old_guest_focus_state = ShouldGuestBeFocused();
  plugin_focused_ = focused;

  if (ShouldGuestBeFocused() != old_guest_focus_state)
    UpdateGuestFocusState();
}

void BrowserPlugin::updateVisibility(bool visible) {
  if (visible_ == visible)
    return;

  visible_ = visible;
  if (!HasGuestInstanceID())
    return;

  if (compositing_helper_.get())
    compositing_helper_->UpdateVisibility(visible);

  browser_plugin_manager()->Send(new BrowserPluginHostMsg_SetVisibility(
      render_view_routing_id_,
      guest_instance_id_,
      visible));
}

bool BrowserPlugin::acceptsInputEvents() {
  return true;
}

bool BrowserPlugin::handleInputEvent(const WebKit::WebInputEvent& event,
                                     WebKit::WebCursorInfo& cursor_info) {
  if (guest_crashed_ || !HasGuestInstanceID())
    return false;

  if (event.type == WebKit::WebInputEvent::ContextMenu)
    return true;

  const WebKit::WebInputEvent* modified_event = &event;
  scoped_ptr<WebKit::WebTouchEvent> touch_event;
  // WebKit gives BrowserPlugin a list of touches that are down, but the browser
  // process expects a list of all touches. We modify the TouchEnd event here to
  // match these expectations.
  if (event.type == WebKit::WebInputEvent::TouchEnd) {
    const WebKit::WebTouchEvent* orig_touch_event =
        static_cast<const WebKit::WebTouchEvent*>(&event);
    touch_event.reset(new WebKit::WebTouchEvent());
    memcpy(touch_event.get(), orig_touch_event, sizeof(WebKit::WebTouchEvent));
    if (touch_event->changedTouchesLength > 0) {
      memcpy(&touch_event->touches[touch_event->touchesLength],
             &touch_event->changedTouches,
            touch_event->changedTouchesLength * sizeof(WebKit::WebTouchPoint));
    }
    touch_event->touchesLength += touch_event->changedTouchesLength;
    modified_event = touch_event.get();
  }

  if (WebKit::WebInputEvent::isKeyboardEventType(event.type) &&
      !edit_commands_.empty()) {
    browser_plugin_manager()->Send(
        new BrowserPluginHostMsg_SetEditCommandsForNextKeyEvent(
            render_view_routing_id_,
            guest_instance_id_,
            edit_commands_));
    edit_commands_.clear();
  }

  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(render_view_routing_id_,
                                                guest_instance_id_,
                                                plugin_rect_,
                                                modified_event));
  GetWebKitCursorInfo(cursor_, &cursor_info);
  return true;
}

bool BrowserPlugin::handleDragStatusUpdate(WebKit::WebDragStatus drag_status,
                                           const WebKit::WebDragData& drag_data,
                                           WebKit::WebDragOperationsMask mask,
                                           const WebKit::WebPoint& position,
                                           const WebKit::WebPoint& screen) {
  if (guest_crashed_ || !HasGuestInstanceID())
    return false;
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_DragStatusUpdate(
        render_view_routing_id_,
        guest_instance_id_,
        drag_status,
        DropDataBuilder::Build(drag_data),
        mask,
        position));
  return true;
}

void BrowserPlugin::didReceiveResponse(
    const WebKit::WebURLResponse& response) {
}

void BrowserPlugin::didReceiveData(const char* data, int data_length) {
}

void BrowserPlugin::didFinishLoading() {
}

void BrowserPlugin::didFailLoading(const WebKit::WebURLError& error) {
}

void BrowserPlugin::didFinishLoadingFrameRequest(const WebKit::WebURL& url,
                                                 void* notify_data) {
}

void BrowserPlugin::didFailLoadingFrameRequest(
    const WebKit::WebURL& url,
    void* notify_data,
    const WebKit::WebURLError& error) {
}

bool BrowserPlugin::executeEditCommand(const WebKit::WebString& name) {
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_ExecuteEditCommand(
      render_view_routing_id_,
      guest_instance_id_,
      name.utf8()));

  // BrowserPlugin swallows edit commands.
  return true;
}

bool BrowserPlugin::executeEditCommand(const WebKit::WebString& name,
                                       const WebKit::WebString& value) {
  edit_commands_.push_back(EditCommand(name.utf8(), value.utf8()));
  // BrowserPlugin swallows edit commands.
  return true;
}

void BrowserPlugin::OnLockMouseACK(bool succeeded) {
  mouse_locked_ = succeeded;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_LockMouse_ACK(
      render_view_routing_id_,
      guest_instance_id_,
      succeeded));
}

void BrowserPlugin::OnMouseLockLost() {
  mouse_locked_ = false;
  browser_plugin_manager()->Send(new BrowserPluginHostMsg_UnlockMouse_ACK(
      render_view_routing_id_,
      guest_instance_id_));
}

bool BrowserPlugin::HandleMouseLockedInputEvent(
    const WebKit::WebMouseEvent& event) {
  browser_plugin_manager()->Send(
      new BrowserPluginHostMsg_HandleInputEvent(render_view_routing_id_,
                                                guest_instance_id_,
                                                plugin_rect_,
                                                &event));
  return true;
}

}  // namespace content
