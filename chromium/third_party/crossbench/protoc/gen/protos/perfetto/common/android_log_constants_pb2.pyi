from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidLogId(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    LID_DEFAULT: _ClassVar[AndroidLogId]
    LID_RADIO: _ClassVar[AndroidLogId]
    LID_EVENTS: _ClassVar[AndroidLogId]
    LID_SYSTEM: _ClassVar[AndroidLogId]
    LID_CRASH: _ClassVar[AndroidLogId]
    LID_STATS: _ClassVar[AndroidLogId]
    LID_SECURITY: _ClassVar[AndroidLogId]
    LID_KERNEL: _ClassVar[AndroidLogId]

class AndroidLogPriority(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    PRIO_UNSPECIFIED: _ClassVar[AndroidLogPriority]
    PRIO_UNUSED: _ClassVar[AndroidLogPriority]
    PRIO_VERBOSE: _ClassVar[AndroidLogPriority]
    PRIO_DEBUG: _ClassVar[AndroidLogPriority]
    PRIO_INFO: _ClassVar[AndroidLogPriority]
    PRIO_WARN: _ClassVar[AndroidLogPriority]
    PRIO_ERROR: _ClassVar[AndroidLogPriority]
    PRIO_FATAL: _ClassVar[AndroidLogPriority]
LID_DEFAULT: AndroidLogId
LID_RADIO: AndroidLogId
LID_EVENTS: AndroidLogId
LID_SYSTEM: AndroidLogId
LID_CRASH: AndroidLogId
LID_STATS: AndroidLogId
LID_SECURITY: AndroidLogId
LID_KERNEL: AndroidLogId
PRIO_UNSPECIFIED: AndroidLogPriority
PRIO_UNUSED: AndroidLogPriority
PRIO_VERBOSE: AndroidLogPriority
PRIO_DEBUG: AndroidLogPriority
PRIO_INFO: AndroidLogPriority
PRIO_WARN: AndroidLogPriority
PRIO_ERROR: AndroidLogPriority
PRIO_FATAL: AndroidLogPriority
