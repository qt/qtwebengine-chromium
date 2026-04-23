from frameworks.base.core.proto.android.app import window_configuration_pb2 as _window_configuration_pb2
from frameworks.base.core.proto.android.content import locale_pb2 as _locale_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ConfigurationProto(_message.Message):
    __slots__ = ("font_scale", "mcc", "mnc", "locales", "screen_layout", "color_mode", "touchscreen", "keyboard", "keyboard_hidden", "hard_keyboard_hidden", "navigation", "navigation_hidden", "orientation", "ui_mode", "screen_width_dp", "screen_height_dp", "smallest_screen_width_dp", "density_dpi", "window_configuration", "locale_list", "font_weight_adjustment", "grammatical_gender")
    FONT_SCALE_FIELD_NUMBER: _ClassVar[int]
    MCC_FIELD_NUMBER: _ClassVar[int]
    MNC_FIELD_NUMBER: _ClassVar[int]
    LOCALES_FIELD_NUMBER: _ClassVar[int]
    SCREEN_LAYOUT_FIELD_NUMBER: _ClassVar[int]
    COLOR_MODE_FIELD_NUMBER: _ClassVar[int]
    TOUCHSCREEN_FIELD_NUMBER: _ClassVar[int]
    KEYBOARD_FIELD_NUMBER: _ClassVar[int]
    KEYBOARD_HIDDEN_FIELD_NUMBER: _ClassVar[int]
    HARD_KEYBOARD_HIDDEN_FIELD_NUMBER: _ClassVar[int]
    NAVIGATION_FIELD_NUMBER: _ClassVar[int]
    NAVIGATION_HIDDEN_FIELD_NUMBER: _ClassVar[int]
    ORIENTATION_FIELD_NUMBER: _ClassVar[int]
    UI_MODE_FIELD_NUMBER: _ClassVar[int]
    SCREEN_WIDTH_DP_FIELD_NUMBER: _ClassVar[int]
    SCREEN_HEIGHT_DP_FIELD_NUMBER: _ClassVar[int]
    SMALLEST_SCREEN_WIDTH_DP_FIELD_NUMBER: _ClassVar[int]
    DENSITY_DPI_FIELD_NUMBER: _ClassVar[int]
    WINDOW_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    LOCALE_LIST_FIELD_NUMBER: _ClassVar[int]
    FONT_WEIGHT_ADJUSTMENT_FIELD_NUMBER: _ClassVar[int]
    GRAMMATICAL_GENDER_FIELD_NUMBER: _ClassVar[int]
    font_scale: float
    mcc: int
    mnc: int
    locales: _containers.RepeatedCompositeFieldContainer[_locale_pb2.LocaleProto]
    screen_layout: int
    color_mode: int
    touchscreen: int
    keyboard: int
    keyboard_hidden: int
    hard_keyboard_hidden: int
    navigation: int
    navigation_hidden: int
    orientation: int
    ui_mode: int
    screen_width_dp: int
    screen_height_dp: int
    smallest_screen_width_dp: int
    density_dpi: int
    window_configuration: _window_configuration_pb2.WindowConfigurationProto
    locale_list: str
    font_weight_adjustment: int
    grammatical_gender: int
    def __init__(self, font_scale: _Optional[float] = ..., mcc: _Optional[int] = ..., mnc: _Optional[int] = ..., locales: _Optional[_Iterable[_Union[_locale_pb2.LocaleProto, _Mapping]]] = ..., screen_layout: _Optional[int] = ..., color_mode: _Optional[int] = ..., touchscreen: _Optional[int] = ..., keyboard: _Optional[int] = ..., keyboard_hidden: _Optional[int] = ..., hard_keyboard_hidden: _Optional[int] = ..., navigation: _Optional[int] = ..., navigation_hidden: _Optional[int] = ..., orientation: _Optional[int] = ..., ui_mode: _Optional[int] = ..., screen_width_dp: _Optional[int] = ..., screen_height_dp: _Optional[int] = ..., smallest_screen_width_dp: _Optional[int] = ..., density_dpi: _Optional[int] = ..., window_configuration: _Optional[_Union[_window_configuration_pb2.WindowConfigurationProto, _Mapping]] = ..., locale_list: _Optional[str] = ..., font_weight_adjustment: _Optional[int] = ..., grammatical_gender: _Optional[int] = ...) -> None: ...

