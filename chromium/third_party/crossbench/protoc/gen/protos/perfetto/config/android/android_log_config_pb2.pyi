from protos.perfetto.common import android_log_constants_pb2 as _android_log_constants_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidLogConfig(_message.Message):
    __slots__ = ("log_ids", "min_prio", "filter_tags")
    LOG_IDS_FIELD_NUMBER: _ClassVar[int]
    MIN_PRIO_FIELD_NUMBER: _ClassVar[int]
    FILTER_TAGS_FIELD_NUMBER: _ClassVar[int]
    log_ids: _containers.RepeatedScalarFieldContainer[_android_log_constants_pb2.AndroidLogId]
    min_prio: _android_log_constants_pb2.AndroidLogPriority
    filter_tags: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, log_ids: _Optional[_Iterable[_Union[_android_log_constants_pb2.AndroidLogId, str]]] = ..., min_prio: _Optional[_Union[_android_log_constants_pb2.AndroidLogPriority, str]] = ..., filter_tags: _Optional[_Iterable[str]] = ...) -> None: ...
