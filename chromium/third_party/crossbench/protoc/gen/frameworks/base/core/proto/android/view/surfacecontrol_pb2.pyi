from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class SurfaceControlProto(_message.Message):
    __slots__ = ("hash_code", "name", "layerId")
    HASH_CODE_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    LAYERID_FIELD_NUMBER: _ClassVar[int]
    hash_code: int
    name: str
    layerId: int
    def __init__(self, hash_code: _Optional[int] = ..., name: _Optional[str] = ..., layerId: _Optional[int] = ...) -> None: ...