class ResourcesConfigurationProto(_message.Message):
    __slots__ = ("configuration", "sdk_version", "screen_width_px", "screen_height_px")
    CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    SDK_VERSION_FIELD_NUMBER: _ClassVar[int]
    SCREEN_WIDTH_PX_FIELD_NUMBER: _ClassVar[int]
    SCREEN_HEIGHT_PX_FIELD_NUMBER: _ClassVar[int]
    configuration: ConfigurationProto
    sdk_version: int
    screen_width_px: int
    screen_height_px: int
    def __init__(self, configuration: _Optional[_Union[ConfigurationProto, _Mapping]] = ..., sdk_version: _Optional[int] = ..., screen_width_px: _Optional[int] = ..., screen_height_px: _Optional[int] = ...) -> None: ...

class DeviceConfigurationProto(_message.Message):
    __slots__ = ("stable_screen_width_px", "stable_screen_height_px", "stable_density_dpi", "total_ram", "low_ram", "max_cores", "has_secure_screen_lock", "opengl_version", "opengl_extensions", "shared_libraries", "features", "cpu_architectures")
    STABLE_SCREEN_WIDTH_PX_FIELD_NUMBER: _ClassVar[int]
    STABLE_SCREEN_HEIGHT_PX_FIELD_NUMBER: _ClassVar[int]
    STABLE_DENSITY_DPI_FIELD_NUMBER: _ClassVar[int]
    TOTAL_RAM_FIELD_NUMBER: _ClassVar[int]
    LOW_RAM_FIELD_NUMBER: _ClassVar[int]
    MAX_CORES_FIELD_NUMBER: _ClassVar[int]
    HAS_SECURE_SCREEN_LOCK_FIELD_NUMBER: _ClassVar[int]
    OPENGL_VERSION_FIELD_NUMBER: _ClassVar[int]
    OPENGL_EXTENSIONS_FIELD_NUMBER: _ClassVar[int]
    SHARED_LIBRARIES_FIELD_NUMBER: _ClassVar[int]
    FEATURES_FIELD_NUMBER: _ClassVar[int]
    CPU_ARCHITECTURES_FIELD_NUMBER: _ClassVar[int]
    stable_screen_width_px: int
    stable_screen_height_px: int
    stable_density_dpi: int
    total_ram: int
    low_ram: bool
    max_cores: int
    has_secure_screen_lock: bool
    opengl_version: int
    opengl_extensions: _containers.RepeatedScalarFieldContainer[str]
    shared_libraries: _containers.RepeatedScalarFieldContainer[str]
    features: _containers.RepeatedScalarFieldContainer[str]
    cpu_architectures: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, stable_screen_width_px: _Optional[int] = ..., stable_screen_height_px: _Optional[int] = ..., stable_density_dpi: _Optional[int] = ..., total_ram: _Optional[int] = ..., low_ram: _Optional[bool] = ..., max_cores: _Optional[int] = ..., has_secure_screen_lock: _Optional[bool] = ..., opengl_version: _Optional[int] = ..., opengl_extensions: _Optional[_Iterable[str]] = ..., shared_libraries: _Optional[_Iterable[str]] = ..., features: _Optional[_Iterable[str]] = ..., cpu_architectures: _Optional[_Iterable[str]] = ...) -> None: ...

class GlobalConfigurationProto(_message.Message):
    __slots__ = ("resources", "device")
    RESOURCES_FIELD_NUMBER: _ClassVar[int]
    DEVICE_FIELD_NUMBER: _ClassVar[int]
    resources: ResourcesConfigurationProto
    device: DeviceConfigurationProto
    def __init__(self, resources: _Optional[_Union[ResourcesConfigurationProto, _Mapping]] = ..., device: _Optional[_Union[DeviceConfigurationProto, _Mapping]] = ...) -> None: ...
