from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class GpuRenderStagesConfig(_message.Message):
    __slots__ = ("full_loadstore", "low_overhead", "trace_metrics")
    FULL_LOADSTORE_FIELD_NUMBER: _ClassVar[int]
    LOW_OVERHEAD_FIELD_NUMBER: _ClassVar[int]
    TRACE_METRICS_FIELD_NUMBER: _ClassVar[int]
    full_loadstore: bool
    low_overhead: bool
    trace_metrics: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, full_loadstore: _Optional[bool] = ..., low_overhead: _Optional[bool] = ..., trace_metrics: _Optional[_Iterable[str]] = ...) -> None: ...
