from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class ProtoLogLevel(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    PROTOLOG_LEVEL_UNDEFINED: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_DEBUG: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_VERBOSE: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_INFO: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_WARN: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_ERROR: _ClassVar[ProtoLogLevel]
    PROTOLOG_LEVEL_WTF: _ClassVar[ProtoLogLevel]
PROTOLOG_LEVEL_UNDEFINED: ProtoLogLevel
PROTOLOG_LEVEL_DEBUG: ProtoLogLevel
PROTOLOG_LEVEL_VERBOSE: ProtoLogLevel
PROTOLOG_LEVEL_INFO: ProtoLogLevel
PROTOLOG_LEVEL_WARN: ProtoLogLevel
PROTOLOG_LEVEL_ERROR: ProtoLogLevel
PROTOLOG_LEVEL_WTF: ProtoLogLevel
