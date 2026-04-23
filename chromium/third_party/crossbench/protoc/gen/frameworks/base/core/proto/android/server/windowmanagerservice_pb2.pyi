from frameworks.base.core.proto.android.app import statusbarmanager_pb2 as _statusbarmanager_pb2
from frameworks.base.core.proto.android.content import activityinfo_pb2 as _activityinfo_pb2
from frameworks.base.core.proto.android.content import configuration_pb2 as _configuration_pb2
from frameworks.base.core.proto.android.graphics import rect_pb2 as _rect_pb2
from frameworks.base.core.proto.android.server import windowcontainerthumbnail_pb2 as _windowcontainerthumbnail_pb2
from frameworks.base.core.proto.android.server import surfaceanimator_pb2 as _surfaceanimator_pb2
from frameworks.base.core.proto.android.view import displaycutout_pb2 as _displaycutout_pb2
from frameworks.base.core.proto.android.view import displayinfo_pb2 as _displayinfo_pb2
from frameworks.base.core.proto.android.view import surface_pb2 as _surface_pb2
from frameworks.base.core.proto.android.view import windowlayoutparams_pb2 as _windowlayoutparams_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.base.core.proto.android import typedef_pb2 as _typedef_pb2
from frameworks.base.core.proto.android.view import surfacecontrol_pb2 as _surfacecontrol_pb2
from frameworks.base.core.proto.android.view import insetssource_pb2 as _insetssource_pb2
from frameworks.base.core.proto.android.view import insetssourcecontrol_pb2 as _insetssourcecontrol_pb2
from frameworks.proto_logging.stats.enums.view import enums_pb2 as _enums_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WindowManagerServiceDumpProto(_message.Message):
    __slots__ = ("policy", "root_window_container", "focused_window", "focused_app", "input_method_window", "display_frozen", "rotation", "last_orientation", "focused_display_id", "hard_keyboard_available", "window_frames_valid", "back_navigation")
    POLICY_FIELD_NUMBER: _ClassVar[int]
    ROOT_WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_WINDOW_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_APP_FIELD_NUMBER: _ClassVar[int]
    INPUT_METHOD_WINDOW_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_FROZEN_FIELD_NUMBER: _ClassVar[int]
    ROTATION_FIELD_NUMBER: _ClassVar[int]
    LAST_ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    HARD_KEYBOARD_AVAILABLE_FIELD_NUMBER: _ClassVar[int]
    WINDOW_FRAMES_VALID_FIELD_NUMBER: _ClassVar[int]
    BACK_NAVIGATION_FIELD_NUMBER: _ClassVar[int]
    policy: WindowManagerPolicyProto
    root_window_container: RootWindowContainerProto
    focused_window: IdentifierProto
    focused_app: str
    input_method_window: IdentifierProto
    display_frozen: bool
    rotation: int
    last_orientation: int
    focused_display_id: int
    hard_keyboard_available: bool
    window_frames_valid: bool
    back_navigation: BackNavigationProto
    def __init__(self, policy: _Optional[_Union[WindowManagerPolicyProto, _Mapping]] = ..., root_window_container: _Optional[_Union[RootWindowContainerProto, _Mapping]] = ..., focused_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., focused_app: _Optional[str] = ..., input_method_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., display_frozen: _Optional[bool] = ..., rotation: _Optional[int] = ..., last_orientation: _Optional[int] = ..., focused_display_id: _Optional[int] = ..., hard_keyboard_available: _Optional[bool] = ..., window_frames_valid: _Optional[bool] = ..., back_navigation: _Optional[_Union[BackNavigationProto, _Mapping]] = ...) -> None: ...

class RootWindowContainerProto(_message.Message):
    __slots__ = ("window_container", "displays", "windows", "keyguard_controller", "is_home_recents_component", "pending_activities", "default_min_size_resizable_task")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    DISPLAYS_FIELD_NUMBER: _ClassVar[int]
    WINDOWS_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    IS_HOME_RECENTS_COMPONENT_FIELD_NUMBER: _ClassVar[int]
    PENDING_ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
    DEFAULT_MIN_SIZE_RESIZABLE_TASK_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    displays: _containers.RepeatedCompositeFieldContainer[DisplayContentProto]
    windows: _containers.RepeatedCompositeFieldContainer[WindowStateProto]
    keyguard_controller: KeyguardControllerProto
    is_home_recents_component: bool
    pending_activities: _containers.RepeatedCompositeFieldContainer[IdentifierProto]
    default_min_size_resizable_task: int
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., displays: _Optional[_Iterable[_Union[DisplayContentProto, _Mapping]]] = ..., windows: _Optional[_Iterable[_Union[WindowStateProto, _Mapping]]] = ..., keyguard_controller: _Optional[_Union[KeyguardControllerProto, _Mapping]] = ..., is_home_recents_component: _Optional[bool] = ..., pending_activities: _Optional[_Iterable[_Union[IdentifierProto, _Mapping]]] = ..., default_min_size_resizable_task: _Optional[int] = ...) -> None: ...

class BarControllerProto(_message.Message):
    __slots__ = ("state", "transient_state")
    STATE_FIELD_NUMBER: _ClassVar[int]
    TRANSIENT_STATE_FIELD_NUMBER: _ClassVar[int]
    state: _statusbarmanager_pb2.StatusBarManagerProto.WindowState
    transient_state: _statusbarmanager_pb2.StatusBarManagerProto.TransientWindowState
    def __init__(self, state: _Optional[_Union[_statusbarmanager_pb2.StatusBarManagerProto.WindowState, str]] = ..., transient_state: _Optional[_Union[_statusbarmanager_pb2.StatusBarManagerProto.TransientWindowState, str]] = ...) -> None: ...

class WindowOrientationListenerProto(_message.Message):
    __slots__ = ("enabled", "rotation")
    ENABLED_FIELD_NUMBER: _ClassVar[int]
    ROTATION_FIELD_NUMBER: _ClassVar[int]
    enabled: bool
    rotation: _surface_pb2.SurfaceProto.Rotation
    def __init__(self, enabled: _Optional[bool] = ..., rotation: _Optional[_Union[_surface_pb2.SurfaceProto.Rotation, str]] = ...) -> None: ...

class KeyguardServiceDelegateProto(_message.Message):
    __slots__ = ("showing", "occluded", "secure", "screen_state", "interactive_state")
    class ScreenState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        SCREEN_STATE_OFF: _ClassVar[KeyguardServiceDelegateProto.ScreenState]
        SCREEN_STATE_TURNING_ON: _ClassVar[KeyguardServiceDelegateProto.ScreenState]
        SCREEN_STATE_ON: _ClassVar[KeyguardServiceDelegateProto.ScreenState]
        SCREEN_STATE_TURNING_OFF: _ClassVar[KeyguardServiceDelegateProto.ScreenState]
    SCREEN_STATE_OFF: KeyguardServiceDelegateProto.ScreenState
    SCREEN_STATE_TURNING_ON: KeyguardServiceDelegateProto.ScreenState
    SCREEN_STATE_ON: KeyguardServiceDelegateProto.ScreenState
    SCREEN_STATE_TURNING_OFF: KeyguardServiceDelegateProto.ScreenState
    class InteractiveState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        INTERACTIVE_STATE_SLEEP: _ClassVar[KeyguardServiceDelegateProto.InteractiveState]
        INTERACTIVE_STATE_WAKING: _ClassVar[KeyguardServiceDelegateProto.InteractiveState]
        INTERACTIVE_STATE_AWAKE: _ClassVar[KeyguardServiceDelegateProto.InteractiveState]
        INTERACTIVE_STATE_GOING_TO_SLEEP: _ClassVar[KeyguardServiceDelegateProto.InteractiveState]
    INTERACTIVE_STATE_SLEEP: KeyguardServiceDelegateProto.InteractiveState
    INTERACTIVE_STATE_WAKING: KeyguardServiceDelegateProto.InteractiveState
    INTERACTIVE_STATE_AWAKE: KeyguardServiceDelegateProto.InteractiveState
    INTERACTIVE_STATE_GOING_TO_SLEEP: KeyguardServiceDelegateProto.InteractiveState
    SHOWING_FIELD_NUMBER: _ClassVar[int]
    OCCLUDED_FIELD_NUMBER: _ClassVar[int]
    SECURE_FIELD_NUMBER: _ClassVar[int]
    SCREEN_STATE_FIELD_NUMBER: _ClassVar[int]
    INTERACTIVE_STATE_FIELD_NUMBER: _ClassVar[int]
    showing: bool
    occluded: bool
    secure: bool
    screen_state: KeyguardServiceDelegateProto.ScreenState
    interactive_state: KeyguardServiceDelegateProto.InteractiveState
    def __init__(self, showing: _Optional[bool] = ..., occluded: _Optional[bool] = ..., secure: _Optional[bool] = ..., screen_state: _Optional[_Union[KeyguardServiceDelegateProto.ScreenState, str]] = ..., interactive_state: _Optional[_Union[KeyguardServiceDelegateProto.InteractiveState, str]] = ...) -> None: ...

