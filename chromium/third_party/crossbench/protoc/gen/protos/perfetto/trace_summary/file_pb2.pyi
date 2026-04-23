from protos.perfetto.perfetto_sql import structured_query_pb2 as _structured_query_pb2
from protos.perfetto.trace_summary import v2_metric_pb2 as _v2_metric_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class TraceSummarySpec(_message.Message):
    __slots__ = ("metric_spec", "query", "metric_template_spec")
    METRIC_SPEC_FIELD_NUMBER: _ClassVar[int]
    QUERY_FIELD_NUMBER: _ClassVar[int]
    METRIC_TEMPLATE_SPEC_FIELD_NUMBER: _ClassVar[int]
    metric_spec: _containers.RepeatedCompositeFieldContainer[_v2_metric_pb2.TraceMetricV2Spec]
    query: _containers.RepeatedCompositeFieldContainer[_structured_query_pb2.PerfettoSqlStructuredQuery]
    metric_template_spec: _containers.RepeatedCompositeFieldContainer[_v2_metric_pb2.TraceMetricV2TemplateSpec]
    def __init__(self, metric_spec: _Optional[_Iterable[_Union[_v2_metric_pb2.TraceMetricV2Spec, _Mapping]]] = ..., query: _Optional[_Iterable[_Union[_structured_query_pb2.PerfettoSqlStructuredQuery, _Mapping]]] = ..., metric_template_spec: _Optional[_Iterable[_Union[_v2_metric_pb2.TraceMetricV2TemplateSpec, _Mapping]]] = ...) -> None: ...

class TraceSummary(_message.Message):
    __slots__ = ("metric_bundles", "metadata")
    class Metadata(_message.Message):
        __slots__ = ("key", "value")
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: str
        def __init__(self, key: _Optional[str] = ..., value: _Optional[str] = ...) -> None: ...
    METRIC_BUNDLES_FIELD_NUMBER: _ClassVar[int]
    METADATA_FIELD_NUMBER: _ClassVar[int]
    metric_bundles: _containers.RepeatedCompositeFieldContainer[_v2_metric_pb2.TraceMetricV2Bundle]
    metadata: _containers.RepeatedCompositeFieldContainer[TraceSummary.Metadata]
    def __init__(self, metric_bundles: _Optional[_Iterable[_Union[_v2_metric_pb2.TraceMetricV2Bundle, _Mapping]]] = ..., metadata: _Optional[_Iterable[_Union[TraceSummary.Metadata, _Mapping]]] = ...) -> None: ...
