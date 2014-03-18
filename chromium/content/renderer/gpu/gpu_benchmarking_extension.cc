// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/gpu_benchmarking_extension.h"

#include <string>

#include "base/base64.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "cc/layers/layer.h"
#include "content/common/browser_rendering_stats.h"
#include "content/common/gpu/gpu_rendering_stats.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_pinch_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/common/input/synthetic_tap_gesture_params.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/v8_value_converter.h"
#include "content/renderer/gpu/render_widget_compositor.h"
#include "content/renderer/render_thread_impl.h"
#include "content/renderer/render_view_impl.h"
#include "content/renderer/skia_benchmarking_extension.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebImageCache.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkGraphics.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/gfx/codec/png_codec.h"
#include "v8/include/v8.h"
#include "webkit/renderer/compositor_bindings/web_rendering_stats_impl.h"

using blink::WebCanvas;
using blink::WebFrame;
using blink::WebImageCache;
using blink::WebPrivatePtr;
using blink::WebRenderingStatsImpl;
using blink::WebSize;
using blink::WebView;

const char kGpuBenchmarkingExtensionName[] = "v8/GpuBenchmarking";

static SkData* EncodeBitmapToData(size_t* offset, const SkBitmap& bm) {
    SkPixelRef* pr = bm.pixelRef();
    if (pr != NULL) {
        SkData* data = pr->refEncodedData();
        if (data != NULL) {
            *offset = bm.pixelRefOffset();
            return data;
        }
    }
    std::vector<unsigned char> vector;
    if (gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &vector)) {
        return SkData::NewWithCopy(&vector.front() , vector.size());
    }
    return NULL;
}

namespace {

class SkPictureSerializer {
 public:
  explicit SkPictureSerializer(const base::FilePath& dirpath)
      : dirpath_(dirpath),
        layer_id_(0) {
    // Let skia register known effect subclasses. This basically enables
    // reflection on those subclasses required for picture serialization.
    content::SkiaBenchmarkingExtension::InitSkGraphics();
  }

  // Recursively serializes the layer tree.
  // Each layer in the tree is serialized into a separate skp file
  // in the given directory.
  void Serialize(const cc::Layer* layer) {
    const cc::LayerList& children = layer->children();
    for (size_t i = 0; i < children.size(); ++i) {
      Serialize(children[i].get());
    }

    skia::RefPtr<SkPicture> picture = layer->GetPicture();
    if (!picture)
      return;

    // Serialize picture to file.
    // TODO(alokp): Note that for this to work Chrome needs to be launched with
    // --no-sandbox command-line flag. Get rid of this limitation.
    // CRBUG: 139640.
    std::string filename = "layer_" + base::IntToString(layer_id_++) + ".skp";
    std::string filepath = dirpath_.AppendASCII(filename).MaybeAsASCII();
    DCHECK(!filepath.empty());
    SkFILEWStream file(filepath.c_str());
    DCHECK(file.isValid());
    picture->serialize(&file, &EncodeBitmapToData);
  }

 private:
  base::FilePath dirpath_;
  int layer_id_;
};

class RenderingStatsEnumerator : public cc::RenderingStats::Enumerator {
 public:
  RenderingStatsEnumerator(v8::Isolate* isolate,
                           v8::Handle<v8::Object> stats_object)
      : isolate(isolate), stats_object(stats_object) {}

  virtual void AddInt64(const char* name, int64 value) OVERRIDE {
    stats_object->Set(v8::String::NewFromUtf8(isolate, name),
                      v8::Number::New(isolate, value));
  }

  virtual void AddDouble(const char* name, double value) OVERRIDE {
    stats_object->Set(v8::String::NewFromUtf8(isolate, name),
                      v8::Number::New(isolate, value));
  }

  virtual void AddInt(const char* name, int value) OVERRIDE {
    stats_object->Set(v8::String::NewFromUtf8(isolate, name),
                      v8::Integer::New(value));
  }

