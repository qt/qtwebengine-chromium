from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.base.core.proto.android.view import displaycutout_pb2 as _displaycutout_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class DisplayInfoProto(_message.Message):
    __slots__ = ("logical_width", "logical_height", "app_width", "app_height", "name", "flags", "cutout", "type")
    LOGICAL_WIDTH_FIELD_NUMBER: _ClassVar[int]
    LOGICAL_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    APP_WIDTH_FIELD_NUMBER: _ClassVar[int]
    APP_HEIGHT_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    CUTOUT_FIELD_NUMBER: _ClassVar[int]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    logical_width: int
    logical_height: int
    app_width: int
    app_height: int
    name: str
    flags: int
    cutout: _displaycutout_pb2.DisplayCutoutProto
    type: int
    def __init__(self, logical_width: _Optional[int] = ..., logical_height: _Optional[int] = ..., app_width: _Optional[int] = ..., app_height: _Optional[int] = ..., name: _Optional[str] = ..., flags: _Optional[int] = ..., cutout: _Optional[_Union[_displaycutout_pb2.DisplayCutoutProto, _Mapping]] = ..., type: _Optional[int] = ...) -> None: ...
