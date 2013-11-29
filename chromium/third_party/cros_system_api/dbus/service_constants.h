// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_SERVICE_CONSTANTS_H_
#define DBUS_SERVICE_CONSTANTS_H_

#include <stdint.h>  // for uint32_t

namespace dbus {
const char kDBusServiceName[] = "org.freedesktop.DBus";
const char kDBusServicePath[] = "/org/freedesktop/DBus";

// Object Manager interface
const char kDBusObjectManagerInterface[] = "org.freedesktop.DBus.ObjectManager";
// Methods
const char kDBusObjectManagerGetManagedObjects[] = "GetManagedObjects";
// Signals
const char kDBusObjectManagerInterfacesAddedSignal[] = "InterfacesAdded";
const char kDBusObjectManagerInterfacesRemovedSignal[] = "InterfacesRemoved";

// Properties interface
const char kDBusPropertiesInterface[] = "org.freedesktop.DBus.Properties";
// Methods
const char kDBusPropertiesGet[] = "Get";
const char kDBusPropertiesSet[] = "Set";
const char kDBusPropertiesGetAll[] = "GetAll";
// Signals
const char kDBusPropertiesChangedSignal[] = "PropertiesChanged";
}  // namespace dbus


namespace cryptohome {
const char kCryptohomeInterface[] = "org.chromium.CryptohomeInterface";
const char kCryptohomeServicePath[] = "/org/chromium/Cryptohome";
const char kCryptohomeServiceName[] = "org.chromium.Cryptohome";
// Methods
const char kCryptohomeAsyncAddKey[] = "AsyncAddKey";
const char kCryptohomeCheckKey[] = "CheckKey";
const char kCryptohomeMigrateKey[] = "MigrateKey";
const char kCryptohomeRemove[] = "Remove";
const char kCryptohomeGetSystemSalt[] = "GetSystemSalt";
const char kCryptohomeGetSanitizedUsername[] = "GetSanitizedUsername";
const char kCryptohomeIsMounted[] = "IsMounted";
const char kCryptohomeMount[] = "Mount";
const char kCryptohomeMountGuest[] = "MountGuest";
const char kCryptohomeMountPublic[] = "MountPublic";
const char kCryptohomeUnmount[] = "Unmount";
const char kCryptohomeTpmIsReady[] = "TpmIsReady";
const char kCryptohomeTpmIsEnabled[] = "TpmIsEnabled";
const char kCryptohomeTpmIsOwned[] = "TpmIsOwned";
const char kCryptohomeTpmIsBeingOwned[] = "TpmIsBeingOwned";
const char kCryptohomeTpmGetPassword[] = "TpmGetPassword";
const char kCryptohomeTpmCanAttemptOwnership[] = "TpmCanAttemptOwnership";
const char kCryptohomeTpmClearStoredPassword[] = "TpmClearStoredPassword";
const char kCryptohomePkcs11GetTpmTokenInfo[] = "Pkcs11GetTpmTokenInfo";
const char kCryptohomePkcs11GetTpmTokenInfoForUser[] =
    "Pkcs11GetTpmTokenInfoForUser";
const char kCryptohomePkcs11IsTpmTokenReady[] = "Pkcs11IsTpmTokenReady";
const char kCryptohomePkcs11IsTpmTokenReadyForUser[] =
    "Pkcs11IsTpmTokenReadyForUser";
const char kCryptohomeAsyncCheckKey[] = "AsyncCheckKey";
const char kCryptohomeAsyncMigrateKey[] = "AsyncMigrateKey";
const char kCryptohomeAsyncMount[] = "AsyncMount";
const char kCryptohomeAsyncMountGuest[] = "AsyncMountGuest";
const char kCryptohomeAsyncMountPublic[] = "AsyncMountPublic";
const char kCryptohomeAsyncRemove[] = "AsyncRemove";
const char kCryptohomeGetStatusString[] = "GetStatusString";
const char kCryptohomeRemoveTrackedSubdirectories[] =
    "RemoveTrackedSubdirectories";
const char kCryptohomeAsyncRemoveTrackedSubdirectories[] =
    "AsyncRemoveTrackedSubdirectories";
const char kCryptohomeDoAutomaticFreeDiskSpaceControl[] =
    "DoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoAutomaticFreeDiskSpaceControl[] =
    "AsyncDoAutomaticFreeDiskSpaceControl";
const char kCryptohomeAsyncDoesUsersExist[] = "AsyncDoesUsersExist";
const char kCryptohomeInstallAttributesGet[] = "InstallAttributesGet";
const char kCryptohomeInstallAttributesSet[] = "InstallAttributesSet";
const char kCryptohomeInstallAttributesCount[] = "InstallAttributesCount";
const char kCryptohomeInstallAttributesFinalize[] =
    "InstallAttributesFinalize";
const char kCryptohomeInstallAttributesIsReady[] = "InstallAttributesIsReady";
const char kCryptohomeInstallAttributesIsSecure[] =
    "InstallAttributesIsSecure";
const char kCryptohomeInstallAttributesIsInvalid[] =
    "InstallAttributesIsInvalid";
const char kCryptohomeInstallAttributesIsFirstInstall[] =
    "InstallAttributesIsFirstInstall";
const char kCryptohomeTpmIsAttestationPrepared[] = "TpmIsAttestationPrepared";
const char kCryptohomeTpmIsAttestationEnrolled[] = "TpmIsAttestationEnrolled";
const char kCryptohomeAsyncTpmAttestationCreateEnrollRequest[] =
    "AsyncTpmAttestationCreateEnrollRequest";
const char kCryptohomeAsyncTpmAttestationEnroll[] = "AsyncTpmAttestationEnroll";
const char kCryptohomeAsyncTpmAttestationCreateCertRequest[] =
    "AsyncTpmAttestationCreateCertRequest";
const char kCryptohomeAsyncTpmAttestationFinishCertRequest[] =
    "AsyncTpmAttestationFinishCertRequest";
const char kCryptohomeTpmAttestationDoesKeyExist[] =
    "TpmAttestationDoesKeyExist";
const char kCryptohomeTpmAttestationGetCertificate[] =
    "TpmAttestationGetCertificate";
const char kCryptohomeTpmAttestationGetPublicKey[] =
    "TpmAttestationGetPublicKey";
const char kCryptohomeTpmAttestationRegisterKey[] = "TpmAttestationRegisterKey";
const char kCryptohomeTpmAttestationSignEnterpriseChallenge[] =
    "TpmAttestationSignEnterpriseChallenge";
const char kCryptohomeTpmAttestationSignSimpleChallenge[] =
    "TpmAttestationSignSimpleChallenge";
const char kCryptohomeTpmAttestationGetKeyPayload[] =
    "TpmAttestationGetKeyPayload";
const char kCryptohomeTpmAttestationSetKeyPayload[] =
    "TpmAttestationSetKeyPayload";
// Signals
const char kSignalAsyncCallStatus[] = "AsyncCallStatus";
const char kSignalAsyncCallStatusWithData[] = "AsyncCallStatusWithData";
const char kSignalTpmInitStatus[] = "TpmInitStatus";
const char kSignalCleanupUsersRemoved[] = "CleanupUsersRemoved";
// Error code
enum MountError {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_FATAL = 1 << 0,
  MOUNT_ERROR_KEY_FAILURE = 1 << 1,
  MOUNT_ERROR_MOUNT_POINT_BUSY = 1 << 2,
  MOUNT_ERROR_TPM_COMM_ERROR = 1 << 3,
  MOUNT_ERROR_TPM_DEFEND_LOCK = 1 << 4,
  MOUNT_ERROR_USER_DOES_NOT_EXIST = 1 << 5,
  MOUNT_ERROR_TPM_NEEDS_REBOOT = 1 << 6,
  MOUNT_ERROR_RECREATED = 1 << 31,
};
}  // namespace cryptohome

namespace imageburn {
const char kImageBurnServiceName[] = "org.chromium.ImageBurner";
const char kImageBurnServicePath[] = "/org/chromium/ImageBurner";
const char kImageBurnServiceInterface[] = "org.chromium.ImageBurnerInterface";
// Methods
const char kBurnImage[] = "BurnImage";
// Signals
const char kSignalBurnFinishedName[] = "burn_finished";
const char kSignalBurnUpdateName[] = "burn_progress_update";
}  // namespace imageburn

