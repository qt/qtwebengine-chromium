from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PatternMatcherProto(_message.Message):
    __slots__ = ("pattern", "type")
    class Type(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        TYPE_LITERAL: _ClassVar[PatternMatcherProto.Type]
        TYPE_PREFIX: _ClassVar[PatternMatcherProto.Type]
        TYPE_SIMPLE_GLOB: _ClassVar[PatternMatcherProto.Type]
        TYPE_ADVANCED_GLOB: _ClassVar[PatternMatcherProto.Type]
    TYPE_LITERAL: PatternMatcherProto.Type
    TYPE_PREFIX: PatternMatcherProto.Type
    TYPE_SIMPLE_GLOB: PatternMatcherProto.Type
    TYPE_ADVANCED_GLOB: PatternMatcherProto.Type
    PATTERN_FIELD_NUMBER: _ClassVar[int]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    pattern: str
    type: PatternMatcherProto.Type
    def __init__(self, pattern: _Optional[str] = ..., type: _Optional[_Union[PatternMatcherProto.Type, str]] = ...) -> None: ...
