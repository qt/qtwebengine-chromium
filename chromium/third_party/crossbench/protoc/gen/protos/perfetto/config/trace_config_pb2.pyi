from protos.perfetto.common import builtin_clock_pb2 as _builtin_clock_pb2
from protos.perfetto.config import data_source_config_pb2 as _data_source_config_pb2
from protos.perfetto.config.priority_boost import priority_boost_config_pb2 as _priority_boost_config_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class TraceConfig(_message.Message):
    __slots__ = ("buffers", "data_sources", "builtin_data_sources", "duration_ms", "prefer_suspend_clock_for_duration", "enable_extra_guardrails", "lockdown_mode", "producers", "statsd_metadata", "write_into_file", "output_path", "file_write_period_ms", "max_file_size_bytes", "guardrail_overrides", "deferred_start", "flush_period_ms", "flush_timeout_ms", "data_source_stop_timeout_ms", "notify_traceur", "bugreport_score", "bugreport_filename", "trigger_config", "activate_triggers", "incremental_state_config", "allow_user_build_tracing", "unique_session_name", "compression_type", "incident_report_config", "statsd_logging", "trace_uuid_msb", "trace_uuid_lsb", "trace_filter", "android_report_config", "cmd_trace_start_delay", "session_semaphores", "priority_boost", "exclusive_prio")
    class LockdownModeOperation(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        LOCKDOWN_UNCHANGED: _ClassVar[TraceConfig.LockdownModeOperation]
        LOCKDOWN_CLEAR: _ClassVar[TraceConfig.LockdownModeOperation]
        LOCKDOWN_SET: _ClassVar[TraceConfig.LockdownModeOperation]
    LOCKDOWN_UNCHANGED: TraceConfig.LockdownModeOperation
    LOCKDOWN_CLEAR: TraceConfig.LockdownModeOperation
    LOCKDOWN_SET: TraceConfig.LockdownModeOperation
    class CompressionType(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        COMPRESSION_TYPE_UNSPECIFIED: _ClassVar[TraceConfig.CompressionType]
        COMPRESSION_TYPE_DEFLATE: _ClassVar[TraceConfig.CompressionType]
    COMPRESSION_TYPE_UNSPECIFIED: TraceConfig.CompressionType
    COMPRESSION_TYPE_DEFLATE: TraceConfig.CompressionType
    class StatsdLogging(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        STATSD_LOGGING_UNSPECIFIED: _ClassVar[TraceConfig.StatsdLogging]
        STATSD_LOGGING_ENABLED: _ClassVar[TraceConfig.StatsdLogging]
        STATSD_LOGGING_DISABLED: _ClassVar[TraceConfig.StatsdLogging]
    STATSD_LOGGING_UNSPECIFIED: TraceConfig.StatsdLogging
    STATSD_LOGGING_ENABLED: TraceConfig.StatsdLogging
    STATSD_LOGGING_DISABLED: TraceConfig.StatsdLogging
    class BufferConfig(_message.Message):
        __slots__ = ("size_kb", "fill_policy", "transfer_on_clone", "clear_before_clone")
        class FillPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
            __slots__ = ()
            UNSPECIFIED: _ClassVar[TraceConfig.BufferConfig.FillPolicy]
            RING_BUFFER: _ClassVar[TraceConfig.BufferConfig.FillPolicy]
            DISCARD: _ClassVar[TraceConfig.BufferConfig.FillPolicy]
        UNSPECIFIED: TraceConfig.BufferConfig.FillPolicy
        RING_BUFFER: TraceConfig.BufferConfig.FillPolicy
        DISCARD: TraceConfig.BufferConfig.FillPolicy
        SIZE_KB_FIELD_NUMBER: _ClassVar[int]
        FILL_POLICY_FIELD_NUMBER: _ClassVar[int]
        TRANSFER_ON_CLONE_FIELD_NUMBER: _ClassVar[int]
        CLEAR_BEFORE_CLONE_FIELD_NUMBER: _ClassVar[int]
        size_kb: int
        fill_policy: TraceConfig.BufferConfig.FillPolicy
        transfer_on_clone: bool
        clear_before_clone: bool
        def __init__(self, size_kb: _Optional[int] = ..., fill_policy: _Optional[_Union[TraceConfig.BufferConfig.FillPolicy, str]] = ..., transfer_on_clone: _Optional[bool] = ..., clear_before_clone: _Optional[bool] = ...) -> None: ...
    class DataSource(_message.Message):
        __slots__ = ("config", "producer_name_filter", "producer_name_regex_filter", "machine_name_filter")
        CONFIG_FIELD_NUMBER: _ClassVar[int]
        PRODUCER_NAME_FILTER_FIELD_NUMBER: _ClassVar[int]
        PRODUCER_NAME_REGEX_FILTER_FIELD_NUMBER: _ClassVar[int]
        MACHINE_NAME_FILTER_FIELD_NUMBER: _ClassVar[int]
        config: _data_source_config_pb2.DataSourceConfig
        producer_name_filter: _containers.RepeatedScalarFieldContainer[str]
        producer_name_regex_filter: _containers.RepeatedScalarFieldContainer[str]
        machine_name_filter: _containers.RepeatedScalarFieldContainer[str]
        def __init__(self, config: _Optional[_Union[_data_source_config_pb2.DataSourceConfig, _Mapping]] = ..., producer_name_filter: _Optional[_Iterable[str]] = ..., producer_name_regex_filter: _Optional[_Iterable[str]] = ..., machine_name_filter: _Optional[_Iterable[str]] = ...) -> None: ...
    class BuiltinDataSource(_message.Message):
        __slots__ = ("disable_clock_snapshotting", "disable_trace_config", "disable_system_info", "disable_service_events", "primary_trace_clock", "snapshot_interval_ms", "prefer_suspend_clock_for_snapshot", "disable_chunk_usage_histograms")
        DISABLE_CLOCK_SNAPSHOTTING_FIELD_NUMBER: _ClassVar[int]
        DISABLE_TRACE_CONFIG_FIELD_NUMBER: _ClassVar[int]
        DISABLE_SYSTEM_INFO_FIELD_NUMBER: _ClassVar[int]
        DISABLE_SERVICE_EVENTS_FIELD_NUMBER: _ClassVar[int]
        PRIMARY_TRACE_CLOCK_FIELD_NUMBER: _ClassVar[int]
        SNAPSHOT_INTERVAL_MS_FIELD_NUMBER: _ClassVar[int]
        PREFER_SUSPEND_CLOCK_FOR_SNAPSHOT_FIELD_NUMBER: _ClassVar[int]
        DISABLE_CHUNK_USAGE_HISTOGRAMS_FIELD_NUMBER: _ClassVar[int]
        disable_clock_snapshotting: bool
        disable_trace_config: bool
        disable_system_info: bool
        disable_service_events: bool
        primary_trace_clock: _builtin_clock_pb2.BuiltinClock
        snapshot_interval_ms: int
        prefer_suspend_clock_for_snapshot: bool
        disable_chunk_usage_histograms: bool
        def __init__(self, disable_clock_snapshotting: _Optional[bool] = ..., disable_trace_config: _Optional[bool] = ..., disable_system_info: _Optional[bool] = ..., disable_service_events: _Optional[bool] = ..., primary_trace_clock: _Optional[_Union[_builtin_clock_pb2.BuiltinClock, str]] = ..., snapshot_interval_ms: _Optional[int] = ..., prefer_suspend_clock_for_snapshot: _Optional[bool] = ..., disable_chunk_usage_histograms: _Optional[bool] = ...) -> None: ...
    class ProducerConfig(_message.Message):
        __slots__ = ("producer_name", "shm_size_kb", "page_size_kb")
        PRODUCER_NAME_FIELD_NUMBER: _ClassVar[int]
        SHM_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
        PAGE_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
        producer_name: str
        shm_size_kb: int
        page_size_kb: int
        def __init__(self, producer_name: _Optional[str] = ..., shm_size_kb: _Optional[int] = ..., page_size_kb: _Optional[int] = ...) -> None: ...
    class StatsdMetadata(_message.Message):
        __slots__ = ("triggering_alert_id", "triggering_config_uid", "triggering_config_id", "triggering_subscription_id")
        TRIGGERING_ALERT_ID_FIELD_NUMBER: _ClassVar[int]
        TRIGGERING_CONFIG_UID_FIELD_NUMBER: _ClassVar[int]
        TRIGGERING_CONFIG_ID_FIELD_NUMBER: _ClassVar[int]
        TRIGGERING_SUBSCRIPTION_ID_FIELD_NUMBER: _ClassVar[int]
        triggering_alert_id: int
        triggering_config_uid: int
        triggering_config_id: int
        triggering_subscription_id: int
        def __init__(self, triggering_alert_id: _Optional[int] = ..., triggering_config_uid: _Optional[int] = ..., triggering_config_id: _Optional[int] = ..., triggering_subscription_id: _Optional[int] = ...) -> None: ...
    class GuardrailOverrides(_message.Message):
        __slots__ = ("max_upload_per_day_bytes", "max_tracing_buffer_size_kb")
        MAX_UPLOAD_PER_DAY_BYTES_FIELD_NUMBER: _ClassVar[int]
        MAX_TRACING_BUFFER_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
        max_upload_per_day_bytes: int
        max_tracing_buffer_size_kb: int
        def __init__(self, max_upload_per_day_bytes: _Optional[int] = ..., max_tracing_buffer_size_kb: _Optional[int] = ...) -> None: ...
    class TriggerConfig(_message.Message):
        __slots__ = ("trigger_mode", "use_clone_snapshot_if_available", "triggers", "trigger_timeout_ms")
        class TriggerMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
            __slots__ = ()
            UNSPECIFIED: _ClassVar[TraceConfig.TriggerConfig.TriggerMode]
            START_TRACING: _ClassVar[TraceConfig.TriggerConfig.TriggerMode]
            STOP_TRACING: _ClassVar[TraceConfig.TriggerConfig.TriggerMode]
            CLONE_SNAPSHOT: _ClassVar[TraceConfig.TriggerConfig.TriggerMode]
        UNSPECIFIED: TraceConfig.TriggerConfig.TriggerMode
        START_TRACING: TraceConfig.TriggerConfig.TriggerMode
        STOP_TRACING: TraceConfig.TriggerConfig.TriggerMode
        CLONE_SNAPSHOT: TraceConfig.TriggerConfig.TriggerMode
        class Trigger(_message.Message):
            __slots__ = ("name", "producer_name_regex", "stop_delay_ms", "max_per_24_h", "skip_probability")
            NAME_FIELD_NUMBER: _ClassVar[int]
            PRODUCER_NAME_REGEX_FIELD_NUMBER: _ClassVar[int]
            STOP_DELAY_MS_FIELD_NUMBER: _ClassVar[int]
            MAX_PER_24_H_FIELD_NUMBER: _ClassVar[int]
            SKIP_PROBABILITY_FIELD_NUMBER: _ClassVar[int]
            name: str
            producer_name_regex: str
            stop_delay_ms: int
            max_per_24_h: int
            skip_probability: float
            def __init__(self, name: _Optional[str] = ..., producer_name_regex: _Optional[str] = ..., stop_delay_ms: _Optional[int] = ..., max_per_24_h: _Optional[int] = ..., skip_probability: _Optional[float] = ...) -> None: ...
        TRIGGER_MODE_FIELD_NUMBER: _ClassVar[int]
        USE_CLONE_SNAPSHOT_IF_AVAILABLE_FIELD_NUMBER: _ClassVar[int]
        TRIGGERS_FIELD_NUMBER: _ClassVar[int]
        TRIGGER_TIMEOUT_MS_FIELD_NUMBER: _ClassVar[int]
        trigger_mode: TraceConfig.TriggerConfig.TriggerMode
        use_clone_snapshot_if_available: bool
        triggers: _containers.RepeatedCompositeFieldContainer[TraceConfig.TriggerConfig.Trigger]
        trigger_timeout_ms: int
        def __init__(self, trigger_mode: _Optional[_Union[TraceConfig.TriggerConfig.TriggerMode, str]] = ..., use_clone_snapshot_if_available: _Optional[bool] = ..., triggers: _Optional[_Iterable[_Union[TraceConfig.TriggerConfig.Trigger, _Mapping]]] = ..., trigger_timeout_ms: _Optional[int] = ...) -> None: ...
    class IncrementalStateConfig(_message.Message):
        __slots__ = ("clear_period_ms",)
        CLEAR_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
        clear_period_ms: int
        def __init__(self, clear_period_ms: _Optional[int] = ...) -> None: ...
    class IncidentReportConfig(_message.Message):
        __slots__ = ("destination_package", "destination_class", "privacy_level", "skip_incidentd", "skip_dropbox")
        DESTINATION_PACKAGE_FIELD_NUMBER: _ClassVar[int]
        DESTINATION_CLASS_FIELD_NUMBER: _ClassVar[int]
        PRIVACY_LEVEL_FIELD_NUMBER: _ClassVar[int]
        SKIP_INCIDENTD_FIELD_NUMBER: _ClassVar[int]
        SKIP_DROPBOX_FIELD_NUMBER: _ClassVar[int]
        destination_package: str
        destination_class: str
        privacy_level: int
        skip_incidentd: bool
        skip_dropbox: bool
        def __init__(self, destination_package: _Optional[str] = ..., destination_class: _Optional[str] = ..., privacy_level: _Optional[int] = ..., skip_incidentd: _Optional[bool] = ..., skip_dropbox: _Optional[bool] = ...) -> None: ...
    class TraceFilter(_message.Message):
        __slots__ = ("bytecode", "bytecode_v2", "string_filter_chain")
        class StringFilterPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
            __slots__ = ()
            SFP_UNSPECIFIED: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
            SFP_MATCH_REDACT_GROUPS: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
            SFP_ATRACE_MATCH_REDACT_GROUPS: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
            SFP_MATCH_BREAK: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
            SFP_ATRACE_MATCH_BREAK: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
            SFP_ATRACE_REPEATED_SEARCH_REDACT_GROUPS: _ClassVar[TraceConfig.TraceFilter.StringFilterPolicy]
        SFP_UNSPECIFIED: TraceConfig.TraceFilter.StringFilterPolicy
        SFP_MATCH_REDACT_GROUPS: TraceConfig.TraceFilter.StringFilterPolicy
        SFP_ATRACE_MATCH_REDACT_GROUPS: TraceConfig.TraceFilter.StringFilterPolicy
        SFP_MATCH_BREAK: TraceConfig.TraceFilter.StringFilterPolicy
        SFP_ATRACE_MATCH_BREAK: TraceConfig.TraceFilter.StringFilterPolicy
        SFP_ATRACE_REPEATED_SEARCH_REDACT_GROUPS: TraceConfig.TraceFilter.StringFilterPolicy
        class StringFilterRule(_message.Message):
            __slots__ = ("policy", "regex_pattern", "atrace_payload_starts_with")
            POLICY_FIELD_NUMBER: _ClassVar[int]
            REGEX_PATTERN_FIELD_NUMBER: _ClassVar[int]
            ATRACE_PAYLOAD_STARTS_WITH_FIELD_NUMBER: _ClassVar[int]
            policy: TraceConfig.TraceFilter.StringFilterPolicy
            regex_pattern: str
            atrace_payload_starts_with: str
            def __init__(self, policy: _Optional[_Union[TraceConfig.TraceFilter.StringFilterPolicy, str]] = ..., regex_pattern: _Optional[str] = ..., atrace_payload_starts_with: _Optional[str] = ...) -> None: ...
        class StringFilterChain(_message.Message):
            __slots__ = ("rules",)
            RULES_FIELD_NUMBER: _ClassVar[int]
            rules: _containers.RepeatedCompositeFieldContainer[TraceConfig.TraceFilter.StringFilterRule]
            def __init__(self, rules: _Optional[_Iterable[_Union[TraceConfig.TraceFilter.StringFilterRule, _Mapping]]] = ...) -> None: ...
        BYTECODE_FIELD_NUMBER: _ClassVar[int]
        BYTECODE_V2_FIELD_NUMBER: _ClassVar[int]
        STRING_FILTER_CHAIN_FIELD_NUMBER: _ClassVar[int]
        bytecode: bytes
        bytecode_v2: bytes
        string_filter_chain: TraceConfig.TraceFilter.StringFilterChain
        def __init__(self, bytecode: _Optional[bytes] = ..., bytecode_v2: _Optional[bytes] = ..., string_filter_chain: _Optional[_Union[TraceConfig.TraceFilter.StringFilterChain, _Mapping]] = ...) -> None: ...
    class AndroidReportConfig(_message.Message):
        __slots__ = ("reporter_service_package", "reporter_service_class", "skip_report", "use_pipe_in_framework_for_testing")
        REPORTER_SERVICE_PACKAGE_FIELD_NUMBER: _ClassVar[int]
        REPORTER_SERVICE_CLASS_FIELD_NUMBER: _ClassVar[int]
        SKIP_REPORT_FIELD_NUMBER: _ClassVar[int]
        USE_PIPE_IN_FRAMEWORK_FOR_TESTING_FIELD_NUMBER: _ClassVar[int]
        reporter_service_package: str
        reporter_service_class: str
        skip_report: bool
        use_pipe_in_framework_for_testing: bool
        def __init__(self, reporter_service_package: _Optional[str] = ..., reporter_service_class: _Optional[str] = ..., skip_report: _Optional[bool] = ..., use_pipe_in_framework_for_testing: _Optional[bool] = ...) -> None: ...
    class CmdTraceStartDelay(_message.Message):
        __slots__ = ("min_delay_ms", "max_delay_ms")
        MIN_DELAY_MS_FIELD_NUMBER: _ClassVar[int]
        MAX_DELAY_MS_FIELD_NUMBER: _ClassVar[int]
        min_delay_ms: int
        max_delay_ms: int
        def __init__(self, min_delay_ms: _Optional[int] = ..., max_delay_ms: _Optional[int] = ...) -> None: ...
    class SessionSemaphore(_message.Message):
        __slots__ = ("name", "max_other_session_count")
        NAME_FIELD_NUMBER: _ClassVar[int]
        MAX_OTHER_SESSION_COUNT_FIELD_NUMBER: _ClassVar[int]
        name: str
        max_other_session_count: int
        def __init__(self, name: _Optional[str] = ..., max_other_session_count: _Optional[int] = ...) -> None: ...
    BUFFERS_FIELD_NUMBER: _ClassVar[int]
    DATA_SOURCES_FIELD_NUMBER: _ClassVar[int]
    BUILTIN_DATA_SOURCES_FIELD_NUMBER: _ClassVar[int]
    DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    PREFER_SUSPEND_CLOCK_FOR_DURATION_FIELD_NUMBER: _ClassVar[int]
    ENABLE_EXTRA_GUARDRAILS_FIELD_NUMBER: _ClassVar[int]
    LOCKDOWN_MODE_FIELD_NUMBER: _ClassVar[int]
    PRODUCERS_FIELD_NUMBER: _ClassVar[int]
    STATSD_METADATA_FIELD_NUMBER: _ClassVar[int]
    WRITE_INTO_FILE_FIELD_NUMBER: _ClassVar[int]
    OUTPUT_PATH_FIELD_NUMBER: _ClassVar[int]
    FILE_WRITE_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    MAX_FILE_SIZE_BYTES_FIELD_NUMBER: _ClassVar[int]
    GUARDRAIL_OVERRIDES_FIELD_NUMBER: _ClassVar[int]
    DEFERRED_START_FIELD_NUMBER: _ClassVar[int]
    FLUSH_PERIOD_MS_FIELD_NUMBER: _ClassVar[int]
    FLUSH_TIMEOUT_MS_FIELD_NUMBER: _ClassVar[int]
    DATA_SOURCE_STOP_TIMEOUT_MS_FIELD_NUMBER: _ClassVar[int]
    NOTIFY_TRACEUR_FIELD_NUMBER: _ClassVar[int]
    BUGREPORT_SCORE_FIELD_NUMBER: _ClassVar[int]
    BUGREPORT_FILENAME_FIELD_NUMBER: _ClassVar[int]
    TRIGGER_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ACTIVATE_TRIGGERS_FIELD_NUMBER: _ClassVar[int]
    INCREMENTAL_STATE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ALLOW_USER_BUILD_TRACING_FIELD_NUMBER: _ClassVar[int]
    UNIQUE_SESSION_NAME_FIELD_NUMBER: _ClassVar[int]
    COMPRESSION_TYPE_FIELD_NUMBER: _ClassVar[int]
    INCIDENT_REPORT_CONFIG_FIELD_NUMBER: _ClassVar[int]
    STATSD_LOGGING_FIELD_NUMBER: _ClassVar[int]
    TRACE_UUID_MSB_FIELD_NUMBER: _ClassVar[int]
    TRACE_UUID_LSB_FIELD_NUMBER: _ClassVar[int]
    TRACE_FILTER_FIELD_NUMBER: _ClassVar[int]
    ANDROID_REPORT_CONFIG_FIELD_NUMBER: _ClassVar[int]
    CMD_TRACE_START_DELAY_FIELD_NUMBER: _ClassVar[int]
    SESSION_SEMAPHORES_FIELD_NUMBER: _ClassVar[int]
    PRIORITY_BOOST_FIELD_NUMBER: _ClassVar[int]
    EXCLUSIVE_PRIO_FIELD_NUMBER: _ClassVar[int]
    buffers: _containers.RepeatedCompositeFieldContainer[TraceConfig.BufferConfig]
    data_sources: _containers.RepeatedCompositeFieldContainer[TraceConfig.DataSource]
    builtin_data_sources: TraceConfig.BuiltinDataSource
    duration_ms: int
    prefer_suspend_clock_for_duration: bool
    enable_extra_guardrails: bool
    lockdown_mode: TraceConfig.LockdownModeOperation
    producers: _containers.RepeatedCompositeFieldContainer[TraceConfig.ProducerConfig]
    statsd_metadata: TraceConfig.StatsdMetadata
    write_into_file: bool
    output_path: str
    file_write_period_ms: int
    max_file_size_bytes: int
    guardrail_overrides: TraceConfig.GuardrailOverrides
    deferred_start: bool
    flush_period_ms: int
    flush_timeout_ms: int
    data_source_stop_timeout_ms: int
    notify_traceur: bool
    bugreport_score: int
    bugreport_filename: str
    trigger_config: TraceConfig.TriggerConfig
    activate_triggers: _containers.RepeatedScalarFieldContainer[str]
    incremental_state_config: TraceConfig.IncrementalStateConfig
    allow_user_build_tracing: bool
    unique_session_name: str
    compression_type: TraceConfig.CompressionType
    incident_report_config: TraceConfig.IncidentReportConfig
    statsd_logging: TraceConfig.StatsdLogging
    trace_uuid_msb: int
    trace_uuid_lsb: int
    trace_filter: TraceConfig.TraceFilter
    android_report_config: TraceConfig.AndroidReportConfig
    cmd_trace_start_delay: TraceConfig.CmdTraceStartDelay
    session_semaphores: _containers.RepeatedCompositeFieldContainer[TraceConfig.SessionSemaphore]
    priority_boost: _priority_boost_config_pb2.PriorityBoostConfig
    exclusive_prio: int
    def __init__(self, buffers: _Optional[_Iterable[_Union[TraceConfig.BufferConfig, _Mapping]]] = ..., data_sources: _Optional[_Iterable[_Union[TraceConfig.DataSource, _Mapping]]] = ..., builtin_data_sources: _Optional[_Union[TraceConfig.BuiltinDataSource, _Mapping]] = ..., duration_ms: _Optional[int] = ..., prefer_suspend_clock_for_duration: _Optional[bool] = ..., enable_extra_guardrails: _Optional[bool] = ..., lockdown_mode: _Optional[_Union[TraceConfig.LockdownModeOperation, str]] = ..., producers: _Optional[_Iterable[_Union[TraceConfig.ProducerConfig, _Mapping]]] = ..., statsd_metadata: _Optional[_Union[TraceConfig.StatsdMetadata, _Mapping]] = ..., write_into_file: _Optional[bool] = ..., output_path: _Optional[str] = ..., file_write_period_ms: _Optional[int] = ..., max_file_size_bytes: _Optional[int] = ..., guardrail_overrides: _Optional[_Union[TraceConfig.GuardrailOverrides, _Mapping]] = ..., deferred_start: _Optional[bool] = ..., flush_period_ms: _Optional[int] = ..., flush_timeout_ms: _Optional[int] = ..., data_source_stop_timeout_ms: _Optional[int] = ..., notify_traceur: _Optional[bool] = ..., bugreport_score: _Optional[int] = ..., bugreport_filename: _Optional[str] = ..., trigger_config: _Optional[_Union[TraceConfig.TriggerConfig, _Mapping]] = ..., activate_triggers: _Optional[_Iterable[str]] = ..., incremental_state_config: _Optional[_Union[TraceConfig.IncrementalStateConfig, _Mapping]] = ..., allow_user_build_tracing: _Optional[bool] = ..., unique_session_name: _Optional[str] = ..., compression_type: _Optional[_Union[TraceConfig.CompressionType, str]] = ..., incident_report_config: _Optional[_Union[TraceConfig.IncidentReportConfig, _Mapping]] = ..., statsd_logging: _Optional[_Union[TraceConfig.StatsdLogging, str]] = ..., trace_uuid_msb: _Optional[int] = ..., trace_uuid_lsb: _Optional[int] = ..., trace_filter: _Optional[_Union[TraceConfig.TraceFilter, _Mapping]] = ..., android_report_config: _Optional[_Union[TraceConfig.AndroidReportConfig, _Mapping]] = ..., cmd_trace_start_delay: _Optional[_Union[TraceConfig.CmdTraceStartDelay, _Mapping]] = ..., session_semaphores: _Optional[_Iterable[_Union[TraceConfig.SessionSemaphore, _Mapping]]] = ..., priority_boost: _Optional[_Union[_priority_boost_config_pb2.PriorityBoostConfig, _Mapping]] = ..., exclusive_prio: _Optional[int] = ...) -> None: ...
