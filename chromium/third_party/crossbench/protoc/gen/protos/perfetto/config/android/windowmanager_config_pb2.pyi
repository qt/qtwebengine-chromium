from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WindowManagerConfig(_message.Message):
    __slots__ = ("log_frequency", "log_level")
    class LogFrequency(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        LOG_FREQUENCY_UNSPECIFIED: _ClassVar[WindowManagerConfig.LogFrequency]
        LOG_FREQUENCY_FRAME: _ClassVar[WindowManagerConfig.LogFrequency]
        LOG_FREQUENCY_TRANSACTION: _ClassVar[WindowManagerConfig.LogFrequency]
        LOG_FREQUENCY_SINGLE_DUMP: _ClassVar[WindowManagerConfig.LogFrequency]
    LOG_FREQUENCY_UNSPECIFIED: WindowManagerConfig.LogFrequency
    LOG_FREQUENCY_FRAME: WindowManagerConfig.LogFrequency
    LOG_FREQUENCY_TRANSACTION: WindowManagerConfig.LogFrequency
    LOG_FREQUENCY_SINGLE_DUMP: WindowManagerConfig.LogFrequency
    class LogLevel(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        LOG_LEVEL_UNSPECIFIED: _ClassVar[WindowManagerConfig.LogLevel]
        LOG_LEVEL_VERBOSE: _ClassVar[WindowManagerConfig.LogLevel]
        LOG_LEVEL_DEBUG: _ClassVar[WindowManagerConfig.LogLevel]
        LOG_LEVEL_CRITICAL: _ClassVar[WindowManagerConfig.LogLevel]
    LOG_LEVEL_UNSPECIFIED: WindowManagerConfig.LogLevel
    LOG_LEVEL_VERBOSE: WindowManagerConfig.LogLevel
    LOG_LEVEL_DEBUG: WindowManagerConfig.LogLevel
    LOG_LEVEL_CRITICAL: WindowManagerConfig.LogLevel
    LOG_FREQUENCY_FIELD_NUMBER: _ClassVar[int]
    LOG_LEVEL_FIELD_NUMBER: _ClassVar[int]
    log_frequency: WindowManagerConfig.LogFrequency
    log_level: WindowManagerConfig.LogLevel
    def __init__(self, log_frequency: _Optional[_Union[WindowManagerConfig.LogFrequency, str]] = ..., log_level: _Optional[_Union[WindowManagerConfig.LogLevel, str]] = ...) -> None: ...
