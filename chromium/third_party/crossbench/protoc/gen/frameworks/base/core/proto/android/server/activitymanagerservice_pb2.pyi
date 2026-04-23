from frameworks.base.core.proto.android.app import activitymanager_pb2 as _activitymanager_pb2
from frameworks.base.core.proto.android.app import appexitinfo_pb2 as _appexitinfo_pb2
from frameworks.base.core.proto.android.app import appstartinfo_pb2 as _appstartinfo_pb2
from frameworks.base.core.proto.android.app import notification_pb2 as _notification_pb2
from frameworks.base.core.proto.android.app import profilerinfo_pb2 as _profilerinfo_pb2
from frameworks.base.core.proto.android.content import component_name_pb2 as _component_name_pb2
from frameworks.base.core.proto.android.content import configuration_pb2 as _configuration_pb2
from frameworks.base.core.proto.android.content import intent_pb2 as _intent_pb2
from frameworks.base.core.proto.android.content import package_item_info_pb2 as _package_item_info_pb2
from frameworks.base.core.proto.android.internal import processstats_pb2 as _processstats_pb2
from frameworks.base.core.proto.android.os import bundle_pb2 as _bundle_pb2
from frameworks.base.core.proto.android.os import looper_pb2 as _looper_pb2
from frameworks.base.core.proto.android.os import powermanager_pb2 as _powermanager_pb2
from frameworks.base.core.proto.android.server import intentresolver_pb2 as _intentresolver_pb2
from frameworks.base.core.proto.android.server import windowmanagerservice_pb2 as _windowmanagerservice_pb2
from frameworks.base.core.proto.android.util import common_pb2 as _common_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.proto_logging.stats.enums.app_shared import app_enums_pb2 as _app_enums_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ActivityManagerServiceProto(_message.Message):
    __slots__ = ("activities", "broadcasts", "services", "processes")
    ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
    BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    SERVICES_FIELD_NUMBER: _ClassVar[int]
    PROCESSES_FIELD_NUMBER: _ClassVar[int]
    activities: ActivityManagerServiceDumpActivitiesProto
    broadcasts: ActivityManagerServiceDumpBroadcastsProto
    services: ActivityManagerServiceDumpServicesProto
    processes: ActivityManagerServiceDumpProcessesProto
    def __init__(self, activities: _Optional[_Union[ActivityManagerServiceDumpActivitiesProto, _Mapping]] = ..., broadcasts: _Optional[_Union[ActivityManagerServiceDumpBroadcastsProto, _Mapping]] = ..., services: _Optional[_Union[ActivityManagerServiceDumpServicesProto, _Mapping]] = ..., processes: _Optional[_Union[ActivityManagerServiceDumpProcessesProto, _Mapping]] = ...) -> None: ...

class ActivityManagerServiceDumpActivitiesProto(_message.Message):
    __slots__ = ("root_window_container",)
    ROOT_WINDOW_CONTAINER_FIELD_NUMBER: _ClassVar[int]
    root_window_container: _windowmanagerservice_pb2.RootWindowContainerProto
    def __init__(self, root_window_container: _Optional[_Union[_windowmanagerservice_pb2.RootWindowContainerProto, _Mapping]] = ...) -> None: ...

class ActivityManagerServiceDumpBroadcastsProto(_message.Message):
    __slots__ = ("receiver_list", "receiver_resolver", "broadcast_queue", "sticky_broadcasts", "handler")
    class MainHandler(_message.Message):
        __slots__ = ("handler", "looper")
        HANDLER_FIELD_NUMBER: _ClassVar[int]
        LOOPER_FIELD_NUMBER: _ClassVar[int]
        handler: str
        looper: _looper_pb2.LooperProto
        def __init__(self, handler: _Optional[str] = ..., looper: _Optional[_Union[_looper_pb2.LooperProto, _Mapping]] = ...) -> None: ...
    RECEIVER_LIST_FIELD_NUMBER: _ClassVar[int]
    RECEIVER_RESOLVER_FIELD_NUMBER: _ClassVar[int]
    BROADCAST_QUEUE_FIELD_NUMBER: _ClassVar[int]
    STICKY_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    HANDLER_FIELD_NUMBER: _ClassVar[int]
    receiver_list: _containers.RepeatedCompositeFieldContainer[ReceiverListProto]
    receiver_resolver: _intentresolver_pb2.IntentResolverProto
    broadcast_queue: _containers.RepeatedCompositeFieldContainer[BroadcastQueueProto]
    sticky_broadcasts: _containers.RepeatedCompositeFieldContainer[StickyBroadcastProto]
    handler: ActivityManagerServiceDumpBroadcastsProto.MainHandler
    def __init__(self, receiver_list: _Optional[_Iterable[_Union[ReceiverListProto, _Mapping]]] = ..., receiver_resolver: _Optional[_Union[_intentresolver_pb2.IntentResolverProto, _Mapping]] = ..., broadcast_queue: _Optional[_Iterable[_Union[BroadcastQueueProto, _Mapping]]] = ..., sticky_broadcasts: _Optional[_Iterable[_Union[StickyBroadcastProto, _Mapping]]] = ..., handler: _Optional[_Union[ActivityManagerServiceDumpBroadcastsProto.MainHandler, _Mapping]] = ...) -> None: ...

class ReceiverListProto(_message.Message):
    __slots__ = ("app", "pid", "uid", "user", "current", "linked_to_death", "filters", "hex_hash", "number_receivers")
    APP_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    UID_FIELD_NUMBER: _ClassVar[int]
    USER_FIELD_NUMBER: _ClassVar[int]
    CURRENT_FIELD_NUMBER: _ClassVar[int]
    LINKED_TO_DEATH_FIELD_NUMBER: _ClassVar[int]
    FILTERS_FIELD_NUMBER: _ClassVar[int]
    HEX_HASH_FIELD_NUMBER: _ClassVar[int]
    NUMBER_RECEIVERS_FIELD_NUMBER: _ClassVar[int]
    app: ProcessRecordProto
    pid: int
    uid: int
    user: int
    current: BroadcastRecordProto
    linked_to_death: bool
    filters: _containers.RepeatedCompositeFieldContainer[BroadcastFilterProto]
    hex_hash: str
    number_receivers: int
    def __init__(self, app: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., pid: _Optional[int] = ..., uid: _Optional[int] = ..., user: _Optional[int] = ..., current: _Optional[_Union[BroadcastRecordProto, _Mapping]] = ..., linked_to_death: _Optional[bool] = ..., filters: _Optional[_Iterable[_Union[BroadcastFilterProto, _Mapping]]] = ..., hex_hash: _Optional[str] = ..., number_receivers: _Optional[int] = ...) -> None: ...

class ProcessRecordProto(_message.Message):
    __slots__ = ("pid", "process_name", "uid", "user_id", "app_id", "isolated_app_id", "persistent", "lru_index")
    PID_FIELD_NUMBER: _ClassVar[int]
    PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
    UID_FIELD_NUMBER: _ClassVar[int]
    USER_ID_FIELD_NUMBER: _ClassVar[int]
    APP_ID_FIELD_NUMBER: _ClassVar[int]
    ISOLATED_APP_ID_FIELD_NUMBER: _ClassVar[int]
    PERSISTENT_FIELD_NUMBER: _ClassVar[int]
    LRU_INDEX_FIELD_NUMBER: _ClassVar[int]
    pid: int
    process_name: str
    uid: int
    user_id: int
    app_id: int
    isolated_app_id: int
    persistent: bool
    lru_index: int
    def __init__(self, pid: _Optional[int] = ..., process_name: _Optional[str] = ..., uid: _Optional[int] = ..., user_id: _Optional[int] = ..., app_id: _Optional[int] = ..., isolated_app_id: _Optional[int] = ..., persistent: _Optional[bool] = ..., lru_index: _Optional[int] = ...) -> None: ...

class BroadcastRecordProto(_message.Message):
    __slots__ = ("user_id", "intent_action")
    USER_ID_FIELD_NUMBER: _ClassVar[int]
    INTENT_ACTION_FIELD_NUMBER: _ClassVar[int]
    user_id: int
    intent_action: str
    def __init__(self, user_id: _Optional[int] = ..., intent_action: _Optional[str] = ...) -> None: ...

class BroadcastFilterProto(_message.Message):
    __slots__ = ("intent_filter", "required_permission", "hex_hash", "owning_user_id")
    INTENT_FILTER_FIELD_NUMBER: _ClassVar[int]
    REQUIRED_PERMISSION_FIELD_NUMBER: _ClassVar[int]
    HEX_HASH_FIELD_NUMBER: _ClassVar[int]
    OWNING_USER_ID_FIELD_NUMBER: _ClassVar[int]
    intent_filter: _intent_pb2.IntentFilterProto
    required_permission: str
    hex_hash: str
    owning_user_id: int
    def __init__(self, intent_filter: _Optional[_Union[_intent_pb2.IntentFilterProto, _Mapping]] = ..., required_permission: _Optional[str] = ..., hex_hash: _Optional[str] = ..., owning_user_id: _Optional[int] = ...) -> None: ...

class BroadcastQueueProto(_message.Message):
    __slots__ = ("queue_name", "parallel_broadcasts", "ordered_broadcasts", "pending_broadcast", "historical_broadcasts", "historical_broadcasts_summary", "pending_broadcasts", "frozen_broadcasts")
    class BroadcastSummary(_message.Message):
        __slots__ = ("intent", "enqueue_clock_time_ms", "dispatch_clock_time_ms", "finish_clock_time_ms")
        INTENT_FIELD_NUMBER: _ClassVar[int]
        ENQUEUE_CLOCK_TIME_MS_FIELD_NUMBER: _ClassVar[int]
        DISPATCH_CLOCK_TIME_MS_FIELD_NUMBER: _ClassVar[int]
        FINISH_CLOCK_TIME_MS_FIELD_NUMBER: _ClassVar[int]
        intent: _intent_pb2.IntentProto
        enqueue_clock_time_ms: int
        dispatch_clock_time_ms: int
        finish_clock_time_ms: int
        def __init__(self, intent: _Optional[_Union[_intent_pb2.IntentProto, _Mapping]] = ..., enqueue_clock_time_ms: _Optional[int] = ..., dispatch_clock_time_ms: _Optional[int] = ..., finish_clock_time_ms: _Optional[int] = ...) -> None: ...
    QUEUE_NAME_FIELD_NUMBER: _ClassVar[int]
    PARALLEL_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    ORDERED_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    PENDING_BROADCAST_FIELD_NUMBER: _ClassVar[int]
    HISTORICAL_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    HISTORICAL_BROADCASTS_SUMMARY_FIELD_NUMBER: _ClassVar[int]
    PENDING_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    FROZEN_BROADCASTS_FIELD_NUMBER: _ClassVar[int]
    queue_name: str
    parallel_broadcasts: _containers.RepeatedCompositeFieldContainer[BroadcastRecordProto]
    ordered_broadcasts: _containers.RepeatedCompositeFieldContainer[BroadcastRecordProto]
    pending_broadcast: BroadcastRecordProto
    historical_broadcasts: _containers.RepeatedCompositeFieldContainer[BroadcastRecordProto]
    historical_broadcasts_summary: _containers.RepeatedCompositeFieldContainer[BroadcastQueueProto.BroadcastSummary]
    pending_broadcasts: _containers.RepeatedCompositeFieldContainer[BroadcastRecordProto]
    frozen_broadcasts: _containers.RepeatedCompositeFieldContainer[BroadcastRecordProto]
    def __init__(self, queue_name: _Optional[str] = ..., parallel_broadcasts: _Optional[_Iterable[_Union[BroadcastRecordProto, _Mapping]]] = ..., ordered_broadcasts: _Optional[_Iterable[_Union[BroadcastRecordProto, _Mapping]]] = ..., pending_broadcast: _Optional[_Union[BroadcastRecordProto, _Mapping]] = ..., historical_broadcasts: _Optional[_Iterable[_Union[BroadcastRecordProto, _Mapping]]] = ..., historical_broadcasts_summary: _Optional[_Iterable[_Union[BroadcastQueueProto.BroadcastSummary, _Mapping]]] = ..., pending_broadcasts: _Optional[_Iterable[_Union[BroadcastRecordProto, _Mapping]]] = ..., frozen_broadcasts: _Optional[_Iterable[_Union[BroadcastRecordProto, _Mapping]]] = ...) -> None: ...

