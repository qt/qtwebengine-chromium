// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_internals_ui.h"

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "cc/base/switches.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_info.h"
#include "grit/content_resources.h"
#include "third_party/angle_dx11/src/common/version.h"

namespace content {
namespace {

struct GpuFeatureInfo {
  std::string name;
  uint32 blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

WebUIDataSource* CreateGpuHTMLSource() {
  WebUIDataSource* source = WebUIDataSource::Create(kChromeUIGpuHost);

  source->SetJsonPath("strings.js");
  source->AddResourcePath("gpu_internals.js", IDR_GPU_INTERNALS_JS);
  source->SetDefaultResource(IDR_GPU_INTERNALS_HTML);
  return source;
}

base::DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

base::DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    base::Value* value) {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

base::Value* NewStatusValue(const char* name, const char* status) {
  base::DictionaryValue* value = new base::DictionaryValue();
  value->SetString("name", name);
  value->SetString("status", status);
  return value;
}

#if defined(OS_WIN)
// Output DxDiagNode tree as nested array of {description,value} pairs
base::ListValue* DxDiagNodeToList(const gpu::DxDiagNode& node) {
  base::ListValue* list = new base::ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, gpu::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    base::ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}
#endif

std::string GPUDeviceToString(const gpu::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  return base::StringPrintf(
      "VENDOR = %s, DEVICE= %s", vendor.c_str(), device.c_str());
}

base::DictionaryValue* GpuInfoAsDictionaryValue() {
  gpu::GPUInfo gpu_info = GpuDataManagerImpl::GetInstance()->GetGPUInfo();
  base::ListValue* basic_info = new base::ListValue();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "Sandboxed", new base::FundamentalValue(gpu_info.sandboxed)));
  basic_info->Append(NewDescriptionValuePair(
      "GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", new base::FundamentalValue(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable", new base::FundamentalValue(gpu_info.amd_switchable)));
  if (gpu_info.lenovo_dcute) {
    basic_info->Append(NewDescriptionValuePair(
        "Lenovo dCute", new base::FundamentalValue(true)));
  }
  if (gpu_info.display_link_version.IsValid()) {
    basic_info->Append(NewDescriptionValuePair(
        "DisplayLink Version", gpu_info.display_link_version.GetString()));
  }
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
                                             gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("Machine model",
                                             gpu_info.machine_model));
  basic_info->Append(NewDescriptionValuePair("GL version",
                                             gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info.gl_version_string));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info.gl_extensions));
  basic_info->Append(NewDescriptionValuePair("Window system binding vendor",
                                             gpu_info.gl_ws_vendor));
  basic_info->Append(NewDescriptionValuePair("Window system binding version",
                                             gpu_info.gl_ws_version));
  basic_info->Append(NewDescriptionValuePair("Window system binding extensions",
                                             gpu_info.gl_ws_extensions));
  std::string reset_strategy =
      base::StringPrintf("0x%04x", gpu_info.gl_reset_notification_strategy);
  basic_info->Append(NewDescriptionValuePair(
      "Reset notification strategy", reset_strategy));

  base::DictionaryValue* info = new base::DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  base::ListValue* perf_info = new base::ListValue();
  perf_info->Append(NewDescriptionValuePair(
      "Graphics",
      base::StringPrintf("%.1f", gpu_info.performance_stats.graphics)));
  perf_info->Append(NewDescriptionValuePair(
      "Gaming",
      base::StringPrintf("%.1f", gpu_info.performance_stats.gaming)));
  perf_info->Append(NewDescriptionValuePair(
      "Overall",
      base::StringPrintf("%.1f", gpu_info.performance_stats.overall)));
  info->Set("performance_info", perf_info);

  base::Value* dx_info = gpu_info.dx_diagnostics.children.size() ?
    DxDiagNodeToList(gpu_info.dx_diagnostics) :
    base::Value::CreateNullValue();
  info->Set("diagnostics", dx_info);
#endif

  return info;
}

// Determine if accelerated-2d-canvas is supported, which depends on whether
// lose_context could happen.
bool SupportsAccelerated2dCanvas() {
  if (GpuDataManagerImpl::GetInstance()->GetGPUInfo().can_lose_context)
    return false;
  return true;
}

base::Value* GetFeatureStatus() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  GpuDataManagerImpl* manager = GpuDataManagerImpl::GetInstance();
  std::string gpu_access_blocked_reason;
  bool gpu_access_blocked =
      !manager->GpuAccessAllowed(&gpu_access_blocked_reason);

