from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class SurfaceFlingerLayersConfig(_message.Message):
    __slots__ = ("mode", "trace_flags")
    class Mode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        MODE_UNSPECIFIED: _ClassVar[SurfaceFlingerLayersConfig.Mode]
        MODE_ACTIVE: _ClassVar[SurfaceFlingerLayersConfig.Mode]
        MODE_GENERATED: _ClassVar[SurfaceFlingerLayersConfig.Mode]
        MODE_DUMP: _ClassVar[SurfaceFlingerLayersConfig.Mode]
        MODE_GENERATED_BUGREPORT_ONLY: _ClassVar[SurfaceFlingerLayersConfig.Mode]
    MODE_UNSPECIFIED: SurfaceFlingerLayersConfig.Mode
    MODE_ACTIVE: SurfaceFlingerLayersConfig.Mode
    MODE_GENERATED: SurfaceFlingerLayersConfig.Mode
    MODE_DUMP: SurfaceFlingerLayersConfig.Mode
    MODE_GENERATED_BUGREPORT_ONLY: SurfaceFlingerLayersConfig.Mode
    class TraceFlag(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        TRACE_FLAG_UNSPECIFIED: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_INPUT: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_COMPOSITION: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_EXTRA: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_HWC: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_BUFFERS: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_VIRTUAL_DISPLAYS: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
        TRACE_FLAG_ALL: _ClassVar[SurfaceFlingerLayersConfig.TraceFlag]
    TRACE_FLAG_UNSPECIFIED: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_INPUT: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_COMPOSITION: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_EXTRA: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_HWC: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_BUFFERS: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_VIRTUAL_DISPLAYS: SurfaceFlingerLayersConfig.TraceFlag
    TRACE_FLAG_ALL: SurfaceFlingerLayersConfig.TraceFlag
    MODE_FIELD_NUMBER: _ClassVar[int]
    TRACE_FLAGS_FIELD_NUMBER: _ClassVar[int]
    mode: SurfaceFlingerLayersConfig.Mode
    trace_flags: _containers.RepeatedScalarFieldContainer[SurfaceFlingerLayersConfig.TraceFlag]
    def __init__(self, mode: _Optional[_Union[SurfaceFlingerLayersConfig.Mode, str]] = ..., trace_flags: _Optional[_Iterable[_Union[SurfaceFlingerLayersConfig.TraceFlag, str]]] = ...) -> None: ...