class MemInfoDumpProto(_message.Message):
    __slots__ = ("uptime_duration_ms", "elapsed_realtime_ms", "native_processes", "app_processes", "total_rss_by_process", "total_rss_by_oom_adjustment", "total_rss_by_category", "total_pss_by_process", "total_pss_by_oom_adjustment", "total_pss_by_category", "total_ram_kb", "status", "cached_pss_kb", "cached_kernel_kb", "free_kb", "used_pss_kb", "used_kernel_kb", "lost_ram_kb", "total_zram_kb", "zram_physical_used_in_swap_kb", "total_zram_swap_kb", "ksm_sharing_kb", "ksm_shared_kb", "ksm_unshared_kb", "ksm_volatile_kb", "tuning_mb", "tuning_large_mb", "oom_kb", "restore_limit_kb", "is_low_ram_device", "is_high_end_gfx")
    class ProcessMemory(_message.Message):
        __slots__ = ("pid", "process_name", "native_heap", "dalvik_heap", "other_heaps", "unknown_heap", "total_heap", "dalvik_details", "app_summary")
        class MemoryInfo(_message.Message):
            __slots__ = ("name", "total_pss_kb", "clean_pss_kb", "shared_dirty_kb", "private_dirty_kb", "shared_clean_kb", "private_clean_kb", "dirty_swap_kb", "dirty_swap_pss_kb", "total_rss_kb")
            NAME_FIELD_NUMBER: _ClassVar[int]
            TOTAL_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            CLEAN_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            SHARED_DIRTY_KB_FIELD_NUMBER: _ClassVar[int]
            PRIVATE_DIRTY_KB_FIELD_NUMBER: _ClassVar[int]
            SHARED_CLEAN_KB_FIELD_NUMBER: _ClassVar[int]
            PRIVATE_CLEAN_KB_FIELD_NUMBER: _ClassVar[int]
            DIRTY_SWAP_KB_FIELD_NUMBER: _ClassVar[int]
            DIRTY_SWAP_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            TOTAL_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            name: str
            total_pss_kb: int
            clean_pss_kb: int
            shared_dirty_kb: int
            private_dirty_kb: int
            shared_clean_kb: int
            private_clean_kb: int
            dirty_swap_kb: int
            dirty_swap_pss_kb: int
            total_rss_kb: int
            def __init__(self, name: _Optional[str] = ..., total_pss_kb: _Optional[int] = ..., clean_pss_kb: _Optional[int] = ..., shared_dirty_kb: _Optional[int] = ..., private_dirty_kb: _Optional[int] = ..., shared_clean_kb: _Optional[int] = ..., private_clean_kb: _Optional[int] = ..., dirty_swap_kb: _Optional[int] = ..., dirty_swap_pss_kb: _Optional[int] = ..., total_rss_kb: _Optional[int] = ...) -> None: ...
        class HeapInfo(_message.Message):
            __slots__ = ("mem_info", "heap_size_kb", "heap_alloc_kb", "heap_free_kb")
            MEM_INFO_FIELD_NUMBER: _ClassVar[int]
            HEAP_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
            HEAP_ALLOC_KB_FIELD_NUMBER: _ClassVar[int]
            HEAP_FREE_KB_FIELD_NUMBER: _ClassVar[int]
            mem_info: MemInfoDumpProto.ProcessMemory.MemoryInfo
            heap_size_kb: int
            heap_alloc_kb: int
            heap_free_kb: int
            def __init__(self, mem_info: _Optional[_Union[MemInfoDumpProto.ProcessMemory.MemoryInfo, _Mapping]] = ..., heap_size_kb: _Optional[int] = ..., heap_alloc_kb: _Optional[int] = ..., heap_free_kb: _Optional[int] = ...) -> None: ...
        class AppSummary(_message.Message):
            __slots__ = ("java_heap_pss_kb", "native_heap_pss_kb", "code_pss_kb", "stack_pss_kb", "graphics_pss_kb", "private_other_pss_kb", "system_pss_kb", "total_swap_pss", "total_swap_kb", "java_heap_rss_kb", "native_heap_rss_kb", "code_rss_kb", "stack_rss_kb", "graphics_rss_kb", "unknown_rss_kb")
            JAVA_HEAP_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            NATIVE_HEAP_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            CODE_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            STACK_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            GRAPHICS_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            PRIVATE_OTHER_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            SYSTEM_PSS_KB_FIELD_NUMBER: _ClassVar[int]
            TOTAL_SWAP_PSS_FIELD_NUMBER: _ClassVar[int]
            TOTAL_SWAP_KB_FIELD_NUMBER: _ClassVar[int]
            JAVA_HEAP_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            NATIVE_HEAP_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            CODE_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            STACK_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            GRAPHICS_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            UNKNOWN_RSS_KB_FIELD_NUMBER: _ClassVar[int]
            java_heap_pss_kb: int
            native_heap_pss_kb: int
            code_pss_kb: int
            stack_pss_kb: int
            graphics_pss_kb: int
            private_other_pss_kb: int
            system_pss_kb: int
            total_swap_pss: int
            total_swap_kb: int
            java_heap_rss_kb: int
            native_heap_rss_kb: int
            code_rss_kb: int
            stack_rss_kb: int
            graphics_rss_kb: int
            unknown_rss_kb: int
            def __init__(self, java_heap_pss_kb: _Optional[int] = ..., native_heap_pss_kb: _Optional[int] = ..., code_pss_kb: _Optional[int] = ..., stack_pss_kb: _Optional[int] = ..., graphics_pss_kb: _Optional[int] = ..., private_other_pss_kb: _Optional[int] = ..., system_pss_kb: _Optional[int] = ..., total_swap_pss: _Optional[int] = ..., total_swap_kb: _Optional[int] = ..., java_heap_rss_kb: _Optional[int] = ..., native_heap_rss_kb: _Optional[int] = ..., code_rss_kb: _Optional[int] = ..., stack_rss_kb: _Optional[int] = ..., graphics_rss_kb: _Optional[int] = ..., unknown_rss_kb: _Optional[int] = ...) -> None: ...
        PID_FIELD_NUMBER: _ClassVar[int]
        PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
        NATIVE_HEAP_FIELD_NUMBER: _ClassVar[int]
        DALVIK_HEAP_FIELD_NUMBER: _ClassVar[int]
        OTHER_HEAPS_FIELD_NUMBER: _ClassVar[int]
        UNKNOWN_HEAP_FIELD_NUMBER: _ClassVar[int]
        TOTAL_HEAP_FIELD_NUMBER: _ClassVar[int]
        DALVIK_DETAILS_FIELD_NUMBER: _ClassVar[int]
        APP_SUMMARY_FIELD_NUMBER: _ClassVar[int]
        pid: int
        process_name: str
        native_heap: MemInfoDumpProto.ProcessMemory.HeapInfo
        dalvik_heap: MemInfoDumpProto.ProcessMemory.HeapInfo
        other_heaps: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.ProcessMemory.MemoryInfo]
        unknown_heap: MemInfoDumpProto.ProcessMemory.MemoryInfo
        total_heap: MemInfoDumpProto.ProcessMemory.HeapInfo
        dalvik_details: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.ProcessMemory.MemoryInfo]
        app_summary: MemInfoDumpProto.ProcessMemory.AppSummary
        def __init__(self, pid: _Optional[int] = ..., process_name: _Optional[str] = ..., native_heap: _Optional[_Union[MemInfoDumpProto.ProcessMemory.HeapInfo, _Mapping]] = ..., dalvik_heap: _Optional[_Union[MemInfoDumpProto.ProcessMemory.HeapInfo, _Mapping]] = ..., other_heaps: _Optional[_Iterable[_Union[MemInfoDumpProto.ProcessMemory.MemoryInfo, _Mapping]]] = ..., unknown_heap: _Optional[_Union[MemInfoDumpProto.ProcessMemory.MemoryInfo, _Mapping]] = ..., total_heap: _Optional[_Union[MemInfoDumpProto.ProcessMemory.HeapInfo, _Mapping]] = ..., dalvik_details: _Optional[_Iterable[_Union[MemInfoDumpProto.ProcessMemory.MemoryInfo, _Mapping]]] = ..., app_summary: _Optional[_Union[MemInfoDumpProto.ProcessMemory.AppSummary, _Mapping]] = ...) -> None: ...
    class AppData(_message.Message):
        __slots__ = ("process_memory", "objects", "sql", "asset_allocations", "unreachable_memory")
        class ObjectStats(_message.Message):
            __slots__ = ("view_instance_count", "view_root_instance_count", "app_context_instance_count", "activity_instance_count", "global_asset_count", "global_asset_manager_count", "local_binder_object_count", "proxy_binder_object_count", "parcel_memory_kb", "parcel_count", "binder_object_death_count", "open_ssl_socket_count", "webview_instance_count")
            VIEW_INSTANCE_COUNT_FIELD_NUMBER: _ClassVar[int]
            VIEW_ROOT_INSTANCE_COUNT_FIELD_NUMBER: _ClassVar[int]
            APP_CONTEXT_INSTANCE_COUNT_FIELD_NUMBER: _ClassVar[int]
            ACTIVITY_INSTANCE_COUNT_FIELD_NUMBER: _ClassVar[int]
            GLOBAL_ASSET_COUNT_FIELD_NUMBER: _ClassVar[int]
            GLOBAL_ASSET_MANAGER_COUNT_FIELD_NUMBER: _ClassVar[int]
            LOCAL_BINDER_OBJECT_COUNT_FIELD_NUMBER: _ClassVar[int]
            PROXY_BINDER_OBJECT_COUNT_FIELD_NUMBER: _ClassVar[int]
            PARCEL_MEMORY_KB_FIELD_NUMBER: _ClassVar[int]
            PARCEL_COUNT_FIELD_NUMBER: _ClassVar[int]
            BINDER_OBJECT_DEATH_COUNT_FIELD_NUMBER: _ClassVar[int]
            OPEN_SSL_SOCKET_COUNT_FIELD_NUMBER: _ClassVar[int]
            WEBVIEW_INSTANCE_COUNT_FIELD_NUMBER: _ClassVar[int]
            view_instance_count: int
            view_root_instance_count: int
            app_context_instance_count: int
            activity_instance_count: int
            global_asset_count: int
            global_asset_manager_count: int
            local_binder_object_count: int
            proxy_binder_object_count: int
            parcel_memory_kb: int
            parcel_count: int
            binder_object_death_count: int
            open_ssl_socket_count: int
            webview_instance_count: int
            def __init__(self, view_instance_count: _Optional[int] = ..., view_root_instance_count: _Optional[int] = ..., app_context_instance_count: _Optional[int] = ..., activity_instance_count: _Optional[int] = ..., global_asset_count: _Optional[int] = ..., global_asset_manager_count: _Optional[int] = ..., local_binder_object_count: _Optional[int] = ..., proxy_binder_object_count: _Optional[int] = ..., parcel_memory_kb: _Optional[int] = ..., parcel_count: _Optional[int] = ..., binder_object_death_count: _Optional[int] = ..., open_ssl_socket_count: _Optional[int] = ..., webview_instance_count: _Optional[int] = ...) -> None: ...
        class SqlStats(_message.Message):
            __slots__ = ("memory_used_kb", "pagecache_overflow_kb", "malloc_size_kb", "databases")
            class Database(_message.Message):
                __slots__ = ("name", "page_size", "db_size", "lookaside_b", "cache", "cache_hits", "cache_misses", "cache_size")
                NAME_FIELD_NUMBER: _ClassVar[int]
                PAGE_SIZE_FIELD_NUMBER: _ClassVar[int]
                DB_SIZE_FIELD_NUMBER: _ClassVar[int]
                LOOKASIDE_B_FIELD_NUMBER: _ClassVar[int]
                CACHE_FIELD_NUMBER: _ClassVar[int]
                CACHE_HITS_FIELD_NUMBER: _ClassVar[int]
                CACHE_MISSES_FIELD_NUMBER: _ClassVar[int]
                CACHE_SIZE_FIELD_NUMBER: _ClassVar[int]
                name: str
                page_size: int
                db_size: int
                lookaside_b: int
                cache: str
                cache_hits: int
                cache_misses: int
                cache_size: int
                def __init__(self, name: _Optional[str] = ..., page_size: _Optional[int] = ..., db_size: _Optional[int] = ..., lookaside_b: _Optional[int] = ..., cache: _Optional[str] = ..., cache_hits: _Optional[int] = ..., cache_misses: _Optional[int] = ..., cache_size: _Optional[int] = ...) -> None: ...
            MEMORY_USED_KB_FIELD_NUMBER: _ClassVar[int]
            PAGECACHE_OVERFLOW_KB_FIELD_NUMBER: _ClassVar[int]
            MALLOC_SIZE_KB_FIELD_NUMBER: _ClassVar[int]
            DATABASES_FIELD_NUMBER: _ClassVar[int]
            memory_used_kb: int
            pagecache_overflow_kb: int
            malloc_size_kb: int
            databases: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.AppData.SqlStats.Database]
            def __init__(self, memory_used_kb: _Optional[int] = ..., pagecache_overflow_kb: _Optional[int] = ..., malloc_size_kb: _Optional[int] = ..., databases: _Optional[_Iterable[_Union[MemInfoDumpProto.AppData.SqlStats.Database, _Mapping]]] = ...) -> None: ...
        PROCESS_MEMORY_FIELD_NUMBER: _ClassVar[int]
        OBJECTS_FIELD_NUMBER: _ClassVar[int]
        SQL_FIELD_NUMBER: _ClassVar[int]
        ASSET_ALLOCATIONS_FIELD_NUMBER: _ClassVar[int]
        UNREACHABLE_MEMORY_FIELD_NUMBER: _ClassVar[int]
        process_memory: MemInfoDumpProto.ProcessMemory
        objects: MemInfoDumpProto.AppData.ObjectStats
        sql: MemInfoDumpProto.AppData.SqlStats
        asset_allocations: str
        unreachable_memory: str
        def __init__(self, process_memory: _Optional[_Union[MemInfoDumpProto.ProcessMemory, _Mapping]] = ..., objects: _Optional[_Union[MemInfoDumpProto.AppData.ObjectStats, _Mapping]] = ..., sql: _Optional[_Union[MemInfoDumpProto.AppData.SqlStats, _Mapping]] = ..., asset_allocations: _Optional[str] = ..., unreachable_memory: _Optional[str] = ...) -> None: ...
    class MemItem(_message.Message):
        __slots__ = ("tag", "label", "id", "is_proc", "has_activities", "pss_kb", "rss_kb", "swap_pss_kb", "sub_items")
        TAG_FIELD_NUMBER: _ClassVar[int]
        LABEL_FIELD_NUMBER: _ClassVar[int]
        ID_FIELD_NUMBER: _ClassVar[int]
        IS_PROC_FIELD_NUMBER: _ClassVar[int]
        HAS_ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
        PSS_KB_FIELD_NUMBER: _ClassVar[int]
        RSS_KB_FIELD_NUMBER: _ClassVar[int]
        SWAP_PSS_KB_FIELD_NUMBER: _ClassVar[int]
        SUB_ITEMS_FIELD_NUMBER: _ClassVar[int]
        tag: str
        label: str
        id: int
        is_proc: bool
        has_activities: bool
        pss_kb: int
        rss_kb: int
        swap_pss_kb: int
        sub_items: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
        def __init__(self, tag: _Optional[str] = ..., label: _Optional[str] = ..., id: _Optional[int] = ..., is_proc: _Optional[bool] = ..., has_activities: _Optional[bool] = ..., pss_kb: _Optional[int] = ..., rss_kb: _Optional[int] = ..., swap_pss_kb: _Optional[int] = ..., sub_items: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ...) -> None: ...
    UPTIME_DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    ELAPSED_REALTIME_MS_FIELD_NUMBER: _ClassVar[int]
    NATIVE_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    APP_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    TOTAL_RSS_BY_PROCESS_FIELD_NUMBER: _ClassVar[int]
    TOTAL_RSS_BY_OOM_ADJUSTMENT_FIELD_NUMBER: _ClassVar[int]
    TOTAL_RSS_BY_CATEGORY_FIELD_NUMBER: _ClassVar[int]
    TOTAL_PSS_BY_PROCESS_FIELD_NUMBER: _ClassVar[int]
    TOTAL_PSS_BY_OOM_ADJUSTMENT_FIELD_NUMBER: _ClassVar[int]
    TOTAL_PSS_BY_CATEGORY_FIELD_NUMBER: _ClassVar[int]
    TOTAL_RAM_KB_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    CACHED_PSS_KB_FIELD_NUMBER: _ClassVar[int]
    CACHED_KERNEL_KB_FIELD_NUMBER: _ClassVar[int]
    FREE_KB_FIELD_NUMBER: _ClassVar[int]
    USED_PSS_KB_FIELD_NUMBER: _ClassVar[int]
    USED_KERNEL_KB_FIELD_NUMBER: _ClassVar[int]
    LOST_RAM_KB_FIELD_NUMBER: _ClassVar[int]
    TOTAL_ZRAM_KB_FIELD_NUMBER: _ClassVar[int]
    ZRAM_PHYSICAL_USED_IN_SWAP_KB_FIELD_NUMBER: _ClassVar[int]
    TOTAL_ZRAM_SWAP_KB_FIELD_NUMBER: _ClassVar[int]
    KSM_SHARING_KB_FIELD_NUMBER: _ClassVar[int]
    KSM_SHARED_KB_FIELD_NUMBER: _ClassVar[int]
    KSM_UNSHARED_KB_FIELD_NUMBER: _ClassVar[int]
    KSM_VOLATILE_KB_FIELD_NUMBER: _ClassVar[int]
    TUNING_MB_FIELD_NUMBER: _ClassVar[int]
    TUNING_LARGE_MB_FIELD_NUMBER: _ClassVar[int]
    OOM_KB_FIELD_NUMBER: _ClassVar[int]
    RESTORE_LIMIT_KB_FIELD_NUMBER: _ClassVar[int]
    IS_LOW_RAM_DEVICE_FIELD_NUMBER: _ClassVar[int]
    IS_HIGH_END_GFX_FIELD_NUMBER: _ClassVar[int]
    uptime_duration_ms: int
    elapsed_realtime_ms: int
    native_processes: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.ProcessMemory]
    app_processes: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.AppData]
    total_rss_by_process: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_rss_by_oom_adjustment: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_rss_by_category: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_pss_by_process: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_pss_by_oom_adjustment: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_pss_by_category: _containers.RepeatedCompositeFieldContainer[MemInfoDumpProto.MemItem]
    total_ram_kb: int
    status: _processstats_pb2.ProcessStatsProto.MemoryFactor
    cached_pss_kb: int
    cached_kernel_kb: int
    free_kb: int
    used_pss_kb: int
    used_kernel_kb: int
    lost_ram_kb: int
    total_zram_kb: int
    zram_physical_used_in_swap_kb: int
    total_zram_swap_kb: int
    ksm_sharing_kb: int
    ksm_shared_kb: int
    ksm_unshared_kb: int
    ksm_volatile_kb: int
    tuning_mb: int
    tuning_large_mb: int
    oom_kb: int
    restore_limit_kb: int
    is_low_ram_device: bool
    is_high_end_gfx: bool
    def __init__(self, uptime_duration_ms: _Optional[int] = ..., elapsed_realtime_ms: _Optional[int] = ..., native_processes: _Optional[_Iterable[_Union[MemInfoDumpProto.ProcessMemory, _Mapping]]] = ..., app_processes: _Optional[_Iterable[_Union[MemInfoDumpProto.AppData, _Mapping]]] = ..., total_rss_by_process: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_rss_by_oom_adjustment: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_rss_by_category: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_pss_by_process: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_pss_by_oom_adjustment: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_pss_by_category: _Optional[_Iterable[_Union[MemInfoDumpProto.MemItem, _Mapping]]] = ..., total_ram_kb: _Optional[int] = ..., status: _Optional[_Union[_processstats_pb2.ProcessStatsProto.MemoryFactor, str]] = ..., cached_pss_kb: _Optional[int] = ..., cached_kernel_kb: _Optional[int] = ..., free_kb: _Optional[int] = ..., used_pss_kb: _Optional[int] = ..., used_kernel_kb: _Optional[int] = ..., lost_ram_kb: _Optional[int] = ..., total_zram_kb: _Optional[int] = ..., zram_physical_used_in_swap_kb: _Optional[int] = ..., total_zram_swap_kb: _Optional[int] = ..., ksm_sharing_kb: _Optional[int] = ..., ksm_shared_kb: _Optional[int] = ..., ksm_unshared_kb: _Optional[int] = ..., ksm_volatile_kb: _Optional[int] = ..., tuning_mb: _Optional[int] = ..., tuning_large_mb: _Optional[int] = ..., oom_kb: _Optional[int] = ..., restore_limit_kb: _Optional[int] = ..., is_low_ram_device: _Optional[bool] = ..., is_high_end_gfx: _Optional[bool] = ...) -> None: ...