namespace login_manager {
const char kSessionManagerInterface[] = "org.chromium.SessionManagerInterface";
const char kSessionManagerServicePath[] = "/org/chromium/SessionManager";
const char kSessionManagerServiceName[] = "org.chromium.SessionManager";
// Methods
const char kSessionManagerEmitLoginPromptReady[] = "EmitLoginPromptReady";
const char kSessionManagerEmitLoginPromptVisible[] = "EmitLoginPromptVisible";
const char kSessionManagerStartSession[] = "StartSession";
const char kSessionManagerStopSession[] = "StopSession";
const char kSessionManagerRestartJob[] = "RestartJob";
const char kSessionManagerRestartJobWithAuth[] = "RestartJobWithAuth";
const char kSessionManagerRestartEntd[] = "RestartEntd";
const char kSessionManagerSetOwnerKey[] = "SetOwnerKey";
const char kSessionManagerUnwhitelist[] = "Unwhitelist";
const char kSessionManagerCheckWhitelist[] = "CheckWhitelist";
const char kSessionManagerEnumerateWhitelisted[] = "EnumerateWhitelisted";
const char kSessionManagerWhitelist[] = "Whitelist";
const char kSessionManagerStoreProperty[] = "StoreProperty";
const char kSessionManagerRetrieveProperty[] = "RetrieveProperty";
const char kSessionManagerStorePolicy[] = "StorePolicy";
const char kSessionManagerRetrievePolicy[] = "RetrievePolicy";
const char kSessionManagerStorePolicyForUser[] = "StorePolicyForUser";
const char kSessionManagerRetrievePolicyForUser[] = "RetrievePolicyForUser";
const char kSessionManagerStoreDeviceLocalAccountPolicy[] =
    "StoreDeviceLocalAccountPolicy";
const char kSessionManagerRetrieveDeviceLocalAccountPolicy[] =
    "RetrieveDeviceLocalAccountPolicy";
const char kSessionManagerRetrieveSessionState[] = "RetrieveSessionState";
const char kSessionManagerRetrieveActiveSessions[] = "RetrieveActiveSessions";
const char kSessionManagerStartSessionService[] = "StartSessionService";
const char kSessionManagerStopSessionService[] = "StopSessionService";
const char kSessionManagerStartDeviceWipe[] = "StartDeviceWipe";
const char kSessionManagerLockScreen[] = "LockScreen";
const char kSessionManagerHandleLockScreenShown[] = "HandleLockScreenShown";
const char kSessionManagerUnlockScreen[] = "UnlockScreen";
const char kSessionManagerHandleLockScreenDismissed[] =
    "HandleLockScreenDismissed";
const char kSessionManagerHandleLivenessConfirmed[] = "HandleLivenessConfirmed";
const char kSessionManagerSetFlagsForUser[] = "SetFlagsForUser";
// Signals
const char kLoginPromptVisibleSignal[] = "LoginPromptVisible";
const char kSessionStateChangedSignal[] = "SessionStateChanged";
// ScreenLock signals.
const char kScreenIsLockedSignal[] = "ScreenIsLocked";
const char kScreenIsUnlockedSignal[] = "ScreenIsUnlocked";
// Ownership API signals.
const char kOwnerKeySetSignal[] = "SetOwnerKeyComplete";
const char kPropertyChangeCompleteSignal[] = "PropertyChangeComplete";

// DEPRECATED.
const char kSessionManagerSessionStateChanged[] = "SessionStateChanged";
}  // namespace login_manager

namespace speech_synthesis {
const char kSpeechSynthesizerInterface[] =
    "org.chromium.SpeechSynthesizerInterface";
const char kSpeechSynthesizerServicePath[] = "/org/chromium/SpeechSynthesizer";
const char kSpeechSynthesizerServiceName[] = "org.chromium.SpeechSynthesizer";
// Methods
const char kSpeak[] = "Speak";
const char kStop[] = "Stop";
const char kIsSpeaking[] = "IsSpeaking";
const char kShutdown[] = "Shutdown";
}  // namespace speech_synthesis

namespace chromium {
const char kChromiumInterface[] = "org.chromium.Chromium";
// ScreenLock signals.
const char kLockScreenSignal[] = "LockScreen";
const char kUnlockScreenSignal[] = "UnlockScreen";
// Text-to-speech service signals.
const char kTTSReadySignal[] = "TTSReady";
const char kTTSFailedSignal[] = "TTSFailed";

// DEPRECATED in favor of constants in appropriate (login_manager) namespace.
// Ownership API signals.
const char kOwnerKeySetSignal[] = "SetOwnerKeyComplete";
const char kPropertyChangeCompleteSignal[] = "PropertyChangeComplete";
// Liveness detection signals.
const char kLivenessRequestedSignal[] = "LivenessRequested";
}  // namespace chromium

namespace power_manager {
// powerd
const char kPowerManagerInterface[] = "org.chromium.PowerManager";
const char kPowerManagerServicePath[] = "/org/chromium/PowerManager";
const char kPowerManagerServiceName[] = "org.chromium.PowerManager";
// powerd methods
const char kDecreaseScreenBrightness[] = "DecreaseScreenBrightness";
const char kIncreaseScreenBrightness[] = "IncreaseScreenBrightness";
const char kGetScreenBrightnessPercent[] = "GetScreenBrightnessPercent";
const char kSetScreenBrightnessPercent[] = "SetScreenBrightnessPercent";
const char kDecreaseKeyboardBrightness[] = "DecreaseKeyboardBrightness";
const char kIncreaseKeyboardBrightness[] = "IncreaseKeyboardBrightness";
const char kRequestIdleNotification[] = "RequestIdleNotification";
const char kRequestRestartMethod[] = "RequestRestart";
const char kRequestShutdownMethod[] = "RequestShutdown";
const char kRequestSuspendMethod[] = "RequestSuspend";
const char kGetPowerSupplyPropertiesMethod[] = "GetPowerSupplyProperties";
const char kHandleUserActivityMethod[] = "HandleUserActivity";
const char kHandleVideoActivityMethod[] = "HandleVideoActivity";
const char kSetIsProjectingMethod[] = "SetIsProjecting";
const char kSetPolicyMethod[] = "SetPolicy";
const char kRegisterSuspendDelayMethod[] = "RegisterSuspendDelay";
const char kUnregisterSuspendDelayMethod[] = "UnregisterSuspendDelay";
const char kHandleSuspendReadinessMethod[] = "HandleSuspendReadiness";
// Signals emitted by powerd.
const char kCleanShutdown[] = "CleanShutdown";
const char kBrightnessChangedSignal[] = "BrightnessChanged";
const char kIdleNotifySignal[] = "IdleNotify";
const char kKeyboardBrightnessChangedSignal[] = "KeyboardBrightnessChanged";
const char kPeripheralBatteryStatusSignal[] = "PeripheralBatteryStatus";
const char kPowerStateChangedSignal[] = "PowerStateChanged";
const char kPowerSupplyPollSignal[] = "PowerSupplyPoll";
const char kSoftwareScreenDimmingRequestedSignal[] =
    "SoftwareScreenDimmingRequested";
const char kSuspendImminentSignal[] = "SuspendImminent";
const char kInputEventSignal[] = "InputEvent";
const char kSuspendStateChangedSignal[] = "SuspendStateChanged";
const char kIdleActionImminentSignal[] = "IdleActionImminent";
const char kIdleActionDeferredSignal[] = "IdleActionDeferred";
// Values
const int  kBrightnessTransitionGradual = 1;
const int  kBrightnessTransitionInstant = 2;
const int  kSoftwareScreenDimmingNone = 1;
const int  kSoftwareScreenDimmingIdle = 2;
enum UserActivityType {
  USER_ACTIVITY_OTHER = 0,
  USER_ACTIVITY_BRIGHTNESS_UP_KEY_PRESS = 1,
  USER_ACTIVITY_BRIGHTNESS_DOWN_KEY_PRESS = 2,
};
}  // namespace power_manager

namespace chromeos {
const char kLibCrosServiceName[] = "org.chromium.LibCrosService";
const char kLibCrosServicePath[] = "/org/chromium/LibCrosService";
const char kLibCrosServiceInterface[] = "org.chromium.LibCrosServiceInterface";
// Methods
const char kResolveNetworkProxy[] = "ResolveNetworkProxy";
const char kCheckLiveness[] = "CheckLiveness";
const char kSetDisplayPower[] = "SetDisplayPower";
const char kSetDisplaySoftwareDimming[] = "SetDisplaySoftwareDimming";
// Values
enum DisplayPowerState {
  DISPLAY_POWER_ALL_ON = 0,
  DISPLAY_POWER_ALL_OFF = 1,
  DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON = 2,
  DISPLAY_POWER_INTERNAL_ON_EXTERNAL_OFF = 3,
};
}  // namespace chromeos

