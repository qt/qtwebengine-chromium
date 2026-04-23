from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class ActivityInfoProto(_message.Message):
    __slots__ = ()
    class ScreenOrientation(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        SCREEN_ORIENTATION_UNSET: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_UNSPECIFIED: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_LANDSCAPE: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_PORTRAIT: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_USER: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_BEHIND: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_SENSOR: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_NOSENSOR: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_SENSOR_LANDSCAPE: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_SENSOR_PORTRAIT: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_REVERSE_LANDSCAPE: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_REVERSE_PORTRAIT: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_FULL_SENSOR: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_USER_LANDSCAPE: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_USER_PORTRAIT: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_FULL_USER: _ClassVar[ActivityInfoProto.ScreenOrientation]
        SCREEN_ORIENTATION_LOCKED: _ClassVar[ActivityInfoProto.ScreenOrientation]
    SCREEN_ORIENTATION_UNSET: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_UNSPECIFIED: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_LANDSCAPE: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_PORTRAIT: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_USER: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_BEHIND: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_SENSOR: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_NOSENSOR: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_SENSOR_LANDSCAPE: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_SENSOR_PORTRAIT: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_REVERSE_LANDSCAPE: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_REVERSE_PORTRAIT: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_FULL_SENSOR: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_USER_LANDSCAPE: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_USER_PORTRAIT: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_FULL_USER: ActivityInfoProto.ScreenOrientation
    SCREEN_ORIENTATION_LOCKED: ActivityInfoProto.ScreenOrientation
    def __init__(self) -> None: ...
