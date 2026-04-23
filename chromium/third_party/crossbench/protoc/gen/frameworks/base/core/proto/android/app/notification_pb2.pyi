from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class NotificationProto(_message.Message):
    __slots__ = ("channel_id", "has_ticker_text", "flags", "color", "category", "group_key", "sort_key", "action_length", "visibility", "public_version")
    class Visibility(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        VISIBILITY_SECRET: _ClassVar[NotificationProto.Visibility]
        VISIBILITY_PRIVATE: _ClassVar[NotificationProto.Visibility]
        VISIBILITY_PUBLIC: _ClassVar[NotificationProto.Visibility]
    VISIBILITY_SECRET: NotificationProto.Visibility
    VISIBILITY_PRIVATE: NotificationProto.Visibility
    VISIBILITY_PUBLIC: NotificationProto.Visibility
    CHANNEL_ID_FIELD_NUMBER: _ClassVar[int]
    HAS_TICKER_TEXT_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    COLOR_FIELD_NUMBER: _ClassVar[int]
    CATEGORY_FIELD_NUMBER: _ClassVar[int]
    GROUP_KEY_FIELD_NUMBER: _ClassVar[int]
    SORT_KEY_FIELD_NUMBER: _ClassVar[int]
    ACTION_LENGTH_FIELD_NUMBER: _ClassVar[int]
    VISIBILITY_FIELD_NUMBER: _ClassVar[int]
    PUBLIC_VERSION_FIELD_NUMBER: _ClassVar[int]
    channel_id: str
    has_ticker_text: bool
    flags: int
    color: int
    category: str
    group_key: str
    sort_key: str
    action_length: int
    visibility: NotificationProto.Visibility
    public_version: NotificationProto
    def __init__(self, channel_id: _Optional[str] = ..., has_ticker_text: _Optional[bool] = ..., flags: _Optional[int] = ..., color: _Optional[int] = ..., category: _Optional[str] = ..., group_key: _Optional[str] = ..., sort_key: _Optional[str] = ..., action_length: _Optional[int] = ..., visibility: _Optional[_Union[NotificationProto.Visibility, str]] = ..., public_version: _Optional[_Union[NotificationProto, _Mapping]] = ...) -> None: ...