namespace flimflam {
// Flimflam D-Bus service identifiers.
const char kFlimflamManagerInterface[] = "org.chromium.flimflam.Manager";
const char kFlimflamServiceName[] = "org.chromium.flimflam";
const char kFlimflamServicePath[] = "/";  // crosbug.com/20135
const char kFlimflamServiceInterface[] = "org.chromium.flimflam.Service";
const char kFlimflamIPConfigInterface[] = "org.chromium.flimflam.IPConfig";
const char kFlimflamDeviceInterface[] = "org.chromium.flimflam.Device";
const char kFlimflamProfileInterface[] = "org.chromium.flimflam.Profile";
const char kFlimflamNetworkInterface[] = "org.chromium.flimflam.Network";

// Flimflam function names.
const char kGetPropertiesFunction[] = "GetProperties";
const char kSetPropertyFunction[] = "SetProperty";
const char kClearPropertyFunction[] = "ClearProperty";
const char kConnectFunction[] = "Connect";
const char kDisconnectFunction[] = "Disconnect";
const char kRequestScanFunction[] = "RequestScan";
const char kGetServiceFunction[] = "GetService";
const char kGetWifiServiceFunction[] = "GetWifiService";
const char kGetVPNServiceFunction[] = "GetVPNService";
const char kRemoveServiceFunction[] = "Remove";
const char kEnableTechnologyFunction[] = "EnableTechnology";
const char kDisableTechnologyFunction[] = "DisableTechnology";
const char kAddIPConfigFunction[] = "AddIPConfig";
const char kRemoveConfigFunction[] = "Remove";
const char kGetEntryFunction[] = "GetEntry";
const char kDeleteEntryFunction[] = "DeleteEntry";
const char kActivateCellularModemFunction[] = "ActivateCellularModem";
const char kRequirePinFunction[] = "RequirePin";
const char kEnterPinFunction[] = "EnterPin";
const char kUnblockPinFunction[] = "UnblockPin";
const char kChangePinFunction[] = "ChangePin";
const char kProposeScanFunction[] = "ProposeScan";
const char kRegisterFunction[] = "Register";
const char kConfigureServiceFunction[] = "ConfigureService";
const char kConfigureWifiServiceFunction[] = "ConfigureWifiService";

// Flimflam Service property names.
const char kSecurityProperty[] = "Security";
const char kPriorityProperty[] = "Priority";
const char kPassphraseProperty[] = "Passphrase";
const char kIdentityProperty[] = "Identity";
const char kAuthorityPathProperty[] = "AuthorityPath";
const char kPassphraseRequiredProperty[] = "PassphraseRequired";
const char kSaveCredentialsProperty[] = "SaveCredentials";
const char kSignalStrengthProperty[] = "Strength";
const char kNameProperty[] = "Name";
const char kGuidProperty[] = "GUID";
const char kStateProperty[] = "State";
const char kTypeProperty[] = "Type";
const char kDeviceProperty[] = "Device";
const char kProfileProperty[] = "Profile";
const char kConnectivityStateProperty[] = "ConnectivityState";
const char kFavoriteProperty[] = "Favorite";
const char kConnectableProperty[] = "Connectable";
const char kAutoConnectProperty[] = "AutoConnect";
const char kIsActiveProperty[] = "IsActive";
const char kModeProperty[] = "Mode";
const char kErrorProperty[] = "Error";
const char kProviderProperty[] = "Provider";
const char kHostProperty[] = "Host";
const char kDomainProperty[] = "Domain";
const char kProxyConfigProperty[] = "ProxyConfig";
const char kCheckPortalProperty[] = "CheckPortal";
const char kSSIDProperty[] = "SSID";
const char kConnectedProperty[] = "Connected";
const char kUIDataProperty[] = "UIData";

// Flimflam provider property names.
const char kProviderHostProperty[] = "Provider.Host";
const char kProviderNameProperty[] = "Provider.Name";
const char kProviderTypeProperty[] = "Provider.Type";

// Flimflam Wifi Service property names.
const char kWifiBSsid[] = "WiFi.BSSID";
const char kWifiHexSsid[] = "WiFi.HexSSID";
const char kWifiFrequency[] = "WiFi.Frequency";
const char kWifiHiddenSsid[] = "WiFi.HiddenSSID";
const char kWifiPhyMode[] = "WiFi.PhyMode";
const char kWifiAuthMode[] = "WiFi.AuthMode";
const char kWifiChannelProperty[] = "WiFi.Channel";

// Flimflam EAP property names.
const char kEapIdentityProperty[] = "EAP.Identity";
const char kEapMethodProperty[] = "EAP.EAP";
const char kEapPhase2AuthProperty[] = "EAP.InnerEAP";
const char kEapAnonymousIdentityProperty[] = "EAP.AnonymousIdentity";
const char kEapClientCertProperty[] = "EAP.ClientCert";
const char kEapCertIdProperty[] = "EAP.CertID";
const char kEapClientCertNssProperty[] = "EAP.ClientCertNSS";
const char kEapPrivateKeyProperty[] = "EAP.PrivateKey";
const char kEapPrivateKeyPasswordProperty[] = "EAP.PrivateKeyPassword";
const char kEapKeyIdProperty[] = "EAP.KeyID";
const char kEapCaCertProperty[] = "EAP.CACert";
const char kEapCaCertIdProperty[] = "EAP.CACertID";
const char kEapCaCertNssProperty[] = "EAP.CACertNSS";
const char kEapUseSystemCasProperty[] = "EAP.UseSystemCAs";
const char kEapPinProperty[] = "EAP.PIN";
const char kEapPasswordProperty[] = "EAP.Password";
const char kEapKeyMgmtProperty[] = "EAP.KeyMgmt";
// Deprecated (duplicates)
const char kEAPEAPProperty[] = "EAP.EAP";
const char kEAPClientCertProperty[] = "EAP.ClientCert";
const char kEAPCertIDProperty[] = "EAP.CertID";
const char kEAPKeyIDProperty[] = "EAP.KeyID";
const char kEapCaCertIDProperty[] = "EAP.CACertID";
const char kEapUseSystemCAsProperty[] = "EAP.UseSystemCAs";
const char kEAPPINProperty[] = "EAP.PIN";

// Flimflam Cellular Service property names.
const char kTechnologyFamilyProperty[] = "Cellular.Family";
const char kActivationStateProperty[] = "Cellular.ActivationState";
const char kNetworkTechnologyProperty[] = "Cellular.NetworkTechnology";
const char kRoamingStateProperty[] = "Cellular.RoamingState";
const char kOperatorNameProperty[] = "Cellular.OperatorName";
const char kOperatorCodeProperty[] = "Cellular.OperatorCode";
const char kServingOperatorProperty[] = "Cellular.ServingOperator";
// DEPRECATED
const char kPaymentURLProperty[] = "Cellular.OlpUrl";
const char kPaymentPortalProperty[] = "Cellular.Olp";
const char kUsageURLProperty[] = "Cellular.UsageUrl";
const char kCellularApnProperty[] = "Cellular.APN";
const char kCellularLastGoodApnProperty[] = "Cellular.LastGoodAPN";
const char kCellularApnListProperty[] = "Cellular.APNList";

// Flimflam Manager property names.
const char kProfilesProperty[] = "Profiles";
const char kServicesProperty[] = "Services";
const char kServiceWatchListProperty[] = "ServiceWatchList";
const char kAvailableTechnologiesProperty[] = "AvailableTechnologies";
const char kEnabledTechnologiesProperty[] = "EnabledTechnologies";
const char kConnectedTechnologiesProperty[] = "ConnectedTechnologies";
const char kDefaultTechnologyProperty[] = "DefaultTechnology";
const char kOfflineModeProperty[] = "OfflineMode";
const char kActiveProfileProperty[] = "ActiveProfile";
const char kDevicesProperty[] = "Devices";
const char kCheckPortalListProperty[] = "CheckPortalList";
const char kArpGatewayProperty[] = "ArpGateway";
const char kCountryProperty[] = "Country";
const char kPortalURLProperty[] = "PortalURL";

// Flimflam Profile property names.
const char kEntriesProperty[] = "Entries";

// Flimflam Device property names.
const char kScanningProperty[] = "Scanning";
const char kPoweredProperty[] = "Powered";
const char kNetworksProperty[] = "Networks";
const char kScanIntervalProperty[] = "ScanInterval";
const char kBgscanMethodProperty[] = "BgscanMethod";
const char kBgscanShortIntervalProperty[] = "BgscanShortInterval";
// DEPRECATED
// remove after references are gone from chrome, libcros, and flimflam/shill.
const char kDBusConnectionProperty[] = "DBus.Connection";
const char kDBusObjectProperty[] = "DBus.Object";
const char kDBusServiceProperty[] = "DBus.Service";
const char kBgscanSignalThresholdProperty[] = "BgscanSignalThreshold";
// The name of the network interface, ie. wlan0, eth0, etc.
const char kInterfaceProperty[] = "Interface";

// Flimflam Cellular Device property names.
const char kCarrierProperty[] = "Cellular.Carrier";
const char kCellularAllowRoamingProperty[] = "Cellular.AllowRoaming";
const char kHomeProviderProperty[] = "Cellular.HomeProvider";
const char kMeidProperty[] = "Cellular.MEID";
const char kImeiProperty[] = "Cellular.IMEI";
const char kIccidProperty[] = "Cellular.ICCID";
const char kImsiProperty[] = "Cellular.IMSI";
const char kEsnProperty[] = "Cellular.ESN";
const char kMdnProperty[] = "Cellular.MDN";
const char kMinProperty[] = "Cellular.MIN";
const char kModelIDProperty[] = "Cellular.ModelID";
const char kManufacturerProperty[] = "Cellular.Manufacturer";
const char kFirmwareRevisionProperty[] = "Cellular.FirmwareRevision";
const char kHardwareRevisionProperty[] = "Cellular.HardwareRevision";
const char kPRLVersionProperty[] = "Cellular.PRLVersion";
const char kSelectedNetworkProperty[] = "Cellular.SelectedNetwork";
const char kSupportNetworkScanProperty[] = "Cellular.SupportNetworkScan";
const char kFoundNetworksProperty[] = "Cellular.FoundNetworks";
const char kIPConfigsProperty[] = "IPConfigs";

// Flimflam state options.
const char kStateIdle[] = "idle";
const char kStateCarrier[] = "carrier";
const char kStateAssociation[] = "association";
const char kStateConfiguration[] = "configuration";
const char kStateReady[] = "ready";
const char kStatePortal[] = "portal";
const char kStateOffline[] = "offline";
const char kStateOnline[] = "online";
const char kStateDisconnect[] = "disconnect";
const char kStateFailure[] = "failure";
const char kStateActivationFailure[] = "activation-failure";

// Flimflam property names for SIMLock status.
const char kSIMLockStatusProperty[] = "Cellular.SIMLockStatus";
const char kSIMLockTypeProperty[] = "LockType";
const char kSIMLockRetriesLeftProperty[] = "RetriesLeft";
const char kSIMLockEnabledProperty[] = "LockEnabled";

// Flimflam property names for Cellular.FoundNetworks.
const char kLongNameProperty[] = "long_name";
const char kStatusProperty[] = "status";
const char kShortNameProperty[] = "short_name";
const char kTechnologyProperty[] = "technology";
const char kNetworkIdProperty[] = "network_id";

// Flimflam SIMLock status types.
const char kSIMLockPin[] = "sim-pin";
const char kSIMLockPuk[] = "sim-puk";

// APN info property names.
const char kApnProperty[] = "apn";
const char kApnNetworkIdProperty[] = "network_id";
const char kApnUsernameProperty[] = "username";
const char kApnPasswordProperty[] = "password";
const char kApnNameProperty[] = "name";
const char kApnLocalizedNameProperty[] = "localized_name";
const char kApnLanguageProperty[] = "language";

// Payment Portal property names.
const char kPaymentPortalURL[] = "url";
const char kPaymentPortalMethod[] = "method";
const char kPaymentPortalPostData[] = "postdata";

// Operator info property names.
const char kOperatorNameKey[] = "name";
const char kOperatorCodeKey[] = "code";
const char kOperatorCountryKey[] = "country";

// Flimflam network technology options.
const char kNetworkTechnology1Xrtt[] = "1xRTT";
const char kNetworkTechnologyEvdo[] = "EVDO";
const char kNetworkTechnologyGsm[] = "GSM";
const char kNetworkTechnologyGprs[] = "GPRS";
const char kNetworkTechnologyEdge[] = "EDGE";
const char kNetworkTechnologyUmts[] = "UMTS";
const char kNetworkTechnologyHspa[] = "HSPA";
const char kNetworkTechnologyHspaPlus[] = "HSPA+";
const char kNetworkTechnologyLte[] = "LTE";
const char kNetworkTechnologyLteAdvanced[] = "LTE Advanced";

// Flimflam roaming state options
const char kRoamingStateHome[] = "home";
const char kRoamingStateRoaming[] = "roaming";
const char kRoamingStateUnknown[] = "unknown";

// Flimflam activation state options
const char kActivationStateActivated[] = "activated";
const char kActivationStateActivating[] = "activating";
const char kActivationStateNotActivated[] = "not-activated";
const char kActivationStatePartiallyActivated[] = "partially-activated";
const char kActivationStateUnknown[] = "unknown";

// Flimflam EAP method options.
const char kEapMethodPEAP[] = "PEAP";
const char kEapMethodTLS[] = "TLS";
const char kEapMethodTTLS[] = "TTLS";
const char kEapMethodLEAP[] = "LEAP";

// Flimflam EAP phase 2 auth options.
const char kEapPhase2AuthPEAPMD5[] = "auth=MD5";
const char kEapPhase2AuthPEAPMSCHAPV2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTTLSMD5[] = "autheap=MD5";  // crosbug/26822
const char kEapPhase2AuthTTLSEAPMD5[] = "autheap=MD5";
const char kEapPhase2AuthTTLSEAPMSCHAPV2[] = "autheap=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAPV2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAP[] = "auth=MSCHAP";
const char kEapPhase2AuthTTLSPAP[] = "auth=PAP";
const char kEapPhase2AuthTTLSCHAP[] = "auth=CHAP";

// Flimflam VPN provider types.
const char kProviderL2tpIpsec[] = "l2tpipsec";
const char kProviderOpenVpn[] = "openvpn";

// Flimflam VPN service properties
const char kVPNDomainProperty[] = "VPN.Domain";

// Flimflam monitored properties
const char kMonitorPropertyChanged[] = "PropertyChanged";

// Flimflam type options.
const char kTypeEthernet[] = "ethernet";
const char kTypeWifi[] = "wifi";
const char kTypeWimax[] = "wimax";
const char kTypeBluetooth[] = "bluetooth";
const char kTypeCellular[] = "cellular";
const char kTypeVPN[] = "vpn";

// Flimflam mode options.
const char kModeManaged[] = "managed";
const char kModeAdhoc[] = "adhoc";

// Flimflam security options.
const char kSecurityWpa[] = "wpa";
const char kSecurityWep[] = "wep";
const char kSecurityRsn[] = "rsn";
const char kSecurity8021x[] = "802_1x";
const char kSecurityPsk[] = "psk";
const char kSecurityNone[] = "none";

// Flimflam L2TPIPsec property names.
const char kL2tpIpsecAuthenticationType[] = "L2TPIPsec.AuthenticationType";
const char kL2tpIpsecCaCertNssProperty[] = "L2TPIPsec.CACertNSS";
const char kL2tpIpsecClientCertIdProperty[] = "L2TPIPsec.ClientCertID";
const char kL2tpIpsecClientCertSlotProperty[] = "L2TPIPsec.ClientCertSlot";
const char kL2tpIpsecIkeVersion[] = "L2TPIPsec.IKEVersion";
const char kL2tpIpsecPinProperty[] = "L2TPIPsec.PIN";
const char kL2tpIpsecPskProperty[] = "L2TPIPsec.PSK";
const char kL2tpIpsecPskRequiredProperty[] = "L2TPIPsec.PSKRequired";
const char kL2tpIpsecUserProperty[] = "L2TPIPsec.User";
const char kL2tpIpsecPasswordProperty[] = "L2TPIPsec.Password";
// deprecated:
const char kL2tpIpsecClientCertSlotProp[] = "L2TPIPsec.ClientCertSlot";

// Flimflam OpenVPN property names.
const char kOpenVPNAuthNoCacheProperty[] = "OpenVPN.AuthNoCache";
const char kOpenVPNAuthProperty[] = "OpenVPN.Auth";
const char kOpenVPNAuthRetryProperty[] = "OpenVPN.AuthRetry";
const char kOpenVPNAuthUserPassProperty[] = "OpenVPN.AuthUserPass";
const char kOpenVPNCaCertProperty[] = "OpenVPN.CACert";
const char kOpenVPNCaCertNSSProperty[] = "OpenVPN.CACertNSS";
const char kOpenVPNClientCertIdProperty[] = "OpenVPN.Pkcs11.ID";
const char kOpenVPNClientCertSlotProperty[] = "OpenVPN.Pkcs11.Slot";
const char kOpenVPNCipherProperty[] = "OpenVPN.Cipher";
const char kOpenVPNCompLZOProperty[] = "OpenVPN.CompLZO";
const char kOpenVPNCompNoAdaptProperty[] = "OpenVPN.CompNoAdapt";
const char kOpenVPNKeyDirectionProperty[] = "OpenVPN.KeyDirection";
const char kOpenVPNMgmtEnableProperty[] = "OpenVPN.Mgmt.Enable";
const char kOpenVPNNsCertTypeProperty[] = "OpenVPN.NsCertType";
const char kOpenVPNOTPProperty[] = "OpenVPN.OTP";
const char kOpenVPNPasswordProperty[] = "OpenVPN.Password";
const char kOpenVPNPinProperty[] = "OpenVPN.Pkcs11.PIN";
const char kOpenVPNPortProperty[] = "OpenVPN.Port";
const char kOpenVPNProtoProperty[] = "OpenVPN.Proto";
const char kOpenVPNProviderProperty[] = "OpenVPN.Pkcs11.Provider";
const char kOpenVPNPushPeerInfoProperty[] = "OpenVPN.PushPeerInfo";
const char kOpenVPNRemoteCertEKUProperty[] = "OpenVPN.RemoteCertEKU";
const char kOpenVPNRemoteCertKUProperty[] = "OpenVPN.RemoteCertKU";
const char kOpenVPNRemoteCertTLSProperty[] = "OpenVPN.RemoteCertTLS";
const char kOpenVPNRenegSecProperty[] = "OpenVPN.RenegSec";
const char kOpenVPNServerPollTimeoutProperty[] = "OpenVPN.ServerPollTimeout";
const char kOpenVPNShaperProperty[] = "OpenVPN.Shaper";
const char kOpenVPNStaticChallengeProperty[] = "OpenVPN.StaticChallenge";
const char kOpenVPNTLSAuthContentsProperty[] = "OpenVPN.TLSAuthContents";
const char kOpenVPNTLSRemoteProperty[] = "OpenVPN.TLSRemote";
const char kOpenVPNUserProperty[] = "OpenVPN.User";

// FlimFlam technology family options
const char kTechnologyFamilyCdma[] = "CDMA";
const char kTechnologyFamilyGsm[] = "GSM";

// IPConfig property names.
const char kMethodProperty[] = "Method";
const char kAddressProperty[] = "Address";
const char kMtuProperty[] = "Mtu";
const char kPrefixlenProperty[] = "Prefixlen";
const char kBroadcastProperty[] = "Broadcast";
const char kPeerAddressProperty[] = "PeerAddress";
const char kGatewayProperty[] = "Gateway";
const char kDomainNameProperty[] = "DomainName";
const char kNameServersProperty[] = "NameServers";

// IPConfig type options.
const char kTypeIPv4[] = "ipv4";
const char kTypeIPv6[] = "ipv6";
const char kTypeDHCP[] = "dhcp";
const char kTypeBOOTP[] = "bootp";
const char kTypeZeroConf[] = "zeroconf";
const char kTypeDHCP6[] = "dhcp6";
const char kTypePPP[] = "ppp";

// Flimflam error options.
const char kErrorAaaFailed[] = "aaa-failed";
const char kErrorActivationFailed[] = "activation-failed";
const char kErrorBadPassphrase[] = "bad-passphrase";
const char kErrorBadWEPKey[] = "bad-wepkey";
const char kErrorConnectFailed[] = "connect-failed";
const char kErrorDNSLookupFailed[] = "dns-lookup-failed";
const char kErrorDhcpFailed[] = "dhcp-failed";
const char kErrorHTTPGetFailed[] = "http-get-failed";
const char kErrorInternal[] = "internal-error";
const char kErrorIpsecCertAuthFailed[] = "ipsec-cert-auth-failed";
const char kErrorIpsecPskAuthFailed[] = "ipsec-psk-auth-failed";
const char kErrorNeedEvdo[] = "need-evdo";
const char kErrorNeedHomeNetwork[] = "need-home-network";
const char kErrorOtaspFailed[] = "otasp-failed";
const char kErrorOutOfRange[] = "out-of-range";
const char kErrorPinMissing[] = "pin-missing";
const char kErrorPppAuthFailed[] = "ppp-auth-failed";

// Flimflam error messages.
const char kErrorPassphraseRequiredMsg[] = "Passphrase required";
const char kErrorIncorrectPinMsg[] = "org.chromium.flimflam.Error.IncorrectPin";
const char kErrorPinBlockedMsg[] = "org.chromium.flimflam.Error.PinBlocked";
const char kErrorPinRequiredMsg[] = "org.chromium.flimflam.Error.PinRequired";

const char kUnknownString[] = "UNKNOWN";
}  // namespace flimflam

