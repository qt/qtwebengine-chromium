from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PerfEvents(_message.Message):
    __slots__ = ()
    class Counter(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNKNOWN_COUNTER: _ClassVar[PerfEvents.Counter]
        SW_CPU_CLOCK: _ClassVar[PerfEvents.Counter]
        SW_PAGE_FAULTS: _ClassVar[PerfEvents.Counter]
        SW_TASK_CLOCK: _ClassVar[PerfEvents.Counter]
        SW_CONTEXT_SWITCHES: _ClassVar[PerfEvents.Counter]
        SW_CPU_MIGRATIONS: _ClassVar[PerfEvents.Counter]
        SW_PAGE_FAULTS_MIN: _ClassVar[PerfEvents.Counter]
        SW_PAGE_FAULTS_MAJ: _ClassVar[PerfEvents.Counter]
        SW_ALIGNMENT_FAULTS: _ClassVar[PerfEvents.Counter]
        SW_EMULATION_FAULTS: _ClassVar[PerfEvents.Counter]
        SW_DUMMY: _ClassVar[PerfEvents.Counter]
        HW_CPU_CYCLES: _ClassVar[PerfEvents.Counter]
        HW_INSTRUCTIONS: _ClassVar[PerfEvents.Counter]
        HW_CACHE_REFERENCES: _ClassVar[PerfEvents.Counter]
        HW_CACHE_MISSES: _ClassVar[PerfEvents.Counter]
        HW_BRANCH_INSTRUCTIONS: _ClassVar[PerfEvents.Counter]
        HW_BRANCH_MISSES: _ClassVar[PerfEvents.Counter]
        HW_BUS_CYCLES: _ClassVar[PerfEvents.Counter]
        HW_STALLED_CYCLES_FRONTEND: _ClassVar[PerfEvents.Counter]
        HW_STALLED_CYCLES_BACKEND: _ClassVar[PerfEvents.Counter]
        HW_REF_CPU_CYCLES: _ClassVar[PerfEvents.Counter]
    UNKNOWN_COUNTER: PerfEvents.Counter
    SW_CPU_CLOCK: PerfEvents.Counter
    SW_PAGE_FAULTS: PerfEvents.Counter
    SW_TASK_CLOCK: PerfEvents.Counter
    SW_CONTEXT_SWITCHES: PerfEvents.Counter
    SW_CPU_MIGRATIONS: PerfEvents.Counter
    SW_PAGE_FAULTS_MIN: PerfEvents.Counter
    SW_PAGE_FAULTS_MAJ: PerfEvents.Counter
    SW_ALIGNMENT_FAULTS: PerfEvents.Counter
    SW_EMULATION_FAULTS: PerfEvents.Counter
    SW_DUMMY: PerfEvents.Counter
    HW_CPU_CYCLES: PerfEvents.Counter
    HW_INSTRUCTIONS: PerfEvents.Counter
    HW_CACHE_REFERENCES: PerfEvents.Counter
    HW_CACHE_MISSES: PerfEvents.Counter
    HW_BRANCH_INSTRUCTIONS: PerfEvents.Counter
    HW_BRANCH_MISSES: PerfEvents.Counter
    HW_BUS_CYCLES: PerfEvents.Counter
    HW_STALLED_CYCLES_FRONTEND: PerfEvents.Counter
    HW_STALLED_CYCLES_BACKEND: PerfEvents.Counter
    HW_REF_CPU_CYCLES: PerfEvents.Counter
    class PerfClock(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNKNOWN_PERF_CLOCK: _ClassVar[PerfEvents.PerfClock]
        PERF_CLOCK_REALTIME: _ClassVar[PerfEvents.PerfClock]
        PERF_CLOCK_MONOTONIC: _ClassVar[PerfEvents.PerfClock]
        PERF_CLOCK_MONOTONIC_RAW: _ClassVar[PerfEvents.PerfClock]
        PERF_CLOCK_BOOTTIME: _ClassVar[PerfEvents.PerfClock]
    UNKNOWN_PERF_CLOCK: PerfEvents.PerfClock
    PERF_CLOCK_REALTIME: PerfEvents.PerfClock
    PERF_CLOCK_MONOTONIC: PerfEvents.PerfClock
    PERF_CLOCK_MONOTONIC_RAW: PerfEvents.PerfClock
    PERF_CLOCK_BOOTTIME: PerfEvents.PerfClock
    class EventModifier(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNKNOWN_EVENT_MODIFIER: _ClassVar[PerfEvents.EventModifier]
        EVENT_MODIFIER_COUNT_USERSPACE: _ClassVar[PerfEvents.EventModifier]
        EVENT_MODIFIER_COUNT_KERNEL: _ClassVar[PerfEvents.EventModifier]
        EVENT_MODIFIER_COUNT_HYPERVISOR: _ClassVar[PerfEvents.EventModifier]
    UNKNOWN_EVENT_MODIFIER: PerfEvents.EventModifier
    EVENT_MODIFIER_COUNT_USERSPACE: PerfEvents.EventModifier
    EVENT_MODIFIER_COUNT_KERNEL: PerfEvents.EventModifier
    EVENT_MODIFIER_COUNT_HYPERVISOR: PerfEvents.EventModifier
    class Timebase(_message.Message):
        __slots__ = ("frequency", "period", "poll_period_ms", "counter", "tracepoint", "raw_event", "modifiers", "timestamp_clock", "name")
        FREQUENCY_FIELD_NUMBER: _ClassVar[int]
        PERIOD_FIELD_NUMBER: _ClassVar[int]
        POLL_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
        COUNTER_FIELD_NUMBER: _ClassVar[int]
        TRACEPOINT_FIELD_NUMBER: _ClassVar[int]
        RAW_EVENT_FIELD_NUMBER: _ClassVar[int]
        MODIFIERS_FIELD_NUMBER: _ClassVar[int]
        TIMESTAMP_CLOCK_FIELD_NUMBER: _ClassVar[int]
        NAME_FIELD_NUMBER: _ClassVar[int]
        frequency: int
        period: int
        poll_period_ms: int
        counter: PerfEvents.Counter
        tracepoint: PerfEvents.Tracepoint
        raw_event: PerfEvents.RawEvent
        modifiers: _containers.RepeatedScalarFieldContainer[PerfEvents.EventModifier]
        timestamp_clock: PerfEvents.PerfClock
        name: str
        def __init__(self, frequency: _Optional[int] = ..., period: _Optional[int] = ..., poll_period_ms: _Optional[int] = ..., counter: _Optional[_Union[PerfEvents.Counter, str]] = ..., tracepoint: _Optional[_Union[PerfEvents.Tracepoint, _Mapping]] = ..., raw_event: _Optional[_Union[PerfEvents.RawEvent, _Mapping]] = ..., modifiers: _Optional[_Iterable[_Union[PerfEvents.EventModifier, str]]] = ..., timestamp_clock: _Optional[_Union[PerfEvents.PerfClock, str]] = ..., name: _Optional[str] = ...) -> None: ...
    class Tracepoint(_message.Message):
        __slots__ = ("name", "filter")
        NAME_FIELD_NUMBER: _ClassVar[int]
        FILTER_FIELD_NUMBER: _ClassVar[int]
        name: str
        filter: str
        def __init__(self, name: _Optional[str] = ..., filter: _Optional[str] = ...) -> None: ...
    class RawEvent(_message.Message):
        __slots__ = ("type", "config", "config1", "config2")
        TYPE_FIELD_NUMBER: _ClassVar[int]
        CONFIG_FIELD_NUMBER: _ClassVar[int]
        CONFIG1_FIELD_NUMBER: _ClassVar[int]
        CONFIG2_FIELD_NUMBER: _ClassVar[int]
        type: int
        config: int
        config1: int
        config2: int
        def __init__(self, type: _Optional[int] = ..., config: _Optional[int] = ..., config1: _Optional[int] = ..., config2: _Optional[int] = ...) -> None: ...
    def __init__(self) -> None: ...

class FollowerEvent(_message.Message):
    __slots__ = ("counter", "tracepoint", "raw_event", "modifiers", "name")
    COUNTER_FIELD_NUMBER: _ClassVar[int]
    TRACEPOINT_FIELD_NUMBER: _ClassVar[int]
    RAW_EVENT_FIELD_NUMBER: _ClassVar[int]
    MODIFIERS_FIELD_NUMBER: _ClassVar[int]
    NAME_FIELD_NUMBER: _ClassVar[int]
    counter: PerfEvents.Counter
    tracepoint: PerfEvents.Tracepoint
    raw_event: PerfEvents.RawEvent
    modifiers: _containers.RepeatedScalarFieldContainer[PerfEvents.EventModifier]
    name: str
    def __init__(self, counter: _Optional[_Union[PerfEvents.Counter, str]] = ..., tracepoint: _Optional[_Union[PerfEvents.Tracepoint, _Mapping]] = ..., raw_event: _Optional[_Union[PerfEvents.RawEvent, _Mapping]] = ..., modifiers: _Optional[_Iterable[_Union[PerfEvents.EventModifier, str]]] = ..., name: _Optional[str] = ...) -> None: ...
