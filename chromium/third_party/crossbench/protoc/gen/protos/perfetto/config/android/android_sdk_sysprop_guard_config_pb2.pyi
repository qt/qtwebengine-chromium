from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidSdkSyspropGuardConfig(_message.Message):
    __slots__ = ("surfaceflinger_skia_track_events", "hwui_skia_track_events", "hwui_package_name_filter")
    SURFACEFLINGER_SKIA_TRACK_EVENTS_FIELD_NUMBER: _ClassVar[int]
    HWUI_SKIA_TRACK_EVENTS_FIELD_NUMBER: _ClassVar[int]
    HWUI_PACKAGE_NAME_FILTER_FIELD_NUMBER: _ClassVar[int]
    surfaceflinger_skia_track_events: bool
    hwui_skia_track_events: bool
    hwui_package_name_filter: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, surfaceflinger_skia_track_events: _Optional[bool] = ..., hwui_skia_track_events: _Optional[bool] = ..., hwui_package_name_filter: _Optional[_Iterable[str]] = ...) -> None: ...
