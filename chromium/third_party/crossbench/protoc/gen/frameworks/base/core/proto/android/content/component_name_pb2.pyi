from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class ComponentNameProto(_message.Message):
    __slots__ = ("package_name", "class_name")
    PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
    CLASS_NAME_FIELD_NUMBER: _ClassVar[int]
    package_name: str
    class_name: str
    def __init__(self, package_name: _Optional[str] = ..., class_name: _Optional[str] = ...) -> None: ...