class StickyBroadcastProto(_message.Message):
    __slots__ = ("user", "actions")
    class StickyAction(_message.Message):
        __slots__ = ("name", "intents")
        NAME_FIELD_NUMBER: _ClassVar[int]
        INTENTS_FIELD_NUMBER: _ClassVar[int]
        name: str
        intents: _containers.RepeatedCompositeFieldContainer[_intent_pb2.IntentProto]
        def __init__(self, name: _Optional[str] = ..., intents: _Optional[_Iterable[_Union[_intent_pb2.IntentProto, _Mapping]]] = ...) -> None: ...
    USER_FIELD_NUMBER: _ClassVar[int]
    ACTIONS_FIELD_NUMBER: _ClassVar[int]
    user: int
    actions: _containers.RepeatedCompositeFieldContainer[StickyBroadcastProto.StickyAction]
    def __init__(self, user: _Optional[int] = ..., actions: _Optional[_Iterable[_Union[StickyBroadcastProto.StickyAction, _Mapping]]] = ...) -> None: ...

class ActivityManagerServiceDumpServicesProto(_message.Message):
    __slots__ = ("active_services",)
    ACTIVE_SERVICES_FIELD_NUMBER: _ClassVar[int]
    active_services: ActiveServicesProto
    def __init__(self, active_services: _Optional[_Union[ActiveServicesProto, _Mapping]] = ...) -> None: ...

class ActiveServicesProto(_message.Message):
    __slots__ = ("services_by_users",)
    class ServicesByUser(_message.Message):
        __slots__ = ("user_id", "service_records")
        USER_ID_FIELD_NUMBER: _ClassVar[int]
        SERVICE_RECORDS_FIELD_NUMBER: _ClassVar[int]
        user_id: int
        service_records: _containers.RepeatedCompositeFieldContainer[ServiceRecordProto]
        def __init__(self, user_id: _Optional[int] = ..., service_records: _Optional[_Iterable[_Union[ServiceRecordProto, _Mapping]]] = ...) -> None: ...
    SERVICES_BY_USERS_FIELD_NUMBER: _ClassVar[int]
    services_by_users: _containers.RepeatedCompositeFieldContainer[ActiveServicesProto.ServicesByUser]
    def __init__(self, services_by_users: _Optional[_Iterable[_Union[ActiveServicesProto.ServicesByUser, _Mapping]]] = ...) -> None: ...

class GrantUriProto(_message.Message):
    __slots__ = ("source_user_id", "uri")
    SOURCE_USER_ID_FIELD_NUMBER: _ClassVar[int]
    URI_FIELD_NUMBER: _ClassVar[int]
    source_user_id: int
    uri: str
    def __init__(self, source_user_id: _Optional[int] = ..., uri: _Optional[str] = ...) -> None: ...

class NeededUriGrantsProto(_message.Message):
    __slots__ = ("target_package", "target_uid", "flags", "grants")
    TARGET_PACKAGE_FIELD_NUMBER: _ClassVar[int]
    TARGET_UID_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    GRANTS_FIELD_NUMBER: _ClassVar[int]
    target_package: str
    target_uid: int
    flags: int
    grants: _containers.RepeatedCompositeFieldContainer[GrantUriProto]
    def __init__(self, target_package: _Optional[str] = ..., target_uid: _Optional[int] = ..., flags: _Optional[int] = ..., grants: _Optional[_Iterable[_Union[GrantUriProto, _Mapping]]] = ...) -> None: ...

class UriPermissionOwnerProto(_message.Message):
    __slots__ = ("owner", "read_perms", "write_perms")
    OWNER_FIELD_NUMBER: _ClassVar[int]
    READ_PERMS_FIELD_NUMBER: _ClassVar[int]
    WRITE_PERMS_FIELD_NUMBER: _ClassVar[int]
    owner: str
    read_perms: _containers.RepeatedCompositeFieldContainer[GrantUriProto]
    write_perms: _containers.RepeatedCompositeFieldContainer[GrantUriProto]
    def __init__(self, owner: _Optional[str] = ..., read_perms: _Optional[_Iterable[_Union[GrantUriProto, _Mapping]]] = ..., write_perms: _Optional[_Iterable[_Union[GrantUriProto, _Mapping]]] = ...) -> None: ...

