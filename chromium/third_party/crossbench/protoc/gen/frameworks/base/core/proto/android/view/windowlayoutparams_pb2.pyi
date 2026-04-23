from frameworks.base.core.proto.android.graphics import pixelformat_pb2 as _pixelformat_pb2
from frameworks.base.core.proto.android.view import display_pb2 as _display_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.base.core.proto.android import typedef_pb2 as _typedef_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WindowLayoutParamsProto(_message.Message):
    __slots__ = ("type", "x", "y", "width", "height", "horizontal_margin", "vertical_margin", "gravity", "soft_input_mode", "format", "window_animations", "alpha", "screen_brightness", "button_brightness", "rotation_animation", "preferred_refresh_rate", "preferred_display_mode_id", "has_system_ui_listeners", "input_feature_flags", "user_activity_timeout", "color_mode", "flags", "private_flags", "system_ui_visibility_flags", "subtree_system_ui_visibility_flags", "appearance", "behavior", "fit_insets_types", "fit_insets_sides", "fit_ignore_visibility")
    class RotationAnimation(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        ROTATION_ANIMATION_UNSPECIFIED: _ClassVar[WindowLayoutParamsProto.RotationAnimation]
        ROTATION_ANIMATION_CROSSFADE: _ClassVar[WindowLayoutParamsProto.RotationAnimation]
        ROTATION_ANIMATION_JUMPCUT: _ClassVar[WindowLayoutParamsProto.RotationAnimation]
        ROTATION_ANIMATION_SEAMLESS: _ClassVar[WindowLayoutParamsProto.RotationAnimation]
    ROTATION_ANIMATION_UNSPECIFIED: WindowLayoutParamsProto.RotationAnimation
    ROTATION_ANIMATION_CROSSFADE: WindowLayoutParamsProto.RotationAnimation
    ROTATION_ANIMATION_JUMPCUT: WindowLayoutParamsProto.RotationAnimation
    ROTATION_ANIMATION_SEAMLESS: WindowLayoutParamsProto.RotationAnimation
    TYPE_FIELD_NUMBER: _ClassVar[int]
    X_FIELD_NUMBER: _ClassVar[int]
    Y_FIELD_NUMBER: _ClassVar[int]
    WIDTH_FIELD_NUMBER: _ClassVar[int]
    HEIGHT_FIELD_NUMBER: _ClassVar[int]
    HORIZONTAL_MARGIN_FIELD_NUMBER: _ClassVar[int]
    VERTICAL_MARGIN_FIELD_NUMBER: _ClassVar[int]
    GRAVITY_FIELD_NUMBER: _ClassVar[int]
    SOFT_INPUT_MODE_FIELD_NUMBER: _ClassVar[int]
    FORMAT_FIELD_NUMBER: _ClassVar[int]
    WINDOW_ANIMATIONS_FIELD_NUMBER: _ClassVar[int]
    ALPHA_FIELD_NUMBER: _ClassVar[int]
    SCREEN_BRIGHTNESS_FIELD_NUMBER: _ClassVar[int]
    BUTTON_BRIGHTNESS_FIELD_NUMBER: _ClassVar[int]
    ROTATION_ANIMATION_FIELD_NUMBER: _ClassVar[int]
    PREFERRED_REFRESH_RATE_FIELD_NUMBER: _ClassVar[int]
    PREFERRED_DISPLAY_MODE_ID_FIELD_NUMBER: _ClassVar[int]
    HAS_SYSTEM_UI_LISTENERS_FIELD_NUMBER: _ClassVar[int]
    INPUT_FEATURE_FLAGS_FIELD_NUMBER: _ClassVar[int]
    USER_ACTIVITY_TIMEOUT_FIELD_NUMBER: _ClassVar[int]
    COLOR_MODE_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    PRIVATE_FLAGS_FIELD_NUMBER: _ClassVar[int]
    SYSTEM_UI_VISIBILITY_FLAGS_FIELD_NUMBER: _ClassVar[int]
    SUBTREE_SYSTEM_UI_VISIBILITY_FLAGS_FIELD_NUMBER: _ClassVar[int]
    APPEARANCE_FIELD_NUMBER: _ClassVar[int]
    BEHAVIOR_FIELD_NUMBER: _ClassVar[int]
    FIT_INSETS_TYPES_FIELD_NUMBER: _ClassVar[int]
    FIT_INSETS_SIDES_FIELD_NUMBER: _ClassVar[int]
    FIT_IGNORE_VISIBILITY_FIELD_NUMBER: _ClassVar[int]
    type: int
    x: int
    y: int
    width: int
    height: int
    horizontal_margin: float
    vertical_margin: float
    gravity: int
    soft_input_mode: int
    format: _pixelformat_pb2.PixelFormatProto.Format
    window_animations: int
    alpha: float
    screen_brightness: float
    button_brightness: float
    rotation_animation: WindowLayoutParamsProto.RotationAnimation
    preferred_refresh_rate: float
    preferred_display_mode_id: int
    has_system_ui_listeners: bool
    input_feature_flags: int
    user_activity_timeout: int
    color_mode: _display_pb2.DisplayProto.ColorMode
    flags: int
    private_flags: int
    system_ui_visibility_flags: int
    subtree_system_ui_visibility_flags: int
    appearance: int
    behavior: int
    fit_insets_types: int
    fit_insets_sides: int
    fit_ignore_visibility: bool
    def __init__(self, type: _Optional[int] = ..., x: _Optional[int] = ..., y: _Optional[int] = ..., width: _Optional[int] = ..., height: _Optional[int] = ..., horizontal_margin: _Optional[float] = ..., vertical_margin: _Optional[float] = ..., gravity: _Optional[int] = ..., soft_input_mode: _Optional[int] = ..., format: _Optional[_Union[_pixelformat_pb2.PixelFormatProto.Format, str]] = ..., window_animations: _Optional[int] = ..., alpha: _Optional[float] = ..., screen_brightness: _Optional[float] = ..., button_brightness: _Optional[float] = ..., rotation_animation: _Optional[_Union[WindowLayoutParamsProto.RotationAnimation, str]] = ..., preferred_refresh_rate: _Optional[float] = ..., preferred_display_mode_id: _Optional[int] = ..., has_system_ui_listeners: _Optional[bool] = ..., input_feature_flags: _Optional[int] = ..., user_activity_timeout: _Optional[int] = ..., color_mode: _Optional[_Union[_display_pb2.DisplayProto.ColorMode, str]] = ..., flags: _Optional[int] = ..., private_flags: _Optional[int] = ..., system_ui_visibility_flags: _Optional[int] = ..., subtree_system_ui_visibility_flags: _Optional[int] = ..., appearance: _Optional[int] = ..., behavior: _Optional[int] = ..., fit_insets_types: _Optional[int] = ..., fit_insets_sides: _Optional[int] = ..., fit_ignore_visibility: _Optional[bool] = ...) -> None: ...