namespace shill {
// Function names.
const char kSetPropertiesFunction[] = "SetProperties";
const char kClearPropertiesFunction[] = "ClearProperties";
const char kCompleteCellularActivationFunction[] = "CompleteCellularActivation";
const char kConfigureServiceForProfileFunction[] = "ConfigureServiceForProfile";
const char kConnectToBestServicesFunction[] = "ConnectToBestServices";
const char kGetLoadableProfileEntriesFunction[] = "GetLoadableProfileEntries";
const char kGetNetworksForGeolocation[] = "GetNetworksForGeolocation";
const char kRefreshFunction[] = "Refresh";
const char kResetFunction[] = "Reset";
const char kSetCarrierFunction[] = "SetCarrier";
const char kVerifyAndEncryptCredentialsFunction[] =
    "VerifyAndEncryptCredentials";
const char kVerifyAndEncryptDataFunction[] = "VerifyAndEncryptData";
const char kVerifyDestinationFunction[] = "VerifyDestination";

// Device property names.
const char kEapAuthenticationCompletedProperty[] = "EapAuthenticationCompleted";
const char kEapAuthenticatorDetectedProperty[] = "EapAuthenticatorDetected";
const char kLinkMonitorResponseTimeProperty[] = "LinkMonitorResponseTime";
const char kProviderRequiresRoamingProperty[] =
    "Cellular.ProviderRequiresRoaming";
const char kReceiveByteCountProperty[] = "ReceiveByteCount";
const char kSIMOperatorIdProperty[] = "Cellular.SIMOperatorID";
const char kSIMPresentProperty[] = "Cellular.SIMPresent";
const char kSupportedCarriersProperty[] = "Cellular.SupportedCarriers";
const char kTransmitByteCountProperty[] = "TransmitByteCount";

// Technology types (augments "Flimflam type options" above).
const char kTypeEthernetEap[] = "etherneteap";

// Error strings.
const char kErrorEapAuthenticationFailed[] = "eap-authentication-failed";
const char kErrorEapLocalTlsFailed[] = "eap-local-tls-failed";
const char kErrorEapRemoteTlsFailed[] = "eap-remote-tls-failed";

// IPConfig property names.
const char kSearchDomainsProperty[] = "SearchDomains";
const char kWebProxyAutoDiscoveryUrlProperty[] = "WebProxyAutoDiscoveryUrl";

// Manager property names.
const char kDefaultServiceProperty[] = "DefaultService";
const char kHostNameProperty[] = "HostName";
const char kIgnoredDNSSearchPathsProperty[] = "IgnoredDNSSearchPaths";
const char kLinkMonitorTechnologiesProperty[] =
    "LinkMonitorTechnologies";
const char kPortalCheckIntervalProperty[] = "PortalCheckInterval";
const char kServiceCompleteListProperty[] = "ServiceCompleteList";
const char kShortDNSTimeoutTechnologiesProperty[] =
    "ShortDNSTimeoutTechnologies";
const char kUninitializedTechnologiesProperty[] = "UninitializedTechnologies";

// Service property names.
const char kActivateOverNonCellularNetworkProperty[] =
    "Cellular.ActivateOverNonCellularNetwork";
const char kDiagnosticsDisconnectsProperty[] = "Diagnostics.Disconnects";
const char kDiagnosticsMisconnectsProperty[] = "Diagnostics.Misconnects";
const char kEapRemoteCertificationProperty[] = "EAP.RemoteCertification";
const char kEapCaCertPemProperty[] = "EAP.CACertPEM";
const char kEapSubjectMatchProperty[] = "EAP.SubjectMatch";
const char kErrorDetailsProperty[] = "ErrorDetails";
const char kHTTPProxyPortProperty[] = "HTTPProxyPort";
const char kIPConfigProperty[] = "IPConfig";
const char kL2tpIpsecCaCertPemProperty[] = "L2TPIPsec.CACertPEM";
const char kL2tpIpsecTunnelGroupProperty[] = "L2TPIPsec.TunnelGroup";
const char kOpenVPNCaCertPemProperty[] = "OpenVPN.CACertPEM";
const char kOpenVPNCertProperty[] = "OpenVPN.Cert";
const char kOpenVPNExtraCertPemProperty[] = "OpenVPN.ExtraCertPEM";
const char kOpenVPNKeyProperty[] = "OpenVPN.Key";
const char kOpenVPNPingProperty[] = "OpenVPN.Ping";
const char kOpenVPNPingExitProperty[] = "OpenVPN.PingExit";
const char kOpenVPNPingRestartProperty[] = "OpenVPN.PingRestart";
const char kOpenVPNTLSAuthProperty[] = "OpenVPN.TLSAuth";
const char kOpenVPNVerbProperty[] = "OpenVPN.Verb";
const char kOutOfCreditsProperty[] = "Cellular.OutOfCredits";
const char kPhysicalTechnologyProperty[] = "PhysicalTechnology";
const char kStaticIPAddressProperty[] = "StaticIP.Address";
const char kStaticIPGatewayProperty[] = "StaticIP.Gateway";
const char kStaticIPMtuProperty[] = "StaticIP.Mtu";
const char kStaticIPNameServersProperty[] = "StaticIP.NameServers";
const char kStaticIPPeerAddressProperty[] = "StaticIP.PeerAddress";
const char kStaticIPPrefixlenProperty[] = "StaticIP.Prefixlen";
const char kSavedIPAddressProperty[] = "SavedIP.Address";
const char kSavedIPGatewayProperty[] = "SavedIP.Gateway";
const char kSavedIPMtuProperty[] = "SavedIP.Mtu";
const char kSavedIPNameServersProperty[] = "SavedIP.NameServers";
const char kSavedIPPeerAddressProperty[] = "SavedIP.PeerAddress";
const char kSavedIPPrefixlenProperty[] = "SavedIP.Prefixlen";
const char kVPNMTUProperty[] = "VPN.MTU";
const char kWifiFrequencyListProperty[] = "WiFi.FrequencyList";
const char kWifiVendorInformationProperty[] = "WiFi.VendorInformation";
const char kWifiProtectedManagementFrameRequiredProperty[] =
    "WiFi.ProtectedManagementFrameRequired";

// Profile property names.
const char kUserHashProperty[] = "UserHash";

// WiFi Service Vendor Information dictionary properties.
const char kVendorWPSManufacturerProperty[] = "Manufacturer";
const char kVendorWPSModelNameProperty[] = "ModelName";
const char kVendorWPSModelNumberProperty[] = "ModelNumber";
const char kVendorWPSDeviceNameProperty[] = "DeviceName";
const char kVendorOUIListProperty[] = "OUIList";

// Cellular service carriers.
const char kCarrierGenericUMTS[] = "Generic UMTS";
const char kCarrierSprint[] = "Sprint";
const char kCarrierVerizon[] = "Verizon Wireless";

// Geolocation property field names.
// Reference:
//    https://devsite.googleplex.com/maps/documentation/business/geolocation/
// Top level properties for a Geolocation request.
const char kGeoHomeMobileCountryCodeProperty[] = "homeMobileCountryCode";
const char kGeoHomeMobileNetworkCodeProperty[] = "homeMobileNetworkCode";
const char kGeoRadioTypePropertyProperty[] = "radioType";
const char kGeoCellTowersProperty[] = "cellTowers";
const char kGeoWifiAccessPointsProperty[] = "wifiAccessPoints";
// Cell tower object property names.
const char kGeoCellIdProperty[] = "cellId";
const char kGeoLocationAreaCodeProperty[] = "locationAreaCode";
const char kGeoMobileCountryCodeProperty[] = "mobileCountryCode";
const char kGeoMobileNetworkCodeProperty[] = "mobileNetworkCode";
const char kGeoTimingAdvanceProperty[] = "timingAdvance";
// WiFi access point property names.
const char kGeoMacAddressProperty[] = "macAddress";
const char kGeoChannelProperty[] = "channel";
const char kGeoSignalToNoiseRatioProperty[] = "signalToNoiseRatio";
// Common property names for geolocation objects.
const char kGeoAgeProperty[] = "age";
const char kGeoSignalStrengthProperty[] = "signalStrength";
}  // namespace shill

