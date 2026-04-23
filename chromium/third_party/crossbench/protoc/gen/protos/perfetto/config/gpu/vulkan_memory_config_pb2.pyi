from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class VulkanMemoryConfig(_message.Message):
    __slots__ = ("track_driver_memory_usage", "track_device_memory_usage")
    TRACK_DRIVER_MEMORY_USAGE_FIELD_NUMBER: _ClassVar[int]
    TRACK_DEVICE_MEMORY_USAGE_FIELD_NUMBER: _ClassVar[int]
    track_driver_memory_usage: bool
    track_device_memory_usage: bool
    def __init__(self, track_driver_memory_usage: _Optional[bool] = ..., track_device_memory_usage: _Optional[bool] = ...) -> None: ...
