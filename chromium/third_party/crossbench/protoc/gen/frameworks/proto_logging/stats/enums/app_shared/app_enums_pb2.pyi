from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class AppTransitionReasonEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    APP_TRANSITION_REASON_UNKNOWN: _ClassVar[AppTransitionReasonEnum]
    APP_TRANSITION_SPLASH_SCREEN: _ClassVar[AppTransitionReasonEnum]
    APP_TRANSITION_WINDOWS_DRAWN: _ClassVar[AppTransitionReasonEnum]
    APP_TRANSITION_TIMEOUT: _ClassVar[AppTransitionReasonEnum]
    APP_TRANSITION_SNAPSHOT: _ClassVar[AppTransitionReasonEnum]
    APP_TRANSITION_RECENTS_ANIM: _ClassVar[AppTransitionReasonEnum]

class ProcessStateEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    PROCESS_STATE_UNKNOWN_TO_PROTO: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_UNKNOWN: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_PERSISTENT: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_PERSISTENT_UI: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_TOP: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_BOUND_TOP: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_FOREGROUND_SERVICE: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_BOUND_FOREGROUND_SERVICE: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_IMPORTANT_FOREGROUND: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_IMPORTANT_BACKGROUND: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_TRANSIENT_BACKGROUND: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_BACKUP: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_SERVICE: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_RECEIVER: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_TOP_SLEEPING: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_HEAVY_WEIGHT: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_HOME: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_LAST_ACTIVITY: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_CACHED_ACTIVITY: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_CACHED_ACTIVITY_CLIENT: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_CACHED_RECENT: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_CACHED_EMPTY: _ClassVar[ProcessStateEnum]
    PROCESS_STATE_NONEXISTENT: _ClassVar[ProcessStateEnum]

class OomChangeReasonEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    OOM_ADJ_REASON_UNKNOWN_TO_PROTO: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_NONE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_ACTIVITY: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_FINISH_RECEIVER: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_START_RECEIVER: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_BIND_SERVICE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_UNBIND_SERVICE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_START_SERVICE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_GET_PROVIDER: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_REMOVE_PROVIDER: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_UI_VISIBILITY: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_ALLOWLIST: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_PROCESS_BEGIN: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_PROCESS_END: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_SHORT_FGS_TIMEOUT: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_SYSTEM_INIT: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_BACKUP: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_SHELL: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_REMOVE_TASK: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_UID_IDLE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_STOP_SERVICE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_EXECUTING_SERVICE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_RESTRICTION_CHANGE: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_COMPONENT_DISABLED: _ClassVar[OomChangeReasonEnum]
    OOM_ADJ_REASON_FOLLOW_UP: _ClassVar[OomChangeReasonEnum]

class AppExitReasonCode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    REASON_UNKNOWN: _ClassVar[AppExitReasonCode]
    REASON_EXIT_SELF: _ClassVar[AppExitReasonCode]
    REASON_SIGNALED: _ClassVar[AppExitReasonCode]
    REASON_LOW_MEMORY: _ClassVar[AppExitReasonCode]
    REASON_CRASH: _ClassVar[AppExitReasonCode]
    REASON_CRASH_NATIVE: _ClassVar[AppExitReasonCode]
    REASON_ANR: _ClassVar[AppExitReasonCode]
    REASON_INITIALIZATION_FAILURE: _ClassVar[AppExitReasonCode]
    REASON_PERMISSION_CHANGE: _ClassVar[AppExitReasonCode]
    REASON_EXCESSIVE_RESOURCE_USAGE: _ClassVar[AppExitReasonCode]
    REASON_USER_REQUESTED: _ClassVar[AppExitReasonCode]
    REASON_USER_STOPPED: _ClassVar[AppExitReasonCode]
    REASON_DEPENDENCY_DIED: _ClassVar[AppExitReasonCode]
    REASON_OTHER: _ClassVar[AppExitReasonCode]
    REASON_FREEZER: _ClassVar[AppExitReasonCode]
    REASON_PACKAGE_STATE_CHANGE: _ClassVar[AppExitReasonCode]
    REASON_PACKAGE_UPDATED: _ClassVar[AppExitReasonCode]