namespace modemmanager {
// ModemManager D-Bus service identifiers
const char kModemManagerSMSInterface[] =
    "org.freedesktop.ModemManager.Modem.Gsm.SMS";

// ModemManager function names.
const char kSMSGetFunction[] = "Get";
const char kSMSDeleteFunction[] = "Delete";
const char kSMSListFunction[] = "List";

// ModemManager monitored signals
const char kSMSReceivedSignal[] = "SmsReceived";

// ModemManager1 interfaces and signals
// The canonical source for these is /usr/include/mm/ModemManager-names.h
const char kModemManager1[] = "org.freedesktop.ModemManager1";
const char kModemManager1ServicePath[] = "/org/freedesktop/ModemManager1";
const char kModemManager1MessagingInterface[] =
    "org.freedesktop.ModemManager1.Modem.Messaging";
const char kModemManager1SmsInterface[] =
    "org.freedesktop.ModemManager1.Sms";
const char kSMSAddedSignal[] = "Added";
}  // namespace modemmanager

namespace wimax_manager {
// WiMaxManager D-Bus service identifiers
const char kWiMaxManagerServiceName[] = "org.chromium.WiMaxManager";
const char kWiMaxManagerServicePath[] = "/org/chromium/WiMaxManager";
const char kWiMaxManagerServiceError[] = "org.chromium.WiMaxManager.Error";
const char kWiMaxManagerInterface[] = "org.chromium.WiMaxManager";
const char kWiMaxManagerDeviceInterface[] = "org.chromium.WiMaxManager.Device";
const char kWiMaxManagerNetworkInterface[] =
    "org.chromium.WiMaxManager.Network";
const char kDeviceObjectPathPrefix[] = "/org/chromium/WiMaxManager/Device/";
const char kNetworkObjectPathPrefix[] = "/org/chromium/WiMaxManager/Network/";
const char kDevicesProperty[] = "Devices";
const char kNetworksProperty[] = "Networks";
const char kEAPAnonymousIdentity[] = "EAPAnonymousIdentity";
const char kEAPUserIdentity[] = "EAPUserIdentity";
const char kEAPUserPassword[] = "EAPUserPassword";

enum DeviceStatus {
  kDeviceStatusUninitialized,
  kDeviceStatusDisabled,
  kDeviceStatusReady,
  kDeviceStatusScanning,
  kDeviceStatusConnecting,
  kDeviceStatusConnected
};
}  // namespace wimax_manager