  base::DictionaryValue* status = new base::DictionaryValue();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
      {
          "2d_canvas",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS),
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas) ||
          !SupportsAccelerated2dCanvas(),
          "Accelerated 2D canvas is unavailable: either disabled at the command"
          " line or not supported by the current system.",
          true
      },
      {
          "compositing",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING),
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated compositing has been disabled, either via about:flags or"
          " command line. This adversely affects performance of all hardware"
          " accelerated features.",
          true
      },
      {
          "3d_css",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) ||
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_3D_CSS),
          command_line.HasSwitch(switches::kDisableAcceleratedLayers),
          "Accelerated layers have been disabled at the command line.",
          false
      },
      {
          "css_animation",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING) ||
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_3D_CSS),
          command_line.HasSwitch(cc::switches::kDisableThreadedAnimation) ||
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
          command_line.HasSwitch(switches::kDisableAcceleratedLayers),
          "Accelerated CSS animation has been disabled at the command line.",
          true
      },
      {
          "webgl",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_WEBGL),
          command_line.HasSwitch(switches::kDisableExperimentalWebGL),
          "WebGL has been disabled, either via about:flags or command line.",
          false
      },
      {
          "multisampling",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_MULTISAMPLING),
          command_line.HasSwitch(switches::kDisableGLMultisampling),
          "Multisampling has been disabled, either via about:flags or command"
          " line.",
          false
      },
      {
          "flash_3d",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH3D),
          command_line.HasSwitch(switches::kDisableFlash3d),
          "Using 3d in flash has been disabled, either via about:flags or"
          " command line.",
          false
      },
      {
          "flash_stage3d",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
          command_line.HasSwitch(switches::kDisableFlashStage3d),
          "Using Stage3d in Flash has been disabled, either via about:flags or"
          " command line.",
          false
      },
      {
          "flash_stage3d_baseline",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D_BASELINE) ||
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_FLASH_STAGE3D),
          command_line.HasSwitch(switches::kDisableFlashStage3d),
          "Using Stage3d Baseline profile in Flash has been disabled, either"
          " via about:flags or command line.",
          false
      },
      {
          "texture_sharing",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_TEXTURE_SHARING),
          command_line.HasSwitch(switches::kDisableImageTransportSurface),
          "Sharing textures between processes has been disabled, either via"
          " about:flags or command line.",
          false
      },
      {
          "video_decode",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE),
          command_line.HasSwitch(switches::kDisableAcceleratedVideoDecode),
          "Accelerated video decode has been disabled, either via about:flags"
          " or command line.",
          true
      },
      {
          "video",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_ACCELERATED_VIDEO),
          command_line.HasSwitch(switches::kDisableAcceleratedVideo) ||
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated video presentation has been disabled, either via"
          " about:flags or command line.",
          true
      },
#if defined(OS_CHROMEOS)
      {
          "panel_fitting",
          manager->IsFeatureBlacklisted(gpu::GPU_FEATURE_TYPE_PANEL_FITTING),
          command_line.HasSwitch(switches::kDisablePanelFitting),
          "Panel fitting has been disabled, either via about:flags or command"
          " line.",
          false
      },