class AppExitSubReasonCode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    SUBREASON_UNKNOWN: _ClassVar[AppExitSubReasonCode]
    SUBREASON_WAIT_FOR_DEBUGGER: _ClassVar[AppExitSubReasonCode]
    SUBREASON_TOO_MANY_CACHED: _ClassVar[AppExitSubReasonCode]
    SUBREASON_TOO_MANY_EMPTY: _ClassVar[AppExitSubReasonCode]
    SUBREASON_TRIM_EMPTY: _ClassVar[AppExitSubReasonCode]
    SUBREASON_LARGE_CACHED: _ClassVar[AppExitSubReasonCode]
    SUBREASON_MEMORY_PRESSURE: _ClassVar[AppExitSubReasonCode]
    SUBREASON_EXCESSIVE_CPU: _ClassVar[AppExitSubReasonCode]
    SUBREASON_SYSTEM_UPDATE_DONE: _ClassVar[AppExitSubReasonCode]
    SUBREASON_KILL_ALL_FG: _ClassVar[AppExitSubReasonCode]
    SUBREASON_KILL_ALL_BG_EXCEPT: _ClassVar[AppExitSubReasonCode]
    SUBREASON_KILL_UID: _ClassVar[AppExitSubReasonCode]
    SUBREASON_KILL_PID: _ClassVar[AppExitSubReasonCode]
    SUBREASON_INVALID_START: _ClassVar[AppExitSubReasonCode]
    SUBREASON_INVALID_STATE: _ClassVar[AppExitSubReasonCode]
    SUBREASON_IMPERCEPTIBLE: _ClassVar[AppExitSubReasonCode]
    SUBREASON_REMOVE_LRU: _ClassVar[AppExitSubReasonCode]
    SUBREASON_ISOLATED_NOT_NEEDED: _ClassVar[AppExitSubReasonCode]
    SUBREASON_CACHED_IDLE_FORCED_APP_STANDBY: _ClassVar[AppExitSubReasonCode]
    SUBREASON_FREEZER_BINDER_IOCTL: _ClassVar[AppExitSubReasonCode]
    SUBREASON_FREEZER_BINDER_TRANSACTION: _ClassVar[AppExitSubReasonCode]
    SUBREASON_FORCE_STOP: _ClassVar[AppExitSubReasonCode]
    SUBREASON_REMOVE_TASK: _ClassVar[AppExitSubReasonCode]
    SUBREASON_STOP_APP: _ClassVar[AppExitSubReasonCode]
    SUBREASON_KILL_BACKGROUND: _ClassVar[AppExitSubReasonCode]
    SUBREASON_PACKAGE_UPDATE: _ClassVar[AppExitSubReasonCode]
    SUBREASON_UNDELIVERED_BROADCAST: _ClassVar[AppExitSubReasonCode]
    SUBREASON_SDK_SANDBOX_DIED: _ClassVar[AppExitSubReasonCode]
    SUBREASON_SDK_SANDBOX_NOT_NEEDED: _ClassVar[AppExitSubReasonCode]
    SUBREASON_EXCESSIVE_BINDER_OBJECTS: _ClassVar[AppExitSubReasonCode]
    SUBREASON_OOM_KILL: _ClassVar[AppExitSubReasonCode]
    SUBREASON_FREEZER_BINDER_ASYNC_FULL: _ClassVar[AppExitSubReasonCode]
    SUBREASON_EXCESSIVE_OUTGOING_BROADCASTS_WHILE_CACHED: _ClassVar[AppExitSubReasonCode]

