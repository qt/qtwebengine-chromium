// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Generating a fingerprint consists of two major steps:
//   (1) Gather all the necessary data.
//   (2) Write it into a protocol buffer.
//
// Step (2) is as simple as it sounds -- it's really just a matter of copying
// data.  Step (1) requires waiting on several asynchronous callbacks, which are
// managed by the FingerprintDataLoader class.

#include "components/autofill/content/browser/risk/fingerprint.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/cpu.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/values.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/font_list_async.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/content_client.h"
#include "content/public/common/geoposition.h"
#include "content/public/common/webplugininfo.h"
#include "gpu/config/gpu_info.h"
#include "third_party/WebKit/public/platform/WebRect.h"
#include "third_party/WebKit/public/platform/WebScreenInfo.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

using blink::WebScreenInfo;

namespace autofill {
namespace risk {

namespace {

const int32 kFingerprinterVersion = 1;

// Maximum amount of time, in seconds, to wait for loading asynchronous
// fingerprint data.
const int kTimeoutSeconds = 4;

// Returns the delta between the local timezone and UTC.
base::TimeDelta GetTimezoneOffset() {
  const base::Time utc = base::Time::Now();

  base::Time::Exploded local;
  utc.LocalExplode(&local);

  return base::Time::FromUTCExploded(local) - utc;
}

// Returns the concatenation of the operating system name and version, e.g.
// "Mac OS X 10.6.8".
std::string GetOperatingSystemVersion() {
  return base::SysInfo::OperatingSystemName() + " " +
      base::SysInfo::OperatingSystemVersion();
}

// Adds the list of |fonts| to the |machine|.
void AddFontsToFingerprint(const base::ListValue& fonts,
                           Fingerprint::MachineCharacteristics* machine) {
  for (base::ListValue::const_iterator it = fonts.begin();
       it != fonts.end(); ++it) {
    // Each item in the list is a two-element list such that the first element
    // is the font family and the second is the font name.
    const base::ListValue* font_description = NULL;
    bool success = (*it)->GetAsList(&font_description);
    DCHECK(success);

    std::string font_name;
    success = font_description->GetString(1, &font_name);
    DCHECK(success);

    machine->add_font(font_name);
  }
}

// Adds the list of |plugins| to the |machine|.
void AddPluginsToFingerprint(const std::vector<content::WebPluginInfo>& plugins,
                             Fingerprint::MachineCharacteristics* machine) {
  for (std::vector<content::WebPluginInfo>::const_iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    Fingerprint::MachineCharacteristics::Plugin* plugin =
        machine->add_plugin();
    plugin->set_name(UTF16ToUTF8(it->name));
    plugin->set_description(UTF16ToUTF8(it->desc));
    for (std::vector<content::WebPluginMimeType>::const_iterator mime_type =
             it->mime_types.begin();
         mime_type != it->mime_types.end(); ++mime_type) {
      plugin->add_mime_type(mime_type->mime_type);
    }
    plugin->set_version(UTF16ToUTF8(it->version));
  }
}

// Adds the list of HTTP accept languages to the |machine|.
void AddAcceptLanguagesToFingerprint(
    const std::string& accept_languages_str,
    Fingerprint::MachineCharacteristics* machine) {
  std::vector<std::string> accept_languages;
  base::SplitString(accept_languages_str, ',', &accept_languages);
  for (std::vector<std::string>::const_iterator it = accept_languages.begin();
       it != accept_languages.end(); ++it) {
    machine->add_requested_language(*it);
  }
}

// This function writes
//   (a) the number of screens,
//   (b) the primary display's screen size,
//   (c) the screen's color depth, and
//   (d) the size of the screen unavailable to web page content,
//       i.e. the Taskbar size on Windows
// into the |machine|.
void AddScreenInfoToFingerprint(const WebScreenInfo& screen_info,
                                Fingerprint::MachineCharacteristics* machine) {
  // TODO(scottmg): NativeScreen maybe wrong. http://crbug.com/133312
  machine->set_screen_count(
      gfx::Screen::GetNativeScreen()->GetNumDisplays());

  const gfx::Size screen_size =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().GetSizeInPixel();
  machine->mutable_screen_size()->set_width(screen_size.width());
  machine->mutable_screen_size()->set_height(screen_size.height());

  machine->set_screen_color_depth(screen_info.depth);

  const gfx::Rect screen_rect(screen_info.rect);
  const gfx::Rect available_rect(screen_info.availableRect);
  const gfx::Rect unavailable_rect =
      gfx::SubtractRects(screen_rect, available_rect);
  machine->mutable_unavailable_screen_size()->set_width(
      unavailable_rect.width());
  machine->mutable_unavailable_screen_size()->set_height(
      unavailable_rect.height());
}

// Writes info about the machine's CPU into the |machine|.
void AddCpuInfoToFingerprint(Fingerprint::MachineCharacteristics* machine) {
  base::CPU cpu;
  machine->mutable_cpu()->set_vendor_name(cpu.vendor_name());
  machine->mutable_cpu()->set_brand(cpu.cpu_brand());
}

// Writes info about the machine's GPU into the |machine|.
void AddGpuInfoToFingerprint(Fingerprint::MachineCharacteristics* machine) {
  const gpu::GPUInfo& gpu_info =
      content::GpuDataManager::GetInstance()->GetGPUInfo();
  if (!gpu_info.finalized)
    return;

  Fingerprint::MachineCharacteristics::Graphics* graphics =
      machine->mutable_graphics_card();
  graphics->set_vendor_id(gpu_info.gpu.vendor_id);
  graphics->set_device_id(gpu_info.gpu.device_id);
  graphics->set_driver_version(gpu_info.driver_version);
  graphics->set_driver_date(gpu_info.driver_date);

  Fingerprint::MachineCharacteristics::Graphics::PerformanceStatistics*
      gpu_performance = graphics->mutable_performance_statistics();
  gpu_performance->set_graphics_score(gpu_info.performance_stats.graphics);
  gpu_performance->set_gaming_score(gpu_info.performance_stats.gaming);
  gpu_performance->set_overall_score(gpu_info.performance_stats.overall);
}

// Waits for geoposition data to be loaded.  Lives on the IO thread.
class GeopositionLoader {
 public:
  // |callback_| will be called on the UI thread with the loaded geoposition,
  // once it is available.
  GeopositionLoader(
      const base::TimeDelta& timeout,
      const base::Callback<void(const content::Geoposition&)>& callback);
  ~GeopositionLoader() {}

