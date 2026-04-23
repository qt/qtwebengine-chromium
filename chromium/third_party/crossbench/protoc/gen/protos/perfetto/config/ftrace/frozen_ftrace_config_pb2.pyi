from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class FrozenFtraceConfig(_message.Message):
    __slots__ = ("instance_name",)
    INSTANCE_NAME_FIELD_NUMBER: _ClassVar[int]
    instance_name: str
    def __init__(self, instance_name: _Optional[str] = ...) -> None: ...
