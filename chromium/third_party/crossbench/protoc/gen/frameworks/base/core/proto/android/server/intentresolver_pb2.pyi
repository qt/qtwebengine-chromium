from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class IntentResolverProto(_message.Message):
    __slots__ = ("full_mime_types", "base_mime_types", "wild_mime_types", "schemes", "non_data_actions", "mime_typed_actions")
    class ArrayMapEntry(_message.Message):
        __slots__ = ("key", "values")
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUES_FIELD_NUMBER: _ClassVar[int]
        key: str
        values: _containers.RepeatedScalarFieldContainer[str]
        def __init__(self, key: _Optional[str] = ..., values: _Optional[_Iterable[str]] = ...) -> None: ...
    FULL_MIME_TYPES_FIELD_NUMBER: _ClassVar[int]
    BASE_MIME_TYPES_FIELD_NUMBER: _ClassVar[int]
    WILD_MIME_TYPES_FIELD_NUMBER: _ClassVar[int]
    SCHEMES_FIELD_NUMBER: _ClassVar[int]
    NON_DATA_ACTIONS_FIELD_NUMBER: _ClassVar[int]
    MIME_TYPED_ACTIONS_FIELD_NUMBER: _ClassVar[int]
    full_mime_types: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    base_mime_types: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    wild_mime_types: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    schemes: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    non_data_actions: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    mime_typed_actions: _containers.RepeatedCompositeFieldContainer[IntentResolverProto.ArrayMapEntry]
    def __init__(self, full_mime_types: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ..., base_mime_types: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ..., wild_mime_types: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ..., schemes: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ..., non_data_actions: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ..., mime_typed_actions: _Optional[_Iterable[_Union[IntentResolverProto.ArrayMapEntry, _Mapping]]] = ...) -> None: ...