namespace bluetooth_adapter {
// Bluetooth Adapter service identifiers.
const char kBluetoothAdapterServiceName[] = "org.bluez";
const char kBluetoothAdapterInterface[] = "org.bluez.Adapter1";

// Bluetooth Adapter methods.
const char kStartDiscovery[] = "StartDiscovery";
const char kStopDiscovery[] = "StopDiscovery";
const char kRemoveDevice[] = "RemoveDevice";

// Bluetooth Adapter properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kAliasProperty[] = "Alias";
const char kClassProperty[] = "Class";
const char kPoweredProperty[] = "Powered";
const char kDiscoverableProperty[] = "Discoverable";
const char kPairableProperty[] = "Pairable";
const char kPairableTimeoutProperty[] = "PairableTimeout";
const char kDiscoverableTimeoutProperty[] = "DiscoverableTimeout";
const char kDiscoveringProperty[] = "Discovering";
const char kUUIDsProperty[] = "UUIDs";
const char kModaliasProperty[] = "Modalias";

// Bluetooth Adapter errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorNotAuthorized[] = "org.bluez.Error.NotAuthorized";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
}  // namespace bluetooth_adapter

namespace bluetooth_agent_manager {
// Bluetooth Agent Manager service indentifiers
const char kBluetoothAgentManagerServiceName[] = "org.bluez";
const char kBluetoothAgentManagerServicePath[] = "/org/bluez";
const char kBluetoothAgentManagerInterface[] = "org.bluez.AgentManager1";

// Bluetooth Agent Manager methods.
const char kRegisterAgent[] = "RegisterAgent";
const char kUnregisterAgent[] = "UnregisterAgent";
const char kRequestDefaultAgent[] = "RequestDefaultAgent";

// Bluetooth capabilities.
const char kNoInputNoOutputCapability[] = "NoInputNoOutput";
const char kDisplayOnlyCapability[] = "DisplayOnly";
const char kKeyboardOnlyCapability[] = "KeyboardOnly";
const char kDisplayYesNoCapability[] = "DisplayYesNo";
const char kKeyboardDisplayCapability[] = "KeyboardDisplay";

// Bluetooth Agent Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
}  // namespace bluetooth_agent_manager


