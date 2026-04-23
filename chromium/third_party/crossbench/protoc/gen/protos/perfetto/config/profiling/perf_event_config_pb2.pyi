from protos.perfetto.common import perf_events_pb2 as _perf_events_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PerfEventConfig(_message.Message):
    __slots__ = ("timebase", "followers", "callstack_sampling", "target_cpu", "ring_buffer_read_period_ms", "ring_buffer_pages", "max_enqueued_footprint_kb", "max_daemon_memory_kb", "remote_descriptor_timeout_ms", "unwind_state_clear_period_ms", "target_installed_by", "all_cpus", "sampling_frequency", "kernel_frames", "target_pid", "target_cmdline", "exclude_pid", "exclude_cmdline", "additional_cmdline_count")
    class UnwindMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNWIND_UNKNOWN: _ClassVar[PerfEventConfig.UnwindMode]
        UNWIND_SKIP: _ClassVar[PerfEventConfig.UnwindMode]
        UNWIND_DWARF: _ClassVar[PerfEventConfig.UnwindMode]
        UNWIND_FRAME_POINTER: _ClassVar[PerfEventConfig.UnwindMode]
    UNWIND_UNKNOWN: PerfEventConfig.UnwindMode
    UNWIND_SKIP: PerfEventConfig.UnwindMode
    UNWIND_DWARF: PerfEventConfig.UnwindMode
    UNWIND_FRAME_POINTER: PerfEventConfig.UnwindMode
    class CallstackSampling(_message.Message):
        __slots__ = ("scope", "kernel_frames", "user_frames")
        SCOPE_FIELD_NUMBER: _ClassVar[int]
        KERNEL_FRAMES_FIELD_NUMBER: _ClassVar[int]
        USER_FRAMES_FIELD_NUMBER: _ClassVar[int]
        scope: PerfEventConfig.Scope
        kernel_frames: bool
        user_frames: PerfEventConfig.UnwindMode
        def __init__(self, scope: _Optional[_Union[PerfEventConfig.Scope, _Mapping]] = ..., kernel_frames: _Optional[bool] = ..., user_frames: _Optional[_Union[PerfEventConfig.UnwindMode, str]] = ...) -> None: ...
    class Scope(_message.Message):
        __slots__ = ("target_pid", "target_cmdline", "exclude_pid", "exclude_cmdline", "additional_cmdline_count", "process_shard_count")
        TARGET_PID_FIELD_NUMBER: _ClassVar[int]
        TARGET_CMDLINE_FIELD_NUMBER: _ClassVar[int]
        EXCLUDE_PID_FIELD_NUMBER: _ClassVar[int]
        EXCLUDE_CMDLINE_FIELD_NUMBER: _ClassVar[int]
        ADDITIONAL_CMDLINE_COUNT_FIELD_NUMBER: _ClassVar[int]
        PROCESS_SHARD_COUNT_FIELD_NUMBER: _ClassVar[int]
        target_pid: _containers.RepeatedScalarFieldContainer[int]
        target_cmdline: _containers.RepeatedScalarFieldContainer[str]
        exclude_pid: _containers.RepeatedScalarFieldContainer[int]
        exclude_cmdline: _containers.RepeatedScalarFieldContainer[str]
        additional_cmdline_count: int
        process_shard_count: int
        def __init__(self, target_pid: _Optional[_Iterable[int]] = ..., target_cmdline: _Optional[_Iterable[str]] = ..., exclude_pid: _Optional[_Iterable[int]] = ..., exclude_cmdline: _Optional[_Iterable[str]] = ..., additional_cmdline_count: _Optional[int] = ..., process_shard_count: _Optional[int] = ...) -> None: ...
    TIMEBASE_FIELD_NUMBER: _ClassVar[int]
    FOLLOWERS_FIELD_NUMBER: _ClassVar[int]
    CALLSTACK_SAMPLING_FIELD_NUMBER: _ClassVar[int]
    TARGET_CPU_FIELD_NUMBER: _ClassVar[int]
    RING_BUFFER_READ_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    RING_BUFFER_PAGES_FIELD_NUMBER: _ClassVar[int]
    MAX_ENQUEUED_FOOTPRINT_KB_FIELD_NUMBER: _ClassVar[int]
    MAX_DAEMON_MEMORY_KB_FIELD_NUMBER: _ClassVar[int]
    REMOTE_DESCRIPTOR_TIMEOUT_MS_FIELD_NUMBER: _ClassVar[int]
    UNWIND_STATE_CLEAR_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    TARGET_INSTALLED_BY_FIELD_NUMBER: _ClassVar[int]
    ALL_CPUS_FIELD_NUMBER: _ClassVar[int]
    SAMPLING_FREQUENCY_FIELD_NUMBER: _ClassVar[int]
    KERNEL_FRAMES_FIELD_NUMBER: _ClassVar[int]
    TARGET_PID_FIELD_NUMBER: _ClassVar[int]
    TARGET_CMDLINE_FIELD_NUMBER: _ClassVar[int]
    EXCLUDE_PID_FIELD_NUMBER: _ClassVar[int]
    EXCLUDE_CMDLINE_FIELD_NUMBER: _ClassVar[int]
    ADDITIONAL_CMDLINE_COUNT_FIELD_NUMBER: _ClassVar[int]
    timebase: _perf_events_pb2.PerfEvents.Timebase
    followers: _containers.RepeatedCompositeFieldContainer[_perf_events_pb2.FollowerEvent]
    callstack_sampling: PerfEventConfig.CallstackSampling
    target_cpu: _containers.RepeatedScalarFieldContainer[int]
    ring_buffer_read_period_ms: int
    ring_buffer_pages: int
    max_enqueued_footprint_kb: int
    max_daemon_memory_kb: int
    remote_descriptor_timeout_ms: int
    unwind_state_clear_period_ms: int
    target_installed_by: _containers.RepeatedScalarFieldContainer[str]
    all_cpus: bool
    sampling_frequency: int
    kernel_frames: bool
    target_pid: _containers.RepeatedScalarFieldContainer[int]
    target_cmdline: _containers.RepeatedScalarFieldContainer[str]
    exclude_pid: _containers.RepeatedScalarFieldContainer[int]
    exclude_cmdline: _containers.RepeatedScalarFieldContainer[str]
    additional_cmdline_count: int
    def __init__(self, timebase: _Optional[_Union[_perf_events_pb2.PerfEvents.Timebase, _Mapping]] = ..., followers: _Optional[_Iterable[_Union[_perf_events_pb2.FollowerEvent, _Mapping]]] = ..., callstack_sampling: _Optional[_Union[PerfEventConfig.CallstackSampling, _Mapping]] = ..., target_cpu: _Optional[_Iterable[int]] = ..., ring_buffer_read_period_ms: _Optional[int] = ..., ring_buffer_pages: _Optional[int] = ..., max_enqueued_footprint_kb: _Optional[int] = ..., max_daemon_memory_kb: _Optional[int] = ..., remote_descriptor_timeout_ms: _Optional[int] = ..., unwind_state_clear_period_ms: _Optional[int] = ..., target_installed_by: _Optional[_Iterable[str]] = ..., all_cpus: _Optional[bool] = ..., sampling_frequency: _Optional[int] = ..., kernel_frames: _Optional[bool] = ..., target_pid: _Optional[_Iterable[int]] = ..., target_cmdline: _Optional[_Iterable[str]] = ..., exclude_pid: _Optional[_Iterable[int]] = ..., exclude_cmdline: _Optional[_Iterable[str]] = ..., additional_cmdline_count: _Optional[int] = ...) -> None: ...
