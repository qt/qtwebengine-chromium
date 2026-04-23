from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class NetworkPacketTraceConfig(_message.Message):
    __slots__ = ("poll_ms", "aggregation_threshold", "intern_limit", "drop_local_port", "drop_remote_port", "drop_tcp_flags")
    POLL_MS_FIELD_NUMBER: _ClassVar[int]
    AGGREGATION_THRESHOLD_FIELD_NUMBER: _ClassVar[int]
    INTERN_LIMIT_FIELD_NUMBER: _ClassVar[int]
    DROP_LOCAL_PORT_FIELD_NUMBER: _ClassVar[int]
    DROP_REMOTE_PORT_FIELD_NUMBER: _ClassVar[int]
    DROP_TCP_FLAGS_FIELD_NUMBER: _ClassVar[int]
    poll_ms: int
    aggregation_threshold: int
    intern_limit: int
    drop_local_port: bool
    drop_remote_port: bool
    drop_tcp_flags: bool
    def __init__(self, poll_ms: _Optional[int] = ..., aggregation_threshold: _Optional[int] = ..., intern_limit: _Optional[int] = ..., drop_local_port: _Optional[bool] = ..., drop_remote_port: _Optional[bool] = ..., drop_tcp_flags: _Optional[bool] = ...) -> None: ...
