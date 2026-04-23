from google.protobuf import descriptor_pb2 as _descriptor_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class Destination(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    DEST_LOCAL: _ClassVar[Destination]
    DEST_EXPLICIT: _ClassVar[Destination]
    DEST_AUTOMATIC: _ClassVar[Destination]
    DEST_UNSET: _ClassVar[Destination]
DEST_LOCAL: Destination
DEST_EXPLICIT: Destination
DEST_AUTOMATIC: Destination
DEST_UNSET: Destination
PRIVACY_FIELD_NUMBER: _ClassVar[int]
privacy: _descriptor.FieldDescriptor
MSG_PRIVACY_FIELD_NUMBER: _ClassVar[int]
msg_privacy: _descriptor.FieldDescriptor

class PrivacyFlags(_message.Message):
    __slots__ = ("dest", "patterns")
    DEST_FIELD_NUMBER: _ClassVar[int]
    PATTERNS_FIELD_NUMBER: _ClassVar[int]
    dest: Destination
    patterns: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, dest: _Optional[_Union[Destination, str]] = ..., patterns: _Optional[_Iterable[str]] = ...) -> None: ...
