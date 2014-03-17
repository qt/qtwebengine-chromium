// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/nacl/renderer/ppb_nacl_private_impl.h"

#ifndef DISABLE_NACL

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "components/nacl/common/nacl_host_messages.h"
#include "components/nacl/common/nacl_types.h"
#include "components/nacl/renderer/pnacl_translation_resource_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/sandbox_init.h"
#include "content/public/renderer/pepper_plugin_instance.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/renderer_ppapi_host.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/private/pp_file_handle.h"
#include "ppapi/native_client/src/trusted/plugin/nacl_entry_points.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/web/WebDOMResourceProgressEvent.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebElement.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebPluginContainer.h"
#include "third_party/WebKit/public/web/WebView.h"
#include "v8/include/v8.h"

namespace {

base::LazyInstance<scoped_refptr<PnaclTranslationResourceHost> >
    g_pnacl_resource_host = LAZY_INSTANCE_INITIALIZER;

static bool InitializePnaclResourceHost() {
  // Must run on the main thread.
  content::RenderThread* render_thread = content::RenderThread::Get();
  if (!render_thread)
    return false;
  if (!g_pnacl_resource_host.Get()) {
    g_pnacl_resource_host.Get() = new PnaclTranslationResourceHost(
        render_thread->GetIOMessageLoopProxy());
    render_thread->AddFilter(g_pnacl_resource_host.Get());
  }
  return true;
}

struct InstanceInfo {
  InstanceInfo() : plugin_pid(base::kNullProcessId), plugin_child_id(0) {}
  GURL url;
  ppapi::PpapiPermissions permissions;
  base::ProcessId plugin_pid;
  int plugin_child_id;
  IPC::ChannelHandle channel_handle;
};

typedef std::map<PP_Instance, InstanceInfo> InstanceInfoMap;

base::LazyInstance<InstanceInfoMap> g_instance_info =
    LAZY_INSTANCE_INITIALIZER;

static int GetRoutingID(PP_Instance instance) {
  // Check that we are on the main renderer thread.
  DCHECK(content::RenderThread::Get());
  content::RendererPpapiHost *host =
      content::RendererPpapiHost::GetForPPInstance(instance);
  if (!host)
    return 0;
  return host->GetRoutingIDForWidget(instance);
}

// Launch NaCl's sel_ldr process.
PP_ExternalPluginResult LaunchSelLdr(PP_Instance instance,
                                     const char* alleged_url,
                                     PP_Bool uses_irt,
                                     PP_Bool uses_ppapi,
                                     PP_Bool enable_ppapi_dev,
                                     PP_Bool enable_dyncode_syscalls,
                                     PP_Bool enable_exception_handling,
                                     PP_Bool enable_crash_throttling,
                                     void* imc_handle,
                                     struct PP_Var* error_message) {
  nacl::FileDescriptor result_socket;
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *error_message = PP_MakeUndefined();
  int routing_id = 0;
  // If the nexe uses ppapi APIs, we need a routing ID.
  // To get the routing ID, we must be on the main thread.
  // Some nexes do not use ppapi and launch from the background thread,
  // so those nexes can skip finding a routing_id.
  if (uses_ppapi) {
    routing_id = GetRoutingID(instance);
    if (!routing_id)
      return PP_EXTERNAL_PLUGIN_FAILED;
  }

  InstanceInfo instance_info;
  instance_info.url = GURL(alleged_url);

  uint32_t perm_bits = ppapi::PERMISSION_NONE;
  // Conditionally block 'Dev' interfaces. We do this for the NaCl process, so
  // it's clearer to developers when they are using 'Dev' inappropriately. We
  // must also check on the trusted side of the proxy.
  if (enable_ppapi_dev)
    perm_bits |= ppapi::PERMISSION_DEV;
  instance_info.permissions =
      ppapi::PpapiPermissions::GetForCommandLine(perm_bits);
  std::string error_message_string;
  nacl::NaClLaunchResult launch_result;

  if (!sender->Send(new NaClHostMsg_LaunchNaCl(
          nacl::NaClLaunchParams(instance_info.url.spec(),
                                 routing_id,
                                 perm_bits,
                                 PP_ToBool(uses_irt),
                                 PP_ToBool(enable_dyncode_syscalls),
                                 PP_ToBool(enable_exception_handling),
                                 PP_ToBool(enable_crash_throttling)),
          &launch_result,
          &error_message_string))) {
    return PP_EXTERNAL_PLUGIN_FAILED;
  }
  if (!error_message_string.empty()) {
    *error_message = ppapi::StringVar::StringToPPVar(error_message_string);
    return PP_EXTERNAL_PLUGIN_FAILED;
  }
  result_socket = launch_result.imc_channel_handle;
  instance_info.channel_handle = launch_result.ipc_channel_handle;
  instance_info.plugin_pid = launch_result.plugin_pid;
  instance_info.plugin_child_id = launch_result.plugin_child_id;
  // Don't save instance_info if channel handle is invalid.
  bool invalid_handle = instance_info.channel_handle.name.empty();
#if defined(OS_POSIX)
  if (!invalid_handle)
    invalid_handle = (instance_info.channel_handle.socket.fd == -1);
#endif
  if (!invalid_handle)
    g_instance_info.Get()[instance] = instance_info;