 private:
  // Methods to communicate with the GeolocationProvider.
  void OnGotGeoposition(const content::Geoposition& geoposition);

  // The callback that will be called once the geoposition is available.
  // Will be called on the UI thread.
  const base::Callback<void(const content::Geoposition&)> callback_;

  // The callback used as an "observer" of the GeolocationProvider.
  content::GeolocationProvider::LocationUpdateCallback geolocation_callback_;

  // Timer to enforce a maximum timeout before the |callback_| is called, even
  // if the geoposition has not been loaded.
  base::OneShotTimer<GeopositionLoader> timeout_timer_;
};

GeopositionLoader::GeopositionLoader(
    const base::TimeDelta& timeout,
    const base::Callback<void(const content::Geoposition&)>& callback)
  : callback_(callback) {
  timeout_timer_.Start(FROM_HERE, timeout,
                       base::Bind(&GeopositionLoader::OnGotGeoposition,
                                  base::Unretained(this),
                                  content::Geoposition()));

  geolocation_callback_ =
      base::Bind(&GeopositionLoader::OnGotGeoposition, base::Unretained(this));
  content::GeolocationProvider::GetInstance()->AddLocationUpdateCallback(
      geolocation_callback_, false);
}

void GeopositionLoader::OnGotGeoposition(
    const content::Geoposition& geoposition) {
  content::BrowserThread::PostTask(content::BrowserThread::UI, FROM_HERE,
                                   base::Bind(callback_, geoposition));

  // Unregister as an observer, since this class instance might be destroyed
  // after this callback.  Note: It's important to unregister *after* posting
  // the task above.  Unregistering as an observer can have the side-effect of
  // modifying the value of |geoposition|.
  bool removed =
      content::GeolocationProvider::GetInstance()->RemoveLocationUpdateCallback(
          geolocation_callback_);
  DCHECK(removed);

  delete this;
}

// Asynchronously loads the user's current geoposition and calls |callback_| on
// the UI thread with the loaded geoposition, once it is available. Expected to
// be called on the IO thread.
void LoadGeoposition(
    const base::TimeDelta& timeout,
    const base::Callback<void(const content::Geoposition&)>& callback) {
  // The loader is responsible for freeing its own memory.
  new GeopositionLoader(timeout, callback);
}

// Waits for all asynchronous data required for the fingerprint to be loaded,
// then fills out the fingerprint.
class FingerprintDataLoader : public content::GpuDataManagerObserver {
 public:
  FingerprintDataLoader(
      uint64 obfuscated_gaia_id,
      const gfx::Rect& window_bounds,
      const gfx::Rect& content_bounds,
      const WebScreenInfo& screen_info,
      const std::string& version,
      const std::string& charset,
      const std::string& accept_languages,
      const base::Time& install_time,
      const std::string& app_locale,
      const base::TimeDelta& timeout,
      const base::Callback<void(scoped_ptr<Fingerprint>)>& callback);

