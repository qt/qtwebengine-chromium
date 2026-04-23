from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class BundleProto(_message.Message):
    __slots__ = ("parcelled_data_size", "map_data")
    PARCELLED_DATA_SIZE_FIELD_NUMBER: _ClassVar[int]
    MAP_DATA_FIELD_NUMBER: _ClassVar[int]
    parcelled_data_size: int
    map_data: str
    def __init__(self, parcelled_data_size: _Optional[int] = ..., map_data: _Optional[str] = ...) -> None: ...
