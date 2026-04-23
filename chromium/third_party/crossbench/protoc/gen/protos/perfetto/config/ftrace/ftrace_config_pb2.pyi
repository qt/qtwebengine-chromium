from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class FtraceConfig(_message.Message):
    __slots__ = ("ftrace_events", "atrace_categories", "atrace_apps", "atrace_categories_prefer_sdk", "atrace_userspace_only", "buffer_size_kb", "buffer_size_lower_bound", "drain_period_ms", "drain_buffer_percent", "compact_sched", "print_filter", "symbolize_ksyms", "ksyms_mem_policy", "throttle_rss_stat", "denser_generic_event_encoding", "disable_generic_events", "syscall_events", "enable_function_graph", "function_filters", "function_graph_roots", "function_graph_max_depth", "kprobe_events", "preserve_ftrace_buffer", "use_monotonic_raw_clock", "instance_name", "debug_ftrace_abi", "tids_to_trace", "tracefs_options", "tracing_cpumask", "initialize_ksyms_synchronously_for_testing")
    class KsymsMemPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        KSYMS_UNSPECIFIED: _ClassVar[FtraceConfig.KsymsMemPolicy]
        KSYMS_CLEANUP_ON_STOP: _ClassVar[FtraceConfig.KsymsMemPolicy]
        KSYMS_RETAIN: _ClassVar[FtraceConfig.KsymsMemPolicy]
    KSYMS_UNSPECIFIED: FtraceConfig.KsymsMemPolicy
    KSYMS_CLEANUP_ON_STOP: FtraceConfig.KsymsMemPolicy
    KSYMS_RETAIN: FtraceConfig.KsymsMemPolicy
    class CompactSchedConfig(_message.Message):
        __slots__ = ("enabled",)
        ENABLED_FIELD_NUMBER: _ClassVar[int]
        enabled: bool
        def __init__(self, enabled: _Optional[bool] = ...) -> None: ...
    class PrintFilter(_message.Message):
        __slots__ = ("rules",)
        class Rule(_message.Message):
            __slots__ = ("prefix", "atrace_msg", "allow")
            class AtraceMessage(_message.Message):
                __slots__ = ("type", "prefix")
                TYPE_FIELD_NUMBER: _ClassVar[int]
                PREFIX_FIELD_NUMBER: _ClassVar[int]
                type: str
                prefix: str
                def __init__(self, type: _Optional[str] = ..., prefix: _Optional[str] = ...) -> None: ...
            PREFIX_FIELD_NUMBER: _ClassVar[int]
            ATRACE_MSG_FIELD_NUMBER: _ClassVar[int]
            ALLOW_FIELD_NUMBER: _ClassVar[int]
            prefix: str
            atrace_msg: FtraceConfig.PrintFilter.Rule.AtraceMessage
            allow: bool
            def __init__(self, prefix: _Optional[str] = ..., atrace_msg: _Optional[_Union[FtraceConfig.PrintFilter.Rule.AtraceMessage, _Mapping]] = ..., allow: _Optional[bool] = ...) -> None: ...
        RULES_FIELD_NUMBER: _ClassVar[int]
        rules: _containers.RepeatedCompositeFieldContainer[FtraceConfig.PrintFilter.Rule]
        def __init__(self, rules: _Optional[_Iterable[_Union[FtraceConfig.PrintFilter.Rule, _Mapping]]] = ...) -> None: ...
    class KprobeEvent(_message.Message):
        __slots__ = ("probe", "type")
        class KprobeType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
            __slots__ = ()
            KPROBE_TYPE_UNKNOWN: _ClassVar[FtraceConfig.KprobeEvent.KprobeType]
            KPROBE_TYPE_KPROBE: _ClassVar[FtraceConfig.KprobeEvent.KprobeType]
            KPROBE_TYPE_KRETPROBE: _ClassVar[FtraceConfig.KprobeEvent.KprobeType]
            KPROBE_TYPE_BOTH: _ClassVar[FtraceConfig.KprobeEvent.KprobeType]
        KPROBE_TYPE_UNKNOWN: FtraceConfig.KprobeEvent.KprobeType
        KPROBE_TYPE_KPROBE: FtraceConfig.KprobeEvent.KprobeType
        KPROBE_TYPE_KRETPROBE: FtraceConfig.KprobeEvent.KprobeType
        KPROBE_TYPE_BOTH: FtraceConfig.KprobeEvent.KprobeType
        PROBE_FIELD_NUMBER: _ClassVar[int]
        TYPE_FIELD_NUMBER: _ClassVar[int]
        probe: str
        type: FtraceConfig.KprobeEvent.KprobeType
        def __init__(self, probe: _Optional[str] = ..., type: _Optional[_Union[FtraceConfig.KprobeEvent.KprobeType, str]] = ...) -> None: ...
    class TracefsOption(_message.Message):
        __slots__ = ("name", "state")
        class State(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
            __slots__ = ()
            STATE_UNKNOWN: _ClassVar[FtraceConfig.TracefsOption.State]
            STATE_ENABLED: _ClassVar[FtraceConfig.TracefsOption.State]
            STATE_DISABLED: _ClassVar[FtraceConfig.TracefsOption.State]
        STATE_UNKNOWN: FtraceConfig.TracefsOption.State
        STATE_ENABLED: FtraceConfig.TracefsOption.State
        STATE_DISABLED: FtraceConfig.TracefsOption.State
        NAME_FIELD_NUMBER: _ClassVar[int]
        STATE_FIELD_NUMBER: _ClassVar[int]
        name: str
        state: FtraceConfig.TracefsOption.State
        def __init__(self, name: _Optional[str] = ..., state: _Optional[_Union[FtraceConfig.TracefsOption.State, str]] = ...) -> None: ...
    FTRACE_EVENTS_FIELD_NUMBER: _ClassVar[int]
    ATRACE_CATEGORIES_FIELD_NUMBER: _ClassVar[int]
    ATRACE_APPS_FIELD_NUMBER: _ClassVar[int]
    ATRACE_CATEGORIES_PREFER_SDK_FIELD_NUMBER: _ClassVar[int]
    ATRACE_USERSPACE_ONLY_FIELD_NUMBER: _ClassVar[int]
    BUFFER_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
    BUFFER_SIZE_LOWER_BOUND_FIELD_NUMBER: _ClassVar[int]
    DRAIN_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    DRAIN_BUFFER_PERCENT_FIELD_NUMBER: _ClassVar[int]
    COMPACT_SCHED_FIELD_NUMBER: _ClassVar[int]
    PRINT_FILTER_FIELD_NUMBER: _ClassVar[int]
    SYMBOLIZE_KSYMS_FIELD_NUMBER: _ClassVar[int]
    KSYMS_MEM_POLICY_FIELD_NUMBER: _ClassVar[int]
    THROTTLE_RSS_STAT_FIELD_NUMBER: _ClassVar[int]
    DENSER_GENERIC_EVENT_ENCODING_FIELD_NUMBER: _ClassVar[int]
    DISABLE_GENERIC_EVENTS_FIELD_NUMBER: _ClassVar[int]
    SYSCALL_EVENTS_FIELD_NUMBER: _ClassVar[int]
    ENABLE_FUNCTION_GRAPH_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_FILTERS_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_GRAPH_ROOTS_FIELD_NUMBER: _ClassVar[int]
    FUNCTION_GRAPH_MAX_DEPTH_FIELD_NUMBER: _ClassVar[int]
    KPROBE_EVENTS_FIELD_NUMBER: _ClassVar[int]
    PRESERVE_FTRACE_BUFFER_FIELD_NUMBER: _ClassVar[int]
    USE_MONOTONIC_RAW_CLOCK_FIELD_NUMBER: _ClassVar[int]
    INSTANCE_NAME_FIELD_NUMBER: _ClassVar[int]
    DEBUG_FTRACE_ABI_FIELD_NUMBER: _ClassVar[int]
    TIDS_TO_TRACE_FIELD_NUMBER: _ClassVar[int]
    TRACEFS_OPTIONS_FIELD_NUMBER: _ClassVar[int]
    TRACING_CPUMASK_FIELD_NUMBER: _ClassVar[int]
    INITIALIZE_KSYMS_SYNCHRONOUSLY_FOR_TESTING_FIELD_NUMBER: _ClassVar[int]
    ftrace_events: _containers.RepeatedScalarFieldContainer[str]
    atrace_categories: _containers.RepeatedScalarFieldContainer[str]
    atrace_apps: _containers.RepeatedScalarFieldContainer[str]
    atrace_categories_prefer_sdk: _containers.RepeatedScalarFieldContainer[str]
    atrace_userspace_only: bool
    buffer_size_kb: int
    buffer_size_lower_bound: bool
    drain_period_ms: int
    drain_buffer_percent: int
    compact_sched: FtraceConfig.CompactSchedConfig
    print_filter: FtraceConfig.PrintFilter
    symbolize_ksyms: bool
    ksyms_mem_policy: FtraceConfig.KsymsMemPolicy
    throttle_rss_stat: bool
    denser_generic_event_encoding: bool
    disable_generic_events: bool
    syscall_events: _containers.RepeatedScalarFieldContainer[str]
    enable_function_graph: bool
    function_filters: _containers.RepeatedScalarFieldContainer[str]
    function_graph_roots: _containers.RepeatedScalarFieldContainer[str]
    function_graph_max_depth: int
    kprobe_events: _containers.RepeatedCompositeFieldContainer[FtraceConfig.KprobeEvent]
    preserve_ftrace_buffer: bool
    use_monotonic_raw_clock: bool
    instance_name: str
    debug_ftrace_abi: bool
    tids_to_trace: _containers.RepeatedScalarFieldContainer[int]
    tracefs_options: _containers.RepeatedCompositeFieldContainer[FtraceConfig.TracefsOption]
    tracing_cpumask: str
    initialize_ksyms_synchronously_for_testing: bool
    def __init__(self, ftrace_events: _Optional[_Iterable[str]] = ..., atrace_categories: _Optional[_Iterable[str]] = ..., atrace_apps: _Optional[_Iterable[str]] = ..., atrace_categories_prefer_sdk: _Optional[_Iterable[str]] = ..., atrace_userspace_only: _Optional[bool] = ..., buffer_size_kb: _Optional[int] = ..., buffer_size_lower_bound: _Optional[bool] = ..., drain_period_ms: _Optional[int] = ..., drain_buffer_percent: _Optional[int] = ..., compact_sched: _Optional[_Union[FtraceConfig.CompactSchedConfig, _Mapping]] = ..., print_filter: _Optional[_Union[FtraceConfig.PrintFilter, _Mapping]] = ..., symbolize_ksyms: _Optional[bool] = ..., ksyms_mem_policy: _Optional[_Union[FtraceConfig.KsymsMemPolicy, str]] = ..., throttle_rss_stat: _Optional[bool] = ..., denser_generic_event_encoding: _Optional[bool] = ..., disable_generic_events: _Optional[bool] = ..., syscall_events: _Optional[_Iterable[str]] = ..., enable_function_graph: _Optional[bool] = ..., function_filters: _Optional[_Iterable[str]] = ..., function_graph_roots: _Optional[_Iterable[str]] = ..., function_graph_max_depth: _Optional[int] = ..., kprobe_events: _Optional[_Iterable[_Union[FtraceConfig.KprobeEvent, _Mapping]]] = ..., preserve_ftrace_buffer: _Optional[bool] = ..., use_monotonic_raw_clock: _Optional[bool] = ..., instance_name: _Optional[str] = ..., debug_ftrace_abi: _Optional[bool] = ..., tids_to_trace: _Optional[_Iterable[int]] = ..., tracefs_options: _Optional[_Iterable[_Union[FtraceConfig.TracefsOption, _Mapping]]] = ..., tracing_cpumask: _Optional[str] = ..., initialize_ksyms_synchronously_for_testing: _Optional[bool] = ...) -> None: ...
