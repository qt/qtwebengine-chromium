from protos.perfetto.common import protolog_common_pb2 as _protolog_common_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ProtoLogConfig(_message.Message):
    __slots__ = ("group_overrides", "tracing_mode", "default_log_from_level")
    class TracingMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        DEFAULT: _ClassVar[ProtoLogConfig.TracingMode]
        ENABLE_ALL: _ClassVar[ProtoLogConfig.TracingMode]
    DEFAULT: ProtoLogConfig.TracingMode
    ENABLE_ALL: ProtoLogConfig.TracingMode
    GROUP_OVERRIDES_FIELD_NUMBER: _ClassVar[int]
    TRACING_MODE_FIELD_NUMBER: _ClassVar[int]
    DEFAULT_LOG_FROM_LEVEL_FIELD_NUMBER: _ClassVar[int]
    group_overrides: _containers.RepeatedCompositeFieldContainer[ProtoLogGroup]
    tracing_mode: ProtoLogConfig.TracingMode
    default_log_from_level: _protolog_common_pb2.ProtoLogLevel
    def __init__(self, group_overrides: _Optional[_Iterable[_Union[ProtoLogGroup, _Mapping]]] = ..., tracing_mode: _Optional[_Union[ProtoLogConfig.TracingMode, str]] = ..., default_log_from_level: _Optional[_Union[_protolog_common_pb2.ProtoLogLevel, str]] = ...) -> None: ...

class ProtoLogGroup(_message.Message):
    __slots__ = ("group_name", "log_from", "collect_stacktrace")
    GROUP_NAME_FIELD_NUMBER: _ClassVar[int]
    LOG_FROM_FIELD_NUMBER: _ClassVar[int]
    COLLECT_STACKTRACE_FIELD_NUMBER: _ClassVar[int]
    group_name: str
    log_from: _protolog_common_pb2.ProtoLogLevel
    collect_stacktrace: bool
    def __init__(self, group_name: _Optional[str] = ..., log_from: _Optional[_Union[_protolog_common_pb2.ProtoLogLevel, str]] = ..., collect_stacktrace: _Optional[bool] = ...) -> None: ...
