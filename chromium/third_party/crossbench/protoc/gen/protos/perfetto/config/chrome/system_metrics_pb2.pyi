from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class ChromiumSystemMetricsConfig(_message.Message):
    __slots__ = ("sampling_interval_ms",)
    SAMPLING_INTERVAL_MS_FIELD_NUMBER: _ClassVar[int]
    sampling_interval_ms: int
    def __init__(self, sampling_interval_ms: _Optional[int] = ...) -> None: ...
