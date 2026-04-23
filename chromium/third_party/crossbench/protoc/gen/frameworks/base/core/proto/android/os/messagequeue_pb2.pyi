from frameworks.base.core.proto.android.os import message_pb2 as _message_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class MessageQueueProto(_message.Message):
    __slots__ = ("messages", "is_polling_locked", "is_quitting")
    MESSAGES_FIELD_NUMBER: _ClassVar[int]
    IS_POLLING_LOCKED_FIELD_NUMBER: _ClassVar[int]
    IS_QUITTING_FIELD_NUMBER: _ClassVar[int]
    messages: _containers.RepeatedCompositeFieldContainer[_message_pb2.MessageProto]
    is_polling_locked: bool
    is_quitting: bool
    def __init__(self, messages: _Optional[_Iterable[_Union[_message_pb2.MessageProto, _Mapping]]] = ..., is_polling_locked: _Optional[bool] = ..., is_quitting: _Optional[bool] = ...) -> None: ...
