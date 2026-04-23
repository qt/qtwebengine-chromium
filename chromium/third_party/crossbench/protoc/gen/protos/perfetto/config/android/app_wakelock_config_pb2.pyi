from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class AppWakelocksConfig(_message.Message):
    __slots__ = ("write_delay_ms", "filter_duration_below_ms", "drop_owner_pid")
    WRITE_DELAY_MS_FIELD_NUMBER: _ClassVar[int]
    FILTER_DURATION_BELOW_MS_FIELD_NUMBER: _ClassVar[int]
    DROP_OWNER_PID_FIELD_NUMBER: _ClassVar[int]
    write_delay_ms: int
    filter_duration_below_ms: int
    drop_owner_pid: bool
    def __init__(self, write_delay_ms: _Optional[int] = ..., filter_duration_below_ms: _Optional[int] = ..., drop_owner_pid: _Optional[bool] = ...) -> None: ...
