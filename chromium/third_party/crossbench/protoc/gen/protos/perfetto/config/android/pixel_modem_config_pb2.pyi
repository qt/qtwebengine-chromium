from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PixelModemConfig(_message.Message):
    __slots__ = ("event_group", "pigweed_hash_allow_list", "pigweed_hash_deny_list")
    class EventGroup(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        EVENT_GROUP_UNKNOWN: _ClassVar[PixelModemConfig.EventGroup]
        EVENT_GROUP_LOW_BANDWIDTH: _ClassVar[PixelModemConfig.EventGroup]
        EVENT_GROUP_HIGH_AND_LOW_BANDWIDTH: _ClassVar[PixelModemConfig.EventGroup]
    EVENT_GROUP_UNKNOWN: PixelModemConfig.EventGroup
    EVENT_GROUP_LOW_BANDWIDTH: PixelModemConfig.EventGroup
    EVENT_GROUP_HIGH_AND_LOW_BANDWIDTH: PixelModemConfig.EventGroup
    EVENT_GROUP_FIELD_NUMBER: _ClassVar[int]
    PIGWEED_HASH_ALLOW_LIST_FIELD_NUMBER: _ClassVar[int]
    PIGWEED_HASH_DENY_LIST_FIELD_NUMBER: _ClassVar[int]
    event_group: PixelModemConfig.EventGroup
    pigweed_hash_allow_list: _containers.RepeatedScalarFieldContainer[int]
    pigweed_hash_deny_list: _containers.RepeatedScalarFieldContainer[int]
    def __init__(self, event_group: _Optional[_Union[PixelModemConfig.EventGroup, str]] = ..., pigweed_hash_allow_list: _Optional[_Iterable[int]] = ..., pigweed_hash_deny_list: _Optional[_Iterable[int]] = ...) -> None: ...