 private:
  virtual ~FingerprintDataLoader() {}

  // content::GpuDataManagerObserver:
  virtual void OnGpuInfoUpdate() OVERRIDE;

  // Callbacks for asynchronously loaded data.
  void OnGotFonts(scoped_ptr<base::ListValue> fonts);
  void OnGotPlugins(const std::vector<content::WebPluginInfo>& plugins);
  void OnGotGeoposition(const content::Geoposition& geoposition);

  // If all of the asynchronous data has been loaded, calls |callback_| with
  // the fingerprint data.
  void MaybeFillFingerprint();

  // Calls |callback_| with the fingerprint data.
  void FillFingerprint();

  // The GPU data provider.
  // Weak reference because the GpuDataManager class is a singleton.
  content::GpuDataManager* const gpu_data_manager_;

  // Ensures that any observer registrations for the GPU data are cleaned up by
  // the time this object is destroyed.
  ScopedObserver<content::GpuDataManager, FingerprintDataLoader> gpu_observer_;

  // Data that will be passed on to the next loading phase.  See the comment for
  // GetFingerprint() for a description of these variables.
  const uint64 obfuscated_gaia_id_;
  const gfx::Rect window_bounds_;
  const gfx::Rect content_bounds_;
  const WebScreenInfo screen_info_;
  const std::string version_;
  const std::string charset_;
  const std::string accept_languages_;
  const base::Time install_time_;

  // Data that will be loaded asynchronously.
  scoped_ptr<base::ListValue> fonts_;
  std::vector<content::WebPluginInfo> plugins_;
  bool waiting_on_plugins_;
  content::Geoposition geoposition_;

  // Timer to enforce a maximum timeout before the |callback_| is called, even
  // if not all asynchronous data has been loaded.
  base::OneShotTimer<FingerprintDataLoader> timeout_timer_;

  // For invalidating asynchronous callbacks that might arrive after |this|
  // instance is destroyed.
  base::WeakPtrFactory<FingerprintDataLoader> weak_ptr_factory_;

  // The current application locale.
  std::string app_locale_;

  // The callback that will be called once all the data is available.
  base::Callback<void(scoped_ptr<Fingerprint>)> callback_;

