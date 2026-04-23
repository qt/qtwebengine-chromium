from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class BuiltinClock(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BUILTIN_CLOCK_UNKNOWN: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_REALTIME: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_REALTIME_COARSE: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_MONOTONIC: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_MONOTONIC_COARSE: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_MONOTONIC_RAW: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_BOOTTIME: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_TSC: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_PERF: _ClassVar[BuiltinClock]
    BUILTIN_CLOCK_MAX_ID: _ClassVar[BuiltinClock]
BUILTIN_CLOCK_UNKNOWN: BuiltinClock
BUILTIN_CLOCK_REALTIME: BuiltinClock
BUILTIN_CLOCK_REALTIME_COARSE: BuiltinClock
BUILTIN_CLOCK_MONOTONIC: BuiltinClock
BUILTIN_CLOCK_MONOTONIC_COARSE: BuiltinClock
BUILTIN_CLOCK_MONOTONIC_RAW: BuiltinClock
BUILTIN_CLOCK_BOOTTIME: BuiltinClock
BUILTIN_CLOCK_TSC: BuiltinClock
BUILTIN_CLOCK_PERF: BuiltinClock
BUILTIN_CLOCK_MAX_ID: BuiltinClock