class ServiceRecordProto(_message.Message):
    __slots__ = ("short_name", "is_running", "pid", "intent", "package_name", "process_name", "permission", "appinfo", "app", "isolated_proc", "whitelist_manager", "delayed", "foreground", "create_real_time", "starting_bg_timeout", "last_activity_time", "restart_time", "created_from_fg", "start", "execute", "destory_time", "crash", "delivered_starts", "pending_starts", "bindings", "connections", "allow_while_in_use_permission_in_fgs", "short_fgs_info")
    class AppInfo(_message.Message):
        __slots__ = ("base_dir", "res_dir", "data_dir", "targetSdkVersion")
        BASE_DIR_FIELD_NUMBER: _ClassVar[int]
        RES_DIR_FIELD_NUMBER: _ClassVar[int]
        DATA_DIR_FIELD_NUMBER: _ClassVar[int]
        TARGETSDKVERSION_FIELD_NUMBER: _ClassVar[int]
        base_dir: str
        res_dir: str
        data_dir: str
        targetSdkVersion: int
        def __init__(self, base_dir: _Optional[str] = ..., res_dir: _Optional[str] = ..., data_dir: _Optional[str] = ..., targetSdkVersion: _Optional[int] = ...) -> None: ...
    class Foreground(_message.Message):
        __slots__ = ("id", "notification", "foregroundServiceType")
        ID_FIELD_NUMBER: _ClassVar[int]
        NOTIFICATION_FIELD_NUMBER: _ClassVar[int]
        FOREGROUNDSERVICETYPE_FIELD_NUMBER: _ClassVar[int]
        id: int
        notification: _notification_pb2.NotificationProto
        foregroundServiceType: int
        def __init__(self, id: _Optional[int] = ..., notification: _Optional[_Union[_notification_pb2.NotificationProto, _Mapping]] = ..., foregroundServiceType: _Optional[int] = ...) -> None: ...
    class Start(_message.Message):
        __slots__ = ("start_requested", "delayed_stop", "stop_if_killed", "call_start", "last_start_id", "start_command_result")
        START_REQUESTED_FIELD_NUMBER: _ClassVar[int]
        DELAYED_STOP_FIELD_NUMBER: _ClassVar[int]
        STOP_IF_KILLED_FIELD_NUMBER: _ClassVar[int]
        CALL_START_FIELD_NUMBER: _ClassVar[int]
        LAST_START_ID_FIELD_NUMBER: _ClassVar[int]
        START_COMMAND_RESULT_FIELD_NUMBER: _ClassVar[int]
        start_requested: bool
        delayed_stop: bool
        stop_if_killed: bool
        call_start: bool
        last_start_id: int
        start_command_result: int
        def __init__(self, start_requested: _Optional[bool] = ..., delayed_stop: _Optional[bool] = ..., stop_if_killed: _Optional[bool] = ..., call_start: _Optional[bool] = ..., last_start_id: _Optional[int] = ..., start_command_result: _Optional[int] = ...) -> None: ...
    class ExecuteNesting(_message.Message):
        __slots__ = ("execute_nesting", "execute_fg", "executing_start")
        EXECUTE_NESTING_FIELD_NUMBER: _ClassVar[int]
        EXECUTE_FG_FIELD_NUMBER: _ClassVar[int]
        EXECUTING_START_FIELD_NUMBER: _ClassVar[int]
        execute_nesting: int
        execute_fg: bool
        executing_start: _common_pb2.Duration
        def __init__(self, execute_nesting: _Optional[int] = ..., execute_fg: _Optional[bool] = ..., executing_start: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ...) -> None: ...
    class Crash(_message.Message):
        __slots__ = ("restart_count", "restart_delay", "next_restart_time", "crash_count")
        RESTART_COUNT_FIELD_NUMBER: _ClassVar[int]
        RESTART_DELAY_FIELD_NUMBER: _ClassVar[int]
        NEXT_RESTART_TIME_FIELD_NUMBER: _ClassVar[int]
        CRASH_COUNT_FIELD_NUMBER: _ClassVar[int]
        restart_count: int
        restart_delay: _common_pb2.Duration
        next_restart_time: _common_pb2.Duration
        crash_count: int
        def __init__(self, restart_count: _Optional[int] = ..., restart_delay: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., next_restart_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., crash_count: _Optional[int] = ...) -> None: ...
    class StartItem(_message.Message):
        __slots__ = ("id", "duration", "delivery_count", "done_executing_count", "intent", "needed_grants", "uri_permissions")
        ID_FIELD_NUMBER: _ClassVar[int]
        DURATION_FIELD_NUMBER: _ClassVar[int]
        DELIVERY_COUNT_FIELD_NUMBER: _ClassVar[int]
        DONE_EXECUTING_COUNT_FIELD_NUMBER: _ClassVar[int]
        INTENT_FIELD_NUMBER: _ClassVar[int]
        NEEDED_GRANTS_FIELD_NUMBER: _ClassVar[int]
        URI_PERMISSIONS_FIELD_NUMBER: _ClassVar[int]
        id: int
        duration: _common_pb2.Duration
        delivery_count: int
        done_executing_count: int
        intent: _intent_pb2.IntentProto
        needed_grants: NeededUriGrantsProto
        uri_permissions: UriPermissionOwnerProto
        def __init__(self, id: _Optional[int] = ..., duration: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., delivery_count: _Optional[int] = ..., done_executing_count: _Optional[int] = ..., intent: _Optional[_Union[_intent_pb2.IntentProto, _Mapping]] = ..., needed_grants: _Optional[_Union[NeededUriGrantsProto, _Mapping]] = ..., uri_permissions: _Optional[_Union[UriPermissionOwnerProto, _Mapping]] = ...) -> None: ...
    class ShortFgsInfo(_message.Message):
        __slots__ = ("start_time", "start_foreground_count", "start_id", "timeout_time", "proc_state_demote_time", "anr_time")
        START_TIME_FIELD_NUMBER: _ClassVar[int]
        START_FOREGROUND_COUNT_FIELD_NUMBER: _ClassVar[int]
        START_ID_FIELD_NUMBER: _ClassVar[int]
        TIMEOUT_TIME_FIELD_NUMBER: _ClassVar[int]
        PROC_STATE_DEMOTE_TIME_FIELD_NUMBER: _ClassVar[int]
        ANR_TIME_FIELD_NUMBER: _ClassVar[int]
        start_time: int
        start_foreground_count: int
        start_id: int
        timeout_time: int
        proc_state_demote_time: int
        anr_time: int
        def __init__(self, start_time: _Optional[int] = ..., start_foreground_count: _Optional[int] = ..., start_id: _Optional[int] = ..., timeout_time: _Optional[int] = ..., proc_state_demote_time: _Optional[int] = ..., anr_time: _Optional[int] = ...) -> None: ...
    SHORT_NAME_FIELD_NUMBER: _ClassVar[int]
    IS_RUNNING_FIELD_NUMBER: _ClassVar[int]
    PID_FIELD_NUMBER: _ClassVar[int]
    INTENT_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
    PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
    PERMISSION_FIELD_NUMBER: _ClassVar[int]
    APPINFO_FIELD_NUMBER: _ClassVar[int]
    APP_FIELD_NUMBER: _ClassVar[int]
    ISOLATED_PROC_FIELD_NUMBER: _ClassVar[int]
    WHITELIST_MANAGER_FIELD_NUMBER: _ClassVar[int]
    DELAYED_FIELD_NUMBER: _ClassVar[int]
    FOREGROUND_FIELD_NUMBER: _ClassVar[int]
    CREATE_REAL_TIME_FIELD_NUMBER: _ClassVar[int]
    STARTING_BG_TIMEOUT_FIELD_NUMBER: _ClassVar[int]
    LAST_ACTIVITY_TIME_FIELD_NUMBER: _ClassVar[int]
    RESTART_TIME_FIELD_NUMBER: _ClassVar[int]
    CREATED_FROM_FG_FIELD_NUMBER: _ClassVar[int]
    START_FIELD_NUMBER: _ClassVar[int]
    EXECUTE_FIELD_NUMBER: _ClassVar[int]
    DESTORY_TIME_FIELD_NUMBER: _ClassVar[int]
    CRASH_FIELD_NUMBER: _ClassVar[int]
    DELIVERED_STARTS_FIELD_NUMBER: _ClassVar[int]
    PENDING_STARTS_FIELD_NUMBER: _ClassVar[int]
    BINDINGS_FIELD_NUMBER: _ClassVar[int]
    CONNECTIONS_FIELD_NUMBER: _ClassVar[int]
    ALLOW_WHILE_IN_USE_PERMISSION_IN_FGS_FIELD_NUMBER: _ClassVar[int]
    SHORT_FGS_INFO_FIELD_NUMBER: _ClassVar[int]
    short_name: str
    is_running: bool
    pid: int
    intent: _intent_pb2.IntentProto
    package_name: str
    process_name: str
    permission: str
    appinfo: ServiceRecordProto.AppInfo
    app: ProcessRecordProto
    isolated_proc: ProcessRecordProto
    whitelist_manager: bool
    delayed: bool
    foreground: ServiceRecordProto.Foreground
    create_real_time: _common_pb2.Duration
    starting_bg_timeout: _common_pb2.Duration
    last_activity_time: _common_pb2.Duration
    restart_time: _common_pb2.Duration
    created_from_fg: bool
    start: ServiceRecordProto.Start
    execute: ServiceRecordProto.ExecuteNesting
    destory_time: _common_pb2.Duration
    crash: ServiceRecordProto.Crash
    delivered_starts: _containers.RepeatedCompositeFieldContainer[ServiceRecordProto.StartItem]
    pending_starts: _containers.RepeatedCompositeFieldContainer[ServiceRecordProto.StartItem]
    bindings: _containers.RepeatedCompositeFieldContainer[IntentBindRecordProto]
    connections: _containers.RepeatedCompositeFieldContainer[ConnectionRecordProto]
    allow_while_in_use_permission_in_fgs: bool
    short_fgs_info: ServiceRecordProto.ShortFgsInfo
    def __init__(self, short_name: _Optional[str] = ..., is_running: _Optional[bool] = ..., pid: _Optional[int] = ..., intent: _Optional[_Union[_intent_pb2.IntentProto, _Mapping]] = ..., package_name: _Optional[str] = ..., process_name: _Optional[str] = ..., permission: _Optional[str] = ..., appinfo: _Optional[_Union[ServiceRecordProto.AppInfo, _Mapping]] = ..., app: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., isolated_proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., whitelist_manager: _Optional[bool] = ..., delayed: _Optional[bool] = ..., foreground: _Optional[_Union[ServiceRecordProto.Foreground, _Mapping]] = ..., create_real_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., starting_bg_timeout: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., last_activity_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., restart_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., created_from_fg: _Optional[bool] = ..., start: _Optional[_Union[ServiceRecordProto.Start, _Mapping]] = ..., execute: _Optional[_Union[ServiceRecordProto.ExecuteNesting, _Mapping]] = ..., destory_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., crash: _Optional[_Union[ServiceRecordProto.Crash, _Mapping]] = ..., delivered_starts: _Optional[_Iterable[_Union[ServiceRecordProto.StartItem, _Mapping]]] = ..., pending_starts: _Optional[_Iterable[_Union[ServiceRecordProto.StartItem, _Mapping]]] = ..., bindings: _Optional[_Iterable[_Union[IntentBindRecordProto, _Mapping]]] = ..., connections: _Optional[_Iterable[_Union[ConnectionRecordProto, _Mapping]]] = ..., allow_while_in_use_permission_in_fgs: _Optional[bool] = ..., short_fgs_info: _Optional[_Union[ServiceRecordProto.ShortFgsInfo, _Mapping]] = ...) -> None: ...

class ConnectionRecordProto(_message.Message):
    __slots__ = ("hex_hash", "user_id", "flags", "service_name")
    class Flag(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        AUTO_CREATE: _ClassVar[ConnectionRecordProto.Flag]
        DEBUG_UNBIND: _ClassVar[ConnectionRecordProto.Flag]
        NOT_FG: _ClassVar[ConnectionRecordProto.Flag]
        IMPORTANT_BG: _ClassVar[ConnectionRecordProto.Flag]
        ABOVE_CLIENT: _ClassVar[ConnectionRecordProto.Flag]
        ALLOW_OOM_MANAGEMENT: _ClassVar[ConnectionRecordProto.Flag]
        WAIVE_PRIORITY: _ClassVar[ConnectionRecordProto.Flag]
        IMPORTANT: _ClassVar[ConnectionRecordProto.Flag]
        ADJUST_WITH_ACTIVITY: _ClassVar[ConnectionRecordProto.Flag]
        FG_SERVICE_WHILE_AWAKE: _ClassVar[ConnectionRecordProto.Flag]
        FG_SERVICE: _ClassVar[ConnectionRecordProto.Flag]
        TREAT_LIKE_ACTIVITY: _ClassVar[ConnectionRecordProto.Flag]
        VISIBLE: _ClassVar[ConnectionRecordProto.Flag]
        SHOWING_UI: _ClassVar[ConnectionRecordProto.Flag]
        NOT_VISIBLE: _ClassVar[ConnectionRecordProto.Flag]
        DEAD: _ClassVar[ConnectionRecordProto.Flag]
        NOT_PERCEPTIBLE: _ClassVar[ConnectionRecordProto.Flag]
        INCLUDE_CAPABILITIES: _ClassVar[ConnectionRecordProto.Flag]
        ALLOW_ACTIVITY_STARTS: _ClassVar[ConnectionRecordProto.Flag]
    AUTO_CREATE: ConnectionRecordProto.Flag
    DEBUG_UNBIND: ConnectionRecordProto.Flag
    NOT_FG: ConnectionRecordProto.Flag
    IMPORTANT_BG: ConnectionRecordProto.Flag
    ABOVE_CLIENT: ConnectionRecordProto.Flag
    ALLOW_OOM_MANAGEMENT: ConnectionRecordProto.Flag
    WAIVE_PRIORITY: ConnectionRecordProto.Flag
    IMPORTANT: ConnectionRecordProto.Flag
    ADJUST_WITH_ACTIVITY: ConnectionRecordProto.Flag
    FG_SERVICE_WHILE_AWAKE: ConnectionRecordProto.Flag
    FG_SERVICE: ConnectionRecordProto.Flag
    TREAT_LIKE_ACTIVITY: ConnectionRecordProto.Flag
    VISIBLE: ConnectionRecordProto.Flag
    SHOWING_UI: ConnectionRecordProto.Flag
    NOT_VISIBLE: ConnectionRecordProto.Flag
    DEAD: ConnectionRecordProto.Flag
    NOT_PERCEPTIBLE: ConnectionRecordProto.Flag
    INCLUDE_CAPABILITIES: ConnectionRecordProto.Flag
    ALLOW_ACTIVITY_STARTS: ConnectionRecordProto.Flag
    HEX_HASH_FIELD_NUMBER: _ClassVar[int]
    USER_ID_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    SERVICE_NAME_FIELD_NUMBER: _ClassVar[int]
    hex_hash: str
    user_id: int
    flags: _containers.RepeatedScalarFieldContainer[ConnectionRecordProto.Flag]
    service_name: str
    def __init__(self, hex_hash: _Optional[str] = ..., user_id: _Optional[int] = ..., flags: _Optional[_Iterable[_Union[ConnectionRecordProto.Flag, str]]] = ..., service_name: _Optional[str] = ...) -> None: ...

class AppBindRecordProto(_message.Message):
    __slots__ = ("service_name", "client_proc_name", "connections")
    SERVICE_NAME_FIELD_NUMBER: _ClassVar[int]
    CLIENT_PROC_NAME_FIELD_NUMBER: _ClassVar[int]
    CONNECTIONS_FIELD_NUMBER: _ClassVar[int]
    service_name: str
    client_proc_name: str
    connections: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, service_name: _Optional[str] = ..., client_proc_name: _Optional[str] = ..., connections: _Optional[_Iterable[str]] = ...) -> None: ...