class KeyguardControllerProto(_message.Message):
    __slots__ = ("keyguard_showing", "keyguard_occluded_states", "aod_showing", "keyguard_per_display", "keyguard_going_away")
    KEYGUARD_SHOWING_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_STATES_FIELD_NUMBER: _ClassVar[int]
    AOD_SHOWING_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_PER_DISPLAY_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_GOING_AWAY_FIELD_NUMBER: _ClassVar[int]
    keyguard_showing: bool
    keyguard_occluded_states: _containers.RepeatedCompositeFieldContainer[KeyguardOccludedProto]
    aod_showing: bool
    keyguard_per_display: _containers.RepeatedCompositeFieldContainer[KeyguardPerDisplayProto]
    keyguard_going_away: bool
    def __init__(self, keyguard_showing: _Optional[bool] = ..., keyguard_occluded_states: _Optional[_Iterable[_Union[KeyguardOccludedProto, _Mapping]]] = ..., aod_showing: _Optional[bool] = ..., keyguard_per_display: _Optional[_Iterable[_Union[KeyguardPerDisplayProto, _Mapping]]] = ..., keyguard_going_away: _Optional[bool] = ...) -> None: ...

class KeyguardOccludedProto(_message.Message):
    __slots__ = ("display_id", "keyguard_occluded")
    DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_FIELD_NUMBER: _ClassVar[int]
    display_id: int
    keyguard_occluded: bool
    def __init__(self, display_id: _Optional[int] = ..., keyguard_occluded: _Optional[bool] = ...) -> None: ...

class KeyguardPerDisplayProto(_message.Message):
    __slots__ = ("display_id", "keyguard_showing", "aod_showing", "keyguard_occluded", "keyguard_going_away")
    DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_SHOWING_FIELD_NUMBER: _ClassVar[int]
    AOD_SHOWING_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_GOING_AWAY_FIELD_NUMBER: _ClassVar[int]
    display_id: int
    keyguard_showing: bool
    aod_showing: bool
    keyguard_occluded: bool
    keyguard_going_away: bool
    def __init__(self, display_id: _Optional[int] = ..., keyguard_showing: _Optional[bool] = ..., aod_showing: _Optional[bool] = ..., keyguard_occluded: _Optional[bool] = ..., keyguard_going_away: _Optional[bool] = ...) -> None: ...

class WindowManagerPolicyProto(_message.Message):
    __slots__ = ("last_system_ui_flags", "rotation_mode", "rotation", "orientation", "screen_on_fully", "keyguard_draw_complete", "window_manager_draw_complete", "focused_app_token", "focused_window", "top_fullscreen_opaque_window", "top_fullscreen_opaque_or_dimming_window", "keyguard_occluded", "keyguard_occluded_changed", "keyguard_occluded_pending", "force_status_bar", "force_status_bar_from_keyguard", "status_bar", "navigation_bar", "orientation_listener", "keyguard_delegate")
    class UserRotationMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        USER_ROTATION_FREE: _ClassVar[WindowManagerPolicyProto.UserRotationMode]
        USER_ROTATION_LOCKED: _ClassVar[WindowManagerPolicyProto.UserRotationMode]
    USER_ROTATION_FREE: WindowManagerPolicyProto.UserRotationMode
    USER_ROTATION_LOCKED: WindowManagerPolicyProto.UserRotationMode
    LAST_SYSTEM_UI_FLAGS_FIELD_NUMBER: _ClassVar[int]
    ROTATION_MODE_FIELD_NUMBER: _ClassVar[int]
    ROTATION_FIELD_NUMBER: _ClassVar[int]
    ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    SCREEN_ON_FULLY_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_DRAW_COMPLETE_FIELD_NUMBER: _ClassVar[int]
    WINDOW_MANAGER_DRAW_COMPLETE_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_APP_TOKEN_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_WINDOW_FIELD_NUMBER: _ClassVar[int]
    TOP_FULLSCREEN_OPAQUE_WINDOW_FIELD_NUMBER: _ClassVar[int]
    TOP_FULLSCREEN_OPAQUE_OR_DIMMING_WINDOW_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_CHANGED_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_OCCLUDED_PENDING_FIELD_NUMBER: _ClassVar[int]
    FORCE_STATUS_BAR_FIELD_NUMBER: _ClassVar[int]
    FORCE_STATUS_BAR_FROM_KEYGUARD_FIELD_NUMBER: _ClassVar[int]
    STATUS_BAR_FIELD_NUMBER: _ClassVar[int]
    NAVIGATION_BAR_FIELD_NUMBER: _ClassVar[int]
    ORIENTATION_LISTENER_FIELD_NUMBER: _ClassVar[int]
    KEYGUARD_DELEGATE_FIELD_NUMBER: _ClassVar[int]
    last_system_ui_flags: int
    rotation_mode: WindowManagerPolicyProto.UserRotationMode
    rotation: _surface_pb2.SurfaceProto.Rotation
    orientation: _activityinfo_pb2.ActivityInfoProto.ScreenOrientation
    screen_on_fully: bool
    keyguard_draw_complete: bool
    window_manager_draw_complete: bool
    focused_app_token: str
    focused_window: IdentifierProto
    top_fullscreen_opaque_window: IdentifierProto
    top_fullscreen_opaque_or_dimming_window: IdentifierProto
    keyguard_occluded: bool
    keyguard_occluded_changed: bool
    keyguard_occluded_pending: bool
    force_status_bar: bool
    force_status_bar_from_keyguard: bool
    status_bar: BarControllerProto
    navigation_bar: BarControllerProto
    orientation_listener: WindowOrientationListenerProto
    keyguard_delegate: KeyguardServiceDelegateProto
    def __init__(self, last_system_ui_flags: _Optional[int] = ..., rotation_mode: _Optional[_Union[WindowManagerPolicyProto.UserRotationMode, str]] = ..., rotation: _Optional[_Union[_surface_pb2.SurfaceProto.Rotation, str]] = ..., orientation: _Optional[_Union[_activityinfo_pb2.ActivityInfoProto.ScreenOrientation, str]] = ..., screen_on_fully: _Optional[bool] = ..., keyguard_draw_complete: _Optional[bool] = ..., window_manager_draw_complete: _Optional[bool] = ..., focused_app_token: _Optional[str] = ..., focused_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., top_fullscreen_opaque_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., top_fullscreen_opaque_or_dimming_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., keyguard_occluded: _Optional[bool] = ..., keyguard_occluded_changed: _Optional[bool] = ..., keyguard_occluded_pending: _Optional[bool] = ..., force_status_bar: _Optional[bool] = ..., force_status_bar_from_keyguard: _Optional[bool] = ..., status_bar: _Optional[_Union[BarControllerProto, _Mapping]] = ..., navigation_bar: _Optional[_Union[BarControllerProto, _Mapping]] = ..., orientation_listener: _Optional[_Union[WindowOrientationListenerProto, _Mapping]] = ..., keyguard_delegate: _Optional[_Union[KeyguardServiceDelegateProto, _Mapping]] = ...) -> None: ...

class AppTransitionProto(_message.Message):
    __slots__ = ("app_transition_state", "last_used_app_transition")
    class AppState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        APP_STATE_IDLE: _ClassVar[AppTransitionProto.AppState]
        APP_STATE_READY: _ClassVar[AppTransitionProto.AppState]
        APP_STATE_RUNNING: _ClassVar[AppTransitionProto.AppState]
        APP_STATE_TIMEOUT: _ClassVar[AppTransitionProto.AppState]
    APP_STATE_IDLE: AppTransitionProto.AppState
    APP_STATE_READY: AppTransitionProto.AppState
    APP_STATE_RUNNING: AppTransitionProto.AppState
    APP_STATE_TIMEOUT: AppTransitionProto.AppState
    APP_TRANSITION_STATE_FIELD_NUMBER: _ClassVar[int]
    LAST_USED_APP_TRANSITION_FIELD_NUMBER: _ClassVar[int]
    app_transition_state: AppTransitionProto.AppState
    last_used_app_transition: _enums_pb2.TransitionTypeEnum
    def __init__(self, app_transition_state: _Optional[_Union[AppTransitionProto.AppState, str]] = ..., last_used_app_transition: _Optional[_Union[_enums_pb2.TransitionTypeEnum, str]] = ...) -> None: ...