#endif
      {
          "force_compositing_mode",
          manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE) &&
          !IsForceCompositingModeEnabled(),
          !IsForceCompositingModeEnabled() &&
          !manager->IsFeatureBlacklisted(
              gpu::GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE),
          "Force compositing mode is off, either disabled at the command"
          " line or not supported by the current system.",
          false
      },
  };
  const size_t kNumFeatures = sizeof(kGpuFeatureInfo) / sizeof(GpuFeatureInfo);

  // Build the feature_status field.
  {
    base::ListValue* feature_status_list = new base::ListValue();

    for (size_t i = 0; i < kNumFeatures; ++i) {
      // force_compositing_mode status is part of the compositing status.
      if (kGpuFeatureInfo[i].name == "force_compositing_mode")
        continue;

      std::string status;
      if (kGpuFeatureInfo[i].disabled) {
        status = "disabled";
        if (kGpuFeatureInfo[i].name == "css_animation") {
          status += "_software_animated";
        } else if (kGpuFeatureInfo[i].name == "raster") {
          if (cc::switches::IsImplSidePaintingEnabled())
            status += "_software_multithreaded";
          else
            status += "_software";
        } else {
          if (kGpuFeatureInfo[i].fallback_to_software)
            status += "_software";
          else
            status += "_off";
        }
      } else if (GpuDataManagerImpl::GetInstance()->ShouldUseSwiftShader()) {
        status = "unavailable_software";
      } else if (kGpuFeatureInfo[i].blocked ||
                 gpu_access_blocked) {
        status = "unavailable";
        if (kGpuFeatureInfo[i].fallback_to_software)
          status += "_software";
        else
          status += "_off";
      } else {
        status = "enabled";
        if (kGpuFeatureInfo[i].name == "webgl" &&
            (command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
             manager->IsFeatureBlacklisted(
                 gpu::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)))
          status += "_readback";
        bool has_thread = IsThreadedCompositingEnabled();
        if (kGpuFeatureInfo[i].name == "compositing") {
          bool force_compositing = IsForceCompositingModeEnabled();
          if (force_compositing)
            status += "_force";
          if (has_thread)
            status += "_threaded";
        }
        if (kGpuFeatureInfo[i].name == "css_animation") {
          if (has_thread)
            status = "accelerated_threaded";
          else
            status = "accelerated";
        }
      }
      // TODO(reveman): Remove this when crbug.com/223286 has been fixed.
      if (kGpuFeatureInfo[i].name == "raster" &&
          cc::switches::IsImplSidePaintingEnabled()) {
        status = "disabled_software_multithreaded";
      }
      feature_status_list->Append(
          NewStatusValue(kGpuFeatureInfo[i].name.c_str(), status.c_str()));
    }
    gpu::GpuSwitchingOption gpu_switching_option =
        GpuDataManagerImpl::GetInstance()->GetGpuSwitchingOption();
    if (gpu_switching_option != gpu::GPU_SWITCHING_OPTION_UNKNOWN) {
      std::string gpu_switching;
      switch (gpu_switching_option) {
      case gpu::GPU_SWITCHING_OPTION_AUTOMATIC:
          gpu_switching = "gpu_switching_automatic";
          break;
      case gpu::GPU_SWITCHING_OPTION_FORCE_DISCRETE:
          gpu_switching = "gpu_switching_force_discrete";
          break;
      case gpu::GPU_SWITCHING_OPTION_FORCE_INTEGRATED:
          gpu_switching = "gpu_switching_force_integrated";
          break;
        default:
          break;
      }
      feature_status_list->Append(
          NewStatusValue("gpu_switching", gpu_switching.c_str()));
    }
    status->Set("featureStatus", feature_status_list);
  }

  // Build the problems list.
  {
    base::ListValue* problem_list = new base::ListValue();
    GpuDataManagerImpl::GetInstance()->GetBlacklistReasons(problem_list);

    if (gpu_access_blocked) {
      base::DictionaryValue* problem = new base::DictionaryValue();
      problem->SetString("description",
          "GPU process was unable to boot: " + gpu_access_blocked_reason);
      problem->Set("crBugs", new base::ListValue());
      problem->Set("webkitBugs", new base::ListValue());
      problem_list->Insert(0, problem);
    }

    for (size_t i = 0; i < kNumFeatures; ++i) {
      if (kGpuFeatureInfo[i].disabled) {
        base::DictionaryValue* problem = new base::DictionaryValue();
        problem->SetString(
            "description", kGpuFeatureInfo[i].disabled_description);
        problem->Set("crBugs", new base::ListValue());
        problem->Set("webkitBugs", new base::ListValue());
        problem_list->Append(problem);
      }
    }

    status->Set("problems", problem_list);
  }

  // Build driver bug workaround list.
  {
    base::ListValue* workaround_list = new base::ListValue();
    GpuDataManagerImpl::GetInstance()->GetDriverBugWorkarounds(workaround_list);
    status->Set("workarounds", workaround_list);
  }

  return status;
}

// This class receives javascript messages from the renderer.
// Note that the WebUI infrastructure runs on the UI thread, therefore all of
// this class's methods are expected to run on the UI thread.
class GpuMessageHandler
    : public WebUIMessageHandler,
      public base::SupportsWeakPtr<GpuMessageHandler>,
      public GpuDataManagerObserver {
 public:
  GpuMessageHandler();
  virtual ~GpuMessageHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // GpuDataManagerObserver implementation.
  virtual void OnGpuInfoUpdate() OVERRIDE;
  virtual void OnGpuSwitching() OVERRIDE;

  // Messages
  void OnBrowserBridgeInitialized(const base::ListValue* list);
  void OnCallAsync(const base::ListValue* list);

  // Submessages dispatched from OnCallAsync
  base::Value* OnRequestClientInfo(const base::ListValue* list);
  base::Value* OnRequestLogMessages(const base::ListValue* list);

 private:
  // True if observing the GpuDataManager (re-attaching as observer would
  // DCHECK).
  bool observing_;

  DISALLOW_COPY_AND_ASSIGN(GpuMessageHandler);
};

////////////////////////////////////////////////////////////////////////////////
//
// GpuMessageHandler
//
////////////////////////////////////////////////////////////////////////////////