class IntentBindRecordProto(_message.Message):
    __slots__ = ("intent", "binder", "auto_create", "requested", "received", "has_bound", "do_rebind", "apps")
    INTENT_FIELD_NUMBER: _ClassVar[int]
    BINDER_FIELD_NUMBER: _ClassVar[int]
    AUTO_CREATE_FIELD_NUMBER: _ClassVar[int]
    REQUESTED_FIELD_NUMBER: _ClassVar[int]
    RECEIVED_FIELD_NUMBER: _ClassVar[int]
    HAS_BOUND_FIELD_NUMBER: _ClassVar[int]
    DO_REBIND_FIELD_NUMBER: _ClassVar[int]
    APPS_FIELD_NUMBER: _ClassVar[int]
    intent: _intent_pb2.IntentProto
    binder: str
    auto_create: bool
    requested: bool
    received: bool
    has_bound: bool
    do_rebind: bool
    apps: _containers.RepeatedCompositeFieldContainer[AppBindRecordProto]
    def __init__(self, intent: _Optional[_Union[_intent_pb2.IntentProto, _Mapping]] = ..., binder: _Optional[str] = ..., auto_create: _Optional[bool] = ..., requested: _Optional[bool] = ..., received: _Optional[bool] = ..., has_bound: _Optional[bool] = ..., do_rebind: _Optional[bool] = ..., apps: _Optional[_Iterable[_Union[AppBindRecordProto, _Mapping]]] = ...) -> None: ...