class DisplayContentProto(_message.Message):
    __slots__ = ("window_container", "id", "docked_task_divider_controller", "pinned_task_controller", "above_app_windows", "below_app_windows", "ime_windows", "dpi", "display_info", "rotation", "screen_rotation_animation", "display_frames", "surface_size", "focused_app", "app_transition", "opening_apps", "closing_apps", "changing_apps", "overlay_windows", "root_display_area", "single_task_instance", "focused_root_task_id", "resumed_activity", "tasks", "display_ready", "input_method_target", "input_method_input_target", "input_method_control_target", "current_focus", "ime_insets_source_provider", "can_show_ime", "display_rotation", "ime_policy", "insets_source_providers", "is_sleeping", "sleep_tokens", "keep_clear_areas", "min_size_of_resizeable_task_dp")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    ID_FIELD_NUMBER: _ClassVar[int]
    DOCKED_TASK_DIVIDER_CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    PINNED_TASK_CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    ABOVE_APP_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    BELOW_APP_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    IME_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    DPI_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_INFO_FIELD_NUMBER: _ClassVar[int]
    ROTATION_FIELD_NUMBER: _ClassVar[int]
    SCREEN_ROTATION_ANIMATION_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_FRAMES_FIELD_NUMBER: _ClassVar[int]
    SURFACE_SIZE_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_APP_FIELD_NUMBER: _ClassVar[int]
    APP_TRANSITION_FIELD_NUMBER: _ClassVar[int]
    OPENING_APPS_FIELD_NUMBER: _ClassVar[int]
    CLOSING_APPS_FIELD_NUMBER: _ClassVar[int]
    CHANGING_APPS_FIELD_NUMBER: _ClassVar[int]
    OVERLAY_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    ROOT_DISPLAY_AREA_FIELD_NUMBER: _ClassVar[int]
    SINGLE_TASK_INSTANCE_FIELD_NUMBER: _ClassVar[int]
    FOCUSED_ROOT_TASK_ID_FIELD_NUMBER: _ClassVar[int]
    RESUMED_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    TASKS_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_READY_FIELD_NUMBER: _ClassVar[int]
    INPUT_METHOD_TARGET_FIELD_NUMBER: _ClassVar[int]
    INPUT_METHOD_INPUT_TARGET_FIELD_NUMBER: _ClassVar[int]
    INPUT_METHOD_CONTROL_TARGET_FIELD_NUMBER: _ClassVar[int]
    CURRENT_FOCUS_FIELD_NUMBER: _ClassVar[int]
    IME_INSETS_SOURCE_PROVIDER_FIELD_NUMBER: _ClassVar[int]
    CAN_SHOW_IME_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_ROTATION_FIELD_NUMBER: _ClassVar[int]
    IME_POLICY_FIELD_NUMBER: _ClassVar[int]
    INSETS_SOURCE_PROVIDERS_FIELD_NUMBER: _ClassVar[int]
    IS_SLEEPING_FIELD_NUMBER: _ClassVar[int]
    SLEEP_TOKENS_FIELD_NUMBER: _ClassVar[int]
    KEEP_CLEAR_AREAS_FIELD_NUMBER: _ClassVar[int]
    MIN_SIZE_OF_RESIZEABLE_TASK_DP_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    id: int
    docked_task_divider_controller: DockedTaskDividerControllerProto
    pinned_task_controller: PinnedTaskControllerProto
    above_app_windows: _containers.RepeatedCompositeFieldContainer[WindowTokenProto]
    below_app_windows: _containers.RepeatedCompositeFieldContainer[WindowTokenProto]
    ime_windows: _containers.RepeatedCompositeFieldContainer[WindowTokenProto]
    dpi: int
    display_info: _displayinfo_pb2.DisplayInfoProto
    rotation: int
    screen_rotation_animation: ScreenRotationAnimationProto
    display_frames: DisplayFramesProto
    surface_size: int
    focused_app: str
    app_transition: AppTransitionProto
    opening_apps: _containers.RepeatedCompositeFieldContainer[IdentifierProto]
    closing_apps: _containers.RepeatedCompositeFieldContainer[IdentifierProto]
    changing_apps: _containers.RepeatedCompositeFieldContainer[IdentifierProto]
    overlay_windows: _containers.RepeatedCompositeFieldContainer[WindowTokenProto]
    root_display_area: DisplayAreaProto
    single_task_instance: bool
    focused_root_task_id: int
    resumed_activity: IdentifierProto
    tasks: _containers.RepeatedCompositeFieldContainer[TaskProto]
    display_ready: bool
    input_method_target: WindowStateProto
    input_method_input_target: WindowStateProto
    input_method_control_target: WindowStateProto
    current_focus: WindowStateProto
    ime_insets_source_provider: ImeInsetsSourceProviderProto
    can_show_ime: bool
    display_rotation: DisplayRotationProto
    ime_policy: int
    insets_source_providers: _containers.RepeatedCompositeFieldContainer[InsetsSourceProviderProto]
    is_sleeping: bool
    sleep_tokens: _containers.RepeatedScalarFieldContainer[str]
    keep_clear_areas: _containers.RepeatedCompositeFieldContainer[_rect_pb2.RectProto]
    min_size_of_resizeable_task_dp: int
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., id: _Optional[int] = ..., docked_task_divider_controller: _Optional[_Union[DockedTaskDividerControllerProto, _Mapping]] = ..., pinned_task_controller: _Optional[_Union[PinnedTaskControllerProto, _Mapping]] = ..., above_app_windows: _Optional[_Iterable[_Union[WindowTokenProto, _Mapping]]] = ..., below_app_windows: _Optional[_Iterable[_Union[WindowTokenProto, _Mapping]]] = ..., ime_windows: _Optional[_Iterable[_Union[WindowTokenProto, _Mapping]]] = ..., dpi: _Optional[int] = ..., display_info: _Optional[_Union[_displayinfo_pb2.DisplayInfoProto, _Mapping]] = ..., rotation: _Optional[int] = ..., screen_rotation_animation: _Optional[_Union[ScreenRotationAnimationProto, _Mapping]] = ..., display_frames: _Optional[_Union[DisplayFramesProto, _Mapping]] = ..., surface_size: _Optional[int] = ..., focused_app: _Optional[str] = ..., app_transition: _Optional[_Union[AppTransitionProto, _Mapping]] = ..., opening_apps: _Optional[_Iterable[_Union[IdentifierProto, _Mapping]]] = ..., closing_apps: _Optional[_Iterable[_Union[IdentifierProto, _Mapping]]] = ..., changing_apps: _Optional[_Iterable[_Union[IdentifierProto, _Mapping]]] = ..., overlay_windows: _Optional[_Iterable[_Union[WindowTokenProto, _Mapping]]] = ..., root_display_area: _Optional[_Union[DisplayAreaProto, _Mapping]] = ..., single_task_instance: _Optional[bool] = ..., focused_root_task_id: _Optional[int] = ..., resumed_activity: _Optional[_Union[IdentifierProto, _Mapping]] = ..., tasks: _Optional[_Iterable[_Union[TaskProto, _Mapping]]] = ..., display_ready: _Optional[bool] = ..., input_method_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., input_method_input_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., input_method_control_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., current_focus: _Optional[_Union[WindowStateProto, _Mapping]] = ..., ime_insets_source_provider: _Optional[_Union[ImeInsetsSourceProviderProto, _Mapping]] = ..., can_show_ime: _Optional[bool] = ..., display_rotation: _Optional[_Union[DisplayRotationProto, _Mapping]] = ..., ime_policy: _Optional[int] = ..., insets_source_providers: _Optional[_Iterable[_Union[InsetsSourceProviderProto, _Mapping]]] = ..., is_sleeping: _Optional[bool] = ..., sleep_tokens: _Optional[_Iterable[str]] = ..., keep_clear_areas: _Optional[_Iterable[_Union[_rect_pb2.RectProto, _Mapping]]] = ..., min_size_of_resizeable_task_dp: _Optional[int] = ...) -> None: ...

