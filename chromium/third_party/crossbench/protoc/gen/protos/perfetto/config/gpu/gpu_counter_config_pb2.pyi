from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class GpuCounterConfig(_message.Message):
    __slots__ = ("counter_period_ns", "counter_ids", "instrumented_sampling", "fix_gpu_clock")
    COUNTER_PERIOD_NS_FIELD_NUMBER: _ClassVar[int]
    COUNTER_IDS_FIELD_NUMBER: _ClassVar[int]
    INSTRUMENTED_SAMPLING_FIELD_NUMBER: _ClassVar[int]
    FIX_GPU_CLOCK_FIELD_NUMBER: _ClassVar[int]
    counter_period_ns: int
    counter_ids: _containers.RepeatedScalarFieldContainer[int]
    instrumented_sampling: bool
    fix_gpu_clock: bool
    def __init__(self, counter_period_ns: _Optional[int] = ..., counter_ids: _Optional[_Iterable[int]] = ..., instrumented_sampling: _Optional[bool] = ..., fix_gpu_clock: _Optional[bool] = ...) -> None: ...
