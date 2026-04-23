from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class EtwConfig(_message.Message):
    __slots__ = ("kernel_flags", "scheduler_provider_events", "memory_provider_events", "file_provider_events")
    class KernelFlag(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        CSWITCH: _ClassVar[EtwConfig.KernelFlag]
        DISPATCHER: _ClassVar[EtwConfig.KernelFlag]
    CSWITCH: EtwConfig.KernelFlag
    DISPATCHER: EtwConfig.KernelFlag
    KERNEL_FLAGS_FIELD_NUMBER: _ClassVar[int]
    SCHEDULER_PROVIDER_EVENTS_FIELD_NUMBER: _ClassVar[int]
    MEMORY_PROVIDER_EVENTS_FIELD_NUMBER: _ClassVar[int]
    FILE_PROVIDER_EVENTS_FIELD_NUMBER: _ClassVar[int]
    kernel_flags: _containers.RepeatedScalarFieldContainer[EtwConfig.KernelFlag]
    scheduler_provider_events: _containers.RepeatedScalarFieldContainer[str]
    memory_provider_events: _containers.RepeatedScalarFieldContainer[str]
    file_provider_events: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, kernel_flags: _Optional[_Iterable[_Union[EtwConfig.KernelFlag, str]]] = ..., scheduler_provider_events: _Optional[_Iterable[str]] = ..., memory_provider_events: _Optional[_Iterable[str]] = ..., file_provider_events: _Optional[_Iterable[str]] = ...) -> None: ...