class DisplayAreaProto(_message.Message):
    __slots__ = ("window_container", "name", "children", "is_task_display_area", "is_root_display_area", "feature_id", "is_organized", "is_ignoring_orientation_request")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    CHILDREN_FIELD_NUMBER: _ClassVar[int]
    IS_TASK_DISPLAY_AREA_FIELD_NUMBER: _ClassVar[int]
    IS_ROOT_DISPLAY_AREA_FIELD_NUMBER: _ClassVar[int]
    FEATURE_ID_FIELD_NUMBER: _ClassVar[int]
    IS_ORGANIZED_FIELD_NUMBER: _ClassVar[int]
    IS_IGNORING_ORIENTATION_REQUEST_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    name: str
    children: _containers.RepeatedCompositeFieldContainer[DisplayAreaChildProto]
    is_task_display_area: bool
    is_root_display_area: bool
    feature_id: int
    is_organized: bool
    is_ignoring_orientation_request: bool
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., name: _Optional[str] = ..., children: _Optional[_Iterable[_Union[DisplayAreaChildProto, _Mapping]]] = ..., is_task_display_area: _Optional[bool] = ..., is_root_display_area: _Optional[bool] = ..., feature_id: _Optional[int] = ..., is_organized: _Optional[bool] = ..., is_ignoring_orientation_request: _Optional[bool] = ...) -> None: ...

class DisplayAreaChildProto(_message.Message):
    __slots__ = ("display_area", "window", "unknown")
    DISPLAY_AREA_FIELD_NUMBER: _ClassVar[int]
    WINDOW_FIELD_NUMBER: _ClassVar[int]
    UNKNOWN_FIELD_NUMBER: _ClassVar[int]
    display_area: DisplayAreaProto
    window: WindowTokenProto
    unknown: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, display_area: _Optional[_Union[DisplayAreaProto, _Mapping]] = ..., window: _Optional[_Union[WindowTokenProto, _Mapping]] = ..., unknown: _Optional[_Iterable[str]] = ...) -> None: ...

class DisplayFramesProto(_message.Message):
    __slots__ = ("stable_bounds", "dock", "current")
    STABLE_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    DOCK_FIELD_NUMBER: _ClassVar[int]
    CURRENT_FIELD_NUMBER: _ClassVar[int]
    stable_bounds: _rect_pb2.RectProto
    dock: _rect_pb2.RectProto
    current: _rect_pb2.RectProto
    def __init__(self, stable_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., dock: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., current: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...

class DisplayRotationProto(_message.Message):
    __slots__ = ("rotation", "frozen_to_user_rotation", "user_rotation", "fixed_to_user_rotation_mode", "last_orientation", "is_fixed_to_user_rotation")
    ROTATION_FIELD_NUMBER: _ClassVar[int]
    FROZEN_TO_USER_ROTATION_FIELD_NUMBER: _ClassVar[int]
    USER_ROTATION_FIELD_NUMBER: _ClassVar[int]
    FIXED_TO_USER_ROTATION_MODE_FIELD_NUMBER: _ClassVar[int]
    LAST_ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    IS_FIXED_TO_USER_ROTATION_FIELD_NUMBER: _ClassVar[int]
    rotation: int
    frozen_to_user_rotation: bool
    user_rotation: int
    fixed_to_user_rotation_mode: int
    last_orientation: int
    is_fixed_to_user_rotation: bool
    def __init__(self, rotation: _Optional[int] = ..., frozen_to_user_rotation: _Optional[bool] = ..., user_rotation: _Optional[int] = ..., fixed_to_user_rotation_mode: _Optional[int] = ..., last_orientation: _Optional[int] = ..., is_fixed_to_user_rotation: _Optional[bool] = ...) -> None: ...

class DockedTaskDividerControllerProto(_message.Message):
    __slots__ = ("minimized_dock",)
    MINIMIZED_DOCK_FIELD_NUMBER: _ClassVar[int]
    minimized_dock: bool
    def __init__(self, minimized_dock: _Optional[bool] = ...) -> None: ...

class PinnedTaskControllerProto(_message.Message):
    __slots__ = ("default_bounds", "movement_bounds")
    DEFAULT_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    MOVEMENT_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    default_bounds: _rect_pb2.RectProto
    movement_bounds: _rect_pb2.RectProto
    def __init__(self, default_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., movement_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...

class TaskProto(_message.Message):
    __slots__ = ("window_container", "id", "fills_parent", "bounds", "displayed_bounds", "defer_removal", "surface_width", "surface_height", "tasks", "activities", "resumed_activity", "real_activity", "orig_activity", "display_id", "root_task_id", "activity_type", "resize_mode", "min_width", "min_height", "adjusted_bounds", "last_non_fullscreen_bounds", "adjusted_for_ime", "adjust_ime_amount", "adjust_divider_amount", "animating_bounds", "minimize_amount", "created_by_organizer", "affinity", "has_child_pip_activity", "task_fragment")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    ID_FIELD_NUMBER: _ClassVar[int]
    FILLS_PARENT_FIELD_NUMBER: _ClassVar[int]
    BOUNDS_FIELD_NUMBER: _ClassVar[int]
    DISPLAYED_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    DEFER_REMOVAL_FIELD_NUMBER: _ClassVar[int]
    SURFACE_WIDTH_FIELD_NUMBER: _ClassVar[int]
    SURFACE_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    TASKS_FIELD_NUMBER: _ClassVar[int]
    ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
    RESUMED_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    REAL_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    ORIG_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    ROOT_TASK_ID_FIELD_NUMBER: _ClassVar[int]
    ACTIVITY_TYPE_FIELD_NUMBER: _ClassVar[int]
    RESIZE_MODE_FIELD_NUMBER: _ClassVar[int]
    MIN_WIDTH_FIELD_NUMBER: _ClassVar[int]
    MIN_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    ADJUSTED_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    LAST_NON_FULLSCREEN_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    ADJUSTED_FOR_IME_FIELD_NUMBER: _ClassVar[int]
    ADJUST_IME_AMOUNT_FIELD_NUMBER: _ClassVar[int]
    ADJUST_DIVIDER_AMOUNT_FIELD_NUMBER: _ClassVar[int]
    ANIMATING_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    MINIMIZE_AMOUNT_FIELD_NUMBER: _ClassVar[int]
    CREATED_BY_ORGANIZER_FIELD_NUMBER: _ClassVar[int]
    AFFINITY_FIELD_NUMBER: _ClassVar[int]
    HAS_CHILD_PIP_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    TASK_FRAGMENT_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    id: int
    fills_parent: bool
    bounds: _rect_pb2.RectProto
    displayed_bounds: _rect_pb2.RectProto
    defer_removal: bool
    surface_width: int
    surface_height: int
    tasks: _containers.RepeatedCompositeFieldContainer[TaskProto]
    activities: _containers.RepeatedCompositeFieldContainer[ActivityRecordProto]
    resumed_activity: IdentifierProto
    real_activity: str
    orig_activity: str
    display_id: int
    root_task_id: int
    activity_type: int
    resize_mode: int
    min_width: int
    min_height: int
    adjusted_bounds: _rect_pb2.RectProto
    last_non_fullscreen_bounds: _rect_pb2.RectProto
    adjusted_for_ime: bool
    adjust_ime_amount: float
    adjust_divider_amount: float
    animating_bounds: bool
    minimize_amount: float
    created_by_organizer: bool
    affinity: str
    has_child_pip_activity: bool
    task_fragment: TaskFragmentProto
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., id: _Optional[int] = ..., fills_parent: _Optional[bool] = ..., bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., displayed_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., defer_removal: _Optional[bool] = ..., surface_width: _Optional[int] = ..., surface_height: _Optional[int] = ..., tasks: _Optional[_Iterable[_Union[TaskProto, _Mapping]]] = ..., activities: _Optional[_Iterable[_Union[ActivityRecordProto, _Mapping]]] = ..., resumed_activity: _Optional[_Union[IdentifierProto, _Mapping]] = ..., real_activity: _Optional[str] = ..., orig_activity: _Optional[str] = ..., display_id: _Optional[int] = ..., root_task_id: _Optional[int] = ..., activity_type: _Optional[int] = ..., resize_mode: _Optional[int] = ..., min_width: _Optional[int] = ..., min_height: _Optional[int] = ..., adjusted_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., last_non_fullscreen_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., adjusted_for_ime: _Optional[bool] = ..., adjust_ime_amount: _Optional[float] = ..., adjust_divider_amount: _Optional[float] = ..., animating_bounds: _Optional[bool] = ..., minimize_amount: _Optional[float] = ..., created_by_organizer: _Optional[bool] = ..., affinity: _Optional[str] = ..., has_child_pip_activity: _Optional[bool] = ..., task_fragment: _Optional[_Union[TaskFragmentProto, _Mapping]] = ...) -> None: ...

class TaskFragmentProto(_message.Message):
    __slots__ = ("window_container", "display_id", "activity_type", "min_width", "min_height")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    ACTIVITY_TYPE_FIELD_NUMBER: _ClassVar[int]
    MIN_WIDTH_FIELD_NUMBER: _ClassVar[int]
    MIN_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    display_id: int
    activity_type: int
    min_width: int
    min_height: int
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., display_id: _Optional[int] = ..., activity_type: _Optional[int] = ..., min_width: _Optional[int] = ..., min_height: _Optional[int] = ...) -> None: ...

