from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class KernelWakelocksConfig(_message.Message):
    __slots__ = ("poll_ms",)
    POLL_MS_FIELD_NUMBER: _ClassVar[int]
    poll_ms: int
    def __init__(self, poll_ms: _Optional[int] = ...) -> None: ...
