from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class AggStats(_message.Message):
    __slots__ = ("min", "average", "max", "mean_kb", "max_kb")
    MIN_FIELD_NUMBER: _ClassVar[int]
    AVERAGE_FIELD_NUMBER: _ClassVar[int]
    MAX_FIELD_NUMBER: _ClassVar[int]
    MEAN_KB_FIELD_NUMBER: _ClassVar[int]
    MAX_KB_FIELD_NUMBER: _ClassVar[int]
    min: int
    average: int
    max: int
    mean_kb: int
    max_kb: int
    def __init__(self, min: _Optional[int] = ..., average: _Optional[int] = ..., max: _Optional[int] = ..., mean_kb: _Optional[int] = ..., max_kb: _Optional[int] = ...) -> None: ...

class Duration(_message.Message):
    __slots__ = ("start_ms", "end_ms")
    START_MS_FIELD_NUMBER: _ClassVar[int]
    END_MS_FIELD_NUMBER: _ClassVar[int]
    start_ms: int
    end_ms: int
    def __init__(self, start_ms: _Optional[int] = ..., end_ms: _Optional[int] = ...) -> None: ...
