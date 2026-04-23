from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class HeapprofdConfig(_message.Message):
    __slots__ = ("sampling_interval_bytes", "adaptive_sampling_shmem_threshold", "adaptive_sampling_max_sampling_interval_bytes", "process_cmdline", "pid", "target_installed_by", "heaps", "exclude_heaps", "stream_allocations", "heap_sampling_intervals", "all_heaps", "all", "min_anonymous_memory_kb", "max_heapprofd_memory_kb", "max_heapprofd_cpu_secs", "skip_symbol_prefix", "continuous_dump_config", "shmem_size_bytes", "block_client", "block_client_timeout_us", "no_startup", "no_running", "dump_at_max", "disable_fork_teardown", "disable_vfork_detection")
    class ContinuousDumpConfig(_message.Message):
        __slots__ = ("dump_phase_ms", "dump_interval_ms")
        DUMP_PHASE_MS_FIELD_NUMBER: _ClassVar[int]
        DUMP_INTERVAL_MS_FIELD_NUMBER: _ClassVar[int]
        dump_phase_ms: int
        dump_interval_ms: int
        def __init__(self, dump_phase_ms: _Optional[int] = ..., dump_interval_ms: _Optional[int] = ...) -> None: ...
    SAMPLING_INTERVAL_BYTES_FIELD_NUMBER: _ClassVar[int]
    ADAPTIVE_SAMPLING_SHMEM_THRESHOLD_FIELD_NUMBER: _ClassVar[int]
    ADAPTIVE_SAMPLING_MAX_SAMPLING_INTERVAL_BYTES_FIELD_NUMBER: _ClassVar[int]
    PROCESS_CMDLINE_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    TARGET_INSTALLED_BY_FIELD_NUMBER: _ClassVar[int]
    HEAPS_FIELD_NUMBER: _ClassVar[int]
    EXCLUDE_HEAPS_FIELD_NUMBER: _ClassVar[int]
    STREAM_ALLOCATIONS_FIELD_NUMBER: _ClassVar[int]
    HEAP_SAMPLING_INTERVALS_FIELD_NUMBER: _ClassVar[int]
    ALL_HEAPS_FIELD_NUMBER: _ClassVar[int]
    ALL_FIELD_NUMBER: _ClassVar[int]
    MIN_ANONYMOUS_MEMORY_KB_FIELD_NUMBER: _ClassVar[int]
    MAX_HEAPPROFD_MEMORY_KB_FIELD_NUMBER: _ClassVar[int]
    MAX_HEAPPROFD_CPU_SECS_FIELD_NUMBER: _ClassVar[int]
    SKIP_SYMBOL_PREFIX_FIELD_NUMBER: _ClassVar[int]
    CONTINUOUS_DUMP_CONFIG_FIELD_NUMBER: _ClassVar[int]
    SHMEM_SIZE_BYTES_FIELD_NUMBER: _ClassVar[int]
    BLOCK_CLIENT_FIELD_NUMBER: _ClassVar[int]
    BLOCK_CLIENT_TIMEOUT_US_FIELD_NUMBER: _ClassVar[int]
    NO_STARTUP_FIELD_NUMBER: _ClassVar[int]
    NO_RUNNING_FIELD_NUMBER: _ClassVar[int]
    DUMP_AT_MAX_FIELD_NUMBER: _ClassVar[int]
    DISABLE_FORK_TEARDOWN_FIELD_NUMBER: _ClassVar[int]
    DISABLE_VFORK_DETECTION_FIELD_NUMBER: _ClassVar[int]
    sampling_interval_bytes: int
    adaptive_sampling_shmem_threshold: int
    adaptive_sampling_max_sampling_interval_bytes: int
    process_cmdline: _containers.RepeatedScalarFieldContainer[str]
    pid: _containers.RepeatedScalarFieldContainer[int]
    target_installed_by: _containers.RepeatedScalarFieldContainer[str]
    heaps: _containers.RepeatedScalarFieldContainer[str]
    exclude_heaps: _containers.RepeatedScalarFieldContainer[str]
    stream_allocations: bool
    heap_sampling_intervals: _containers.RepeatedScalarFieldContainer[int]
    all_heaps: bool
    all: bool
    min_anonymous_memory_kb: int
    max_heapprofd_memory_kb: int
    max_heapprofd_cpu_secs: int
    skip_symbol_prefix: _containers.RepeatedScalarFieldContainer[str]
    continuous_dump_config: HeapprofdConfig.ContinuousDumpConfig
    shmem_size_bytes: int
    block_client: bool
    block_client_timeout_us: int
    no_startup: bool
    no_running: bool
    dump_at_max: bool
    disable_fork_teardown: bool
    disable_vfork_detection: bool
    def __init__(self, sampling_interval_bytes: _Optional[int] = ..., adaptive_sampling_shmem_threshold: _Optional[int] = ..., adaptive_sampling_max_sampling_interval_bytes: _Optional[int] = ..., process_cmdline: _Optional[_Iterable[str]] = ..., pid: _Optional[_Iterable[int]] = ..., target_installed_by: _Optional[_Iterable[str]] = ..., heaps: _Optional[_Iterable[str]] = ..., exclude_heaps: _Optional[_Iterable[str]] = ..., stream_allocations: _Optional[bool] = ..., heap_sampling_intervals: _Optional[_Iterable[int]] = ..., all_heaps: _Optional[bool] = ..., all: _Optional[bool] = ..., min_anonymous_memory_kb: _Optional[int] = ..., max_heapprofd_memory_kb: _Optional[int] = ..., max_heapprofd_cpu_secs: _Optional[int] = ..., skip_symbol_prefix: _Optional[_Iterable[str]] = ..., continuous_dump_config: _Optional[_Union[HeapprofdConfig.ContinuousDumpConfig, _Mapping]] = ..., shmem_size_bytes: _Optional[int] = ..., block_client: _Optional[bool] = ..., block_client_timeout_us: _Optional[int] = ..., no_startup: _Optional[bool] = ..., no_running: _Optional[bool] = ..., dump_at_max: _Optional[bool] = ..., disable_fork_teardown: _Optional[bool] = ..., disable_vfork_detection: _Optional[bool] = ...) -> None: ...
