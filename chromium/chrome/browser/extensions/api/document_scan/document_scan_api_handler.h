// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
#define CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/extensions/api/document_scan.h"
#include "chromeos/crosapi/mojom/document_scan.mojom-forward.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_widget_types.h"

class PrefRegistrySimple;

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class Image;
}  // namespace gfx

namespace extensions {

class Extension;
class ScannerDiscoveryRunner;
class StartScanRunner;

// Handles chrome.documentScan API function calls.
class DocumentScanAPIHandler : public BrowserContextKeyedAPI {
 public:
  using SimpleScanCallback = base::OnceCallback<void(
      std::optional<api::document_scan::ScanResults> scan_results,
      std::optional<std::string> error)>;
  using GetScannerListCallback =
      base::OnceCallback<void(api::document_scan::GetScannerListResponse)>;
  using OpenScannerCallback =
      base::OnceCallback<void(api::document_scan::OpenScannerResponse)>;
  using GetOptionGroupsCallback =
      base::OnceCallback<void(api::document_scan::GetOptionGroupsResponse)>;
  using CloseScannerCallback =
      base::OnceCallback<void(api::document_scan::CloseScannerResponse)>;
  using SetOptionsCallback =
      base::OnceCallback<void(api::document_scan::SetOptionsResponse)>;
  using StartScanCallback =
      base::OnceCallback<void(api::document_scan::StartScanResponse)>;
  using CancelScanCallback =
      base::OnceCallback<void(api::document_scan::CancelScanResponse)>;
  using ReadScanDataCallback =
      base::OnceCallback<void(api::document_scan::ReadScanDataResponse)>;

  static std::unique_ptr<DocumentScanAPIHandler> CreateForTesting(
      content::BrowserContext* browser_context,
      crosapi::mojom::DocumentScan* document_scan);

  explicit DocumentScanAPIHandler(content::BrowserContext* browser_context);
  DocumentScanAPIHandler(const DocumentScanAPIHandler&) = delete;
  DocumentScanAPIHandler& operator=(const DocumentScanAPIHandler&) = delete;
  ~DocumentScanAPIHandler() override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>*
  GetFactoryInstance();

  // Returns the current instance for `browser_context`.
  static DocumentScanAPIHandler* Get(content::BrowserContext* browser_context);

  // Registers the documentScan API preference with the |registry|.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // Replaces the DocumentScan service with a mock.
  void SetDocumentScanForTesting(crosapi::mojom::DocumentScan* document_scan);

  // Scans one page from the first available scanner on the system and passes
  // the result to `callback`.  `mime_types` is a list of MIME types the caller
  // is willing to receive back as the image format.
  void SimpleScan(const std::vector<std::string>& mime_types,
                  SimpleScanCallback callback);

  // If the user approves, gets a list of available scanners that match
  // `filter`.  Explicit approval is obtained through a Chrome dialog or by
  // adding the extension ID to the list of trusted document scan extensions.
  // The result of the denial or the backend call will be passed to `callback`.
  void GetScannerList(gfx::NativeWindow native_window,
                      scoped_refptr<const Extension> extension,
                      api::document_scan::DeviceFilter filter,
                      GetScannerListCallback callback);

  // Given `scanner_id` previously returned from `GetScannerList`, opens the
  // device for exclusive access.  The result containing a handle and the set of
  // current device options will be passed to `callback`.
  void OpenScanner(scoped_refptr<const Extension> extension,
                   const std::string& scanner_id,
                   OpenScannerCallback callback);

  // Given `scanner_handle` previously returned from `OpenScanner`, gets the
  // group names and member options for that scanner.  The result will be passed
  // to `callback`.
  void GetOptionGroups(scoped_refptr<const Extension> extension,
                       const std::string& scanner_handle,
                       GetOptionGroupsCallback callback);

  // Given `scanner_handle` previously returned from `OpenScanner`, closes the
  // handle.  No further operations on this handle can be performed even if the
  // result code does not indicate success.  The result of closing the handle on
  // the backend will be passed to `callback`.
  void CloseScanner(scoped_refptr<const Extension> extension,
                    const std::string& scanner_handle,
                    CloseScannerCallback callback);

  // Given `scanner_handle` previously returned from `OpenScanner`, sends the
  // list of new option values in `options` to the backend.  The backend will
  // attempt to set each option in order, then will respond with a result for
  // each operation and a new final set of device options.  The full response
  // will be passed to `callback`.
  void SetOptions(scoped_refptr<const Extension> extension,
                  const std::string& scanner_handle,
                  const std::vector<api::document_scan::OptionSetting>& options,
                  SetOptionsCallback callback);

  // If the user approves, starts a scan using scanner options previously
  // configured via `SetOptions`.  Additionally, `options` are used to specify
  // scanner-framework options.  Explicit approval is obtained through a Chrome
  // dialog or by adding the extension ID to the list of trusted document scan
  // extensions.  The result of the denial or the backend call will be passed to
  // `callback`.
  void StartScan(gfx::NativeWindow native_window,
                 scoped_refptr<const Extension> extension,
                 const std::string& scanner_handle,
                 api::document_scan::StartScanOptions options,
                 StartScanCallback callback);