  DISALLOW_COPY_AND_ASSIGN(FingerprintDataLoader);
};

FingerprintDataLoader::FingerprintDataLoader(
    uint64 obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const WebScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    const std::string& app_locale,
    const base::TimeDelta& timeout,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback)
    : gpu_data_manager_(content::GpuDataManager::GetInstance()),
      gpu_observer_(this),
      obfuscated_gaia_id_(obfuscated_gaia_id),
      window_bounds_(window_bounds),
      content_bounds_(content_bounds),
      screen_info_(screen_info),
      version_(version),
      charset_(charset),
      accept_languages_(accept_languages),
      install_time_(install_time),
      waiting_on_plugins_(true),
      weak_ptr_factory_(this),
      callback_(callback) {
  DCHECK(!install_time_.is_null());

  timeout_timer_.Start(FROM_HERE, timeout,
                       base::Bind(&FingerprintDataLoader::MaybeFillFingerprint,
                                  weak_ptr_factory_.GetWeakPtr()));

  // Load GPU data if needed.
  if (!gpu_data_manager_->IsCompleteGpuInfoAvailable()) {
    gpu_observer_.Add(gpu_data_manager_);
    gpu_data_manager_->RequestCompleteGpuInfoIfNeeded();
  }

#if defined(ENABLE_PLUGINS)
  // Load plugin data.
  content::PluginService::GetInstance()->GetPlugins(
      base::Bind(&FingerprintDataLoader::OnGotPlugins,
                 weak_ptr_factory_.GetWeakPtr()));
#else
  waiting_on_plugins_ = false;
#endif

  // Load font data.
  content::GetFontListAsync(
      base::Bind(&FingerprintDataLoader::OnGotFonts,
                 weak_ptr_factory_.GetWeakPtr()));

  // Load geolocation data.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&LoadGeoposition,
                 timeout,
                 base::Bind(&FingerprintDataLoader::OnGotGeoposition,
                            weak_ptr_factory_.GetWeakPtr())));
}

void FingerprintDataLoader::OnGpuInfoUpdate() {
  if (!gpu_data_manager_->IsCompleteGpuInfoAvailable())
    return;

  gpu_observer_.Remove(gpu_data_manager_);
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotFonts(scoped_ptr<base::ListValue> fonts) {
  DCHECK(!fonts_);
  fonts_.reset(fonts.release());
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotPlugins(
    const std::vector<content::WebPluginInfo>& plugins) {
  DCHECK(waiting_on_plugins_);
  waiting_on_plugins_ = false;
  plugins_ = plugins;
  MaybeFillFingerprint();
}

void FingerprintDataLoader::OnGotGeoposition(
    const content::Geoposition& geoposition) {
  DCHECK(!geoposition_.Validate());

  geoposition_ = geoposition;
  DCHECK(geoposition_.Validate() ||
         geoposition_.error_code != content::Geoposition::ERROR_CODE_NONE);

  MaybeFillFingerprint();
}

void FingerprintDataLoader::MaybeFillFingerprint() {
  // If all of the data has been loaded, or if the |timeout_timer_| has expired,
  // fill the fingerprint and clean up.
  if (!timeout_timer_.IsRunning() ||
      (gpu_data_manager_->IsCompleteGpuInfoAvailable() &&
       fonts_ &&
       !waiting_on_plugins_ &&
       (geoposition_.Validate() ||
        geoposition_.error_code != content::Geoposition::ERROR_CODE_NONE))) {
    FillFingerprint();
    delete this;
  }
}

void FingerprintDataLoader::FillFingerprint() {
  scoped_ptr<Fingerprint> fingerprint(new Fingerprint);
  Fingerprint::MachineCharacteristics* machine =
      fingerprint->mutable_machine_characteristics();

  machine->set_operating_system_build(GetOperatingSystemVersion());
  // We use the delta between the install time and the Unix epoch, in hours.
  machine->set_browser_install_time_hours(
      (install_time_ - base::Time::UnixEpoch()).InHours());
  machine->set_utc_offset_ms(GetTimezoneOffset().InMilliseconds());
  machine->set_browser_language(app_locale_);
  machine->set_charset(charset_);
  machine->set_user_agent(content::GetUserAgent(GURL()));
  machine->set_ram(base::SysInfo::AmountOfPhysicalMemory());
  machine->set_browser_build(version_);
  machine->set_browser_feature(
      Fingerprint::MachineCharacteristics::FEATURE_REQUEST_AUTOCOMPLETE);
  if (fonts_)
    AddFontsToFingerprint(*fonts_, machine);
  AddPluginsToFingerprint(plugins_, machine);
  AddAcceptLanguagesToFingerprint(accept_languages_, machine);
  AddScreenInfoToFingerprint(screen_info_, machine);
  AddCpuInfoToFingerprint(machine);
  AddGpuInfoToFingerprint(machine);

  // TODO(isherman): Record the user_and_device_name_hash.
  // TODO(isherman): Record the partition size of the hard drives?

  Fingerprint::TransientState* transient_state =
      fingerprint->mutable_transient_state();
  Fingerprint::Dimension* inner_window_size =
      transient_state->mutable_inner_window_size();
  inner_window_size->set_width(content_bounds_.width());
  inner_window_size->set_height(content_bounds_.height());
  Fingerprint::Dimension* outer_window_size =
      transient_state->mutable_outer_window_size();
  outer_window_size->set_width(window_bounds_.width());
  outer_window_size->set_height(window_bounds_.height());

  // TODO(isherman): Record network performance data, which is theoretically
  // available to JS.

  // TODO(isherman): Record more user behavior data.
  if (geoposition_.Validate() &&
      geoposition_.error_code == content::Geoposition::ERROR_CODE_NONE) {
    Fingerprint::UserCharacteristics::Location* location =
        fingerprint->mutable_user_characteristics()->mutable_location();
    location->set_altitude(geoposition_.altitude);
    location->set_latitude(geoposition_.latitude);
    location->set_longitude(geoposition_.longitude);
    location->set_accuracy(geoposition_.accuracy);
    location->set_time_in_ms(
        (geoposition_.timestamp - base::Time::UnixEpoch()).InMilliseconds());
  }

  Fingerprint::Metadata* metadata = fingerprint->mutable_metadata();
  metadata->set_timestamp_ms(
      (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds());
  metadata->set_obfuscated_gaia_id(obfuscated_gaia_id_);
  metadata->set_fingerprinter_version(kFingerprinterVersion);

  callback_.Run(fingerprint.Pass());
}

}  // namespace

namespace internal {

void GetFingerprintInternal(
    uint64 obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const gfx::Rect& content_bounds,
    const blink::WebScreenInfo& screen_info,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    const std::string& app_locale,
    const base::TimeDelta& timeout,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback) {
  // Begin loading all of the data that we need to load asynchronously.
  // This class is responsible for freeing its own memory.
  new FingerprintDataLoader(obfuscated_gaia_id, window_bounds, content_bounds,
                            screen_info, version, charset, accept_languages,
                            install_time, app_locale, timeout, callback);
}

}  // namespace internal

void GetFingerprint(
    uint64 obfuscated_gaia_id,
    const gfx::Rect& window_bounds,
    const content::WebContents& web_contents,
    const std::string& version,
    const std::string& charset,
    const std::string& accept_languages,
    const base::Time& install_time,
    const std::string& app_locale,
    const base::Callback<void(scoped_ptr<Fingerprint>)>& callback) {
  gfx::Rect content_bounds;
  web_contents.GetView()->GetContainerBounds(&content_bounds);

  blink::WebScreenInfo screen_info;
  content::RenderWidgetHostView* host_view =
      web_contents.GetRenderWidgetHostView();
  if (host_view)
    host_view->GetRenderWidgetHost()->GetWebScreenInfo(&screen_info);

  internal::GetFingerprintInternal(
      obfuscated_gaia_id, window_bounds, content_bounds, screen_info, version,
      charset, accept_languages, install_time, app_locale,
      base::TimeDelta::FromSeconds(kTimeoutSeconds), callback);
}

}  // namespace risk
}  // namespace autofill
