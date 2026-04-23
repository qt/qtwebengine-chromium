from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class ProcessStatsProto(_message.Message):
    __slots__ = ()
    class MemoryFactor(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        MEM_FACTOR_NORMAL: _ClassVar[ProcessStatsProto.MemoryFactor]
        MEM_FACTOR_MODERATE: _ClassVar[ProcessStatsProto.MemoryFactor]
        MEM_FACTOR_LOW: _ClassVar[ProcessStatsProto.MemoryFactor]
        MEM_FACTOR_CRITICAL: _ClassVar[ProcessStatsProto.MemoryFactor]
    MEM_FACTOR_NORMAL: ProcessStatsProto.MemoryFactor
    MEM_FACTOR_MODERATE: ProcessStatsProto.MemoryFactor
    MEM_FACTOR_LOW: ProcessStatsProto.MemoryFactor
    MEM_FACTOR_CRITICAL: ProcessStatsProto.MemoryFactor
    def __init__(self) -> None: ...