  // Cancels a scan using a `job_handle` that was returned from `StartScan` and
  // passes the result to `callback`.
  void CancelScan(scoped_refptr<const Extension> extension,
                  const std::string& job_handle,
                  CancelScanCallback callback);

  // Given `job_handle` previously returned from `StartScan`, requests the next
  // available chunk of scanned image data.  The result from the backend will be
  // passed to `callback`.
  void ReadScanData(scoped_refptr<const Extension> extension,
                    const std::string& job_handle,
                    ReadScanDataCallback callback);

 private:
  // Needed for BrowserContextKeyedAPI implementation.
  friend class BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>;

  // Info that relates to a physical scanner.
  struct ScannerDevice {
    // The string used on the backend to connect to a scanner.
    std::string connection_string;

    // The name of a scanner.
    std::string name;
  };

  // Tracks open handles and scanner IDs that have been given out to a
  // particular extension.  These are the things this has to track for
  // correctness.  For everything else the source of truth is maintained in the
  // backend.
  struct ExtensionState {
    ExtensionState();
    ~ExtensionState();

    // Map from public-facing scanner ID to the scanner's actual ID, which is
    // the internal connection string used on the backend (the latter can be
    // used to look up scanner in `scanner_devices_`).
    std::map<std::string, std::string> scanner_ids;

    // Map from scanner handle to scanner's ID (the latter can be used to look
    // up scanner in `scanner_devices_`).
    std::map<std::string, std::string> scanner_handles;

    // Active job handles.
    std::set<std::string> active_job_handles;

    // A set of scanner handles the user has approved for scanning.
    std::set<std::string> approved_scanners;
  };

  // BrowserContextKeyedAPI:
  static const char* service_name() { return "DocumentScanAPIHandler"; }
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceIsNULLWhileTesting = true;
  static const bool kServiceHasOwnInstanceInIncognito = true;

  // Used by CreateForTesting:
  DocumentScanAPIHandler(content::BrowserContext* browser_context,
                         crosapi::mojom::DocumentScan* document_scan);

  void OnSimpleScanNamesReceived(bool force_virtual_usb_printer,
                                 SimpleScanCallback callback,
                                 const std::vector<std::string>& scanner_names);
  void OnSimpleScanCompleted(SimpleScanCallback callback,
                             crosapi::mojom::ScanFailureMode failure_mode,
                             const std::optional<std::string>& scan_data);

  void SendGetScannerListRequest(const api::document_scan::DeviceFilter& filter,
                                 GetScannerListCallback callback);
  void ShowScanDiscoveryDialog(const api::document_scan::DeviceFilter& filter,
                               GetScannerListCallback callback,
                               const gfx::Image& icon);
  void OnScannerListReceived(
      std::unique_ptr<ScannerDiscoveryRunner> discovery_runner,
      GetScannerListCallback callback,
      crosapi::mojom::GetScannerListResponsePtr response);
  void OnOpenScannerResponse(const ExtensionId& extension_id,
                             const std::string& scanner_id,
                             OpenScannerCallback callback,
                             crosapi::mojom::OpenScannerResponsePtr response);
  void OnGetOptionGroupsResponse(
      GetOptionGroupsCallback callback,
      crosapi::mojom::GetOptionGroupsResponsePtr response);
  void OnCloseScannerResponse(CloseScannerCallback callback,
                              crosapi::mojom::CloseScannerResponsePtr response);
  void OnSetOptionsResponse(SetOptionsCallback callback,
                            crosapi::mojom::SetOptionsResponsePtr response);
  void OnStartScanResponse(
      std::unique_ptr<StartScanRunner> runner,
      StartScanCallback callback,
      crosapi::mojom::StartPreparedScanResponsePtr response);
  void OnCancelScanResponse(const ExtensionId& extension_id,
                            CancelScanCallback callback,
                            crosapi::mojom::CancelScanResponsePtr response);
  void OnReadScanDataResponse(ReadScanDataCallback callback,
                              crosapi::mojom::ReadScanDataResponsePtr response);

  raw_ptr<content::BrowserContext> browser_context_;
  raw_ptr<crosapi::mojom::DocumentScan> document_scan_;
  std::map<ExtensionId, ExtensionState> extension_state_;

  // A global map (across all extensions) from a scanner's ID to its
  // `ScannerDevice`.  The scanner ID is the connection string used on the
  // backend to connect to a scanner.
  std::map<std::string, ScannerDevice> scanner_devices_;

  base::WeakPtrFactory<DocumentScanAPIHandler> weak_ptr_factory_{this};
};

template <>
KeyedService*
BrowserContextKeyedAPIFactory<DocumentScanAPIHandler>::BuildServiceInstanceFor(
    content::BrowserContext* context) const;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DOCUMENT_SCAN_DOCUMENT_SCAN_API_HANDLER_H_
