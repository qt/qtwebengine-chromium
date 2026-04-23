from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class LocaleProto(_message.Message):
    __slots__ = ("language", "country", "variant", "script")
    LANGUAGE_FIELD_NUMBER: _ClassVar[int]
    COUNTRY_FIELD_NUMBER: _ClassVar[int]
    VARIANT_FIELD_NUMBER: _ClassVar[int]
    SCRIPT_FIELD_NUMBER: _ClassVar[int]
    language: str
    country: str
    variant: str
    script: str
    def __init__(self, language: _Optional[str] = ..., country: _Optional[str] = ..., variant: _Optional[str] = ..., script: _Optional[str] = ...) -> None: ...
