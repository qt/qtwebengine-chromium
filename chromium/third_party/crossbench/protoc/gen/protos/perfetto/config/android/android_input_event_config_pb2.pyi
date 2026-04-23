from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidInputEventConfig(_message.Message):
    __slots__ = ("mode", "rules", "trace_dispatcher_input_events", "trace_dispatcher_window_dispatch")
    class TraceMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        TRACE_MODE_TRACE_ALL: _ClassVar[AndroidInputEventConfig.TraceMode]
        TRACE_MODE_USE_RULES: _ClassVar[AndroidInputEventConfig.TraceMode]
    TRACE_MODE_TRACE_ALL: AndroidInputEventConfig.TraceMode
    TRACE_MODE_USE_RULES: AndroidInputEventConfig.TraceMode
    class TraceLevel(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        TRACE_LEVEL_NONE: _ClassVar[AndroidInputEventConfig.TraceLevel]
        TRACE_LEVEL_REDACTED: _ClassVar[AndroidInputEventConfig.TraceLevel]
        TRACE_LEVEL_COMPLETE: _ClassVar[AndroidInputEventConfig.TraceLevel]
    TRACE_LEVEL_NONE: AndroidInputEventConfig.TraceLevel
    TRACE_LEVEL_REDACTED: AndroidInputEventConfig.TraceLevel
    TRACE_LEVEL_COMPLETE: AndroidInputEventConfig.TraceLevel
    class TraceRule(_message.Message):
        __slots__ = ("trace_level", "match_all_packages", "match_any_packages", "match_secure", "match_ime_connection_active")
        TRACE_LEVEL_FIELD_NUMBER: _ClassVar[int]
        MATCH_ALL_PACKAGES_FIELD_NUMBER: _ClassVar[int]
        MATCH_ANY_PACKAGES_FIELD_NUMBER: _ClassVar[int]
        MATCH_SECURE_FIELD_NUMBER: _ClassVar[int]
        MATCH_IME_CONNECTION_ACTIVE_FIELD_NUMBER: _ClassVar[int]
        trace_level: AndroidInputEventConfig.TraceLevel
        match_all_packages: _containers.RepeatedScalarFieldContainer[str]
        match_any_packages: _containers.RepeatedScalarFieldContainer[str]
        match_secure: bool
        match_ime_connection_active: bool
        def __init__(self, trace_level: _Optional[_Union[AndroidInputEventConfig.TraceLevel, str]] = ..., match_all_packages: _Optional[_Iterable[str]] = ..., match_any_packages: _Optional[_Iterable[str]] = ..., match_secure: _Optional[bool] = ..., match_ime_connection_active: _Optional[bool] = ...) -> None: ...
    MODE_FIELD_NUMBER: _ClassVar[int]
    RULES_FIELD_NUMBER: _ClassVar[int]
    TRACE_DISPATCHER_INPUT_EVENTS_FIELD_NUMBER: _ClassVar[int]
    TRACE_DISPATCHER_WINDOW_DISPATCH_FIELD_NUMBER: _ClassVar[int]
    mode: AndroidInputEventConfig.TraceMode
    rules: _containers.RepeatedCompositeFieldContainer[AndroidInputEventConfig.TraceRule]
    trace_dispatcher_input_events: bool
    trace_dispatcher_window_dispatch: bool
    def __init__(self, mode: _Optional[_Union[AndroidInputEventConfig.TraceMode, str]] = ..., rules: _Optional[_Iterable[_Union[AndroidInputEventConfig.TraceRule, _Mapping]]] = ..., trace_dispatcher_input_events: _Optional[bool] = ..., trace_dispatcher_window_dispatch: _Optional[bool] = ...) -> None: ...