namespace bluetooth_agent {
// Bluetooth Agent service indentifiers
const char kBluetoothAgentInterface[] = "org.bluez.Agent1";

// Bluetooth Agent methods.
const char kRelease[] = "Release";
const char kRequestPinCode[] = "RequestPinCode";
const char kDisplayPinCode[] = "DisplayPinCode";
const char kRequestPasskey[] = "RequestPasskey";
const char kDisplayPasskey[] = "DisplayPasskey";
const char kRequestConfirmation[] = "RequestConfirmation";
const char kRequestAuthorization[] = "RequestAuthorization";
const char kAuthorizeService[] = "AuthorizeService";
const char kCancel[] = "Cancel";

// Bluetooth Agent errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_agent

namespace bluetooth_device {
// Bluetooth Device service identifiers.
const char kBluetoothDeviceServiceName[] = "org.bluez";
const char kBluetoothDeviceInterface[] = "org.bluez.Device1";

// Bluetooth Device methods.
const char kConnect[] = "Connect";
const char kDisconnect[] = "Disconnect";
const char kConnectProfile[] = "ConnectProfile";
const char kDisconnectProfile[] = "DisconnectProfile";
const char kPair[] = "Pair";
const char kCancelPairing[] = "CancelPairing";

// Bluetooth Device properties.
const char kAddressProperty[] = "Address";
const char kNameProperty[] = "Name";
const char kIconProperty[] = "Icon";
const char kClassProperty[] = "Class";
const char kAppearanceProperty[] = "Appearance";
const char kUUIDsProperty[] = "UUIDs";
const char kPairedProperty[] = "Paired";
const char kConnectedProperty[] = "Connected";
const char kTrustedProperty[] = "Trusted";
const char kBlockedProperty[] = "Blocked";
const char kAliasProperty[] = "Alias";
const char kAdapterProperty[] = "Adapter";
const char kLegacyPairingProperty[] = "LegacyPairing";
const char kModaliasProperty[] = "Modalias";
const char kRSSIProperty[] = "RSSI";

// Bluetooth Device errors.
const char kErrorNotReady[] = "org.bluez.Error.NotReady";
const char kErrorFailed[] = "org.bluez.Error.Failed";
const char kErrorInProgress[] = "org.bluez.Error.InProgress";
const char kErrorAlreadyConnected[] = "org.bluez.Error.AlreadyConnected";
const char kErrorNotConnected[] = "org.bluez.Error.NotConnected";
const char kErrorDoesNotExist[] = "org.bluez.Error.DoesNotExist";
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";

// Undocumented errors that we know BlueZ returns for Bluetooth Device methods.
const char kErrorNotSupported[] = "org.bluez.Error.NotSupported";
const char kErrorAuthenticationCanceled[] =
    "org.bluez.Error.AuthenticationCanceled";
const char kErrorAuthenticationFailed[] =
    "org.bluez.Error.AuthenticationFailed";
const char kErrorAuthenticationRejected[] =
    "org.bluez.Error.AuthenticationRejected";
const char kErrorAuthenticationTimeout[] =
    "org.bluez.Error.AuthenticationTimeout";
const char kErrorConnectionAttemptFailed[] =
    "org.bluez.Error.ConnectionAttemptFailed";
}  // namespace bluetooth_device

namespace bluetooth_input {
// Bluetooth Input service identifiers.
const char kBluetoothInputServiceName[] = "org.bluez";
const char kBluetoothInputInterface[] = "org.bluez.Input1";

// Bluetooth Input properties.
const char kReconnectModeProperty[] = "ReconnectMode";

// Bluetooth Input property values.
const char kNoneReconnectModeProperty[] = "none";
const char kHostReconnectModeProperty[] = "host";
const char kDeviceReconnectModeProperty[] = "device";
const char kAnyReconnectModeProperty[] = "any";
}  // namespace bluetooth_input

namespace bluetooth_object_manager {
// Bluetooth daemon Object Manager service identifiers.
const char kBluetoothObjectManagerServiceName[] = "org.bluez";
const char kBluetoothObjectManagerServicePath[] = "/";
}  // namespace bluetooth_object_manager

namespace bluetooth_profile_manager {
// Bluetooth Profile Manager service identifiers.
const char kBluetoothProfileManagerServiceName[] = "org.bluez";
const char kBluetoothProfileManagerServicePath[] = "/org/bluez";
const char kBluetoothProfileManagerInterface[] = "org.bluez.ProfileManager1";

// Bluetooth Profile Manager methods.
const char kRegisterProfile[] = "RegisterProfile";
const char kUnregisterProfile[] = "UnregisterProfile";

// Bluetooth Profile Manager option names.
const char kNameOption[] = "Name";
const char kServiceOption[] = "Service";
const char kRoleOption[] = "Role";
const char kChannelOption[] = "Channel";
const char kPSMOption[] = "PSM";
const char kRequireAuthenticationOption[] = "RequireAuthentication";
const char kRequireAuthorizationOption[] = "RequireAuthorization";
const char kAutoConnectOption[] = "AutoConnect";
const char kServiceRecordOption[] = "ServiceRecord";
const char kVersionOption[] = "Version";
const char kFeaturesOption[] = "Features";

// Bluetooth Profile Manager option values.
const char kClientRoleOption[] = "client";
const char kServerRoleOption[] = "server";

// Bluetooth Profile Manager errors.
const char kErrorInvalidArguments[] = "org.bluez.Error.InvalidArguments";
const char kErrorAlreadyExists[] = "org.bluez.Error.AlreadyExists";
}  // namespace bluetooth_profile_manager

namespace bluetooth_profile {
// Bluetooth Profile service identifiers.
const char kBluetoothProfileInterface[] = "org.bluez.Profile1";

// Bluetooth Profile methods.
const char kRelease[] = "Release";
const char kNewConnection[] = "NewConnection";
const char kRequestDisconnection[] = "RequestDisconnection";
const char kCancel[] = "Cancel";

// Bluetooth Profile property names.
const char kVersionProperty[] = "Version";
const char kFeaturesProperty[] = "Features";

// Bluetooth Profile errors.
const char kErrorRejected[] = "org.bluez.Error.Rejected";
const char kErrorCanceled[] = "org.bluez.Error.Canceled";
}  // namespace bluetooth_profile

namespace cros_disks {
const char kCrosDisksInterface[] = "org.chromium.CrosDisks";
const char kCrosDisksServicePath[] = "/org/chromium/CrosDisks";
const char kCrosDisksServiceName[] = "org.chromium.CrosDisks";
const char kCrosDisksServiceError[] = "org.chromium.CrosDisks.Error";

// Methods.
const char kEnumerateAutoMountableDevices[] = "EnumerateAutoMountableDevices";
const char kFormat[] = "Format";
// TODO(benchan): Deprecate FormatDevice method (crosbug.com/22981)
const char kFormatDevice[] = "FormatDevice";
const char kGetDeviceProperties[] = "GetDeviceProperties";
const char kMount[] = "Mount";
const char kUnmount[] = "Unmount";

// Signals.
const char kDeviceAdded[] = "DeviceAdded";
const char kDeviceScanned[] = "DeviceScanned";
const char kDeviceRemoved[] = "DeviceRemoved";
const char kDiskAdded[] = "DiskAdded";
const char kDiskChanged[] = "DiskChanged";
const char kDiskRemoved[] = "DiskRemoved";
// TODO(benchan): Deprecate FormattingFinished signal (crosbug.com/22981)
const char kFormattingFinished[] = "FormattingFinished";
const char kFormatCompleted[] = "FormatCompleted";
const char kMountCompleted[] = "MountCompleted";

// Properties.
const char kDeviceFile[] = "DeviceFile";
const char kDeviceIsDrive[] = "DeviceIsDrive";
const char kDeviceIsMediaAvailable[] = "DeviceIsMediaAvailable";
const char kDeviceIsMounted[] = "DeviceIsMounted";
const char kDeviceIsOnBootDevice[] = "DeviceIsOnBootDevice";
const char kDeviceIsReadOnly[] = "DeviceIsReadOnly";
const char kDeviceIsVirtual[] = "DeviceIsVirtual";
const char kDeviceMediaType[] = "DeviceMediaType";
const char kDeviceMountPaths[] = "DeviceMountPaths";
const char kDevicePresentationHide[] = "DevicePresentationHide";
const char kDeviceSize[] = "DeviceSize";
const char kDriveIsRotational[] = "DriveIsRotational";
const char kDriveModel[] = "DriveModel";
const char kExperimentalFeaturesEnabled[] = "ExperimentalFeaturesEnabled";
const char kIdLabel[] = "IdLabel";
const char kIdUuid[] = "IdUuid";
const char kVendorId[] = "VendorId";
const char kVendorName[] = "VendorName";
const char kProductId[] = "ProductId";
const char kProductName[] = "ProductName";
const char kNativePath[] = "NativePath";

// Enum values.
// DeviceMediaType enum values are reported through UMA.
// All values but DEVICE_MEDIA_NUM_VALUES should not be changed or removed.
// Additional values can be added but DEVICE_MEDIA_NUM_VALUES should always
// be the last value in the enum.
enum DeviceMediaType {
  DEVICE_MEDIA_UNKNOWN = 0,
  DEVICE_MEDIA_USB = 1,
  DEVICE_MEDIA_SD = 2,
  DEVICE_MEDIA_OPTICAL_DISC = 3,
  DEVICE_MEDIA_MOBILE = 4,
  DEVICE_MEDIA_DVD = 5,
  DEVICE_MEDIA_NUM_VALUES,
};

enum FormatErrorType {
  FORMAT_ERROR_NONE = 0,
  FORMAT_ERROR_UNKNOWN = 1,
  FORMAT_ERROR_INTERNAL = 2,
  FORMAT_ERROR_INVALID_DEVICE_PATH = 3,
  FORMAT_ERROR_DEVICE_BEING_FORMATTED = 4,
  FORMAT_ERROR_UNSUPPORTED_FILESYSTEM = 5,
  FORMAT_ERROR_FORMAT_PROGRAM_NOT_FOUND = 6,
  FORMAT_ERROR_FORMAT_PROGRAM_FAILED = 7,
  FORMAT_ERROR_DEVICE_NOT_ALLOWED = 8,
};

// TODO(benchan): After both Chrome and cros-disks use these enum values,
// make these error values contiguous so that they can be directly reported
// via UMA.
enum MountErrorType {
  MOUNT_ERROR_NONE = 0,
  MOUNT_ERROR_UNKNOWN = 1,
  MOUNT_ERROR_INTERNAL = 2,
  MOUNT_ERROR_INVALID_ARGUMENT = 3,
  MOUNT_ERROR_INVALID_PATH = 4,
  MOUNT_ERROR_PATH_ALREADY_MOUNTED = 5,
  MOUNT_ERROR_PATH_NOT_MOUNTED = 6,
  MOUNT_ERROR_DIRECTORY_CREATION_FAILED = 7,
  MOUNT_ERROR_INVALID_MOUNT_OPTIONS = 8,
  MOUNT_ERROR_INVALID_UNMOUNT_OPTIONS = 9,
  MOUNT_ERROR_INSUFFICIENT_PERMISSIONS = 10,
  MOUNT_ERROR_MOUNT_PROGRAM_NOT_FOUND = 11,
  MOUNT_ERROR_MOUNT_PROGRAM_FAILED = 12,
  MOUNT_ERROR_INVALID_DEVICE_PATH = 100,
  MOUNT_ERROR_UNKNOWN_FILESYSTEM = 101,
  MOUNT_ERROR_UNSUPPORTED_FILESYSTEM = 102,
  MOUNT_ERROR_INVALID_ARCHIVE = 201,
  MOUNT_ERROR_UNSUPPORTED_ARCHIVE = 202,
};

// MountSourceType enum values are solely used by Chrome/CrosDisks in
// the MountCompleted signal, and currently not reported through UMA.
enum MountSourceType {
  MOUNT_SOURCE_INVALID = 0,
  MOUNT_SOURCE_REMOVABLE_DEVICE = 1,
  MOUNT_SOURCE_ARCHIVE = 2,
  MOUNT_SOURCE_NETWORK_STORAGE = 3,
};
}  // namespace cros_disks

namespace mtpd {
const char kMtpdInterface[] = "org.chromium.Mtpd";
const char kMtpdServicePath[] = "/org/chromium/Mtpd";
const char kMtpdServiceName[] = "org.chromium.Mtpd";
const char kMtpdServiceError[] = "org.chromium.Mtpd.Error";

// Methods.
const char kEnumerateStorages[] = "EnumerateStorages";
const char kGetStorageInfo[] = "GetStorageInfo";
const char kOpenStorage[] = "OpenStorage";
const char kCloseStorage[] = "CloseStorage";
const char kReadDirectoryByPath[] = "ReadDirectoryByPath";
const char kReadDirectoryById[] = "ReadDirectoryById";
const char kReadFileChunkByPath[] = "ReadFileChunkByPath";
const char kReadFileChunkById[] = "ReadFileChunkById";
const char kGetFileInfoByPath[] = "GetFileInfoByPath";
const char kGetFileInfoById[] = "GetFileInfoById";

// Signals.
const char kMTPStorageAttached[] = "MTPStorageAttached";
const char kMTPStorageDetached[] = "MTPStorageDetached";

// For FileEntry struct:
const uint32_t kInvalidFileId = 0xffffffff;

// For OpenStorage method:
const char kReadOnlyMode[] = "ro";

// For GetFileInfo() method:
// The id of the root node in a storage, as defined by the PTP/MTP standards.
// Use this when referring to the root node in the context of GetFileInfo().
const uint32_t kRootFileId = 0;
}  // namespace mtpd

namespace update_engine {
const char kUpdateEngineInterface[] = "org.chromium.UpdateEngineInterface";
const char kUpdateEngineServicePath[] = "/org/chromium/UpdateEngine";
const char kUpdateEngineServiceName[] = "org.chromium.UpdateEngine";

// Methods.
const char kAttemptUpdate[] = "AttemptUpdate";
const char kGetStatus[] = "GetStatus";
const char kGetTrack[] = "GetTrack";
const char kRebootIfNeeded[] = "RebootIfNeeded";
const char kSetTrack[] = "SetTrack";
const char kSetChannel[] = "SetChannel";
const char kGetChannel[] = "GetChannel";

// Signals.
const char kStatusUpdate[] = "StatusUpdate";
}  // namespace update_engine

namespace debugd {
const char kDebugdInterface[] = "org.chromium.debugd";
const char kDebugdServicePath[] = "/org/chromium/debugd";
const char kDebugdServiceName[] = "org.chromium.debugd";

// Methods.
const char kGetDebugLogs[] = "GetDebugLogs";
const char kGetInterfaces[] = "GetInterfaces";
const char kGetModemStatus[] = "GetModemStatus";
const char kGetNetworkStatus[] = "GetNetworkStatus";
const char kGetPerfData[] = "GetPerfData";
const char kGetRichPerfData[] = "GetRichPerfData";
const char kGetRoutes[] = "GetRoutes";
const char kGetWiMaxStatus[] = "GetWiMaxStatus";
const char kSetDebugMode[] = "SetDebugMode";
const char kSystraceStart[] = "SystraceStart";
const char kSystraceStop[] = "SystraceStop";
const char kSystraceStatus[] = "SystraceStatus";
const char kGetLog[] = "GetLog";
const char kGetAllLogs[] = "GetAllLogs";
const char kGetUserLogFiles[] = "GetUserLogFiles";
const char kGetFeedbackLogs[] = "GetFeedbackLogs";
const char kTestICMP[] = "TestICMP";
const char kTestICMPWithOptions[] = "TestICMPWithOptions";
const char kLogKernelTaskStates[] = "LogKernelTaskStates";
}  // namespace debugd

namespace permission_broker {
const char kPermissionBrokerInterface[] = "org.chromium.PermissionBroker";
const char kPermissionBrokerServicePath[] = "/org/chromium/PermissionBroker";
const char kPermissionBrokerServiceName[] = "org.chromium.PermissionBroker";

// Methods
const char kRequestPathAccess[] = "RequestPathAccess";
const char kRequestUsbAccess[] = "RequestUsbAccess";
}  // namespace permission_broker

namespace system_clock {
const char kSystemClockInterface[] = "org.torproject.tlsdate";
const char kSystemClockServicePath[] = "/org/torproject/tlsdate";
const char kSystemClockServiceName[] = "org.torproject.tlsdate";

// Signals.
const char kSystemClockUpdated[] = "TimeUpdated";
}  // namespace system_clock

namespace cras {
const char kCrasServicePath[] = "/org/chromium/cras";
const char kCrasServiceName[] = "org.chromium.cras";
const char kCrasControlInterface[] = "org.chromium.cras.Control";

// Methods.
const char kSetOutputVolume[] = "SetOutputVolume";
const char kSetOutputNodeVolume[] = "SetOutputNodeVolume";
const char kSetOutputMute[] = "SetOutputMute";
const char kSetOutputUserMute[] = "SetOutputUserMute";
const char kSetInputGain[] = "SetInputGain";
const char kSetInputNodeGain[] = "SetInputNodeGain";
const char kSetInputMute[] = "SetInputMute";
const char kGetVolumeState[] = "GetVolumeState";
const char kGetNodes[] = "GetNodes";
const char kSetActiveOutputNode[] = "SetActiveOutputNode";
const char kSetActiveInputNode[] = "SetActiveInputNode";

// Names of properties returned by GetNodes()
const char kIsInputProperty[] = "IsInput";
const char kIdProperty[] = "Id";
const char kTypeProperty[] = "Type";
const char kNameProperty[] = "Name";
const char kDeviceNameProperty[] = "DeviceName";
const char kActiveProperty[] = "Active";
const char kPluggedTimeProperty[] = "PluggedTime";

// Signals.
const char kOutputVolumeChanged[] = "OutputVolumeChanged";
const char kOutputMuteChanged[] = "OutputMuteChanged";
const char kInputGainChanged[] = "InputGainChanged";
const char kInputMuteChanged[] = "InputMuteChanged";
const char kNodesChanged[] = "NodesChanged";
const char kActiveOutputNodeChanged[] = "ActiveOutputNodeChanged";
const char kActiveInputNodeChanged[] = "ActiveInputNodeChanged";
}  // namespace cras

#endif  // DBUS_SERVICE_CONSTANTS_H_
