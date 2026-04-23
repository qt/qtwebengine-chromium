from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ConsoleConfig(_message.Message):
    __slots__ = ("output", "enable_colors")
    class Output(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        OUTPUT_UNSPECIFIED: _ClassVar[ConsoleConfig.Output]
        OUTPUT_STDOUT: _ClassVar[ConsoleConfig.Output]
        OUTPUT_STDERR: _ClassVar[ConsoleConfig.Output]
    OUTPUT_UNSPECIFIED: ConsoleConfig.Output
    OUTPUT_STDOUT: ConsoleConfig.Output
    OUTPUT_STDERR: ConsoleConfig.Output
    OUTPUT_FIELD_NUMBER: _ClassVar[int]
    ENABLE_COLORS_FIELD_NUMBER: _ClassVar[int]
    output: ConsoleConfig.Output
    enable_colors: bool
    def __init__(self, output: _Optional[_Union[ConsoleConfig.Output, str]] = ..., enable_colors: _Optional[bool] = ...) -> None: ...