class ActivityManagerServiceDumpProcessesProto(_message.Message):
    __slots__ = ("procs", "isolated_procs", "active_instrumentations", "active_uids", "validate_uids", "lru_procs", "pids_self_locked", "important_procs", "persistent_starting_procs", "removed_procs", "on_hold_procs", "gc_procs", "app_errors", "user_controller", "home_proc", "previous_proc", "previous_proc_visible_time_ms", "heavy_weight_proc", "global_configuration", "config_will_change", "screen_compat_packages", "uid_observers", "device_idle_whitelist", "device_idle_temp_whitelist", "pending_temp_whitelist", "sleep_status", "running_voice", "vr_controller", "debug", "current_tracker", "mem_watch_processes", "track_allocation_app", "profile", "native_debugging_app", "always_finish_activities", "controller", "total_persistent_procs", "processes_ready", "system_ready", "booted", "factory_test", "booting", "call_finish_booting", "boot_animation_complete", "last_power_check_uptime_ms", "going_to_sleep", "launching_activity", "adj_seq", "lru_seq", "num_non_cached_procs", "num_cached_hidden_procs", "num_service_procs", "new_num_service_procs", "allow_lower_mem_level", "last_memory_level", "last_num_processes", "last_idle_time", "low_ram_since_last_idle_ms")
    class LruProcesses(_message.Message):
        __slots__ = ("size", "non_act_at", "non_svc_at", "list")
        SIZE_FIELD_NUMBER: _ClassVar[int]
        NON_ACT_AT_FIELD_NUMBER: _ClassVar[int]
        NON_SVC_AT_FIELD_NUMBER: _ClassVar[int]
        LIST_FIELD_NUMBER: _ClassVar[int]
        size: int
        non_act_at: int
        non_svc_at: int
        list: _containers.RepeatedCompositeFieldContainer[ProcessOomProto]
        def __init__(self, size: _Optional[int] = ..., non_act_at: _Optional[int] = ..., non_svc_at: _Optional[int] = ..., list: _Optional[_Iterable[_Union[ProcessOomProto, _Mapping]]] = ...) -> None: ...
    class ScreenCompatPackage(_message.Message):
        __slots__ = ("package", "mode")
        PACKAGE_FIELD_NUMBER: _ClassVar[int]
        MODE_FIELD_NUMBER: _ClassVar[int]
        package: str
        mode: int
        def __init__(self, package: _Optional[str] = ..., mode: _Optional[int] = ...) -> None: ...
    class UidObserverRegistrationProto(_message.Message):
        __slots__ = ("uid", "package", "flags", "cut_point", "last_proc_states")
        class ProcState(_message.Message):
            __slots__ = ("uid", "state")
            UID_FIELD_NUMBER: _ClassVar[int]
            STATE_FIELD_NUMBER: _ClassVar[int]
            uid: int
            state: int
            def __init__(self, uid: _Optional[int] = ..., state: _Optional[int] = ...) -> None: ...
        UID_FIELD_NUMBER: _ClassVar[int]
        PACKAGE_FIELD_NUMBER: _ClassVar[int]
        FLAGS_FIELD_NUMBER: _ClassVar[int]
        CUT_POINT_FIELD_NUMBER: _ClassVar[int]
        LAST_PROC_STATES_FIELD_NUMBER: _ClassVar[int]
        uid: int
        package: str
        flags: _containers.RepeatedScalarFieldContainer[_activitymanager_pb2.UidObserverFlag]
        cut_point: int
        last_proc_states: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.UidObserverRegistrationProto.ProcState]
        def __init__(self, uid: _Optional[int] = ..., package: _Optional[str] = ..., flags: _Optional[_Iterable[_Union[_activitymanager_pb2.UidObserverFlag, str]]] = ..., cut_point: _Optional[int] = ..., last_proc_states: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.UidObserverRegistrationProto.ProcState, _Mapping]]] = ...) -> None: ...
    class PendingTempWhitelist(_message.Message):
        __slots__ = ("target_uid", "duration_ms", "tag", "type", "reason_code", "calling_uid")
        TARGET_UID_FIELD_NUMBER: _ClassVar[int]
        DURATION_MS_FIELD_NUMBER: _ClassVar[int]
        TAG_FIELD_NUMBER: _ClassVar[int]
        TYPE_FIELD_NUMBER: _ClassVar[int]
        REASON_CODE_FIELD_NUMBER: _ClassVar[int]
        CALLING_UID_FIELD_NUMBER: _ClassVar[int]
        target_uid: int
        duration_ms: int
        tag: str
        type: int
        reason_code: int
        calling_uid: int
        def __init__(self, target_uid: _Optional[int] = ..., duration_ms: _Optional[int] = ..., tag: _Optional[str] = ..., type: _Optional[int] = ..., reason_code: _Optional[int] = ..., calling_uid: _Optional[int] = ...) -> None: ...
    class SleepStatus(_message.Message):
        __slots__ = ("wakefulness", "sleep_tokens", "sleeping", "shutting_down", "test_pss_mode")
        WAKEFULNESS_FIELD_NUMBER: _ClassVar[int]
        SLEEP_TOKENS_FIELD_NUMBER: _ClassVar[int]
        SLEEPING_FIELD_NUMBER: _ClassVar[int]
        SHUTTING_DOWN_FIELD_NUMBER: _ClassVar[int]
        TEST_PSS_MODE_FIELD_NUMBER: _ClassVar[int]
        wakefulness: _powermanager_pb2.PowerManagerInternalProto.Wakefulness
        sleep_tokens: _containers.RepeatedScalarFieldContainer[str]
        sleeping: bool
        shutting_down: bool
        test_pss_mode: bool
        def __init__(self, wakefulness: _Optional[_Union[_powermanager_pb2.PowerManagerInternalProto.Wakefulness, str]] = ..., sleep_tokens: _Optional[_Iterable[str]] = ..., sleeping: _Optional[bool] = ..., shutting_down: _Optional[bool] = ..., test_pss_mode: _Optional[bool] = ...) -> None: ...
    class Voice(_message.Message):
        __slots__ = ("session", "wakelock")
        SESSION_FIELD_NUMBER: _ClassVar[int]
        WAKELOCK_FIELD_NUMBER: _ClassVar[int]
        session: str
        wakelock: _powermanager_pb2.PowerManagerProto.WakeLock
        def __init__(self, session: _Optional[str] = ..., wakelock: _Optional[_Union[_powermanager_pb2.PowerManagerProto.WakeLock, _Mapping]] = ...) -> None: ...
    class DebugApp(_message.Message):
        __slots__ = ("debug_app", "orig_debug_app", "debug_transient", "orig_wait_for_debugger")
        DEBUG_APP_FIELD_NUMBER: _ClassVar[int]
        ORIG_DEBUG_APP_FIELD_NUMBER: _ClassVar[int]
        DEBUG_TRANSIENT_FIELD_NUMBER: _ClassVar[int]
        ORIG_WAIT_FOR_DEBUGGER_FIELD_NUMBER: _ClassVar[int]
        debug_app: str
        orig_debug_app: str
        debug_transient: bool
        orig_wait_for_debugger: bool
        def __init__(self, debug_app: _Optional[str] = ..., orig_debug_app: _Optional[str] = ..., debug_transient: _Optional[bool] = ..., orig_wait_for_debugger: _Optional[bool] = ...) -> None: ...
    class MemWatchProcess(_message.Message):
        __slots__ = ("procs", "dump")
        class Process(_message.Message):
            __slots__ = ("name", "mem_stats")
            class MemStats(_message.Message):
                __slots__ = ("uid", "size", "report_to")
                UID_FIELD_NUMBER: _ClassVar[int]
                SIZE_FIELD_NUMBER: _ClassVar[int]
                REPORT_TO_FIELD_NUMBER: _ClassVar[int]
                uid: int
                size: str
                report_to: str
                def __init__(self, uid: _Optional[int] = ..., size: _Optional[str] = ..., report_to: _Optional[str] = ...) -> None: ...
            NAME_FIELD_NUMBER: _ClassVar[int]
            MEM_STATS_FIELD_NUMBER: _ClassVar[int]
            name: str
            mem_stats: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Process.MemStats]
            def __init__(self, name: _Optional[str] = ..., mem_stats: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Process.MemStats, _Mapping]]] = ...) -> None: ...
        class Dump(_message.Message):
            __slots__ = ("proc_name", "pid", "uid", "is_user_initiated", "uri")
            PROC_NAME_FIELD_NUMBER: _ClassVar[int]
            PID_FIELD_NUMBER: _ClassVar[int]
            UID_FIELD_NUMBER: _ClassVar[int]
            IS_USER_INITIATED_FIELD_NUMBER: _ClassVar[int]
            URI_FIELD_NUMBER: _ClassVar[int]
            proc_name: str
            pid: int
            uid: int
            is_user_initiated: bool
            uri: str
            def __init__(self, proc_name: _Optional[str] = ..., pid: _Optional[int] = ..., uid: _Optional[int] = ..., is_user_initiated: _Optional[bool] = ..., uri: _Optional[str] = ...) -> None: ...
        PROCS_FIELD_NUMBER: _ClassVar[int]
        DUMP_FIELD_NUMBER: _ClassVar[int]
        procs: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Process]
        dump: ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Dump
        def __init__(self, procs: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Process, _Mapping]]] = ..., dump: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.MemWatchProcess.Dump, _Mapping]] = ...) -> None: ...
    class Profile(_message.Message):
        __slots__ = ("app_name", "proc", "info", "type")
        APP_NAME_FIELD_NUMBER: _ClassVar[int]
        PROC_FIELD_NUMBER: _ClassVar[int]
        INFO_FIELD_NUMBER: _ClassVar[int]
        TYPE_FIELD_NUMBER: _ClassVar[int]
        app_name: str
        proc: ProcessRecordProto
        info: _profilerinfo_pb2.ProfilerInfoProto
        type: int
        def __init__(self, app_name: _Optional[str] = ..., proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., info: _Optional[_Union[_profilerinfo_pb2.ProfilerInfoProto, _Mapping]] = ..., type: _Optional[int] = ...) -> None: ...
    class Controller(_message.Message):
        __slots__ = ("controller", "is_a_monkey")
        CONTROLLER_FIELD_NUMBER: _ClassVar[int]
        IS_A_MONKEY_FIELD_NUMBER: _ClassVar[int]
        controller: str
        is_a_monkey: bool
        def __init__(self, controller: _Optional[str] = ..., is_a_monkey: _Optional[bool] = ...) -> None: ...
    PROCS_FIELD_NUMBER: _ClassVar[int]
    ISOLATED_PROCS_FIELD_NUMBER: _ClassVar[int]
    ACTIVE_INSTRUMENTATIONS_FIELD_NUMBER: _ClassVar[int]
    ACTIVE_UIDS_FIELD_NUMBER: _ClassVar[int]
    VALIDATE_UIDS_FIELD_NUMBER: _ClassVar[int]
    LRU_PROCS_FIELD_NUMBER: _ClassVar[int]
    PIDS_SELF_LOCKED_FIELD_NUMBER: _ClassVar[int]
    IMPORTANT_PROCS_FIELD_NUMBER: _ClassVar[int]
    PERSISTENT_STARTING_PROCS_FIELD_NUMBER: _ClassVar[int]
    REMOVED_PROCS_FIELD_NUMBER: _ClassVar[int]
    ON_HOLD_PROCS_FIELD_NUMBER: _ClassVar[int]
    GC_PROCS_FIELD_NUMBER: _ClassVar[int]
    APP_ERRORS_FIELD_NUMBER: _ClassVar[int]
    USER_CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    HOME_PROC_FIELD_NUMBER: _ClassVar[int]
    PREVIOUS_PROC_FIELD_NUMBER: _ClassVar[int]
    PREVIOUS_PROC_VISIBLE_TIME_MS_FIELD_NUMBER: _ClassVar[int]
    HEAVY_WEIGHT_PROC_FIELD_NUMBER: _ClassVar[int]
    GLOBAL_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    CONFIG_WILL_CHANGE_FIELD_NUMBER: _ClassVar[int]
    SCREEN_COMPAT_PACKAGES_FIELD_NUMBER: _ClassVar[int]
    UID_OBSERVERS_FIELD_NUMBER: _ClassVar[int]
    DEVICE_IDLE_WHITELIST_FIELD_NUMBER: _ClassVar[int]
    DEVICE_IDLE_TEMP_WHITELIST_FIELD_NUMBER: _ClassVar[int]
    PENDING_TEMP_WHITELIST_FIELD_NUMBER: _ClassVar[int]
    SLEEP_STATUS_FIELD_NUMBER: _ClassVar[int]
    RUNNING_VOICE_FIELD_NUMBER: _ClassVar[int]
    VR_CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    DEBUG_FIELD_NUMBER: _ClassVar[int]
    CURRENT_TRACKER_FIELD_NUMBER: _ClassVar[int]
    MEM_WATCH_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    TRACK_ALLOCATION_APP_FIELD_NUMBER: _ClassVar[int]
    PROFILE_FIELD_NUMBER: _ClassVar[int]
    NATIVE_DEBUGGING_APP_FIELD_NUMBER: _ClassVar[int]
    ALWAYS_FINISH_ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
    CONTROLLER_FIELD_NUMBER: _ClassVar[int]
    TOTAL_PERSISTENT_PROCS_FIELD_NUMBER: _ClassVar[int]
    PROCESSES_READY_FIELD_NUMBER: _ClassVar[int]
    SYSTEM_READY_FIELD_NUMBER: _ClassVar[int]
    BOOTED_FIELD_NUMBER: _ClassVar[int]
    FACTORY_TEST_FIELD_NUMBER: _ClassVar[int]
    BOOTING_FIELD_NUMBER: _ClassVar[int]
    CALL_FINISH_BOOTING_FIELD_NUMBER: _ClassVar[int]
    BOOT_ANIMATION_COMPLETE_FIELD_NUMBER: _ClassVar[int]
    LAST_POWER_CHECK_UPTIME_MS_FIELD_NUMBER: _ClassVar[int]
    GOING_TO_SLEEP_FIELD_NUMBER: _ClassVar[int]
    LAUNCHING_ACTIVITY_FIELD_NUMBER: _ClassVar[int]
    ADJ_SEQ_FIELD_NUMBER: _ClassVar[int]
    LRU_SEQ_FIELD_NUMBER: _ClassVar[int]
    NUM_NON_CACHED_PROCS_FIELD_NUMBER: _ClassVar[int]
    NUM_CACHED_HIDDEN_PROCS_FIELD_NUMBER: _ClassVar[int]
    NUM_SERVICE_PROCS_FIELD_NUMBER: _ClassVar[int]
    NEW_NUM_SERVICE_PROCS_FIELD_NUMBER: _ClassVar[int]
    ALLOW_LOWER_MEM_LEVEL_FIELD_NUMBER: _ClassVar[int]
    LAST_MEMORY_LEVEL_FIELD_NUMBER: _ClassVar[int]
    LAST_NUM_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    LAST_IDLE_TIME_FIELD_NUMBER: _ClassVar[int]
    LOW_RAM_SINCE_LAST_IDLE_MS_FIELD_NUMBER: _ClassVar[int]
    procs: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    isolated_procs: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    active_instrumentations: _containers.RepeatedCompositeFieldContainer[ActiveInstrumentationProto]
    active_uids: _containers.RepeatedCompositeFieldContainer[UidRecordProto]
    validate_uids: _containers.RepeatedCompositeFieldContainer[UidRecordProto]
    lru_procs: ActivityManagerServiceDumpProcessesProto.LruProcesses
    pids_self_locked: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    important_procs: _containers.RepeatedCompositeFieldContainer[ImportanceTokenProto]
    persistent_starting_procs: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    removed_procs: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    on_hold_procs: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    gc_procs: _containers.RepeatedCompositeFieldContainer[ProcessToGcProto]
    app_errors: AppErrorsProto
    user_controller: UserControllerProto
    home_proc: ProcessRecordProto
    previous_proc: ProcessRecordProto
    previous_proc_visible_time_ms: int
    heavy_weight_proc: ProcessRecordProto
    global_configuration: _configuration_pb2.ConfigurationProto
    config_will_change: bool
    screen_compat_packages: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.ScreenCompatPackage]
    uid_observers: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.UidObserverRegistrationProto]
    device_idle_whitelist: _containers.RepeatedScalarFieldContainer[int]
    device_idle_temp_whitelist: _containers.RepeatedScalarFieldContainer[int]
    pending_temp_whitelist: _containers.RepeatedCompositeFieldContainer[ActivityManagerServiceDumpProcessesProto.PendingTempWhitelist]
    sleep_status: ActivityManagerServiceDumpProcessesProto.SleepStatus
    running_voice: ActivityManagerServiceDumpProcessesProto.Voice
    vr_controller: VrControllerProto
    debug: ActivityManagerServiceDumpProcessesProto.DebugApp
    current_tracker: AppTimeTrackerProto
    mem_watch_processes: ActivityManagerServiceDumpProcessesProto.MemWatchProcess
    track_allocation_app: str
    profile: ActivityManagerServiceDumpProcessesProto.Profile
    native_debugging_app: str
    always_finish_activities: bool
    controller: ActivityManagerServiceDumpProcessesProto.Controller
    total_persistent_procs: int
    processes_ready: bool
    system_ready: bool
    booted: bool
    factory_test: int
    booting: bool
    call_finish_booting: bool
    boot_animation_complete: bool
    last_power_check_uptime_ms: int
    going_to_sleep: _powermanager_pb2.PowerManagerProto.WakeLock
    launching_activity: _powermanager_pb2.PowerManagerProto.WakeLock
    adj_seq: int
    lru_seq: int
    num_non_cached_procs: int
    num_cached_hidden_procs: int
    num_service_procs: int
    new_num_service_procs: int
    allow_lower_mem_level: bool
    last_memory_level: int
    last_num_processes: int
    last_idle_time: _common_pb2.Duration
    low_ram_since_last_idle_ms: int
    def __init__(self, procs: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., isolated_procs: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., active_instrumentations: _Optional[_Iterable[_Union[ActiveInstrumentationProto, _Mapping]]] = ..., active_uids: _Optional[_Iterable[_Union[UidRecordProto, _Mapping]]] = ..., validate_uids: _Optional[_Iterable[_Union[UidRecordProto, _Mapping]]] = ..., lru_procs: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.LruProcesses, _Mapping]] = ..., pids_self_locked: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., important_procs: _Optional[_Iterable[_Union[ImportanceTokenProto, _Mapping]]] = ..., persistent_starting_procs: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., removed_procs: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., on_hold_procs: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., gc_procs: _Optional[_Iterable[_Union[ProcessToGcProto, _Mapping]]] = ..., app_errors: _Optional[_Union[AppErrorsProto, _Mapping]] = ..., user_controller: _Optional[_Union[UserControllerProto, _Mapping]] = ..., home_proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., previous_proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., previous_proc_visible_time_ms: _Optional[int] = ..., heavy_weight_proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., global_configuration: _Optional[_Union[_configuration_pb2.ConfigurationProto, _Mapping]] = ..., config_will_change: _Optional[bool] = ..., screen_compat_packages: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.ScreenCompatPackage, _Mapping]]] = ..., uid_observers: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.UidObserverRegistrationProto, _Mapping]]] = ..., device_idle_whitelist: _Optional[_Iterable[int]] = ..., device_idle_temp_whitelist: _Optional[_Iterable[int]] = ..., pending_temp_whitelist: _Optional[_Iterable[_Union[ActivityManagerServiceDumpProcessesProto.PendingTempWhitelist, _Mapping]]] = ..., sleep_status: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.SleepStatus, _Mapping]] = ..., running_voice: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.Voice, _Mapping]] = ..., vr_controller: _Optional[_Union[VrControllerProto, _Mapping]] = ..., debug: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.DebugApp, _Mapping]] = ..., current_tracker: _Optional[_Union[AppTimeTrackerProto, _Mapping]] = ..., mem_watch_processes: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.MemWatchProcess, _Mapping]] = ..., track_allocation_app: _Optional[str] = ..., profile: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.Profile, _Mapping]] = ..., native_debugging_app: _Optional[str] = ..., always_finish_activities: _Optional[bool] = ..., controller: _Optional[_Union[ActivityManagerServiceDumpProcessesProto.Controller, _Mapping]] = ..., total_persistent_procs: _Optional[int] = ..., processes_ready: _Optional[bool] = ..., system_ready: _Optional[bool] = ..., booted: _Optional[bool] = ..., factory_test: _Optional[int] = ..., booting: _Optional[bool] = ..., call_finish_booting: _Optional[bool] = ..., boot_animation_complete: _Optional[bool] = ..., last_power_check_uptime_ms: _Optional[int] = ..., going_to_sleep: _Optional[_Union[_powermanager_pb2.PowerManagerProto.WakeLock, _Mapping]] = ..., launching_activity: _Optional[_Union[_powermanager_pb2.PowerManagerProto.WakeLock, _Mapping]] = ..., adj_seq: _Optional[int] = ..., lru_seq: _Optional[int] = ..., num_non_cached_procs: _Optional[int] = ..., num_cached_hidden_procs: _Optional[int] = ..., num_service_procs: _Optional[int] = ..., new_num_service_procs: _Optional[int] = ..., allow_lower_mem_level: _Optional[bool] = ..., last_memory_level: _Optional[int] = ..., last_num_processes: _Optional[int] = ..., last_idle_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., low_ram_since_last_idle_ms: _Optional[int] = ...) -> None: ...

class ActiveInstrumentationProto(_message.Message):
    __slots__ = ("finished", "running_processes", "target_processes", "target_info", "profile_file", "watcher", "ui_automation_connection", "arguments")
    CLASS_FIELD_NUMBER: _ClassVar[int]
    FINISHED_FIELD_NUMBER: _ClassVar[int]
    RUNNING_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    TARGET_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    TARGET_INFO_FIELD_NUMBER: _ClassVar[int]
    PROFILE_FILE_FIELD_NUMBER: _ClassVar[int]
    WATCHER_FIELD_NUMBER: _ClassVar[int]
    UI_AUTOMATION_CONNECTION_FIELD_NUMBER: _ClassVar[int]
    ARGUMENTS_FIELD_NUMBER: _ClassVar[int]
    finished: bool
    running_processes: _containers.RepeatedCompositeFieldContainer[ProcessRecordProto]
    target_processes: _containers.RepeatedScalarFieldContainer[str]
    target_info: _package_item_info_pb2.ApplicationInfoProto
    profile_file: str
    watcher: str
    ui_automation_connection: str
    arguments: _bundle_pb2.BundleProto
    def __init__(self, finished: _Optional[bool] = ..., running_processes: _Optional[_Iterable[_Union[ProcessRecordProto, _Mapping]]] = ..., target_processes: _Optional[_Iterable[str]] = ..., target_info: _Optional[_Union[_package_item_info_pb2.ApplicationInfoProto, _Mapping]] = ..., profile_file: _Optional[str] = ..., watcher: _Optional[str] = ..., ui_automation_connection: _Optional[str] = ..., arguments: _Optional[_Union[_bundle_pb2.BundleProto, _Mapping]] = ..., **kwargs) -> None: ...