GpuMessageHandler::GpuMessageHandler()
    : observing_(false) {
}

GpuMessageHandler::~GpuMessageHandler() {
  GpuDataManagerImpl::GetInstance()->RemoveObserver(this);
}

/* BrowserBridge.callAsync prepends a requestID to these messages. */
void GpuMessageHandler::RegisterMessages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  web_ui()->RegisterMessageCallback("browserBridgeInitialized",
      base::Bind(&GpuMessageHandler::OnBrowserBridgeInitialized,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("callAsync",
      base::Bind(&GpuMessageHandler::OnCallAsync,
                 base::Unretained(this)));
}

void GpuMessageHandler::OnCallAsync(const base::ListValue* args) {
  DCHECK_GE(args->GetSize(), static_cast<size_t>(2));
  // unpack args into requestId, submessage and submessageArgs
  bool ok;
  const base::Value* requestId;
  ok = args->Get(0, &requestId);
  DCHECK(ok);

  std::string submessage;
  ok = args->GetString(1, &submessage);
  DCHECK(ok);

  base::ListValue* submessageArgs = new base::ListValue();
  for (size_t i = 2; i < args->GetSize(); ++i) {
    const base::Value* arg;
    ok = args->Get(i, &arg);
    DCHECK(ok);

    base::Value* argCopy = arg->DeepCopy();
    submessageArgs->Append(argCopy);
  }

  // call the submessage handler
  base::Value* ret = NULL;
  if (submessage == "requestClientInfo") {
    ret = OnRequestClientInfo(submessageArgs);
  } else if (submessage == "requestLogMessages") {
    ret = OnRequestLogMessages(submessageArgs);
  } else {  // unrecognized submessage
    NOTREACHED();
    delete submessageArgs;
    return;
  }
  delete submessageArgs;

  // call BrowserBridge.onCallAsyncReply with result
  if (ret) {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId,
        *ret);
    delete ret;
  } else {
    web_ui()->CallJavascriptFunction("browserBridge.onCallAsyncReply",
        *requestId);
  }
}

void GpuMessageHandler::OnBrowserBridgeInitialized(
    const base::ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Watch for changes in GPUInfo
  if (!observing_)
    GpuDataManagerImpl::GetInstance()->AddObserver(this);
  observing_ = true;

  // Tell GpuDataManager it should have full GpuInfo. If the
  // Gpu process has not run yet, this will trigger its launch.
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();

  // Run callback immediately in case the info is ready and no update in the
  // future.
  OnGpuInfoUpdate();
}

base::Value* GpuMessageHandler::OnRequestClientInfo(
    const base::ListValue* list) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  base::DictionaryValue* dict = new base::DictionaryValue();

  dict->SetString("version", GetContentClient()->GetProduct());
  dict->SetString("command_line",
      CommandLine::ForCurrentProcess()->GetCommandLineString());
  dict->SetString("operating_system",
                  base::SysInfo::OperatingSystemName() + " " +
                  base::SysInfo::OperatingSystemVersion());
  dict->SetString("angle_revision", base::UintToString(BUILD_REVISION));
  dict->SetString("graphics_backend", "Skia");
  dict->SetString("blacklist_version",
      GpuDataManagerImpl::GetInstance()->GetBlacklistVersion());
  dict->SetString("driver_bug_list_version",
      GpuDataManagerImpl::GetInstance()->GetDriverBugListVersion());

  return dict;
}

base::Value* GpuMessageHandler::OnRequestLogMessages(const base::ListValue*) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  return GpuDataManagerImpl::GetInstance()->GetLogMessages();
}

void GpuMessageHandler::OnGpuInfoUpdate() {
  // Get GPU Info.
  scoped_ptr<base::DictionaryValue> gpu_info_val(GpuInfoAsDictionaryValue());

  // Add in blacklisting features
  base::Value* feature_status = GetFeatureStatus();
  if (feature_status)
    gpu_info_val->Set("featureStatus", feature_status);

  // Send GPU Info to javascript.
  web_ui()->CallJavascriptFunction("browserBridge.onGpuInfoUpdate",
      *(gpu_info_val.get()));
}

void GpuMessageHandler::OnGpuSwitching() {
  GpuDataManagerImpl::GetInstance()->RequestCompleteGpuInfoIfNeeded();
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// GpuInternalsUI
//
////////////////////////////////////////////////////////////////////////////////

GpuInternalsUI::GpuInternalsUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(new GpuMessageHandler());

  // Set up the chrome://gpu/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  WebUIDataSource::Add(browser_context, CreateGpuHTMLSource());
}

}  // namespace content
