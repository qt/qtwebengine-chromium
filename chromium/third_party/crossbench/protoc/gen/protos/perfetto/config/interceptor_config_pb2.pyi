from protos.perfetto.config.interceptors import console_config_pb2 as _console_config_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class InterceptorConfig(_message.Message):
    __slots__ = ("name", "console_config")
    NAME_FIELD_NUMBER: _ClassVar[int]
    CONSOLE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    name: str
    console_config: _console_config_pb2.ConsoleConfig
    def __init__(self, name: _Optional[str] = ..., console_config: _Optional[_Union[_console_config_pb2.ConsoleConfig, _Mapping]] = ...) -> None: ...