class ActivityRecordProto(_message.Message):
    __slots__ = ("name", "window_token", "last_surface_showing", "is_waiting_for_transition_start", "is_animating", "thumbnail", "fills_parent", "app_stopped", "visible_requested", "client_visible", "defer_hiding_client", "reported_drawn", "reported_visible", "num_interesting_windows", "num_drawn_windows", "all_drawn", "last_all_drawn", "starting_window", "starting_displayed", "starting_moved", "visible_set_from_transferred_starting_window", "frozen_bounds", "visible", "identifier", "state", "front_of_task", "proc_id", "translucent", "pip_auto_enter_enabled", "in_size_compat_mode", "min_aspect_ratio", "provides_max_bounds", "enable_recents_screenshot", "last_drop_input_mode", "override_orientation", "should_send_compat_fake_focus", "should_force_rotate_for_camera_compat", "should_refresh_activity_for_camera_compat", "should_refresh_activity_via_pause_for_camera_compat", "should_override_min_aspect_ratio", "should_ignore_orientation_request_loop", "should_override_force_resize_app", "should_enable_user_aspect_ratio_settings", "is_user_fullscreen_override_enabled")
    NAME_FIELD_NUMBER: _ClassVar[int]
    WINDOW_TOKEN_FIELD_NUMBER: _ClassVar[int]
    LAST_SURFACE_SHOWING_FIELD_NUMBER: _ClassVar[int]
    IS_WAITING_FOR_TRANSITION_START_FIELD_NUMBER: _ClassVar[int]
    IS_ANIMATING_FIELD_NUMBER: _ClassVar[int]
    THUMBNAIL_FIELD_NUMBER: _ClassVar[int]
    FILLS_PARENT_FIELD_NUMBER: _ClassVar[int]
    APP_STOPPED_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_REQUESTED_FIELD_NUMBER: _ClassVar[int]
    CLIENT_VISIBLE_FIELD_NUMBER: _ClassVar[int]
    DEFER_HIDING_CLIENT_FIELD_NUMBER: _ClassVar[int]
    REPORTED_DRAWN_FIELD_NUMBER: _ClassVar[int]
    REPORTED_VISIBLE_FIELD_NUMBER: _ClassVar[int]
    NUM_INTERESTING_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    NUM_DRAWN_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    ALL_DRAWN_FIELD_NUMBER: _ClassVar[int]
    LAST_ALL_DRAWN_FIELD_NUMBER: _ClassVar[int]
    STARTING_WINDOW_FIELD_NUMBER: _ClassVar[int]
    STARTING_DISPLAYED_FIELD_NUMBER: _ClassVar[int]
    STARTING_MOVED_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_SET_FROM_TRANSFERRED_STARTING_WINDOW_FIELD_NUMBER: _ClassVar[int]
    FROZEN_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FIELD_NUMBER: _ClassVar[int]
    IDENTIFIER_FIELD_NUMBER: _ClassVar[int]
    STATE_FIELD_NUMBER: _ClassVar[int]
    FRONT_OF_TASK_FIELD_NUMBER: _ClassVar[int]
    PROC_ID_FIELD_NUMBER: _ClassVar[int]
    TRANSLUCENT_FIELD_NUMBER: _ClassVar[int]
    PIP_AUTO_ENTER_ENABLED_FIELD_NUMBER: _ClassVar[int]
    IN_SIZE_COMPAT_MODE_FIELD_NUMBER: _ClassVar[int]
    MIN_ASPECT_RATIO_FIELD_NUMBER: _ClassVar[int]
    PROVIDES_MAX_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    ENABLE_RECENTS_SCREENSHOT_FIELD_NUMBER: _ClassVar[int]
    LAST_DROP_INPUT_MODE_FIELD_NUMBER: _ClassVar[int]
    OVERRIDE_ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    SHOULD_SEND_COMPAT_FAKE_FOCUS_FIELD_NUMBER: _ClassVar[int]
    SHOULD_FORCE_ROTATE_FOR_CAMERA_COMPAT_FIELD_NUMBER: _ClassVar[int]
    SHOULD_REFRESH_ACTIVITY_FOR_CAMERA_COMPAT_FIELD_NUMBER: _ClassVar[int]
    SHOULD_REFRESH_ACTIVITY_VIA_PAUSE_FOR_CAMERA_COMPAT_FIELD_NUMBER: _ClassVar[int]
    SHOULD_OVERRIDE_MIN_ASPECT_RATIO_FIELD_NUMBER: _ClassVar[int]
    SHOULD_IGNORE_ORIENTATION_REQUEST_LOOP_FIELD_NUMBER: _ClassVar[int]
    SHOULD_OVERRIDE_FORCE_RESIZE_APP_FIELD_NUMBER: _ClassVar[int]
    SHOULD_ENABLE_USER_ASPECT_RATIO_SETTINGS_FIELD_NUMBER: _ClassVar[int]
    IS_USER_FULLSCREEN_OVERRIDE_ENABLED_FIELD_NUMBER: _ClassVar[int]
    name: str
    window_token: WindowTokenProto
    last_surface_showing: bool
    is_waiting_for_transition_start: bool
    is_animating: bool
    thumbnail: _windowcontainerthumbnail_pb2.WindowContainerThumbnailProto
    fills_parent: bool
    app_stopped: bool
    visible_requested: bool
    client_visible: bool
    defer_hiding_client: bool
    reported_drawn: bool
    reported_visible: bool
    num_interesting_windows: int
    num_drawn_windows: int
    all_drawn: bool
    last_all_drawn: bool
    starting_window: IdentifierProto
    starting_displayed: bool
    starting_moved: bool
    visible_set_from_transferred_starting_window: bool
    frozen_bounds: _containers.RepeatedCompositeFieldContainer[_rect_pb2.RectProto]
    visible: bool
    identifier: IdentifierProto
    state: str
    front_of_task: bool
    proc_id: int
    translucent: bool
    pip_auto_enter_enabled: bool
    in_size_compat_mode: bool
    min_aspect_ratio: float
    provides_max_bounds: bool
    enable_recents_screenshot: bool
    last_drop_input_mode: int
    override_orientation: int
    should_send_compat_fake_focus: bool
    should_force_rotate_for_camera_compat: bool
    should_refresh_activity_for_camera_compat: bool
    should_refresh_activity_via_pause_for_camera_compat: bool
    should_override_min_aspect_ratio: bool
    should_ignore_orientation_request_loop: bool
    should_override_force_resize_app: bool
    should_enable_user_aspect_ratio_settings: bool
    is_user_fullscreen_override_enabled: bool
    def __init__(self, name: _Optional[str] = ..., window_token: _Optional[_Union[WindowTokenProto, _Mapping]] = ..., last_surface_showing: _Optional[bool] = ..., is_waiting_for_transition_start: _Optional[bool] = ..., is_animating: _Optional[bool] = ..., thumbnail: _Optional[_Union[_windowcontainerthumbnail_pb2.WindowContainerThumbnailProto, _Mapping]] = ..., fills_parent: _Optional[bool] = ..., app_stopped: _Optional[bool] = ..., visible_requested: _Optional[bool] = ..., client_visible: _Optional[bool] = ..., defer_hiding_client: _Optional[bool] = ..., reported_drawn: _Optional[bool] = ..., reported_visible: _Optional[bool] = ..., num_interesting_windows: _Optional[int] = ..., num_drawn_windows: _Optional[int] = ..., all_drawn: _Optional[bool] = ..., last_all_drawn: _Optional[bool] = ..., starting_window: _Optional[_Union[IdentifierProto, _Mapping]] = ..., starting_displayed: _Optional[bool] = ..., starting_moved: _Optional[bool] = ..., visible_set_from_transferred_starting_window: _Optional[bool] = ..., frozen_bounds: _Optional[_Iterable[_Union[_rect_pb2.RectProto, _Mapping]]] = ..., visible: _Optional[bool] = ..., identifier: _Optional[_Union[IdentifierProto, _Mapping]] = ..., state: _Optional[str] = ..., front_of_task: _Optional[bool] = ..., proc_id: _Optional[int] = ..., translucent: _Optional[bool] = ..., pip_auto_enter_enabled: _Optional[bool] = ..., in_size_compat_mode: _Optional[bool] = ..., min_aspect_ratio: _Optional[float] = ..., provides_max_bounds: _Optional[bool] = ..., enable_recents_screenshot: _Optional[bool] = ..., last_drop_input_mode: _Optional[int] = ..., override_orientation: _Optional[int] = ..., should_send_compat_fake_focus: _Optional[bool] = ..., should_force_rotate_for_camera_compat: _Optional[bool] = ..., should_refresh_activity_for_camera_compat: _Optional[bool] = ..., should_refresh_activity_via_pause_for_camera_compat: _Optional[bool] = ..., should_override_min_aspect_ratio: _Optional[bool] = ..., should_ignore_orientation_request_loop: _Optional[bool] = ..., should_override_force_resize_app: _Optional[bool] = ..., should_enable_user_aspect_ratio_settings: _Optional[bool] = ..., is_user_fullscreen_override_enabled: _Optional[bool] = ...) -> None: ...