  *(static_cast<NaClHandle*>(imc_handle)) =
      nacl::ToNativeHandle(result_socket);

  return PP_EXTERNAL_PLUGIN_OK;
}

PP_ExternalPluginResult StartPpapiProxy(PP_Instance instance) {
  InstanceInfoMap& map = g_instance_info.Get();
  InstanceInfoMap::iterator it = map.find(instance);
  if (it == map.end()) {
    DLOG(ERROR) << "Could not find instance ID";
    return PP_EXTERNAL_PLUGIN_FAILED;
  }
  InstanceInfo instance_info = it->second;
  map.erase(it);

  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    DLOG(ERROR) << "GetInstance() failed";
    return PP_EXTERNAL_PLUGIN_ERROR_MODULE;
  }

  return plugin_instance->SwitchToOutOfProcessProxy(
      base::FilePath().AppendASCII(instance_info.url.spec()),
      instance_info.permissions,
      instance_info.channel_handle,
      instance_info.plugin_pid,
      instance_info.plugin_child_id);
}

int UrandomFD(void) {
#if defined(OS_POSIX)
  return base::GetUrandomFD();
#else
  return -1;
#endif
}

PP_Bool Are3DInterfacesDisabled() {
  return PP_FromBool(CommandLine::ForCurrentProcess()->HasSwitch(
                         switches::kDisable3DAPIs));
}

int32_t BrokerDuplicateHandle(PP_FileHandle source_handle,
                              uint32_t process_id,
                              PP_FileHandle* target_handle,
                              uint32_t desired_access,
                              uint32_t options) {
#if defined(OS_WIN)
  return content::BrokerDuplicateHandle(source_handle, process_id,
                                        target_handle, desired_access,
                                        options);
#else
  return 0;
#endif
}

PP_FileHandle GetReadonlyPnaclFD(const char* filename) {
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_GetReadonlyPnaclFD(
          std::string(filename),
          &out_fd))) {
    return base::kInvalidPlatformFileValue;
  }
  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }
  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
}

