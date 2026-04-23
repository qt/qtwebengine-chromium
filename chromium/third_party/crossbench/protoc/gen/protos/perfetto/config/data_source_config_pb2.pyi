from protos.perfetto.config.android import android_game_intervention_list_config_pb2 as _android_game_intervention_list_config_pb2
from protos.perfetto.config.android import android_input_event_config_pb2 as _android_input_event_config_pb2
from protos.perfetto.config.android import android_log_config_pb2 as _android_log_config_pb2
from protos.perfetto.config.android import android_polled_state_config_pb2 as _android_polled_state_config_pb2
from protos.perfetto.config.android import android_system_property_config_pb2 as _android_system_property_config_pb2
from protos.perfetto.config.android import android_sdk_sysprop_guard_config_pb2 as _android_sdk_sysprop_guard_config_pb2
from protos.perfetto.config.android import app_wakelock_config_pb2 as _app_wakelock_config_pb2
from protos.perfetto.config.android import cpu_per_uid_config_pb2 as _cpu_per_uid_config_pb2
from protos.perfetto.config.android import kernel_wakelocks_config_pb2 as _kernel_wakelocks_config_pb2
from protos.perfetto.config.android import network_trace_config_pb2 as _network_trace_config_pb2
from protos.perfetto.config.android import packages_list_config_pb2 as _packages_list_config_pb2
from protos.perfetto.config.android import pixel_modem_config_pb2 as _pixel_modem_config_pb2
from protos.perfetto.config.android import protolog_config_pb2 as _protolog_config_pb2
from protos.perfetto.config.android import surfaceflinger_layers_config_pb2 as _surfaceflinger_layers_config_pb2
from protos.perfetto.config.android import surfaceflinger_transactions_config_pb2 as _surfaceflinger_transactions_config_pb2
from protos.perfetto.config.android import windowmanager_config_pb2 as _windowmanager_config_pb2
from protos.perfetto.config.chrome import chrome_config_pb2 as _chrome_config_pb2
from protos.perfetto.config.chrome import v8_config_pb2 as _v8_config_pb2
from protos.perfetto.config.etw import etw_config_pb2 as _etw_config_pb2
from protos.perfetto.config.chrome import system_metrics_pb2 as _system_metrics_pb2
from protos.perfetto.config.ftrace import ftrace_config_pb2 as _ftrace_config_pb2
from protos.perfetto.config.ftrace import frozen_ftrace_config_pb2 as _frozen_ftrace_config_pb2
from protos.perfetto.config.gpu import gpu_counter_config_pb2 as _gpu_counter_config_pb2
from protos.perfetto.config.gpu import vulkan_memory_config_pb2 as _vulkan_memory_config_pb2
from protos.perfetto.config.gpu import gpu_renderstages_config_pb2 as _gpu_renderstages_config_pb2
from protos.perfetto.config.inode_file import inode_file_config_pb2 as _inode_file_config_pb2
from protos.perfetto.config import interceptor_config_pb2 as _interceptor_config_pb2
from protos.perfetto.config.power import android_power_config_pb2 as _android_power_config_pb2
from protos.perfetto.config.statsd import statsd_tracing_config_pb2 as _statsd_tracing_config_pb2
from protos.perfetto.config.priority_boost import priority_boost_config_pb2 as _priority_boost_config_pb2
from protos.perfetto.config.process_stats import process_stats_config_pb2 as _process_stats_config_pb2
from protos.perfetto.config.profiling import heapprofd_config_pb2 as _heapprofd_config_pb2
from protos.perfetto.config.profiling import java_hprof_config_pb2 as _java_hprof_config_pb2
from protos.perfetto.config.profiling import perf_event_config_pb2 as _perf_event_config_pb2
from protos.perfetto.config.sys_stats import sys_stats_config_pb2 as _sys_stats_config_pb2
from protos.perfetto.config import test_config_pb2 as _test_config_pb2
from protos.perfetto.config.track_event import track_event_config_pb2 as _track_event_config_pb2
from protos.perfetto.config.system_info import system_info_config_pb2 as _system_info_config_pb2
from protos.perfetto.config.chrome import histogram_samples_pb2 as _histogram_samples_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class DataSourceConfig(_message.Message):
    __slots__ = ("name", "target_buffer", "trace_duration_ms", "prefer_suspend_clock_for_duration", "stop_timeout_ms", "enable_extra_guardrails", "session_initiator", "tracing_session_id", "buffer_exhausted_policy", "priority_boost", "ftrace_config", "inode_file_config", "process_stats_config", "sys_stats_config", "heapprofd_config", "java_hprof_config", "android_power_config", "android_log_config", "gpu_counter_config", "android_game_intervention_list_config", "packages_list_config", "perf_event_config", "vulkan_memory_config", "track_event_config", "android_polled_state_config", "android_system_property_config", "statsd_tracing_config", "system_info_config", "frozen_ftrace_config", "chrome_config", "v8_config", "interceptor_config", "network_packet_trace_config", "surfaceflinger_layers_config", "surfaceflinger_transactions_config", "android_sdk_sysprop_guard_config", "etw_config", "protolog_config", "android_input_event_config", "pixel_modem_config", "windowmanager_config", "chromium_system_metrics", "kernel_wakelocks_config", "gpu_renderstages_config", "chromium_histogram_samples", "app_wakelocks_config", "cpu_per_uid_config", "legacy_config", "for_testing")
    class SessionInitiator(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        SESSION_INITIATOR_UNSPECIFIED: _ClassVar[DataSourceConfig.SessionInitiator]
        SESSION_INITIATOR_TRUSTED_SYSTEM: _ClassVar[DataSourceConfig.SessionInitiator]
    SESSION_INITIATOR_UNSPECIFIED: DataSourceConfig.SessionInitiator
    SESSION_INITIATOR_TRUSTED_SYSTEM: DataSourceConfig.SessionInitiator
    class BufferExhaustedPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        BUFFER_EXHAUSTED_UNSPECIFIED: _ClassVar[DataSourceConfig.BufferExhaustedPolicy]
        BUFFER_EXHAUSTED_DROP: _ClassVar[DataSourceConfig.BufferExhaustedPolicy]
        BUFFER_EXHAUSTED_STALL_THEN_ABORT: _ClassVar[DataSourceConfig.BufferExhaustedPolicy]
        BUFFER_EXHAUSTED_STALL_THEN_DROP: _ClassVar[DataSourceConfig.BufferExhaustedPolicy]
    BUFFER_EXHAUSTED_UNSPECIFIED: DataSourceConfig.BufferExhaustedPolicy
    BUFFER_EXHAUSTED_DROP: DataSourceConfig.BufferExhaustedPolicy
    BUFFER_EXHAUSTED_STALL_THEN_ABORT: DataSourceConfig.BufferExhaustedPolicy
    BUFFER_EXHAUSTED_STALL_THEN_DROP: DataSourceConfig.BufferExhaustedPolicy
    NAME_FIELD_NUMBER: _ClassVar[int]
    TARGET_BUFFER_FIELD_NUMBER: _ClassVar[int]
    TRACE_DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    PREFER_SUSPEND_CLOCK_FOR_DURATION_FIELD_NUMBER: _ClassVar[int]
    STOP_TIMEOUT_MS_FIELD_NUMBER: _ClassVar[int]
    ENABLE_EXTRA_GUARDRAILS_FIELD_NUMBER: _ClassVar[int]
    SESSION_INITIATOR_FIELD_NUMBER: _ClassVar[int]
    TRACING_SESSION_ID_FIELD_NUMBER: _ClassVar[int]
    BUFFER_EXHAUSTED_POLICY_FIELD_NUMBER: _ClassVar[int]
    PRIORITY_BOOST_FIELD_NUMBER: _ClassVar[int]
    FTRACE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    INODE_FILE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PROCESS_STATS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    SYS_STATS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    HEAPPROFD_CONFIG_FIELD_NUMBER: _ClassVar[int]
    JAVA_HPROF_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_POWER_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_LOG_CONFIG_FIELD_NUMBER: _ClassVar[int]
    GPU_COUNTER_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_GAME_INTERVENTION_LIST_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PACKAGES_LIST_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PERF_EVENT_CONFIG_FIELD_NUMBER: _ClassVar[int]
    VULKAN_MEMORY_CONFIG_FIELD_NUMBER: _ClassVar[int]
    TRACK_EVENT_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_POLLED_STATE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_SYSTEM_PROPERTY_CONFIG_FIELD_NUMBER: _ClassVar[int]
    STATSD_TRACING_CONFIG_FIELD_NUMBER: _ClassVar[int]
    SYSTEM_INFO_CONFIG_FIELD_NUMBER: _ClassVar[int]
    FROZEN_FTRACE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    CHROME_CONFIG_FIELD_NUMBER: _ClassVar[int]
    V8_CONFIG_FIELD_NUMBER: _ClassVar[int]
    INTERCEPTOR_CONFIG_FIELD_NUMBER: _ClassVar[int]
    NETWORK_PACKET_TRACE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    SURFACEFLINGER_LAYERS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    SURFACEFLINGER_TRANSACTIONS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_SDK_SYSPROP_GUARD_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ETW_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PROTOLOG_CONFIG_FIELD_NUMBER: _ClassVar[int]
    ANDROID_INPUT_EVENT_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PIXEL_MODEM_CONFIG_FIELD_NUMBER: _ClassVar[int]
    WINDOWMANAGER_CONFIG_FIELD_NUMBER: _ClassVar[int]
    CHROMIUM_SYSTEM_METRICS_FIELD_NUMBER: _ClassVar[int]
    KERNEL_WAKELOCKS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    GPU_RENDERSTAGES_CONFIG_FIELD_NUMBER: _ClassVar[int]
    CHROMIUM_HISTOGRAM_SAMPLES_FIELD_NUMBER: _ClassVar[int]
    APP_WAKELOCKS_CONFIG_FIELD_NUMBER: _ClassVar[int]
    CPU_PER_UID_CONFIG_FIELD_NUMBER: _ClassVar[int]
    LEGACY_CONFIG_FIELD_NUMBER: _ClassVar[int]
    FOR_TESTING_FIELD_NUMBER: _ClassVar[int]
    name: str
    target_buffer: int
    trace_duration_ms: int
    prefer_suspend_clock_for_duration: bool
    stop_timeout_ms: int
    enable_extra_guardrails: bool
    session_initiator: DataSourceConfig.SessionInitiator
    tracing_session_id: int
    buffer_exhausted_policy: DataSourceConfig.BufferExhaustedPolicy
    priority_boost: _priority_boost_config_pb2.PriorityBoostConfig
    ftrace_config: _ftrace_config_pb2.FtraceConfig
    inode_file_config: _inode_file_config_pb2.InodeFileConfig
    process_stats_config: _process_stats_config_pb2.ProcessStatsConfig
    sys_stats_config: _sys_stats_config_pb2.SysStatsConfig
    heapprofd_config: _heapprofd_config_pb2.HeapprofdConfig
    java_hprof_config: _java_hprof_config_pb2.JavaHprofConfig
    android_power_config: _android_power_config_pb2.AndroidPowerConfig
    android_log_config: _android_log_config_pb2.AndroidLogConfig
    gpu_counter_config: _gpu_counter_config_pb2.GpuCounterConfig
    android_game_intervention_list_config: _android_game_intervention_list_config_pb2.AndroidGameInterventionListConfig
    packages_list_config: _packages_list_config_pb2.PackagesListConfig
    perf_event_config: _perf_event_config_pb2.PerfEventConfig
    vulkan_memory_config: _vulkan_memory_config_pb2.VulkanMemoryConfig
    track_event_config: _track_event_config_pb2.TrackEventConfig
    android_polled_state_config: _android_polled_state_config_pb2.AndroidPolledStateConfig
    android_system_property_config: _android_system_property_config_pb2.AndroidSystemPropertyConfig
    statsd_tracing_config: _statsd_tracing_config_pb2.StatsdTracingConfig
    system_info_config: _system_info_config_pb2.SystemInfoConfig
    frozen_ftrace_config: _frozen_ftrace_config_pb2.FrozenFtraceConfig
    chrome_config: _chrome_config_pb2.ChromeConfig
    v8_config: _v8_config_pb2.V8Config
    interceptor_config: _interceptor_config_pb2.InterceptorConfig
    network_packet_trace_config: _network_trace_config_pb2.NetworkPacketTraceConfig
    surfaceflinger_layers_config: _surfaceflinger_layers_config_pb2.SurfaceFlingerLayersConfig
    surfaceflinger_transactions_config: _surfaceflinger_transactions_config_pb2.SurfaceFlingerTransactionsConfig
    android_sdk_sysprop_guard_config: _android_sdk_sysprop_guard_config_pb2.AndroidSdkSyspropGuardConfig
    etw_config: _etw_config_pb2.EtwConfig
    protolog_config: _protolog_config_pb2.ProtoLogConfig
    android_input_event_config: _android_input_event_config_pb2.AndroidInputEventConfig
    pixel_modem_config: _pixel_modem_config_pb2.PixelModemConfig
    windowmanager_config: _windowmanager_config_pb2.WindowManagerConfig
    chromium_system_metrics: _system_metrics_pb2.ChromiumSystemMetricsConfig
    kernel_wakelocks_config: _kernel_wakelocks_config_pb2.KernelWakelocksConfig
    gpu_renderstages_config: _gpu_renderstages_config_pb2.GpuRenderStagesConfig
    chromium_histogram_samples: _histogram_samples_pb2.ChromiumHistogramSamplesConfig
    app_wakelocks_config: _app_wakelock_config_pb2.AppWakelocksConfig
    cpu_per_uid_config: _cpu_per_uid_config_pb2.CpuPerUidConfig
    legacy_config: str
    for_testing: _test_config_pb2.TestConfig
    def __init__(self, name: _Optional[str] = ..., target_buffer: _Optional[int] = ..., trace_duration_ms: _Optional[int] = ..., prefer_suspend_clock_for_duration: _Optional[bool] = ..., stop_timeout_ms: _Optional[int] = ..., enable_extra_guardrails: _Optional[bool] = ..., session_initiator: _Optional[_Union[DataSourceConfig.SessionInitiator, str]] = ..., tracing_session_id: _Optional[int] = ..., buffer_exhausted_policy: _Optional[_Union[DataSourceConfig.BufferExhaustedPolicy, str]] = ..., priority_boost: _Optional[_Union[_priority_boost_config_pb2.PriorityBoostConfig, _Mapping]] = ..., ftrace_config: _Optional[_Union[_ftrace_config_pb2.FtraceConfig, _Mapping]] = ..., inode_file_config: _Optional[_Union[_inode_file_config_pb2.InodeFileConfig, _Mapping]] = ..., process_stats_config: _Optional[_Union[_process_stats_config_pb2.ProcessStatsConfig, _Mapping]] = ..., sys_stats_config: _Optional[_Union[_sys_stats_config_pb2.SysStatsConfig, _Mapping]] = ..., heapprofd_config: _Optional[_Union[_heapprofd_config_pb2.HeapprofdConfig, _Mapping]] = ..., java_hprof_config: _Optional[_Union[_java_hprof_config_pb2.JavaHprofConfig, _Mapping]] = ..., android_power_config: _Optional[_Union[_android_power_config_pb2.AndroidPowerConfig, _Mapping]] = ..., android_log_config: _Optional[_Union[_android_log_config_pb2.AndroidLogConfig, _Mapping]] = ..., gpu_counter_config: _Optional[_Union[_gpu_counter_config_pb2.GpuCounterConfig, _Mapping]] = ..., android_game_intervention_list_config: _Optional[_Union[_android_game_intervention_list_config_pb2.AndroidGameInterventionListConfig, _Mapping]] = ..., packages_list_config: _Optional[_Union[_packages_list_config_pb2.PackagesListConfig, _Mapping]] = ..., perf_event_config: _Optional[_Union[_perf_event_config_pb2.PerfEventConfig, _Mapping]] = ..., vulkan_memory_config: _Optional[_Union[_vulkan_memory_config_pb2.VulkanMemoryConfig, _Mapping]] = ..., track_event_config: _Optional[_Union[_track_event_config_pb2.TrackEventConfig, _Mapping]] = ..., android_polled_state_config: _Optional[_Union[_android_polled_state_config_pb2.AndroidPolledStateConfig, _Mapping]] = ..., android_system_property_config: _Optional[_Union[_android_system_property_config_pb2.AndroidSystemPropertyConfig, _Mapping]] = ..., statsd_tracing_config: _Optional[_Union[_statsd_tracing_config_pb2.StatsdTracingConfig, _Mapping]] = ..., system_info_config: _Optional[_Union[_system_info_config_pb2.SystemInfoConfig, _Mapping]] = ..., frozen_ftrace_config: _Optional[_Union[_frozen_ftrace_config_pb2.FrozenFtraceConfig, _Mapping]] = ..., chrome_config: _Optional[_Union[_chrome_config_pb2.ChromeConfig, _Mapping]] = ..., v8_config: _Optional[_Union[_v8_config_pb2.V8Config, _Mapping]] = ..., interceptor_config: _Optional[_Union[_interceptor_config_pb2.InterceptorConfig, _Mapping]] = ..., network_packet_trace_config: _Optional[_Union[_network_trace_config_pb2.NetworkPacketTraceConfig, _Mapping]] = ..., surfaceflinger_layers_config: _Optional[_Union[_surfaceflinger_layers_config_pb2.SurfaceFlingerLayersConfig, _Mapping]] = ..., surfaceflinger_transactions_config: _Optional[_Union[_surfaceflinger_transactions_config_pb2.SurfaceFlingerTransactionsConfig, _Mapping]] = ..., android_sdk_sysprop_guard_config: _Optional[_Union[_android_sdk_sysprop_guard_config_pb2.AndroidSdkSyspropGuardConfig, _Mapping]] = ..., etw_config: _Optional[_Union[_etw_config_pb2.EtwConfig, _Mapping]] = ..., protolog_config: _Optional[_Union[_protolog_config_pb2.ProtoLogConfig, _Mapping]] = ..., android_input_event_config: _Optional[_Union[_android_input_event_config_pb2.AndroidInputEventConfig, _Mapping]] = ..., pixel_modem_config: _Optional[_Union[_pixel_modem_config_pb2.PixelModemConfig, _Mapping]] = ..., windowmanager_config: _Optional[_Union[_windowmanager_config_pb2.WindowManagerConfig, _Mapping]] = ..., chromium_system_metrics: _Optional[_Union[_system_metrics_pb2.ChromiumSystemMetricsConfig, _Mapping]] = ..., kernel_wakelocks_config: _Optional[_Union[_kernel_wakelocks_config_pb2.KernelWakelocksConfig, _Mapping]] = ..., gpu_renderstages_config: _Optional[_Union[_gpu_renderstages_config_pb2.GpuRenderStagesConfig, _Mapping]] = ..., chromium_histogram_samples: _Optional[_Union[_histogram_samples_pb2.ChromiumHistogramSamplesConfig, _Mapping]] = ..., app_wakelocks_config: _Optional[_Union[_app_wakelock_config_pb2.AppWakelocksConfig, _Mapping]] = ..., cpu_per_uid_config: _Optional[_Union[_cpu_per_uid_config_pb2.CpuPerUidConfig, _Mapping]] = ..., legacy_config: _Optional[str] = ..., for_testing: _Optional[_Union[_test_config_pb2.TestConfig, _Mapping]] = ...) -> None: ...