  virtual void AddTimeDeltaInSecondsF(const char* name,
                                      const base::TimeDelta& value) OVERRIDE {
    stats_object->Set(v8::String::NewFromUtf8(isolate, name),
                      v8::Number::New(isolate, value.InSecondsF()));
  }

 private:
  v8::Isolate* isolate;
  v8::Handle<v8::Object> stats_object;
};

}  // namespace

namespace content {

namespace {

class CallbackAndContext : public base::RefCounted<CallbackAndContext> {
 public:
  CallbackAndContext(v8::Isolate* isolate,
                     v8::Handle<v8::Function> callback,
                     v8::Handle<v8::Context> context)
      : isolate_(isolate) {
    callback_.Reset(isolate_, callback);
    context_.Reset(isolate_, context);
  }

  v8::Isolate* isolate() {
    return isolate_;
  }

  v8::Handle<v8::Function> GetCallback() {
    return v8::Local<v8::Function>::New(isolate_, callback_);
  }

  v8::Handle<v8::Context> GetContext() {
    return v8::Local<v8::Context>::New(isolate_, context_);
  }

 private:
  friend class base::RefCounted<CallbackAndContext>;

  virtual ~CallbackAndContext() {
    callback_.Reset();
    context_.Reset();
  }

  v8::Isolate* isolate_;
  v8::Persistent<v8::Function> callback_;
  v8::Persistent<v8::Context> context_;
  DISALLOW_COPY_AND_ASSIGN(CallbackAndContext);
};

class GpuBenchmarkingContext {
 public:
  GpuBenchmarkingContext()
      : web_frame_(NULL),
        web_view_(NULL),
        render_view_impl_(NULL),
        compositor_(NULL) {}

  bool Init(bool init_compositor) {
    web_frame_ = WebFrame::frameForCurrentContext();
    if (!web_frame_)
      return false;

    web_view_ = web_frame_->view();
    if (!web_view_) {
      web_frame_ = NULL;
      return false;
    }

    render_view_impl_ = RenderViewImpl::FromWebView(web_view_);
    if (!render_view_impl_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      return false;
    }

    if (!init_compositor)
      return true;

    compositor_ = render_view_impl_->compositor();
    if (!compositor_) {
      web_frame_ = NULL;
      web_view_ = NULL;
      render_view_impl_ = NULL;
      return false;
    }

    return true;
  }

  WebFrame* web_frame() const {
    DCHECK(web_frame_ != NULL);
    return web_frame_;
  }
  WebView* web_view() const {
    DCHECK(web_view_ != NULL);
    return web_view_;
  }
  RenderViewImpl* render_view_impl() const {
    DCHECK(render_view_impl_ != NULL);
    return render_view_impl_;
  }
  RenderWidgetCompositor* compositor() const {
    DCHECK(compositor_ != NULL);
    return compositor_;
  }

 private:
  WebFrame* web_frame_;
  WebView* web_view_;
  RenderViewImpl* render_view_impl_;
  RenderWidgetCompositor* compositor_;