class WindowTokenProto(_message.Message):
    __slots__ = ("window_container", "hash_code", "windows", "waiting_to_show", "paused")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    HASH_CODE_FIELD_NUMBER: _ClassVar[int]
    WINDOWS_FIELD_NUMBER: _ClassVar[int]
    WAITING_TO_SHOW_FIELD_NUMBER: _ClassVar[int]
    PAUSED_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    hash_code: int
    windows: _containers.RepeatedCompositeFieldContainer[WindowStateProto]
    waiting_to_show: bool
    paused: bool
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., hash_code: _Optional[int] = ..., windows: _Optional[_Iterable[_Union[WindowStateProto, _Mapping]]] = ..., waiting_to_show: _Optional[bool] = ..., paused: _Optional[bool] = ...) -> None: ...

class WindowStateProto(_message.Message):
    __slots__ = ("window_container", "identifier", "display_id", "stack_id", "attributes", "given_content_insets", "frame", "containing_frame", "parent_frame", "content_frame", "content_insets", "surface_insets", "animator", "animating_exit", "child_windows", "surface_position", "requested_width", "requested_height", "view_visibility", "system_ui_visibility", "has_surface", "is_ready_for_display", "display_frame", "overscan_frame", "visible_frame", "decor_frame", "outset_frame", "overscan_insets", "visible_insets", "stable_insets", "outsets", "cutout", "remove_on_exit", "destroying", "removed", "is_on_screen", "is_visible", "pending_seamless_rotation", "finished_seamless_rotation_frame", "window_frames", "force_seamless_rotation", "has_compat_scale", "global_scale", "keep_clear_areas", "unrestricted_keep_clear_areas", "mergedLocalInsetsSources", "requested_visible_types", "dim_bounds")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    IDENTIFIER_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_ID_FIELD_NUMBER: _ClassVar[int]
    STACK_ID_FIELD_NUMBER: _ClassVar[int]
    ATTRIBUTES_FIELD_NUMBER: _ClassVar[int]
    GIVEN_CONTENT_INSETS_FIELD_NUMBER: _ClassVar[int]
    FRAME_FIELD_NUMBER: _ClassVar[int]
    CONTAINING_FRAME_FIELD_NUMBER: _ClassVar[int]
    PARENT_FRAME_FIELD_NUMBER: _ClassVar[int]
    CONTENT_FRAME_FIELD_NUMBER: _ClassVar[int]
    CONTENT_INSETS_FIELD_NUMBER: _ClassVar[int]
    SURFACE_INSETS_FIELD_NUMBER: _ClassVar[int]
    ANIMATOR_FIELD_NUMBER: _ClassVar[int]
    ANIMATING_EXIT_FIELD_NUMBER: _ClassVar[int]
    CHILD_WINDOWS_FIELD_NUMBER: _ClassVar[int]
    SURFACE_POSITION_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_WIDTH_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    VIEW_VISIBILITY_FIELD_NUMBER: _ClassVar[int]
    SYSTEM_UI_VISIBILITY_FIELD_NUMBER: _ClassVar[int]
    HAS_SURFACE_FIELD_NUMBER: _ClassVar[int]
    IS_READY_FOR_DISPLAY_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_FRAME_FIELD_NUMBER: _ClassVar[int]
    OVERSCAN_FRAME_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FRAME_FIELD_NUMBER: _ClassVar[int]
    DECOR_FRAME_FIELD_NUMBER: _ClassVar[int]
    OUTSET_FRAME_FIELD_NUMBER: _ClassVar[int]
    OVERSCAN_INSETS_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_INSETS_FIELD_NUMBER: _ClassVar[int]
    STABLE_INSETS_FIELD_NUMBER: _ClassVar[int]
    OUTSETS_FIELD_NUMBER: _ClassVar[int]
    CUTOUT_FIELD_NUMBER: _ClassVar[int]
    REMOVE_ON_EXIT_FIELD_NUMBER: _ClassVar[int]
    DESTROYING_FIELD_NUMBER: _ClassVar[int]
    REMOVED_FIELD_NUMBER: _ClassVar[int]
    IS_ON_SCREEN_FIELD_NUMBER: _ClassVar[int]
    IS_VISIBLE_FIELD_NUMBER: _ClassVar[int]
    PENDING_SEAMLESS_ROTATION_FIELD_NUMBER: _ClassVar[int]
    FINISHED_SEAMLESS_ROTATION_FRAME_FIELD_NUMBER: _ClassVar[int]
    WINDOW_FRAMES_FIELD_NUMBER: _ClassVar[int]
    FORCE_SEAMLESS_ROTATION_FIELD_NUMBER: _ClassVar[int]
    HAS_COMPAT_SCALE_FIELD_NUMBER: _ClassVar[int]
    GLOBAL_SCALE_FIELD_NUMBER: _ClassVar[int]
    KEEP_CLEAR_AREAS_FIELD_NUMBER: _ClassVar[int]
    UNRESTRICTED_KEEP_CLEAR_AREAS_FIELD_NUMBER: _ClassVar[int]
    MERGEDLOCALINSETSSOURCES_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_VISIBLE_TYPES_FIELD_NUMBER: _ClassVar[int]
    DIM_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    identifier: IdentifierProto
    display_id: int
    stack_id: int
    attributes: _windowlayoutparams_pb2.WindowLayoutParamsProto
    given_content_insets: _rect_pb2.RectProto
    frame: _rect_pb2.RectProto
    containing_frame: _rect_pb2.RectProto
    parent_frame: _rect_pb2.RectProto
    content_frame: _rect_pb2.RectProto
    content_insets: _rect_pb2.RectProto
    surface_insets: _rect_pb2.RectProto
    animator: WindowStateAnimatorProto
    animating_exit: bool
    child_windows: _containers.RepeatedCompositeFieldContainer[WindowStateProto]
    surface_position: _rect_pb2.RectProto
    requested_width: int
    requested_height: int
    view_visibility: int
    system_ui_visibility: int
    has_surface: bool
    is_ready_for_display: bool
    display_frame: _rect_pb2.RectProto
    overscan_frame: _rect_pb2.RectProto
    visible_frame: _rect_pb2.RectProto
    decor_frame: _rect_pb2.RectProto
    outset_frame: _rect_pb2.RectProto
    overscan_insets: _rect_pb2.RectProto
    visible_insets: _rect_pb2.RectProto
    stable_insets: _rect_pb2.RectProto
    outsets: _rect_pb2.RectProto
    cutout: _displaycutout_pb2.DisplayCutoutProto
    remove_on_exit: bool
    destroying: bool
    removed: bool
    is_on_screen: bool
    is_visible: bool
    pending_seamless_rotation: bool
    finished_seamless_rotation_frame: int
    window_frames: WindowFramesProto
    force_seamless_rotation: bool
    has_compat_scale: bool
    global_scale: float
    keep_clear_areas: _containers.RepeatedCompositeFieldContainer[_rect_pb2.RectProto]
    unrestricted_keep_clear_areas: _containers.RepeatedCompositeFieldContainer[_rect_pb2.RectProto]
    mergedLocalInsetsSources: _containers.RepeatedCompositeFieldContainer[_insetssource_pb2.InsetsSourceProto]
    requested_visible_types: int
    dim_bounds: _rect_pb2.RectProto
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., identifier: _Optional[_Union[IdentifierProto, _Mapping]] = ..., display_id: _Optional[int] = ..., stack_id: _Optional[int] = ..., attributes: _Optional[_Union[_windowlayoutparams_pb2.WindowLayoutParamsProto, _Mapping]] = ..., given_content_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., containing_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., parent_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., content_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., content_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., surface_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., animator: _Optional[_Union[WindowStateAnimatorProto, _Mapping]] = ..., animating_exit: _Optional[bool] = ..., child_windows: _Optional[_Iterable[_Union[WindowStateProto, _Mapping]]] = ..., surface_position: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., requested_width: _Optional[int] = ..., requested_height: _Optional[int] = ..., view_visibility: _Optional[int] = ..., system_ui_visibility: _Optional[int] = ..., has_surface: _Optional[bool] = ..., is_ready_for_display: _Optional[bool] = ..., display_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., overscan_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., decor_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., outset_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., overscan_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., stable_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., outsets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., cutout: _Optional[_Union[_displaycutout_pb2.DisplayCutoutProto, _Mapping]] = ..., remove_on_exit: _Optional[bool] = ..., destroying: _Optional[bool] = ..., removed: _Optional[bool] = ..., is_on_screen: _Optional[bool] = ..., is_visible: _Optional[bool] = ..., pending_seamless_rotation: _Optional[bool] = ..., finished_seamless_rotation_frame: _Optional[int] = ..., window_frames: _Optional[_Union[WindowFramesProto, _Mapping]] = ..., force_seamless_rotation: _Optional[bool] = ..., has_compat_scale: _Optional[bool] = ..., global_scale: _Optional[float] = ..., keep_clear_areas: _Optional[_Iterable[_Union[_rect_pb2.RectProto, _Mapping]]] = ..., unrestricted_keep_clear_areas: _Optional[_Iterable[_Union[_rect_pb2.RectProto, _Mapping]]] = ..., mergedLocalInsetsSources: _Optional[_Iterable[_Union[_insetssource_pb2.InsetsSourceProto, _Mapping]]] = ..., requested_visible_types: _Optional[int] = ..., dim_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...