class Importance(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    IMPORTANCE_FOREGROUND: _ClassVar[Importance]
    IMPORTANCE_FOREGROUND_SERVICE: _ClassVar[Importance]
    IMPORTANCE_TOP_SLEEPING_PRE_28: _ClassVar[Importance]
    IMPORTANCE_VISIBLE: _ClassVar[Importance]
    IMPORTANCE_PERCEPTIBLE_PRE_26: _ClassVar[Importance]
    IMPORTANCE_PERCEPTIBLE: _ClassVar[Importance]
    IMPORTANCE_CANT_SAVE_STATE_PRE_26: _ClassVar[Importance]
    IMPORTANCE_SERVICE: _ClassVar[Importance]
    IMPORTANCE_TOP_SLEEPING: _ClassVar[Importance]
    IMPORTANCE_CANT_SAVE_STATE: _ClassVar[Importance]
    IMPORTANCE_CACHED: _ClassVar[Importance]
    IMPORTANCE_BACKGROUND: _ClassVar[Importance]
    IMPORTANCE_EMPTY: _ClassVar[Importance]
    IMPORTANCE_GONE: _ClassVar[Importance]

class ResourceApiEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    RESOURCE_API_NONE: _ClassVar[ResourceApiEnum]
    RESOURCE_API_GET_VALUE: _ClassVar[ResourceApiEnum]
    RESOURCE_API_RETRIEVE_ATTRIBUTES: _ClassVar[ResourceApiEnum]

class GameMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    GAME_MODE_UNSPECIFIED: _ClassVar[GameMode]
    GAME_MODE_UNSUPPORTED: _ClassVar[GameMode]
    GAME_MODE_STANDARD: _ClassVar[GameMode]
    GAME_MODE_PERFORMANCE: _ClassVar[GameMode]
    GAME_MODE_BATTERY: _ClassVar[GameMode]
    GAME_MODE_CUSTOM: _ClassVar[GameMode]

class FgsTypePolicyCheckEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    FGS_TYPE_POLICY_CHECK_UNKNOWN: _ClassVar[FgsTypePolicyCheckEnum]
    FGS_TYPE_POLICY_CHECK_OK: _ClassVar[FgsTypePolicyCheckEnum]
    FGS_TYPE_POLICY_CHECK_DEPRECATED: _ClassVar[FgsTypePolicyCheckEnum]
    FGS_TYPE_POLICY_CHECK_DISABLED: _ClassVar[FgsTypePolicyCheckEnum]
    FGS_TYPE_POLICY_CHECK_PERMISSION_DENIED_PERMISSIVE: _ClassVar[FgsTypePolicyCheckEnum]
    FGS_TYPE_POLICY_CHECK_PERMISSION_DENIED_ENFORCED: _ClassVar[FgsTypePolicyCheckEnum]

class HostingComponentType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    HOSTING_COMPONENT_TYPE_EMPTY: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_SYSTEM: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_PERSISTENT: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_BACKUP: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_INSTRUMENTATION: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_ACTIVITY: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_BROADCAST_RECEIVER: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_PROVIDER: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_STARTED_SERVICE: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_FOREGROUND_SERVICE: _ClassVar[HostingComponentType]
    HOSTING_COMPONENT_TYPE_BOUND_SERVICE: _ClassVar[HostingComponentType]

class BroadcastType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BROADCAST_TYPE_NONE: _ClassVar[BroadcastType]
    BROADCAST_TYPE_BACKGROUND: _ClassVar[BroadcastType]
    BROADCAST_TYPE_FOREGROUND: _ClassVar[BroadcastType]
    BROADCAST_TYPE_ALARM: _ClassVar[BroadcastType]
    BROADCAST_TYPE_INTERACTIVE: _ClassVar[BroadcastType]
    BROADCAST_TYPE_ORDERED: _ClassVar[BroadcastType]
    BROADCAST_TYPE_PRIORITIZED: _ClassVar[BroadcastType]
    BROADCAST_TYPE_RESULT_TO: _ClassVar[BroadcastType]
    BROADCAST_TYPE_DEFERRABLE_UNTIL_ACTIVE: _ClassVar[BroadcastType]
    BROADCAST_TYPE_PUSH_MESSAGE: _ClassVar[BroadcastType]
    BROADCAST_TYPE_PUSH_MESSAGE_OVER_QUOTA: _ClassVar[BroadcastType]
    BROADCAST_TYPE_STICKY: _ClassVar[BroadcastType]
    BROADCAST_TYPE_INITIAL_STICKY: _ClassVar[BroadcastType]

class BroadcastDeliveryGroupPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BROADCAST_DELIVERY_GROUP_POLICY_ALL: _ClassVar[BroadcastDeliveryGroupPolicy]
    BROADCAST_DELIVERY_GROUP_POLICY_MOST_RECENT: _ClassVar[BroadcastDeliveryGroupPolicy]
    BROADCAST_DELIVERY_GROUP_POLICY_MERGED: _ClassVar[BroadcastDeliveryGroupPolicy]

class AppStartStartupState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    STARTUP_STATE_STARTED: _ClassVar[AppStartStartupState]
    STARTUP_STATE_ERROR: _ClassVar[AppStartStartupState]
    STARTUP_STATE_FIRST_FRAME_DRAWN: _ClassVar[AppStartStartupState]

class AppStartReasonCode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    START_REASON_ALARM: _ClassVar[AppStartReasonCode]
    START_REASON_BACKUP: _ClassVar[AppStartReasonCode]
    START_REASON_BOOT_COMPLETE: _ClassVar[AppStartReasonCode]
    START_REASON_BROADCAST: _ClassVar[AppStartReasonCode]
    START_REASON_CONTENT_PROVIDER: _ClassVar[AppStartReasonCode]
    START_REASON_JOB: _ClassVar[AppStartReasonCode]
    START_REASON_LAUNCHER: _ClassVar[AppStartReasonCode]
    START_REASON_OTHER: _ClassVar[AppStartReasonCode]
    START_REASON_PUSH: _ClassVar[AppStartReasonCode]
    START_REASON_RESUMED_ACTIVITY: _ClassVar[AppStartReasonCode]
    START_REASON_SERVICE: _ClassVar[AppStartReasonCode]
    START_REASON_START_ACTIVITY: _ClassVar[AppStartReasonCode]

class AppStartStartType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    START_TYPE_COLD: _ClassVar[AppStartStartType]
    START_TYPE_WARM: _ClassVar[AppStartStartType]
    START_TYPE_HOT: _ClassVar[AppStartStartType]

class AppStartLaunchMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    LAUNCH_MODE_STANDARD: _ClassVar[AppStartLaunchMode]
    LAUNCH_MODE_SINGLE_TOP: _ClassVar[AppStartLaunchMode]
    LAUNCH_MODE_SINGLE_INSTANCE: _ClassVar[AppStartLaunchMode]
    LAUNCH_MODE_SINGLE_TASK: _ClassVar[AppStartLaunchMode]
    LAUNCH_MODE_SINGLE_INSTANCE_PER_TASK: _ClassVar[AppStartLaunchMode]
APP_TRANSITION_REASON_UNKNOWN: AppTransitionReasonEnum
APP_TRANSITION_SPLASH_SCREEN: AppTransitionReasonEnum
APP_TRANSITION_WINDOWS_DRAWN: AppTransitionReasonEnum
APP_TRANSITION_TIMEOUT: AppTransitionReasonEnum
APP_TRANSITION_SNAPSHOT: AppTransitionReasonEnum
APP_TRANSITION_RECENTS_ANIM: AppTransitionReasonEnum
PROCESS_STATE_UNKNOWN_TO_PROTO: ProcessStateEnum
PROCESS_STATE_UNKNOWN: ProcessStateEnum
PROCESS_STATE_PERSISTENT: ProcessStateEnum
PROCESS_STATE_PERSISTENT_UI: ProcessStateEnum
PROCESS_STATE_TOP: ProcessStateEnum
PROCESS_STATE_BOUND_TOP: ProcessStateEnum
PROCESS_STATE_FOREGROUND_SERVICE: ProcessStateEnum
PROCESS_STATE_BOUND_FOREGROUND_SERVICE: ProcessStateEnum
PROCESS_STATE_IMPORTANT_FOREGROUND: ProcessStateEnum
PROCESS_STATE_IMPORTANT_BACKGROUND: ProcessStateEnum
PROCESS_STATE_TRANSIENT_BACKGROUND: ProcessStateEnum
PROCESS_STATE_BACKUP: ProcessStateEnum
PROCESS_STATE_SERVICE: ProcessStateEnum
PROCESS_STATE_RECEIVER: ProcessStateEnum
PROCESS_STATE_TOP_SLEEPING: ProcessStateEnum
PROCESS_STATE_HEAVY_WEIGHT: ProcessStateEnum
PROCESS_STATE_HOME: ProcessStateEnum
PROCESS_STATE_LAST_ACTIVITY: ProcessStateEnum
PROCESS_STATE_CACHED_ACTIVITY: ProcessStateEnum
PROCESS_STATE_CACHED_ACTIVITY_CLIENT: ProcessStateEnum
PROCESS_STATE_CACHED_RECENT: ProcessStateEnum
PROCESS_STATE_CACHED_EMPTY: ProcessStateEnum
PROCESS_STATE_NONEXISTENT: ProcessStateEnum
OOM_ADJ_REASON_UNKNOWN_TO_PROTO: OomChangeReasonEnum
OOM_ADJ_REASON_NONE: OomChangeReasonEnum
OOM_ADJ_REASON_ACTIVITY: OomChangeReasonEnum
OOM_ADJ_REASON_FINISH_RECEIVER: OomChangeReasonEnum
OOM_ADJ_REASON_START_RECEIVER: OomChangeReasonEnum
OOM_ADJ_REASON_BIND_SERVICE: OomChangeReasonEnum
OOM_ADJ_REASON_UNBIND_SERVICE: OomChangeReasonEnum
OOM_ADJ_REASON_START_SERVICE: OomChangeReasonEnum
OOM_ADJ_REASON_GET_PROVIDER: OomChangeReasonEnum
OOM_ADJ_REASON_REMOVE_PROVIDER: OomChangeReasonEnum
OOM_ADJ_REASON_UI_VISIBILITY: OomChangeReasonEnum
OOM_ADJ_REASON_ALLOWLIST: OomChangeReasonEnum
OOM_ADJ_REASON_PROCESS_BEGIN: OomChangeReasonEnum
OOM_ADJ_REASON_PROCESS_END: OomChangeReasonEnum
OOM_ADJ_REASON_SHORT_FGS_TIMEOUT: OomChangeReasonEnum
OOM_ADJ_REASON_SYSTEM_INIT: OomChangeReasonEnum
OOM_ADJ_REASON_BACKUP: OomChangeReasonEnum
OOM_ADJ_REASON_SHELL: OomChangeReasonEnum
OOM_ADJ_REASON_REMOVE_TASK: OomChangeReasonEnum
OOM_ADJ_REASON_UID_IDLE: OomChangeReasonEnum
OOM_ADJ_REASON_STOP_SERVICE: OomChangeReasonEnum
OOM_ADJ_REASON_EXECUTING_SERVICE: OomChangeReasonEnum
OOM_ADJ_REASON_RESTRICTION_CHANGE: OomChangeReasonEnum
OOM_ADJ_REASON_COMPONENT_DISABLED: OomChangeReasonEnum
OOM_ADJ_REASON_FOLLOW_UP: OomChangeReasonEnum
REASON_UNKNOWN: AppExitReasonCode
REASON_EXIT_SELF: AppExitReasonCode
REASON_SIGNALED: AppExitReasonCode
REASON_LOW_MEMORY: AppExitReasonCode
REASON_CRASH: AppExitReasonCode
REASON_CRASH_NATIVE: AppExitReasonCode
REASON_ANR: AppExitReasonCode
REASON_INITIALIZATION_FAILURE: AppExitReasonCode
REASON_PERMISSION_CHANGE: AppExitReasonCode
REASON_EXCESSIVE_RESOURCE_USAGE: AppExitReasonCode
REASON_USER_REQUESTED: AppExitReasonCode
REASON_USER_STOPPED: AppExitReasonCode
REASON_DEPENDENCY_DIED: AppExitReasonCode
REASON_OTHER: AppExitReasonCode
REASON_FREEZER: AppExitReasonCode
REASON_PACKAGE_STATE_CHANGE: AppExitReasonCode
REASON_PACKAGE_UPDATED: AppExitReasonCode
SUBREASON_UNKNOWN: AppExitSubReasonCode
SUBREASON_WAIT_FOR_DEBUGGER: AppExitSubReasonCode
SUBREASON_TOO_MANY_CACHED: AppExitSubReasonCode
SUBREASON_TOO_MANY_EMPTY: AppExitSubReasonCode
SUBREASON_TRIM_EMPTY: AppExitSubReasonCode
SUBREASON_LARGE_CACHED: AppExitSubReasonCode
SUBREASON_MEMORY_PRESSURE: AppExitSubReasonCode
SUBREASON_EXCESSIVE_CPU: AppExitSubReasonCode
SUBREASON_SYSTEM_UPDATE_DONE: AppExitSubReasonCode
SUBREASON_KILL_ALL_FG: AppExitSubReasonCode
SUBREASON_KILL_ALL_BG_EXCEPT: AppExitSubReasonCode
SUBREASON_KILL_UID: AppExitSubReasonCode
SUBREASON_KILL_PID: AppExitSubReasonCode
SUBREASON_INVALID_START: AppExitSubReasonCode
SUBREASON_INVALID_STATE: AppExitSubReasonCode
SUBREASON_IMPERCEPTIBLE: AppExitSubReasonCode
SUBREASON_REMOVE_LRU: AppExitSubReasonCode
SUBREASON_ISOLATED_NOT_NEEDED: AppExitSubReasonCode
SUBREASON_CACHED_IDLE_FORCED_APP_STANDBY: AppExitSubReasonCode
SUBREASON_FREEZER_BINDER_IOCTL: AppExitSubReasonCode
SUBREASON_FREEZER_BINDER_TRANSACTION: AppExitSubReasonCode
SUBREASON_FORCE_STOP: AppExitSubReasonCode
SUBREASON_REMOVE_TASK: AppExitSubReasonCode
SUBREASON_STOP_APP: AppExitSubReasonCode
SUBREASON_KILL_BACKGROUND: AppExitSubReasonCode
SUBREASON_PACKAGE_UPDATE: AppExitSubReasonCode
SUBREASON_UNDELIVERED_BROADCAST: AppExitSubReasonCode
SUBREASON_SDK_SANDBOX_DIED: AppExitSubReasonCode
SUBREASON_SDK_SANDBOX_NOT_NEEDED: AppExitSubReasonCode
SUBREASON_EXCESSIVE_BINDER_OBJECTS: AppExitSubReasonCode
SUBREASON_OOM_KILL: AppExitSubReasonCode
SUBREASON_FREEZER_BINDER_ASYNC_FULL: AppExitSubReasonCode
SUBREASON_EXCESSIVE_OUTGOING_BROADCASTS_WHILE_CACHED: AppExitSubReasonCode
IMPORTANCE_FOREGROUND: Importance
IMPORTANCE_FOREGROUND_SERVICE: Importance
IMPORTANCE_TOP_SLEEPING_PRE_28: Importance
IMPORTANCE_VISIBLE: Importance
IMPORTANCE_PERCEPTIBLE_PRE_26: Importance
IMPORTANCE_PERCEPTIBLE: Importance
IMPORTANCE_CANT_SAVE_STATE_PRE_26: Importance
IMPORTANCE_SERVICE: Importance
IMPORTANCE_TOP_SLEEPING: Importance
IMPORTANCE_CANT_SAVE_STATE: Importance
IMPORTANCE_CACHED: Importance
IMPORTANCE_BACKGROUND: Importance
IMPORTANCE_EMPTY: Importance
IMPORTANCE_GONE: Importance
RESOURCE_API_NONE: ResourceApiEnum
RESOURCE_API_GET_VALUE: ResourceApiEnum
RESOURCE_API_RETRIEVE_ATTRIBUTES: ResourceApiEnum
GAME_MODE_UNSPECIFIED: GameMode
GAME_MODE_UNSUPPORTED: GameMode
GAME_MODE_STANDARD: GameMode
GAME_MODE_PERFORMANCE: GameMode
GAME_MODE_BATTERY: GameMode
GAME_MODE_CUSTOM: GameMode
FGS_TYPE_POLICY_CHECK_UNKNOWN: FgsTypePolicyCheckEnum
FGS_TYPE_POLICY_CHECK_OK: FgsTypePolicyCheckEnum
FGS_TYPE_POLICY_CHECK_DEPRECATED: FgsTypePolicyCheckEnum
FGS_TYPE_POLICY_CHECK_DISABLED: FgsTypePolicyCheckEnum
FGS_TYPE_POLICY_CHECK_PERMISSION_DENIED_PERMISSIVE: FgsTypePolicyCheckEnum
FGS_TYPE_POLICY_CHECK_PERMISSION_DENIED_ENFORCED: FgsTypePolicyCheckEnum
HOSTING_COMPONENT_TYPE_EMPTY: HostingComponentType
HOSTING_COMPONENT_TYPE_SYSTEM: HostingComponentType
HOSTING_COMPONENT_TYPE_PERSISTENT: HostingComponentType
HOSTING_COMPONENT_TYPE_BACKUP: HostingComponentType
HOSTING_COMPONENT_TYPE_INSTRUMENTATION: HostingComponentType
HOSTING_COMPONENT_TYPE_ACTIVITY: HostingComponentType
HOSTING_COMPONENT_TYPE_BROADCAST_RECEIVER: HostingComponentType
HOSTING_COMPONENT_TYPE_PROVIDER: HostingComponentType
HOSTING_COMPONENT_TYPE_STARTED_SERVICE: HostingComponentType
HOSTING_COMPONENT_TYPE_FOREGROUND_SERVICE: HostingComponentType
HOSTING_COMPONENT_TYPE_BOUND_SERVICE: HostingComponentType
BROADCAST_TYPE_NONE: BroadcastType
BROADCAST_TYPE_BACKGROUND: BroadcastType
BROADCAST_TYPE_FOREGROUND: BroadcastType
BROADCAST_TYPE_ALARM: BroadcastType
BROADCAST_TYPE_INTERACTIVE: BroadcastType
BROADCAST_TYPE_ORDERED: BroadcastType
BROADCAST_TYPE_PRIORITIZED: BroadcastType
BROADCAST_TYPE_RESULT_TO: BroadcastType
BROADCAST_TYPE_DEFERRABLE_UNTIL_ACTIVE: BroadcastType
BROADCAST_TYPE_PUSH_MESSAGE: BroadcastType
BROADCAST_TYPE_PUSH_MESSAGE_OVER_QUOTA: BroadcastType
BROADCAST_TYPE_STICKY: BroadcastType
BROADCAST_TYPE_INITIAL_STICKY: BroadcastType
BROADCAST_DELIVERY_GROUP_POLICY_ALL: BroadcastDeliveryGroupPolicy
BROADCAST_DELIVERY_GROUP_POLICY_MOST_RECENT: BroadcastDeliveryGroupPolicy
BROADCAST_DELIVERY_GROUP_POLICY_MERGED: BroadcastDeliveryGroupPolicy
STARTUP_STATE_STARTED: AppStartStartupState
STARTUP_STATE_ERROR: AppStartStartupState
STARTUP_STATE_FIRST_FRAME_DRAWN: AppStartStartupState
START_REASON_ALARM: AppStartReasonCode
START_REASON_BACKUP: AppStartReasonCode
START_REASON_BOOT_COMPLETE: AppStartReasonCode
START_REASON_BROADCAST: AppStartReasonCode
START_REASON_CONTENT_PROVIDER: AppStartReasonCode
START_REASON_JOB: AppStartReasonCode
START_REASON_LAUNCHER: AppStartReasonCode
START_REASON_OTHER: AppStartReasonCode
START_REASON_PUSH: AppStartReasonCode
START_REASON_RESUMED_ACTIVITY: AppStartReasonCode
START_REASON_SERVICE: AppStartReasonCode
START_REASON_START_ACTIVITY: AppStartReasonCode
START_TYPE_COLD: AppStartStartType
START_TYPE_WARM: AppStartStartType
START_TYPE_HOT: AppStartStartType
LAUNCH_MODE_STANDARD: AppStartLaunchMode
LAUNCH_MODE_SINGLE_TOP: AppStartLaunchMode
LAUNCH_MODE_SINGLE_INSTANCE: AppStartLaunchMode
LAUNCH_MODE_SINGLE_TASK: AppStartLaunchMode
LAUNCH_MODE_SINGLE_INSTANCE_PER_TASK: AppStartLaunchMode
