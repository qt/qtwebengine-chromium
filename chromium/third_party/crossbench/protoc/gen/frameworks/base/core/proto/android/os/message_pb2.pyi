from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class MessageProto(_message.Message):
    __slots__ = ("when", "callback", "what", "arg1", "arg2", "obj", "target", "barrier")
    WHEN_FIELD_NUMBER: _ClassVar[int]
    CALLBACK_FIELD_NUMBER: _ClassVar[int]
    WHAT_FIELD_NUMBER: _ClassVar[int]
    ARG1_FIELD_NUMBER: _ClassVar[int]
    ARG2_FIELD_NUMBER: _ClassVar[int]
    OBJ_FIELD_NUMBER: _ClassVar[int]
    TARGET_FIELD_NUMBER: _ClassVar[int]
    BARRIER_FIELD_NUMBER: _ClassVar[int]
    when: int
    callback: str
    what: int
    arg1: int
    arg2: int
    obj: str
    target: str
    barrier: int
    def __init__(self, when: _Optional[int] = ..., callback: _Optional[str] = ..., what: _Optional[int] = ..., arg1: _Optional[int] = ..., arg2: _Optional[int] = ..., obj: _Optional[str] = ..., target: _Optional[str] = ..., barrier: _Optional[int] = ...) -> None: ...