class UidRecordProto(_message.Message):
    __slots__ = ("uid", "current", "ephemeral", "fg_services", "whilelist", "last_background_time", "idle", "last_reported_changes", "num_procs", "network_state_update")
    class Change(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        CHANGE_GONE: _ClassVar[UidRecordProto.Change]
        CHANGE_IDLE: _ClassVar[UidRecordProto.Change]
        CHANGE_ACTIVE: _ClassVar[UidRecordProto.Change]
        CHANGE_CACHED: _ClassVar[UidRecordProto.Change]
        CHANGE_UNCACHED: _ClassVar[UidRecordProto.Change]
        CHANGE_CAPABILITY: _ClassVar[UidRecordProto.Change]
        CHANGE_PROCSTATE: _ClassVar[UidRecordProto.Change]
        CHANGE_PROCADJ: _ClassVar[UidRecordProto.Change]
    CHANGE_GONE: UidRecordProto.Change
    CHANGE_IDLE: UidRecordProto.Change
    CHANGE_ACTIVE: UidRecordProto.Change
    CHANGE_CACHED: UidRecordProto.Change
    CHANGE_UNCACHED: UidRecordProto.Change
    CHANGE_CAPABILITY: UidRecordProto.Change
    CHANGE_PROCSTATE: UidRecordProto.Change
    CHANGE_PROCADJ: UidRecordProto.Change
    class ProcStateSequence(_message.Message):
        __slots__ = ("cururent", "last_network_updated", "last_dispatched")
        CURURENT_FIELD_NUMBER: _ClassVar[int]
        LAST_NETWORK_UPDATED_FIELD_NUMBER: _ClassVar[int]
        LAST_DISPATCHED_FIELD_NUMBER: _ClassVar[int]
        cururent: int
        last_network_updated: int
        last_dispatched: int
        def __init__(self, cururent: _Optional[int] = ..., last_network_updated: _Optional[int] = ..., last_dispatched: _Optional[int] = ...) -> None: ...
    UID_FIELD_NUMBER: _ClassVar[int]
    CURRENT_FIELD_NUMBER: _ClassVar[int]
    EPHEMERAL_FIELD_NUMBER: _ClassVar[int]
    FG_SERVICES_FIELD_NUMBER: _ClassVar[int]
    WHILELIST_FIELD_NUMBER: _ClassVar[int]
    LAST_BACKGROUND_TIME_FIELD_NUMBER: _ClassVar[int]
    IDLE_FIELD_NUMBER: _ClassVar[int]
    LAST_REPORTED_CHANGES_FIELD_NUMBER: _ClassVar[int]
    NUM_PROCS_FIELD_NUMBER: _ClassVar[int]
    NETWORK_STATE_UPDATE_FIELD_NUMBER: _ClassVar[int]
    uid: int
    current: _app_enums_pb2.ProcessStateEnum
    ephemeral: bool
    fg_services: bool
    whilelist: bool
    last_background_time: _common_pb2.Duration
    idle: bool
    last_reported_changes: _containers.RepeatedScalarFieldContainer[UidRecordProto.Change]
    num_procs: int
    network_state_update: UidRecordProto.ProcStateSequence
    def __init__(self, uid: _Optional[int] = ..., current: _Optional[_Union[_app_enums_pb2.ProcessStateEnum, str]] = ..., ephemeral: _Optional[bool] = ..., fg_services: _Optional[bool] = ..., whilelist: _Optional[bool] = ..., last_background_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., idle: _Optional[bool] = ..., last_reported_changes: _Optional[_Iterable[_Union[UidRecordProto.Change, str]]] = ..., num_procs: _Optional[int] = ..., network_state_update: _Optional[_Union[UidRecordProto.ProcStateSequence, _Mapping]] = ...) -> None: ...

class ImportanceTokenProto(_message.Message):
    __slots__ = ("pid", "token", "reason")
    PID_FIELD_NUMBER: _ClassVar[int]
    TOKEN_FIELD_NUMBER: _ClassVar[int]
    REASON_FIELD_NUMBER: _ClassVar[int]
    pid: int
    token: str
    reason: str
    def __init__(self, pid: _Optional[int] = ..., token: _Optional[str] = ..., reason: _Optional[str] = ...) -> None: ...

class VrControllerProto(_message.Message):
    __slots__ = ("vr_mode", "render_thread_id")
    class VrMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        FLAG_NON_VR_MODE: _ClassVar[VrControllerProto.VrMode]
        FLAG_VR_MODE: _ClassVar[VrControllerProto.VrMode]
        FLAG_PERSISTENT_VR_MODE: _ClassVar[VrControllerProto.VrMode]
    FLAG_NON_VR_MODE: VrControllerProto.VrMode
    FLAG_VR_MODE: VrControllerProto.VrMode
    FLAG_PERSISTENT_VR_MODE: VrControllerProto.VrMode
    VR_MODE_FIELD_NUMBER: _ClassVar[int]
    RENDER_THREAD_ID_FIELD_NUMBER: _ClassVar[int]
    vr_mode: _containers.RepeatedScalarFieldContainer[VrControllerProto.VrMode]
    render_thread_id: int
    def __init__(self, vr_mode: _Optional[_Iterable[_Union[VrControllerProto.VrMode, str]]] = ..., render_thread_id: _Optional[int] = ...) -> None: ...

class ProcessOomProto(_message.Message):
    __slots__ = ("persistent", "num", "oom_adj", "sched_group", "activities", "services", "state", "trim_memory_level", "proc", "adj_type", "adj_target_component_name", "adj_target_object", "adj_source_proc", "adj_source_object", "detail")
    class SchedGroup(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        SCHED_GROUP_UNKNOWN: _ClassVar[ProcessOomProto.SchedGroup]
        SCHED_GROUP_BACKGROUND: _ClassVar[ProcessOomProto.SchedGroup]
        SCHED_GROUP_DEFAULT: _ClassVar[ProcessOomProto.SchedGroup]
        SCHED_GROUP_TOP_APP: _ClassVar[ProcessOomProto.SchedGroup]
        SCHED_GROUP_TOP_APP_BOUND: _ClassVar[ProcessOomProto.SchedGroup]
    SCHED_GROUP_UNKNOWN: ProcessOomProto.SchedGroup
    SCHED_GROUP_BACKGROUND: ProcessOomProto.SchedGroup
    SCHED_GROUP_DEFAULT: ProcessOomProto.SchedGroup
    SCHED_GROUP_TOP_APP: ProcessOomProto.SchedGroup
    SCHED_GROUP_TOP_APP_BOUND: ProcessOomProto.SchedGroup
    class Detail(_message.Message):
        __slots__ = ("max_adj", "cur_raw_adj", "set_raw_adj", "cur_adj", "set_adj", "current_state", "set_state", "last_pss", "last_swap_pss", "last_cached_pss", "cached", "empty", "has_above_client", "service_run_time")
        class CpuRunTime(_message.Message):
            __slots__ = ("over_ms", "used_ms", "ultilization")
            OVER_MS_FIELD_NUMBER: _ClassVar[int]
            USED_MS_FIELD_NUMBER: _ClassVar[int]
            ULTILIZATION_FIELD_NUMBER: _ClassVar[int]
            over_ms: int
            used_ms: int
            ultilization: float
            def __init__(self, over_ms: _Optional[int] = ..., used_ms: _Optional[int] = ..., ultilization: _Optional[float] = ...) -> None: ...
        MAX_ADJ_FIELD_NUMBER: _ClassVar[int]
        CUR_RAW_ADJ_FIELD_NUMBER: _ClassVar[int]
        SET_RAW_ADJ_FIELD_NUMBER: _ClassVar[int]
        CUR_ADJ_FIELD_NUMBER: _ClassVar[int]
        SET_ADJ_FIELD_NUMBER: _ClassVar[int]
        CURRENT_STATE_FIELD_NUMBER: _ClassVar[int]
        SET_STATE_FIELD_NUMBER: _ClassVar[int]
        LAST_PSS_FIELD_NUMBER: _ClassVar[int]
        LAST_SWAP_PSS_FIELD_NUMBER: _ClassVar[int]
        LAST_CACHED_PSS_FIELD_NUMBER: _ClassVar[int]
        CACHED_FIELD_NUMBER: _ClassVar[int]
        EMPTY_FIELD_NUMBER: _ClassVar[int]
        HAS_ABOVE_CLIENT_FIELD_NUMBER: _ClassVar[int]
        SERVICE_RUN_TIME_FIELD_NUMBER: _ClassVar[int]
        max_adj: int
        cur_raw_adj: int
        set_raw_adj: int
        cur_adj: int
        set_adj: int
        current_state: _app_enums_pb2.ProcessStateEnum
        set_state: _app_enums_pb2.ProcessStateEnum
        last_pss: str
        last_swap_pss: str
        last_cached_pss: str
        cached: bool
        empty: bool
        has_above_client: bool
        service_run_time: ProcessOomProto.Detail.CpuRunTime
        def __init__(self, max_adj: _Optional[int] = ..., cur_raw_adj: _Optional[int] = ..., set_raw_adj: _Optional[int] = ..., cur_adj: _Optional[int] = ..., set_adj: _Optional[int] = ..., current_state: _Optional[_Union[_app_enums_pb2.ProcessStateEnum, str]] = ..., set_state: _Optional[_Union[_app_enums_pb2.ProcessStateEnum, str]] = ..., last_pss: _Optional[str] = ..., last_swap_pss: _Optional[str] = ..., last_cached_pss: _Optional[str] = ..., cached: _Optional[bool] = ..., empty: _Optional[bool] = ..., has_above_client: _Optional[bool] = ..., service_run_time: _Optional[_Union[ProcessOomProto.Detail.CpuRunTime, _Mapping]] = ...) -> None: ...
    PERSISTENT_FIELD_NUMBER: _ClassVar[int]
    NUM_FIELD_NUMBER: _ClassVar[int]
    OOM_ADJ_FIELD_NUMBER: _ClassVar[int]
    SCHED_GROUP_FIELD_NUMBER: _ClassVar[int]
    ACTIVITIES_FIELD_NUMBER: _ClassVar[int]
    SERVICES_FIELD_NUMBER: _ClassVar[int]
    STATE_FIELD_NUMBER: _ClassVar[int]
    TRIM_MEMORY_LEVEL_FIELD_NUMBER: _ClassVar[int]
    PROC_FIELD_NUMBER: _ClassVar[int]
    ADJ_TYPE_FIELD_NUMBER: _ClassVar[int]
    ADJ_TARGET_COMPONENT_NAME_FIELD_NUMBER: _ClassVar[int]
    ADJ_TARGET_OBJECT_FIELD_NUMBER: _ClassVar[int]
    ADJ_SOURCE_PROC_FIELD_NUMBER: _ClassVar[int]
    ADJ_SOURCE_OBJECT_FIELD_NUMBER: _ClassVar[int]
    DETAIL_FIELD_NUMBER: _ClassVar[int]
    persistent: bool
    num: int
    oom_adj: str
    sched_group: ProcessOomProto.SchedGroup
    activities: bool
    services: bool
    state: _app_enums_pb2.ProcessStateEnum
    trim_memory_level: int
    proc: ProcessRecordProto
    adj_type: str
    adj_target_component_name: _component_name_pb2.ComponentNameProto
    adj_target_object: str
    adj_source_proc: ProcessRecordProto
    adj_source_object: str
    detail: ProcessOomProto.Detail
    def __init__(self, persistent: _Optional[bool] = ..., num: _Optional[int] = ..., oom_adj: _Optional[str] = ..., sched_group: _Optional[_Union[ProcessOomProto.SchedGroup, str]] = ..., activities: _Optional[bool] = ..., services: _Optional[bool] = ..., state: _Optional[_Union[_app_enums_pb2.ProcessStateEnum, str]] = ..., trim_memory_level: _Optional[int] = ..., proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., adj_type: _Optional[str] = ..., adj_target_component_name: _Optional[_Union[_component_name_pb2.ComponentNameProto, _Mapping]] = ..., adj_target_object: _Optional[str] = ..., adj_source_proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., adj_source_object: _Optional[str] = ..., detail: _Optional[_Union[ProcessOomProto.Detail, _Mapping]] = ...) -> None: ...

class ProcessToGcProto(_message.Message):
    __slots__ = ("proc", "report_low_memory", "now_uptime_ms", "last_gced_ms", "last_low_memory_ms")
    PROC_FIELD_NUMBER: _ClassVar[int]
    REPORT_LOW_MEMORY_FIELD_NUMBER: _ClassVar[int]
    NOW_UPTIME_MS_FIELD_NUMBER: _ClassVar[int]
    LAST_GCED_MS_FIELD_NUMBER: _ClassVar[int]
    LAST_LOW_MEMORY_MS_FIELD_NUMBER: _ClassVar[int]
    proc: ProcessRecordProto
    report_low_memory: bool
    now_uptime_ms: int
    last_gced_ms: int
    last_low_memory_ms: int
    def __init__(self, proc: _Optional[_Union[ProcessRecordProto, _Mapping]] = ..., report_low_memory: _Optional[bool] = ..., now_uptime_ms: _Optional[int] = ..., last_gced_ms: _Optional[int] = ..., last_low_memory_ms: _Optional[int] = ...) -> None: ...

class AppErrorsProto(_message.Message):
    __slots__ = ("now_uptime_ms", "process_crash_times", "bad_processes")
    class ProcessCrashTime(_message.Message):
        __slots__ = ("process_name", "entries")
        class Entry(_message.Message):
            __slots__ = ("uid", "last_crashed_at_ms")
            UID_FIELD_NUMBER: _ClassVar[int]
            LAST_CRASHED_AT_MS_FIELD_NUMBER: _ClassVar[int]
            uid: int
            last_crashed_at_ms: int
            def __init__(self, uid: _Optional[int] = ..., last_crashed_at_ms: _Optional[int] = ...) -> None: ...
        PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
        ENTRIES_FIELD_NUMBER: _ClassVar[int]
        process_name: str
        entries: _containers.RepeatedCompositeFieldContainer[AppErrorsProto.ProcessCrashTime.Entry]
        def __init__(self, process_name: _Optional[str] = ..., entries: _Optional[_Iterable[_Union[AppErrorsProto.ProcessCrashTime.Entry, _Mapping]]] = ...) -> None: ...
    class BadProcess(_message.Message):
        __slots__ = ("process_name", "entries")
        class Entry(_message.Message):
            __slots__ = ("uid", "crashed_at_ms", "short_msg", "long_msg", "stack")
            UID_FIELD_NUMBER: _ClassVar[int]
            CRASHED_AT_MS_FIELD_NUMBER: _ClassVar[int]
            SHORT_MSG_FIELD_NUMBER: _ClassVar[int]
            LONG_MSG_FIELD_NUMBER: _ClassVar[int]
            STACK_FIELD_NUMBER: _ClassVar[int]
            uid: int
            crashed_at_ms: int
            short_msg: str
            long_msg: str
            stack: str
            def __init__(self, uid: _Optional[int] = ..., crashed_at_ms: _Optional[int] = ..., short_msg: _Optional[str] = ..., long_msg: _Optional[str] = ..., stack: _Optional[str] = ...) -> None: ...
        PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
        ENTRIES_FIELD_NUMBER: _ClassVar[int]
        process_name: str
        entries: _containers.RepeatedCompositeFieldContainer[AppErrorsProto.BadProcess.Entry]
        def __init__(self, process_name: _Optional[str] = ..., entries: _Optional[_Iterable[_Union[AppErrorsProto.BadProcess.Entry, _Mapping]]] = ...) -> None: ...
    NOW_UPTIME_MS_FIELD_NUMBER: _ClassVar[int]
    PROCESS_CRASH_TIMES_FIELD_NUMBER: _ClassVar[int]
    BAD_PROCESSES_FIELD_NUMBER: _ClassVar[int]
    now_uptime_ms: int
    process_crash_times: _containers.RepeatedCompositeFieldContainer[AppErrorsProto.ProcessCrashTime]
    bad_processes: _containers.RepeatedCompositeFieldContainer[AppErrorsProto.BadProcess]
    def __init__(self, now_uptime_ms: _Optional[int] = ..., process_crash_times: _Optional[_Iterable[_Union[AppErrorsProto.ProcessCrashTime, _Mapping]]] = ..., bad_processes: _Optional[_Iterable[_Union[AppErrorsProto.BadProcess, _Mapping]]] = ...) -> None: ...

class UserStateProto(_message.Message):
    __slots__ = ("state", "switching")
    class State(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        STATE_BOOTING: _ClassVar[UserStateProto.State]
        STATE_RUNNING_LOCKED: _ClassVar[UserStateProto.State]
        STATE_RUNNING_UNLOCKING: _ClassVar[UserStateProto.State]
        STATE_RUNNING_UNLOCKED: _ClassVar[UserStateProto.State]
        STATE_STOPPING: _ClassVar[UserStateProto.State]
        STATE_SHUTDOWN: _ClassVar[UserStateProto.State]
    STATE_BOOTING: UserStateProto.State
    STATE_RUNNING_LOCKED: UserStateProto.State
    STATE_RUNNING_UNLOCKING: UserStateProto.State
    STATE_RUNNING_UNLOCKED: UserStateProto.State
    STATE_STOPPING: UserStateProto.State
    STATE_SHUTDOWN: UserStateProto.State
    STATE_FIELD_NUMBER: _ClassVar[int]
    SWITCHING_FIELD_NUMBER: _ClassVar[int]
    state: UserStateProto.State
    switching: bool
    def __init__(self, state: _Optional[_Union[UserStateProto.State, str]] = ..., switching: _Optional[bool] = ...) -> None: ...

class UserControllerProto(_message.Message):
    __slots__ = ("started_users", "started_user_array", "user_lru", "user_profile_group_ids", "current_user", "current_profiles")
    class User(_message.Message):
        __slots__ = ("id", "state")
        ID_FIELD_NUMBER: _ClassVar[int]
        STATE_FIELD_NUMBER: _ClassVar[int]
        id: int
        state: UserStateProto
        def __init__(self, id: _Optional[int] = ..., state: _Optional[_Union[UserStateProto, _Mapping]] = ...) -> None: ...
    class UserProfile(_message.Message):
        __slots__ = ("user", "profile")
        USER_FIELD_NUMBER: _ClassVar[int]
        PROFILE_FIELD_NUMBER: _ClassVar[int]
        user: int
        profile: int
        def __init__(self, user: _Optional[int] = ..., profile: _Optional[int] = ...) -> None: ...
    STARTED_USERS_FIELD_NUMBER: _ClassVar[int]
    STARTED_USER_ARRAY_FIELD_NUMBER: _ClassVar[int]
    USER_LRU_FIELD_NUMBER: _ClassVar[int]
    USER_PROFILE_GROUP_IDS_FIELD_NUMBER: _ClassVar[int]
    CURRENT_USER_FIELD_NUMBER: _ClassVar[int]
    CURRENT_PROFILES_FIELD_NUMBER: _ClassVar[int]
    started_users: _containers.RepeatedCompositeFieldContainer[UserControllerProto.User]
    started_user_array: _containers.RepeatedScalarFieldContainer[int]
    user_lru: _containers.RepeatedScalarFieldContainer[int]
    user_profile_group_ids: _containers.RepeatedCompositeFieldContainer[UserControllerProto.UserProfile]
    current_user: int
    current_profiles: _containers.RepeatedScalarFieldContainer[int]
    def __init__(self, started_users: _Optional[_Iterable[_Union[UserControllerProto.User, _Mapping]]] = ..., started_user_array: _Optional[_Iterable[int]] = ..., user_lru: _Optional[_Iterable[int]] = ..., user_profile_group_ids: _Optional[_Iterable[_Union[UserControllerProto.UserProfile, _Mapping]]] = ..., current_user: _Optional[int] = ..., current_profiles: _Optional[_Iterable[int]] = ...) -> None: ...

class AppTimeTrackerProto(_message.Message):
    __slots__ = ("receiver", "total_duration_ms", "package_times", "started_time", "started_package")
    class PackageTime(_message.Message):
        __slots__ = ("package", "duration_ms")
        PACKAGE_FIELD_NUMBER: _ClassVar[int]
        DURATION_MS_FIELD_NUMBER: _ClassVar[int]
        package: str
        duration_ms: int
        def __init__(self, package: _Optional[str] = ..., duration_ms: _Optional[int] = ...) -> None: ...
    RECEIVER_FIELD_NUMBER: _ClassVar[int]
    TOTAL_DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_TIMES_FIELD_NUMBER: _ClassVar[int]
    STARTED_TIME_FIELD_NUMBER: _ClassVar[int]
    STARTED_PACKAGE_FIELD_NUMBER: _ClassVar[int]
    receiver: str
    total_duration_ms: int
    package_times: _containers.RepeatedCompositeFieldContainer[AppTimeTrackerProto.PackageTime]
    started_time: _common_pb2.Duration
    started_package: str
    def __init__(self, receiver: _Optional[str] = ..., total_duration_ms: _Optional[int] = ..., package_times: _Optional[_Iterable[_Union[AppTimeTrackerProto.PackageTime, _Mapping]]] = ..., started_time: _Optional[_Union[_common_pb2.Duration, _Mapping]] = ..., started_package: _Optional[str] = ...) -> None: ...

class AppsExitInfoProto(_message.Message):
    __slots__ = ("last_update_timestamp", "packages")
    class Package(_message.Message):
        __slots__ = ("package_name", "users")
        class User(_message.Message):
            __slots__ = ("uid", "app_exit_info", "app_recoverable_crash")
            UID_FIELD_NUMBER: _ClassVar[int]
            APP_EXIT_INFO_FIELD_NUMBER: _ClassVar[int]
            APP_RECOVERABLE_CRASH_FIELD_NUMBER: _ClassVar[int]
            uid: int
            app_exit_info: _containers.RepeatedCompositeFieldContainer[_appexitinfo_pb2.ApplicationExitInfoProto]
            app_recoverable_crash: _containers.RepeatedCompositeFieldContainer[_appexitinfo_pb2.ApplicationExitInfoProto]
            def __init__(self, uid: _Optional[int] = ..., app_exit_info: _Optional[_Iterable[_Union[_appexitinfo_pb2.ApplicationExitInfoProto, _Mapping]]] = ..., app_recoverable_crash: _Optional[_Iterable[_Union[_appexitinfo_pb2.ApplicationExitInfoProto, _Mapping]]] = ...) -> None: ...
        PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
        USERS_FIELD_NUMBER: _ClassVar[int]
        package_name: str
        users: _containers.RepeatedCompositeFieldContainer[AppsExitInfoProto.Package.User]
        def __init__(self, package_name: _Optional[str] = ..., users: _Optional[_Iterable[_Union[AppsExitInfoProto.Package.User, _Mapping]]] = ...) -> None: ...
    LAST_UPDATE_TIMESTAMP_FIELD_NUMBER: _ClassVar[int]
    PACKAGES_FIELD_NUMBER: _ClassVar[int]
    last_update_timestamp: int
    packages: _containers.RepeatedCompositeFieldContainer[AppsExitInfoProto.Package]
    def __init__(self, last_update_timestamp: _Optional[int] = ..., packages: _Optional[_Iterable[_Union[AppsExitInfoProto.Package, _Mapping]]] = ...) -> None: ...

class AppsStartInfoProto(_message.Message):
    __slots__ = ("last_update_timestamp", "packages", "monotonic_time")
    class Package(_message.Message):
        __slots__ = ("package_name", "users")
        class User(_message.Message):
            __slots__ = ("uid", "app_start_info", "monitoring_enabled")
            UID_FIELD_NUMBER: _ClassVar[int]
            APP_START_INFO_FIELD_NUMBER: _ClassVar[int]
            MONITORING_ENABLED_FIELD_NUMBER: _ClassVar[int]
            uid: int
            app_start_info: _containers.RepeatedCompositeFieldContainer[_appstartinfo_pb2.ApplicationStartInfoProto]
            monitoring_enabled: bool
            def __init__(self, uid: _Optional[int] = ..., app_start_info: _Optional[_Iterable[_Union[_appstartinfo_pb2.ApplicationStartInfoProto, _Mapping]]] = ..., monitoring_enabled: _Optional[bool] = ...) -> None: ...
        PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
        USERS_FIELD_NUMBER: _ClassVar[int]
        package_name: str
        users: _containers.RepeatedCompositeFieldContainer[AppsStartInfoProto.Package.User]
        def __init__(self, package_name: _Optional[str] = ..., users: _Optional[_Iterable[_Union[AppsStartInfoProto.Package.User, _Mapping]]] = ...) -> None: ...
    LAST_UPDATE_TIMESTAMP_FIELD_NUMBER: _ClassVar[int]
    PACKAGES_FIELD_NUMBER: _ClassVar[int]
    MONOTONIC_TIME_FIELD_NUMBER: _ClassVar[int]
    last_update_timestamp: int
    packages: _containers.RepeatedCompositeFieldContainer[AppsStartInfoProto.Package]
    monotonic_time: int
    def __init__(self, last_update_timestamp: _Optional[int] = ..., packages: _Optional[_Iterable[_Union[AppsStartInfoProto.Package, _Mapping]]] = ..., monotonic_time: _Optional[int] = ...) -> None: ...