PP_FileHandle CreateTemporaryFile(PP_Instance instance) {
  IPC::PlatformFileForTransit transit_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  if (!sender->Send(new NaClHostMsg_NaClCreateTemporaryFile(
          &transit_fd))) {
    return base::kInvalidPlatformFileValue;
  }

  if (transit_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle = IPC::PlatformFileForTransitToPlatformFile(
      transit_fd);
  return handle;
}

int32_t GetNexeFd(PP_Instance instance,
                  const char* pexe_url,
                  uint32_t abi_version,
                  uint32_t opt_level,
                  const char* last_modified,
                  const char* etag,
                  PP_Bool has_no_store_header,
                  PP_Bool* is_hit,
                  PP_FileHandle* handle,
                  struct PP_CompletionCallback callback) {
  ppapi::thunk::EnterInstance enter(instance, callback);
  if (enter.failed())
    return enter.retval();
  if (!pexe_url || !last_modified || !etag || !is_hit || !handle)
    return enter.SetResult(PP_ERROR_BADARGUMENT);
  if (!InitializePnaclResourceHost())
    return enter.SetResult(PP_ERROR_FAILED);

  base::Time last_modified_time;
  // If FromString fails, it doesn't touch last_modified_time and we just send
  // the default-constructed null value.
  base::Time::FromString(last_modified, &last_modified_time);

  nacl::PnaclCacheInfo cache_info;
  cache_info.pexe_url = GURL(pexe_url);
  cache_info.abi_version = abi_version;
  cache_info.opt_level = opt_level;
  cache_info.last_modified = last_modified_time;
  cache_info.etag = std::string(etag);
  cache_info.has_no_store_header = PP_ToBool(has_no_store_header);

  g_pnacl_resource_host.Get()->RequestNexeFd(
      GetRoutingID(instance),
      instance,
      cache_info,
      is_hit,
      handle,
      enter.callback());

  return enter.SetResult(PP_OK_COMPLETIONPENDING);
}

void ReportTranslationFinished(PP_Instance instance, PP_Bool success) {
  // If the resource host isn't initialized, don't try to do that here.
  // Just return because something is already very wrong.
  if (g_pnacl_resource_host.Get() == NULL)
    return;
  g_pnacl_resource_host.Get()->ReportTranslationFinished(instance, success);
}

PP_ExternalPluginResult ReportNaClError(PP_Instance instance,
                              PP_NaClError error_id) {
  IPC::Sender* sender = content::RenderThread::Get();

  if (!sender->Send(
          new NaClHostMsg_NaClErrorStatus(
              // TODO(dschuff): does this enum need to be sent as an int,
              // or is it safe to include the appropriate headers in
              // render_messages.h?
              GetRoutingID(instance), static_cast<int>(error_id)))) {
    return PP_EXTERNAL_PLUGIN_FAILED;
  }
  return PP_EXTERNAL_PLUGIN_OK;
}

PP_FileHandle OpenNaClExecutable(PP_Instance instance,
                                 const char* file_url,
                                 uint64_t* nonce_lo,
                                 uint64_t* nonce_hi) {
  IPC::PlatformFileForTransit out_fd = IPC::InvalidPlatformFileForTransit();
  IPC::Sender* sender = content::RenderThread::Get();
  DCHECK(sender);
  *nonce_lo = 0;
  *nonce_hi = 0;
  base::FilePath file_path;
  if (!sender->Send(
      new NaClHostMsg_OpenNaClExecutable(GetRoutingID(instance),
                                               GURL(file_url),
                                               &out_fd,
                                               nonce_lo,
                                               nonce_hi))) {
    return base::kInvalidPlatformFileValue;
  }

  if (out_fd == IPC::InvalidPlatformFileForTransit()) {
    return base::kInvalidPlatformFileValue;
  }

  base::PlatformFile handle =
      IPC::PlatformFileForTransitToPlatformFile(out_fd);
  return handle;
}

blink::WebString EventTypeToString(PP_NaClEventType event_type) {
  switch (event_type) {
    case PP_NACL_EVENT_LOADSTART:
      return blink::WebString::fromUTF8("loadstart");
    case PP_NACL_EVENT_PROGRESS:
      return blink::WebString::fromUTF8("progress");
    case PP_NACL_EVENT_ERROR:
      return blink::WebString::fromUTF8("error");
    case PP_NACL_EVENT_ABORT:
      return blink::WebString::fromUTF8("abort");
    case PP_NACL_EVENT_LOAD:
      return blink::WebString::fromUTF8("load");
    case PP_NACL_EVENT_LOADEND:
      return blink::WebString::fromUTF8("loadend");
    case PP_NACL_EVENT_CRASH:
      return blink::WebString::fromUTF8("crash");
  }
  NOTIMPLEMENTED();
  return blink::WebString();
}

void DispatchEvent(PP_Instance instance,
                   PP_NaClEventType event_type,
                   struct PP_Var resource_url,
                   PP_Bool length_is_computable,
                   uint64_t loaded_bytes,
                   uint64_t total_bytes) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  if (!plugin_instance) {
    NOTREACHED();
    return;
  }
  blink::WebPluginContainer* container = plugin_instance->GetContainer();
  // It's possible that container() is NULL if the plugin has been removed from
  // the DOM (but the PluginInstance is not destroyed yet).
  if (!container)
    return;
  blink::WebFrame* frame = container->element().document().frame();
  if (!frame)
    return;
  v8::HandleScope handle_scope(plugin_instance->GetIsolate());
  v8::Local<v8::Context> context(
      plugin_instance->GetIsolate()->GetCurrentContext());
  if (context.IsEmpty()) {
    // If there's no JavaScript on the stack, we have to make a new Context.
    context = v8::Context::New(plugin_instance->GetIsolate());
  }
  v8::Context::Scope context_scope(context);

  ppapi::StringVar* url_var = ppapi::StringVar::FromPPVar(resource_url);
  if (url_var) {
    blink::WebString url_string = blink::WebString::fromUTF8(
        url_var->value().data(), url_var->value().size());
    blink::WebDOMResourceProgressEvent event(EventTypeToString(event_type),
                                              PP_ToBool(length_is_computable),
                                              loaded_bytes,
                                              total_bytes,
                                              url_string);
    container->element().dispatchEvent(event);
  } else {
    blink::WebDOMProgressEvent event(EventTypeToString(event_type),
                                      PP_ToBool(length_is_computable),
                                      loaded_bytes,
                                      total_bytes);
    container->element().dispatchEvent(event);
  }
}

void SetReadOnlyProperty(PP_Instance instance,
                         struct PP_Var key,
                         struct PP_Var value) {
  content::PepperPluginInstance* plugin_instance =
      content::PepperPluginInstance::Get(instance);
  plugin_instance->SetEmbedProperty(key, value);
}

const PPB_NaCl_Private nacl_interface = {
  &LaunchSelLdr,
  &StartPpapiProxy,
  &UrandomFD,
  &Are3DInterfacesDisabled,
  &BrokerDuplicateHandle,
  &GetReadonlyPnaclFD,
  &CreateTemporaryFile,
  &GetNexeFd,
  &ReportTranslationFinished,
  &ReportNaClError,
  &OpenNaClExecutable,
  &DispatchEvent,
  &SetReadOnlyProperty
};

}  // namespace

namespace nacl {

const PPB_NaCl_Private* GetNaClPrivateInterface() {
  return &nacl_interface;
}

}  // namespace nacl

#endif  // DISABLE_NACL
