from frameworks.base.core.proto.android.graphics import point_pb2 as _point_pb2
from frameworks.base.core.proto.android.view import surfacecontrol_pb2 as _surfacecontrol_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class InsetsSourceControlProto(_message.Message):
    __slots__ = ("type", "position", "leash", "type_number")
    TYPE_FIELD_NUMBER: _ClassVar[int]
    POSITION_FIELD_NUMBER: _ClassVar[int]
    LEASH_FIELD_NUMBER: _ClassVar[int]
    TYPE_NUMBER_FIELD_NUMBER: _ClassVar[int]
    type: str
    position: _point_pb2.PointProto
    leash: _surfacecontrol_pb2.SurfaceControlProto
    type_number: int
    def __init__(self, type: _Optional[str] = ..., position: _Optional[_Union[_point_pb2.PointProto, _Mapping]] = ..., leash: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ..., type_number: _Optional[int] = ...) -> None: ...
