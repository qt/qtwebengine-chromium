from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class SurfaceFlingerTransactionsConfig(_message.Message):
    __slots__ = ("mode",)
    class Mode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        MODE_UNSPECIFIED: _ClassVar[SurfaceFlingerTransactionsConfig.Mode]
        MODE_CONTINUOUS: _ClassVar[SurfaceFlingerTransactionsConfig.Mode]
        MODE_ACTIVE: _ClassVar[SurfaceFlingerTransactionsConfig.Mode]
    MODE_UNSPECIFIED: SurfaceFlingerTransactionsConfig.Mode
    MODE_CONTINUOUS: SurfaceFlingerTransactionsConfig.Mode
    MODE_ACTIVE: SurfaceFlingerTransactionsConfig.Mode
    MODE_FIELD_NUMBER: _ClassVar[int]
    mode: SurfaceFlingerTransactionsConfig.Mode
    def __init__(self, mode: _Optional[_Union[SurfaceFlingerTransactionsConfig.Mode, str]] = ...) -> None: ...
