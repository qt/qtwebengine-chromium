from frameworks.base.core.proto.android.graphics import rect_pb2 as _rect_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class InsetsSourceProto(_message.Message):
    __slots__ = ("type", "frame", "visible_frame", "visible", "type_number")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    FRAME_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FRAME_FIELD_NUMBER: _ClassVar[int]
    VISIBLE_FIELD_NUMBER: _ClassVar[int]
    TYPE_NUMBER_FIELD_NUMBER: _ClassVar[int]
    type: str
    frame: _rect_pb2.RectProto
    visible_frame: _rect_pb2.RectProto
    visible: bool
    type_number: int
    def __init__(self, type: _Optional[str] = ..., frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible_frame: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., visible: _Optional[bool] = ..., type_number: _Optional[int] = ...) -> None: ...
