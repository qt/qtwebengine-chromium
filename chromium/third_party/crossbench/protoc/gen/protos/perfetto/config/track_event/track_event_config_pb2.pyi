from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class TrackEventConfig(_message.Message):
    __slots__ = ("disabled_categories", "enabled_categories", "disabled_tags", "enabled_tags", "disable_incremental_timestamps", "timestamp_unit_multiplier", "filter_debug_annotations", "enable_thread_time_sampling", "thread_time_subsampling_ns", "filter_dynamic_event_names")
    DISABLED_CATEGORIES_FIELD_NUMBER: _ClassVar[int]
    ENABLED_CATEGORIES_FIELD_NUMBER: _ClassVar[int]
    DISABLED_TAGS_FIELD_NUMBER: _ClassVar[int]
    ENABLED_TAGS_FIELD_NUMBER: _ClassVar[int]
    DISABLE_INCREMENTAL_TIMESTAMPS_FIELD_NUMBER: _ClassVar[int]
    TIMESTAMP_UNIT_MULTIPLIER_FIELD_NUMBER: _ClassVar[int]
    FILTER_DEBUG_ANNOTATIONS_FIELD_NUMBER: _ClassVar[int]
    ENABLE_THREAD_TIME_SAMPLING_FIELD_NUMBER: _ClassVar[int]
    THREAD_TIME_SUBSAMPLING_NS_FIELD_NUMBER: _ClassVar[int]
    FILTER_DYNAMIC_EVENT_NAMES_FIELD_NUMBER: _ClassVar[int]
    disabled_categories: _containers.RepeatedScalarFieldContainer[str]
    enabled_categories: _containers.RepeatedScalarFieldContainer[str]
    disabled_tags: _containers.RepeatedScalarFieldContainer[str]
    enabled_tags: _containers.RepeatedScalarFieldContainer[str]
    disable_incremental_timestamps: bool
    timestamp_unit_multiplier: int
    filter_debug_annotations: bool
    enable_thread_time_sampling: bool
    thread_time_subsampling_ns: int
    filter_dynamic_event_names: bool
    def __init__(self, disabled_categories: _Optional[_Iterable[str]] = ..., enabled_categories: _Optional[_Iterable[str]] = ..., disabled_tags: _Optional[_Iterable[str]] = ..., enabled_tags: _Optional[_Iterable[str]] = ..., disable_incremental_timestamps: _Optional[bool] = ..., timestamp_unit_multiplier: _Optional[int] = ..., filter_debug_annotations: _Optional[bool] = ..., enable_thread_time_sampling: _Optional[bool] = ..., thread_time_subsampling_ns: _Optional[int] = ..., filter_dynamic_event_names: _Optional[bool] = ...) -> None: ...
