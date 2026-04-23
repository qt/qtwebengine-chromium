from protos.perfetto.common import sys_stats_counters_pb2 as _sys_stats_counters_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class SysStatsConfig(_message.Message):
    __slots__ = ("meminfo_period_ms", "meminfo_counters", "vmstat_period_ms", "vmstat_counters", "stat_period_ms", "stat_counters", "devfreq_period_ms", "cpufreq_period_ms", "buddyinfo_period_ms", "diskstat_period_ms", "psi_period_ms", "thermal_period_ms", "cpuidle_period_ms", "gpufreq_period_ms")
    class StatCounters(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        STAT_UNSPECIFIED: _ClassVar[SysStatsConfig.StatCounters]
        STAT_CPU_TIMES: _ClassVar[SysStatsConfig.StatCounters]
        STAT_IRQ_COUNTS: _ClassVar[SysStatsConfig.StatCounters]
        STAT_SOFTIRQ_COUNTS: _ClassVar[SysStatsConfig.StatCounters]
        STAT_FORK_COUNT: _ClassVar[SysStatsConfig.StatCounters]
    STAT_UNSPECIFIED: SysStatsConfig.StatCounters
    STAT_CPU_TIMES: SysStatsConfig.StatCounters
    STAT_IRQ_COUNTS: SysStatsConfig.StatCounters
    STAT_SOFTIRQ_COUNTS: SysStatsConfig.StatCounters
    STAT_FORK_COUNT: SysStatsConfig.StatCounters
    MEMINFO_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    MEMINFO_COUNTERS_FIELD_NUMBER: _ClassVar[int]
    VMSTAT_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    VMSTAT_COUNTERS_FIELD_NUMBER: _ClassVar[int]
    STAT_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    STAT_COUNTERS_FIELD_NUMBER: _ClassVar[int]
    DEVFREQ_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    CPUFREQ_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    BUDDYINFO_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    DISKSTAT_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    PSI_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    THERMAL_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    CPUIDLE_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    GPUFREQ_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    meminfo_period_ms: int
    meminfo_counters: _containers.RepeatedScalarFieldContainer[_sys_stats_counters_pb2.MeminfoCounters]
    vmstat_period_ms: int
    vmstat_counters: _containers.RepeatedScalarFieldContainer[_sys_stats_counters_pb2.VmstatCounters]
    stat_period_ms: int
    stat_counters: _containers.RepeatedScalarFieldContainer[SysStatsConfig.StatCounters]
    devfreq_period_ms: int
    cpufreq_period_ms: int
    buddyinfo_period_ms: int
    diskstat_period_ms: int
    psi_period_ms: int
    thermal_period_ms: int
    cpuidle_period_ms: int
    gpufreq_period_ms: int
    def __init__(self, meminfo_period_ms: _Optional[int] = ..., meminfo_counters: _Optional[_Iterable[_Union[_sys_stats_counters_pb2.MeminfoCounters, str]]] = ..., vmstat_period_ms: _Optional[int] = ..., vmstat_counters: _Optional[_Iterable[_Union[_sys_stats_counters_pb2.VmstatCounters, str]]] = ..., stat_period_ms: _Optional[int] = ..., stat_counters: _Optional[_Iterable[_Union[SysStatsConfig.StatCounters, str]]] = ..., devfreq_period_ms: _Optional[int] = ..., cpufreq_period_ms: _Optional[int] = ..., buddyinfo_period_ms: _Optional[int] = ..., diskstat_period_ms: _Optional[int] = ..., psi_period_ms: _Optional[int] = ..., thermal_period_ms: _Optional[int] = ..., cpuidle_period_ms: _Optional[int] = ..., gpufreq_period_ms: _Optional[int] = ...) -> None: ...