  DISALLOW_COPY_AND_ASSIGN(GpuBenchmarkingContext);
};

}  // namespace

class GpuBenchmarkingWrapper : public v8::Extension {
 public:
  GpuBenchmarkingWrapper() :
      v8::Extension(kGpuBenchmarkingExtensionName,
          "if (typeof(chrome) == 'undefined') {"
          "  chrome = {};"
          "};"
          "if (typeof(chrome.gpuBenchmarking) == 'undefined') {"
          "  chrome.gpuBenchmarking = {};"
          "};"
          "chrome.gpuBenchmarking.setNeedsDisplayOnAllLayers = function() {"
          "  native function SetNeedsDisplayOnAllLayers();"
          "  return SetNeedsDisplayOnAllLayers();"
          "};"
          "chrome.gpuBenchmarking.setRasterizeOnlyVisibleContent = function() {"
          "  native function SetRasterizeOnlyVisibleContent();"
          "  return SetRasterizeOnlyVisibleContent();"
          "};"
          "chrome.gpuBenchmarking.renderingStats = function() {"
          "  native function GetRenderingStats();"
          "  return GetRenderingStats();"
          "};"
          "chrome.gpuBenchmarking.gpuRenderingStats = function() {"
          "  native function GetGpuRenderingStats();"
          "  return GetGpuRenderingStats();"
          "};"
          "chrome.gpuBenchmarking.printToSkPicture = function(dirname) {"
          "  native function PrintToSkPicture();"
          "  return PrintToSkPicture(dirname);"
          "};"
          "chrome.gpuBenchmarking.DEFAULT_INPUT = 0;"
          "chrome.gpuBenchmarking.TOUCH_INPUT = 1;"
          "chrome.gpuBenchmarking.MOUSE_INPUT = 2;"
          "chrome.gpuBenchmarking.smoothScrollBy = "
          "    function(pixels_to_scroll, opt_callback, opt_start_x,"
          "             opt_start_y, opt_gesture_source_type,"
          "             opt_direction, opt_speed_in_pixels_s) {"
          "  pixels_to_scroll = pixels_to_scroll || 0;"
          "  callback = opt_callback || function() { };"
          "  gesture_source_type = opt_gesture_source_type ||"
          "      chrome.gpuBenchmarking.DEFAULT_INPUT;"
          "  direction = opt_direction || 'down';"
          "  speed_in_pixels_s = opt_speed_in_pixels_s || 800;"
          "  native function BeginSmoothScroll();"
          "  return BeginSmoothScroll(pixels_to_scroll, callback,"
          "                           gesture_source_type, direction,"
          "                           speed_in_pixels_s, true,"
          "                           opt_start_x, opt_start_y);"
          "};"
          "chrome.gpuBenchmarking.smoothScrollBySendsTouch = function() {"
          "  native function SmoothScrollSendsTouch();"
          "  return SmoothScrollSendsTouch();"
          "};"
          "chrome.gpuBenchmarking.swipe = "
          "    function(direction, distance, opt_callback,"
          "             opt_start_x, opt_start_y,"
          "             opt_speed_in_pixels_s) {"
          "  direction = direction || 'up';"
          "  distance = distance || 0;"
          "  callback = opt_callback || function() { };"
          "  speed_in_pixels_s = opt_speed_in_pixels_s || 800;"
          "  native function BeginSmoothScroll();"
          "  return BeginSmoothScroll(-distance, callback,"
          "                           chrome.gpuBenchmarking.TOUCH_INPUT,"
          "                           direction, speed_in_pixels_s, false,"
          "                           opt_start_x, opt_start_y);"
          "};"
          "chrome.gpuBenchmarking.pinchBy = "
          "    function(zoom_in, pixels_to_cover, anchor_x, anchor_y,"
          "             opt_callback, opt_relative_pointer_speed_in_pixels_s) {"
          "  callback = opt_callback || function() { };"
          "  relative_pointer_speed_in_pixels_s ="
          "      opt_relative_pointer_speed_in_pixels_s || 800;"
          "  native function BeginPinch();"
          "  return BeginPinch(zoom_in, pixels_to_cover,"
          "                    anchor_x, anchor_y, callback,"
          "                    relative_pointer_speed_in_pixels_s);"
          "};"
          "chrome.gpuBenchmarking.tap = "
          "    function(position_x, position_y, opt_callback, opt_duration_ms,"
          "             opt_gesture_source_type) {"
          "  callback = opt_callback || function() { };"
          "  duration_ms = opt_duration_ms || 0;"
          "  gesture_source_type = opt_gesture_source_type ||"
          "      chrome.gpuBenchmarking.DEFAULT_INPUT;"
          "  native function BeginTap();"
          "  return BeginTap(position_x, position_y, callback, duration_ms,"
          "                  gesture_source_type);"
          "};"
          "chrome.gpuBenchmarking.beginWindowSnapshotPNG = function(callback) {"
          "  native function BeginWindowSnapshotPNG();"
          "  BeginWindowSnapshotPNG(callback);"
          "};"
          "chrome.gpuBenchmarking.clearImageCache = function() {"
          "  native function ClearImageCache();"
          "  ClearImageCache();"
          "};"
          "chrome.gpuBenchmarking.runMicroBenchmark ="
          "    function(name, callback, opt_arguments) {"
          "  arguments = opt_arguments || {};"
          "  native function RunMicroBenchmark();"
          "  return RunMicroBenchmark(name, callback, arguments);"
          "};"
          "chrome.gpuBenchmarking.hasGpuProcess = function() {"
          "  native function HasGpuProcess();"
          "  return HasGpuProcess();"
          "};") {}

  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Handle<v8::String> name) OVERRIDE {
    if (name->Equals(
            v8::String::NewFromUtf8(isolate, "SetNeedsDisplayOnAllLayers")))
      return v8::FunctionTemplate::New(isolate, SetNeedsDisplayOnAllLayers);
    if (name->Equals(
            v8::String::NewFromUtf8(isolate, "SetRasterizeOnlyVisibleContent")))
      return v8::FunctionTemplate::New(isolate, SetRasterizeOnlyVisibleContent);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetRenderingStats")))
      return v8::FunctionTemplate::New(isolate, GetRenderingStats);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "GetGpuRenderingStats")))
      return v8::FunctionTemplate::New(isolate, GetGpuRenderingStats);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "PrintToSkPicture")))
      return v8::FunctionTemplate::New(isolate, PrintToSkPicture);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "BeginSmoothScroll")))
      return v8::FunctionTemplate::New(isolate, BeginSmoothScroll);
    if (name->Equals(
            v8::String::NewFromUtf8(isolate, "SmoothScrollSendsTouch")))
      return v8::FunctionTemplate::New(isolate, SmoothScrollSendsTouch);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "BeginPinch")))
      return v8::FunctionTemplate::New(isolate, BeginPinch);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "BeginTap")))
      return v8::FunctionTemplate::New(isolate, BeginTap);
    if (name->Equals(
            v8::String::NewFromUtf8(isolate, "BeginWindowSnapshotPNG")))
      return v8::FunctionTemplate::New(isolate, BeginWindowSnapshotPNG);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "ClearImageCache")))
      return v8::FunctionTemplate::New(isolate, ClearImageCache);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "RunMicroBenchmark")))
      return v8::FunctionTemplate::New(isolate, RunMicroBenchmark);
    if (name->Equals(v8::String::NewFromUtf8(isolate, "HasGpuProcess")))
      return v8::FunctionTemplate::New(isolate, HasGpuProcess);

    return v8::Handle<v8::FunctionTemplate>();
  }

  static void SetNeedsDisplayOnAllLayers(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    context.compositor()->SetNeedsDisplayOnAllLayers();
  }

  static void SetRasterizeOnlyVisibleContent(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    context.compositor()->SetRasterizeOnlyVisibleContent();
  }

  static void GetRenderingStats(
      const v8::FunctionCallbackInfo<v8::Value>& args) {

    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    WebRenderingStatsImpl stats;
    context.render_view_impl()->GetRenderingStats(stats);

    content::GpuRenderingStats gpu_stats;
    context.render_view_impl()->GetGpuRenderingStats(&gpu_stats);
    BrowserRenderingStats browser_stats;
    context.render_view_impl()->GetBrowserRenderingStats(&browser_stats);
    v8::Handle<v8::Object> stats_object = v8::Object::New();

    RenderingStatsEnumerator enumerator(args.GetIsolate(), stats_object);
    stats.rendering_stats.EnumerateFields(&enumerator);
    gpu_stats.EnumerateFields(&enumerator);
    browser_stats.EnumerateFields(&enumerator);

    args.GetReturnValue().Set(stats_object);
  }

  static void GetGpuRenderingStats(
      const v8::FunctionCallbackInfo<v8::Value>& args) {

    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    content::GpuRenderingStats gpu_stats;
    context.render_view_impl()->GetGpuRenderingStats(&gpu_stats);

    v8::Isolate* isolate = args.GetIsolate();
    v8::Handle<v8::Object> stats_object = v8::Object::New(isolate);
    RenderingStatsEnumerator enumerator(isolate, stats_object);
    gpu_stats.EnumerateFields(&enumerator);

    args.GetReturnValue().Set(stats_object);
  }

  static void PrintToSkPicture(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    if (args.Length() != 1)
      return;

    v8::String::Utf8Value dirname(args[0]);
    if (dirname.length() == 0)
      return;

    GpuBenchmarkingContext context;
    if (!context.Init(true))
      return;

    const cc::Layer* root_layer = context.compositor()->GetRootLayer();
    if (!root_layer)
      return;

    base::FilePath dirpath(
        base::FilePath::StringType(*dirname, *dirname + dirname.length()));
    if (!base::CreateDirectory(dirpath) ||
        !base::PathIsWritable(dirpath)) {
      std::string msg("Path is not writable: ");
      msg.append(dirpath.MaybeAsASCII());
      v8::Isolate* isolate = args.GetIsolate();
      isolate->ThrowException(v8::Exception::Error(v8::String::NewFromUtf8(
          isolate, msg.c_str(), v8::String::kNormalString, msg.length())));
      return;
    }

    SkPictureSerializer serializer(dirpath);
    serializer.Serialize(root_layer);
  }

  static void OnSyntheticGestureCompleted(
      CallbackAndContext* callback_and_context) {
    v8::HandleScope scope(callback_and_context->isolate());
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {
      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 0, NULL);
    }
  }

  static void SmoothScrollSendsTouch(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    // TODO(epenner): Should other platforms emulate touch events?
#if defined(OS_ANDROID) || defined(OS_CHROMEOS)
    args.GetReturnValue().Set(true);
#else
    args.GetReturnValue().Set(false);
#endif
  }

  static void BeginSmoothScroll(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    // The last two arguments can be undefined. We check their validity later.
    int arglen = args.Length();
    if (arglen < 8 ||
        !args[0]->IsNumber() ||
        !args[1]->IsFunction() ||
        !args[2]->IsNumber() ||
        !args[3]->IsString() ||
        !args[4]->IsNumber() ||
        !args[5]->IsBoolean()) {
      args.GetReturnValue().Set(false);
      return;
    }

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[1]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());

    scoped_ptr<SyntheticSmoothScrollGestureParams> gesture_params(
        new SyntheticSmoothScrollGestureParams);

    // Convert coordinates from CSS pixels to density independent pixels (DIPs).
    float page_scale_factor = context.web_view()->pageScaleFactor();

    int gesture_source_type = args[2]->IntegerValue();
    if (gesture_source_type < 0 ||
        gesture_source_type > SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX) {
      args.GetReturnValue().Set(false);
      return;
    }
    gesture_params->gesture_source_type =
        static_cast<SyntheticGestureParams::GestureSourceType>(
            gesture_source_type);

    int distance = args[0]->IntegerValue() * page_scale_factor;
    v8::String::Utf8Value direction(args[3]);
    DCHECK(*direction);
    std::string direction_str(*direction);
    if (direction_str == "down")
      gesture_params->distance.set_y(distance);
    else if (direction_str == "up")
      gesture_params->distance.set_y(-distance);
    else if (direction_str == "right")
      gesture_params->distance.set_x(distance);
    else if (direction_str == "left")
      gesture_params->distance.set_x(-distance);
    else {
      args.GetReturnValue().Set(false);
      return;
    }

    gesture_params->speed_in_pixels_s = args[4]->IntegerValue();
    gesture_params->prevent_fling = args[5]->BooleanValue();

    // Account for the 2 optional arguments, start_x and start_y.
    if (args[6]->IsUndefined() || args[7]->IsUndefined()) {
      blink::WebRect rect = context.render_view_impl()->windowRect();
      gesture_params->anchor.SetPoint(rect.width / 2, rect.height / 2);
    } else if (args[6]->IsNumber() && args[7]->IsNumber()) {
      gesture_params->anchor.SetPoint(
          args[6]->IntegerValue() * page_scale_factor,
          args[7]->IntegerValue() * page_scale_factor);
    } else {
      args.GetReturnValue().Set(false);
      return;
    }

    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    context.render_view_impl()->QueueSyntheticGesture(
        gesture_params.PassAs<SyntheticGestureParams>(),
        base::Bind(&OnSyntheticGestureCompleted,
                   callback_and_context));

    args.GetReturnValue().Set(true);
  }

  static void BeginPinch(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    int arglen = args.Length();
    if (arglen < 6 ||
        !args[0]->IsBoolean() ||
        !args[1]->IsNumber() ||
        !args[2]->IsNumber() ||
        !args[3]->IsNumber() ||
        !args[4]->IsFunction() ||
        !args[5]->IsNumber()) {
      args.GetReturnValue().Set(false);
      return;
    }

    scoped_ptr<SyntheticPinchGestureParams> gesture_params(
        new SyntheticPinchGestureParams);

    // Convert coordinates from CSS pixels to density independent pixels (DIPs).
    float page_scale_factor = context.web_view()->pageScaleFactor();

    gesture_params->zoom_in = args[0]->BooleanValue();
    gesture_params->total_num_pixels_covered =
        args[1]->IntegerValue() * page_scale_factor;
    gesture_params->anchor.SetPoint(
        args[2]->IntegerValue() * page_scale_factor,
        args[3]->IntegerValue() * page_scale_factor);
    gesture_params->relative_pointer_speed_in_pixels_s =
        args[5]->IntegerValue();

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[4]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());


    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    context.render_view_impl()->QueueSyntheticGesture(
        gesture_params.PassAs<SyntheticGestureParams>(),
        base::Bind(&OnSyntheticGestureCompleted,
                   callback_and_context));

    args.GetReturnValue().Set(true);
  }

  static void BeginTap(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    int arglen = args.Length();
    if (arglen < 5 ||
        !args[0]->IsNumber() ||
        !args[1]->IsNumber() ||
        !args[2]->IsFunction() ||
        !args[3]->IsNumber() ||
        !args[4]->IsNumber()) {
      args.GetReturnValue().Set(false);
      return;
    }

    scoped_ptr<SyntheticTapGestureParams> gesture_params(
        new SyntheticTapGestureParams);

    // Convert coordinates from CSS pixels to density independent pixels (DIPs).
    float page_scale_factor = context.web_view()->pageScaleFactor();

    gesture_params->position.SetPoint(
        args[0]->IntegerValue() * page_scale_factor,
        args[1]->IntegerValue() * page_scale_factor);
    gesture_params->duration_ms = args[3]->IntegerValue();

    int gesture_source_type = args[4]->IntegerValue();
    if (gesture_source_type < 0 ||
        gesture_source_type > SyntheticGestureParams::GESTURE_SOURCE_TYPE_MAX) {
      args.GetReturnValue().Set(false);
      return;
    }
    gesture_params->gesture_source_type =
        static_cast<SyntheticGestureParams::GestureSourceType>(
            gesture_source_type);

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[2]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());


    // TODO(nduca): If the render_view_impl is destroyed while the gesture is in
    // progress, we will leak the callback and context. This needs to be fixed,
    // somehow.
    context.render_view_impl()->QueueSyntheticGesture(
        gesture_params.PassAs<SyntheticGestureParams>(),
        base::Bind(&OnSyntheticGestureCompleted,
                   callback_and_context));

    args.GetReturnValue().Set(true);
  }

  static void OnSnapshotCompleted(CallbackAndContext* callback_and_context,
                                  const gfx::Size& size,
                                  const std::vector<unsigned char>& png) {
    v8::Isolate* isolate = callback_and_context->isolate();
    v8::HandleScope scope(isolate);
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {

      v8::Handle<v8::Value> result;

      if(!size.IsEmpty()) {
        v8::Handle<v8::Object> result_object;
        result_object = v8::Object::New(isolate);

        result_object->Set(v8::String::NewFromUtf8(isolate, "width"),
                           v8::Number::New(isolate, size.width()));
        result_object->Set(v8::String::NewFromUtf8(isolate, "height"),
                           v8::Number::New(isolate, size.height()));

        std::string base64_png;
        base::Base64Encode(base::StringPiece(
            reinterpret_cast<const char*>(&*png.begin()), png.size()),
            &base64_png);

        result_object->Set(v8::String::NewFromUtf8(isolate, "data"),
                           v8::String::NewFromUtf8(isolate,
                                                   base64_png.c_str(),
                                                   v8::String::kNormalString,
                                                   base64_png.size()));

        result = result_object;
      } else {
        result = v8::Null(isolate);
      }

      v8::Handle<v8::Value> argv[] = { result };

      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 1, argv);
    }
  }

  static void BeginWindowSnapshotPNG(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(false))
      return;

    if (!args[0]->IsFunction())
      return;

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[0]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());

    context.render_view_impl()->GetWindowSnapshot(
        base::Bind(&OnSnapshotCompleted, callback_and_context));
  }

  static void ClearImageCache(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    WebImageCache::clear();
  }

  static void OnMicroBenchmarkCompleted(
      CallbackAndContext* callback_and_context,
      scoped_ptr<base::Value> result) {
    v8::HandleScope scope(callback_and_context->isolate());
    v8::Handle<v8::Context> context = callback_and_context->GetContext();
    v8::Context::Scope context_scope(context);
    WebFrame* frame = WebFrame::frameForContext(context);
    if (frame) {
      scoped_ptr<V8ValueConverter> converter =
          make_scoped_ptr(V8ValueConverter::create());
      v8::Handle<v8::Value> value = converter->ToV8Value(result.get(), context);
      v8::Handle<v8::Value> argv[] = { value };

      frame->callFunctionEvenIfScriptDisabled(
          callback_and_context->GetCallback(), v8::Object::New(), 1, argv);
    }
  }

  static void RunMicroBenchmark(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuBenchmarkingContext context;
    if (!context.Init(true)) {
      args.GetReturnValue().Set(false);
      return;
    }

    if (args.Length() != 3 ||
        !args[0]->IsString() ||
        !args[1]->IsFunction() ||
        !args[2]->IsObject()) {
      args.GetReturnValue().Set(false);
      return;
    }

    v8::Local<v8::Function> callback_local =
        v8::Local<v8::Function>::Cast(args[1]);

    scoped_refptr<CallbackAndContext> callback_and_context =
        new CallbackAndContext(args.GetIsolate(),
                               callback_local,
                               context.web_frame()->mainWorldScriptContext());

    scoped_ptr<V8ValueConverter> converter =
        make_scoped_ptr(V8ValueConverter::create());
    v8::Handle<v8::Context> v8_context = callback_and_context->GetContext();
    scoped_ptr<base::Value> value =
        make_scoped_ptr(converter->FromV8Value(args[2], v8_context));

    v8::String::Utf8Value benchmark(args[0]);
    DCHECK(*benchmark);
    args.GetReturnValue().Set(context.compositor()->ScheduleMicroBenchmark(
        std::string(*benchmark),
        value.Pass(),
        base::Bind(&OnMicroBenchmarkCompleted, callback_and_context)));
  }

  static void HasGpuProcess(const v8::FunctionCallbackInfo<v8::Value>& args) {
    GpuChannelHost* gpu_channel = RenderThreadImpl::current()->GetGpuChannel();
    args.GetReturnValue().Set(!!gpu_channel);
  }
};

v8::Extension* GpuBenchmarkingExtension::Get() {
  return new GpuBenchmarkingWrapper();
}

}  // namespace content
