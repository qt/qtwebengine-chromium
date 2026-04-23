from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ChromiumHistogramSamplesConfig(_message.Message):
    __slots__ = ("histograms", "filter_histogram_names")
    class HistogramSample(_message.Message):
        __slots__ = ("histogram_name", "min_value", "max_value")
        HISTOGRAM_NAME_FIELD_NUMBER: _ClassVar[int]
        MIN_VALUE_FIELD_NUMBER: _ClassVar[int]
        MAX_VALUE_FIELD_NUMBER: _ClassVar[int]
        histogram_name: str
        min_value: int
        max_value: int
        def __init__(self, histogram_name: _Optional[str] = ..., min_value: _Optional[int] = ..., max_value: _Optional[int] = ...) -> None: ...
    HISTOGRAMS_FIELD_NUMBER: _ClassVar[int]
    FILTER_HISTOGRAM_NAMES_FIELD_NUMBER: _ClassVar[int]
    histograms: _containers.RepeatedCompositeFieldContainer[ChromiumHistogramSamplesConfig.HistogramSample]
    filter_histogram_names: bool
    def __init__(self, histograms: _Optional[_Iterable[_Union[ChromiumHistogramSamplesConfig.HistogramSample, _Mapping]]] = ..., filter_histogram_names: _Optional[bool] = ...) -> None: ...
