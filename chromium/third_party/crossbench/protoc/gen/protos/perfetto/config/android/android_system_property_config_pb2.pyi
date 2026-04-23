from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidSystemPropertyConfig(_message.Message):
    __slots__ = ("poll_ms", "property_name")
    POLL_MS_FIELD_NUMBER: _ClassVar[int]
    PROPERTY_NAME_FIELD_NUMBER: _ClassVar[int]
    poll_ms: int
    property_name: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, poll_ms: _Optional[int] = ..., property_name: _Optional[_Iterable[str]] = ...) -> None: ...