class IdentifierProto(_message.Message):
    __slots__ = ("hash_code", "user_id", "title")
    HASH_CODE_FIELD_NUMBER: _ClassVar[int]
    USER_ID_FIELD_NUMBER: _ClassVar[int]
    TITLE_FIELD_NUMBER: _ClassVar[int]
    hash_code: int
    user_id: int
    title: str
    def __init__(self, hash_code: _Optional[int] = ..., user_id: _Optional[int] = ..., title: _Optional[str] = ...) -> None: ...

class WindowStateAnimatorProto(_message.Message):
    __slots__ = ("last_clip_rect", "surface", "draw_state", "system_decor_rect")
    class DrawState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        NO_SURFACE: _ClassVar[WindowStateAnimatorProto.DrawState]
        DRAW_PENDING: _ClassVar[WindowStateAnimatorProto.DrawState]
        COMMIT_DRAW_PENDING: _ClassVar[WindowStateAnimatorProto.DrawState]
        READY_TO_SHOW: _ClassVar[WindowStateAnimatorProto.DrawState]
        HAS_DRAWN: _ClassVar[WindowStateAnimatorProto.DrawState]
    NO_SURFACE: WindowStateAnimatorProto.DrawState
    DRAW_PENDING: WindowStateAnimatorProto.DrawState
    COMMIT_DRAW_PENDING: WindowStateAnimatorProto.DrawState
    READY_TO_SHOW: WindowStateAnimatorProto.DrawState
    HAS_DRAWN: WindowStateAnimatorProto.DrawState
    LAST_CLIP_RECT_FIELD_NUMBER: _ClassVar[int]
    SURFACE_FIELD_NUMBER: _ClassVar[int]
    DRAW_STATE_FIELD_NUMBER: _ClassVar[int]
    SYSTEM_DECOR_RECT_FIELD_NUMBER: _ClassVar[int]
    last_clip_rect: _rect_pb2.RectProto
    surface: WindowSurfaceControllerProto
    draw_state: WindowStateAnimatorProto.DrawState
    system_decor_rect: _rect_pb2.RectProto
    def __init__(self, last_clip_rect: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., surface: _Optional[_Union[WindowSurfaceControllerProto, _Mapping]] = ..., draw_state: _Optional[_Union[WindowStateAnimatorProto.DrawState, str]] = ..., system_decor_rect: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...

class WindowSurfaceControllerProto(_message.Message):
    __slots__ = ("shown", "layer")
    SHOWN_FIELD_NUMBER: _ClassVar[int]
    LAYER_FIELD_NUMBER: _ClassVar[int]
    shown: bool
    layer: int
    def __init__(self, shown: _Optional[bool] = ..., layer: _Optional[int] = ...) -> None: ...

class ScreenRotationAnimationProto(_message.Message):
    __slots__ = ("started", "animation_running")
    STARTED_FIELD_NUMBER: _ClassVar[int]
    ANIMATION_RUNNING_FIELD_NUMBER: _ClassVar[int]
    started: bool
    animation_running: bool
    def __init__(self, started: _Optional[bool] = ..., animation_running: _Optional[bool] = ...) -> None: ...

class WindowContainerProto(_message.Message):
    __slots__ = ("configuration_container", "orientation", "visible", "surface_animator", "children", "identifier", "surface_control")
    CONFIGURATION_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FIELD_NUMBER: _ClassVar[int]
    SURFACE_ANIMATOR_FIELD_NUMBER: _ClassVar[int]
    CHILDREN_FIELD_NUMBER: _ClassVar[int]
    IDENTIFIER_FIELD_NUMBER: _ClassVar[int]
    SURFACE_CONTROL_FIELD_NUMBER: _ClassVar[int]
    configuration_container: ConfigurationContainerProto
    orientation: int
    visible: bool
    surface_animator: _surfaceanimator_pb2.SurfaceAnimatorProto
    children: _containers.RepeatedCompositeFieldContainer[WindowContainerChildProto]
    identifier: IdentifierProto
    surface_control: _surfacecontrol_pb2.SurfaceControlProto
    def __init__(self, configuration_container: _Optional[_Union[ConfigurationContainerProto, _Mapping]] = ..., orientation: _Optional[int] = ..., visible: _Optional[bool] = ..., surface_animator: _Optional[_Union[_surfaceanimator_pb2.SurfaceAnimatorProto, _Mapping]] = ..., children: _Optional[_Iterable[_Union[WindowContainerChildProto, _Mapping]]] = ..., identifier: _Optional[_Union[IdentifierProto, _Mapping]] = ..., surface_control: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ...) -> None: ...

class WindowContainerChildProto(_message.Message):
    __slots__ = ("window_container", "display_content", "display_area", "task", "activity", "window_token", "window", "task_fragment")
    WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_CONTENT_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_AREA_FIELD_NUMBER: _ClassVar[int]
    TASK_FIELD_NUMBER: _ClassVar[int]
    ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    WINDOW_TOKEN_FIELD_NUMBER: _ClassVar[int]
    WINDOW_FIELD_NUMBER: _ClassVar[int]
    TASK_FRAGMENT_FIELD_NUMBER: _ClassVar[int]
    window_container: WindowContainerProto
    display_content: DisplayContentProto
    display_area: DisplayAreaProto
    task: TaskProto
    activity: ActivityRecordProto
    window_token: WindowTokenProto
    window: WindowStateProto
    task_fragment: TaskFragmentProto
    def __init__(self, window_container: _Optional[_Union[WindowContainerProto, _Mapping]] = ..., display_content: _Optional[_Union[DisplayContentProto, _Mapping]] = ..., display_area: _Optional[_Union[DisplayAreaProto, _Mapping]] = ..., task: _Optional[_Union[TaskProto, _Mapping]] = ..., activity: _Optional[_Union[ActivityRecordProto, _Mapping]] = ..., window_token: _Optional[_Union[WindowTokenProto, _Mapping]] = ..., window: _Optional[_Union[WindowStateProto, _Mapping]] = ..., task_fragment: _Optional[_Union[TaskFragmentProto, _Mapping]] = ...) -> None: ...

class ConfigurationContainerProto(_message.Message):
    __slots__ = ("override_configuration", "full_configuration", "merged_override_configuration")
    OVERRIDE_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    FULL_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    MERGED_OVERRIDE_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    override_configuration: _configuration_pb2.ConfigurationProto
    full_configuration: _configuration_pb2.ConfigurationProto
    merged_override_configuration: _configuration_pb2.ConfigurationProto
    def __init__(self, override_configuration: _Optional[_Union[_configuration_pb2.ConfigurationProto, _Mapping]] = ..., full_configuration: _Optional[_Union[_configuration_pb2.ConfigurationProto, _Mapping]] = ..., merged_override_configuration: _Optional[_Union[_configuration_pb2.ConfigurationProto, _Mapping]] = ...) -> None: ...

class WindowFramesProto(_message.Message):
    __slots__ = ("containing_frame", "content_frame", "decor_frame", "display_frame", "frame", "outset_frame", "overscan_frame", "parent_frame", "visible_frame", "cutout", "content_insets", "overscan_insets", "visible_insets", "stable_insets", "outsets", "compat_frame")
    CONTAINING_FRAME_FIELD_NUMBER: _ClassVar[int]
    CONTENT_FRAME_FIELD_NUMBER: _ClassVar[int]
    DECOR_FRAME_FIELD_NUMBER: _ClassVar[int]
    DISPLAY_FRAME_FIELD_NUMBER: _ClassVar[int]
    FRAME_FIELD_NUMBER: _ClassVar[int]
    OUTSET_FRAME_FIELD_NUMBER: _ClassVar[int]
    OVERSCAN_FRAME_FIELD_NUMBER: _ClassVar[int]
    PARENT_FRAME_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FRAME_FIELD_NUMBER: _ClassVar[int]
    CUTOUT_FIELD_NUMBER: _ClassVar[int]
    CONTENT_INSETS_FIELD_NUMBER: _ClassVar[int]
    OVERSCAN_INSETS_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_INSETS_FIELD_NUMBER: _ClassVar[int]
    STABLE_INSETS_FIELD_NUMBER: _ClassVar[int]
    OUTSETS_FIELD_NUMBER: _ClassVar[int]
    COMPAT_FRAME_FIELD_NUMBER: _ClassVar[int]
    containing_frame: _rect_pb2.RectProto
    content_frame: _rect_pb2.RectProto
    decor_frame: _rect_pb2.RectProto
    display_frame: _rect_pb2.RectProto
    frame: _rect_pb2.RectProto
    outset_frame: _rect_pb2.RectProto
    overscan_frame: _rect_pb2.RectProto
    parent_frame: _rect_pb2.RectProto
    visible_frame: _rect_pb2.RectProto
    cutout: _displaycutout_pb2.DisplayCutoutProto
    content_insets: _rect_pb2.RectProto
    overscan_insets: _rect_pb2.RectProto
    visible_insets: _rect_pb2.RectProto
    stable_insets: _rect_pb2.RectProto
    outsets: _rect_pb2.RectProto
    compat_frame: _rect_pb2.RectProto
    def __init__(self, containing_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., content_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., decor_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., display_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., outset_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., overscan_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., parent_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., cutout: _Optional[_Union[_displaycutout_pb2.DisplayCutoutProto, _Mapping]] = ..., content_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., overscan_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., stable_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., outsets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., compat_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...

class InsetsSourceProviderProto(_message.Message):
    __slots__ = ("source", "frame", "fake_control", "control", "control_target", "pending_control_target", "fake_control_target", "captured_leash", "ime_overridden_frame", "is_leash_ready_for_dispatching", "client_visible", "server_visible", "seamless_rotating", "finish_seamless_rotate_frame_number", "controllable", "source_window_state")
    SOURCE_FIELD_NUMBER: _ClassVar[int]
    FRAME_FIELD_NUMBER: _ClassVar[int]
    FAKE_CONTROL_FIELD_NUMBER: _ClassVar[int]
    CONTROL_FIELD_NUMBER: _ClassVar[int]
    CONTROL_TARGET_FIELD_NUMBER: _ClassVar[int]
    PENDING_CONTROL_TARGET_FIELD_NUMBER: _ClassVar[int]
    FAKE_CONTROL_TARGET_FIELD_NUMBER: _ClassVar[int]
    CAPTURED_LEASH_FIELD_NUMBER: _ClassVar[int]
    IME_OVERRIDDEN_FRAME_FIELD_NUMBER: _ClassVar[int]
    IS_LEASH_READY_FOR_DISPATCHING_FIELD_NUMBER: _ClassVar[int]
    CLIENT_VISIBLE_FIELD_NUMBER: _ClassVar[int]
    SERVER_VISIBLE_FIELD_NUMBER: _ClassVar[int]
    SEAMLESS_ROTATING_FIELD_NUMBER: _ClassVar[int]
    FINISH_SEAMLESS_ROTATE_FRAME_NUMBER_FIELD_NUMBER: _ClassVar[int]
    CONTROLLABLE_FIELD_NUMBER: _ClassVar[int]
    SOURCE_WINDOW_STATE_FIELD_NUMBER: _ClassVar[int]
    source: _insetssource_pb2.InsetsSourceProto
    frame: _rect_pb2.RectProto
    fake_control: _insetssourcecontrol_pb2.InsetsSourceControlProto
    control: _insetssourcecontrol_pb2.InsetsSourceControlProto
    control_target: WindowStateProto
    pending_control_target: WindowStateProto
    fake_control_target: WindowStateProto
    captured_leash: _surfacecontrol_pb2.SurfaceControlProto
    ime_overridden_frame: _rect_pb2.RectProto
    is_leash_ready_for_dispatching: bool
    client_visible: bool
    server_visible: bool
    seamless_rotating: bool
    finish_seamless_rotate_frame_number: int
    controllable: bool
    source_window_state: WindowStateProto
    def __init__(self, source: _Optional[_Union[_insetssource_pb2.InsetsSourceProto, _Mapping]] = ..., frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., fake_control: _Optional[_Union[_insetssourcecontrol_pb2.InsetsSourceControlProto, _Mapping]] = ..., control: _Optional[_Union[_insetssourcecontrol_pb2.InsetsSourceControlProto, _Mapping]] = ..., control_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., pending_control_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., fake_control_target: _Optional[_Union[WindowStateProto, _Mapping]] = ..., captured_leash: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ..., ime_overridden_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., is_leash_ready_for_dispatching: _Optional[bool] = ..., client_visible: _Optional[bool] = ..., server_visible: _Optional[bool] = ..., seamless_rotating: _Optional[bool] = ..., finish_seamless_rotate_frame_number: _Optional[int] = ..., controllable: _Optional[bool] = ..., source_window_state: _Optional[_Union[WindowStateProto, _Mapping]] = ...) -> None: ...

class ImeInsetsSourceProviderProto(_message.Message):
    __slots__ = ("insets_source_provider", "ime_target_from_ime", "is_ime_layout_drawn")
    INSETS_SOURCE_PROVIDER_FIELD_NUMBER: _ClassVar[int]
    IME_TARGET_FROM_IME_FIELD_NUMBER: _ClassVar[int]
    IS_IME_LAYOUT_DRAWN_FIELD_NUMBER: _ClassVar[int]
    insets_source_provider: InsetsSourceProviderProto
    ime_target_from_ime: WindowStateProto
    is_ime_layout_drawn: bool
    def __init__(self, insets_source_provider: _Optional[_Union[InsetsSourceProviderProto, _Mapping]] = ..., ime_target_from_ime: _Optional[_Union[WindowStateProto, _Mapping]] = ..., is_ime_layout_drawn: _Optional[bool] = ...) -> None: ...

class BackNavigationProto(_message.Message):
    __slots__ = ("animation_in_progress", "last_back_type", "show_wallpaper", "main_open_activity", "animation_running")
    ANIMATION_IN_PROGRESS_FIELD_NUMBER: _ClassVar[int]
    LAST_BACK_TYPE_FIELD_NUMBER: _ClassVar[int]
    SHOW_WALLPAPER_FIELD_NUMBER: _ClassVar[int]
    MAIN_OPEN_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    ANIMATION_RUNNING_FIELD_NUMBER: _ClassVar[int]
    animation_in_progress: bool
    last_back_type: int
    show_wallpaper: bool
    main_open_activity: str
    animation_running: bool
    def __init__(self, animation_in_progress: _Optional[bool] = ..., last_back_type: _Optional[int] = ..., show_wallpaper: _Optional[bool] = ..., main_open_activity: _Optional[str] = ..., animation_running: _Optional[bool] = ...) -> None: ...
