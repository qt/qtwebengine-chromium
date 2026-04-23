from frameworks.base.core.proto.android.graphics import rect_pb2 as _rect_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.base.core.proto.android import typedef_pb2 as _typedef_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WindowConfigurationProto(_message.Message):
    __slots__ = ("app_bounds", "windowing_mode", "activity_type", "bounds", "max_bounds")
    APP_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    WINDOWING_MODE_FIELD_NUMBER: _ClassVar[int]
    ACTIVITY_TYPE_FIELD_NUMBER: _ClassVar[int]
    BOUNDS_FIELD_NUMBER: _ClassVar[int]
    MAX_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    app_bounds: _rect_pb2.RectProto
    windowing_mode: int
    activity_type: int
    bounds: _rect_pb2.RectProto
    max_bounds: _rect_pb2.RectProto
    def __init__(self, app_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., windowing_mode: _Optional[int] = ..., activity_type: _Optional[int] = ..., bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., max_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...
