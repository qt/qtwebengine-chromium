from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ProcessStatsConfig(_message.Message):
    __slots__ = ("quirks", "scan_all_processes_on_start", "record_thread_names", "proc_stats_poll_ms", "proc_stats_cache_ttl_ms", "scan_smaps_rollup", "record_process_age", "record_process_runtime", "record_process_dmabuf_rss", "resolve_process_fds")
    class Quirks(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        QUIRKS_UNSPECIFIED: _ClassVar[ProcessStatsConfig.Quirks]
        DISABLE_INITIAL_DUMP: _ClassVar[ProcessStatsConfig.Quirks]
        DISABLE_ON_DEMAND: _ClassVar[ProcessStatsConfig.Quirks]
    QUIRKS_UNSPECIFIED: ProcessStatsConfig.Quirks
    DISABLE_INITIAL_DUMP: ProcessStatsConfig.Quirks
    DISABLE_ON_DEMAND: ProcessStatsConfig.Quirks
    QUIRKS_FIELD_NUMBER: _ClassVar[int]
    SCAN_ALL_PROCESSES_ON_START_FIELD_NUMBER: _ClassVar[int]
    RECORD_THREAD_NAMES_FIELD_NUMBER: _ClassVar[int]
    PROC_STATS_POLL_MS_FIELD_NUMBER: _ClassVar[int]
    PROC_STATS_CACHE_TTL_MS_FIELD_NUMBER: _ClassVar[int]
    SCAN_SMAPS_ROLLUP_FIELD_NUMBER: _ClassVar[int]
    RECORD_PROCESS_AGE_FIELD_NUMBER: _ClassVar[int]
    RECORD_PROCESS_RUNTIME_FIELD_NUMBER: _ClassVar[int]
    RECORD_PROCESS_DMABUF_RSS_FIELD_NUMBER: _ClassVar[int]
    RESOLVE_PROCESS_FDS_FIELD_NUMBER: _ClassVar[int]
    quirks: _containers.RepeatedScalarFieldContainer[ProcessStatsConfig.Quirks]
    scan_all_processes_on_start: bool
    record_thread_names: bool
    proc_stats_poll_ms: int
    proc_stats_cache_ttl_ms: int
    scan_smaps_rollup: bool
    record_process_age: bool
    record_process_runtime: bool
    record_process_dmabuf_rss: bool
    resolve_process_fds: bool
    def __init__(self, quirks: _Optional[_Iterable[_Union[ProcessStatsConfig.Quirks, str]]] = ..., scan_all_processes_on_start: _Optional[bool] = ..., record_thread_names: _Optional[bool] = ..., proc_stats_poll_ms: _Optional[int] = ..., proc_stats_cache_ttl_ms: _Optional[int] = ..., scan_smaps_rollup: _Optional[bool] = ..., record_process_age: _Optional[bool] = ..., record_process_runtime: _Optional[bool] = ..., record_process_dmabuf_rss: _Optional[bool] = ..., resolve_process_fds: _Optional[bool] = ...) -> None: ...
