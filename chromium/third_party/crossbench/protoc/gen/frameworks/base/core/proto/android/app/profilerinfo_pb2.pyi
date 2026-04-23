from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class ProfilerInfoProto(_message.Message):
    __slots__ = ("profile_file", "profile_fd", "sampling_interval", "auto_stop_profiler", "streaming_output", "agent", "clock_type", "profiler_output_version")
    PROFILE_FILE_FIELD_NUMBER: _ClassVar[int]
    PROFILE_FD_FIELD_NUMBER: _ClassVar[int]
    SAMPLING_INTERVAL_FIELD_NUMBER: _ClassVar[int]
    AUTO_STOP_PROFILER_FIELD_NUMBER: _ClassVar[int]
    STREAMING_OUTPUT_FIELD_NUMBER: _ClassVar[int]
    AGENT_FIELD_NUMBER: _ClassVar[int]
    CLOCK_TYPE_FIELD_NUMBER: _ClassVar[int]
    PROFILER_OUTPUT_VERSION_FIELD_NUMBER: _ClassVar[int]
    profile_file: str
    profile_fd: int
    sampling_interval: int
    auto_stop_profiler: bool
    streaming_output: bool
    agent: str
    clock_type: int
    profiler_output_version: int
    def __init__(self, profile_file: _Optional[str] = ..., profile_fd: _Optional[int] = ..., sampling_interval: _Optional[int] = ..., auto_stop_profiler: _Optional[bool] = ..., streaming_output: _Optional[bool] = ..., agent: _Optional[str] = ..., clock_type: _Optional[int] = ..., profiler_output_version: _Optional[int] = ...) -> None: ...
