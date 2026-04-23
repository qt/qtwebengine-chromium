from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class V8Config(_message.Message):
    __slots__ = ("log_script_sources", "log_instructions")
    LOG_SCRIPT_SOURCES_FIELD_NUMBER: _ClassVar[int]
    LOG_INSTRUCTIONS_FIELD_NUMBER: _ClassVar[int]
    log_script_sources: bool
    log_instructions: bool
    def __init__(self, log_script_sources: _Optional[bool] = ..., log_instructions: _Optional[bool] = ...) -> None: ...
