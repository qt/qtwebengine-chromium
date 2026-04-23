from frameworks.base.core.proto.android.os import messagequeue_pb2 as _messagequeue_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class LooperProto(_message.Message):
    __slots__ = ("thread_name", "thread_id", "queue")
    THREAD_NAME_FIELD_NUMBER: _ClassVar[int]
    THREAD_ID_FIELD_NUMBER: _ClassVar[int]
    QUEUE_FIELD_NUMBER: _ClassVar[int]
    thread_name: str
    thread_id: int
    queue: _messagequeue_pb2.MessageQueueProto
    def __init__(self, thread_name: _Optional[str] = ..., thread_id: _Optional[int] = ..., queue: _Optional[_Union[_messagequeue_pb2.MessageQueueProto, _Mapping]] = ...) -> None: ...
