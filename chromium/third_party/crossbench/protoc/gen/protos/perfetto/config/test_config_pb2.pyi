from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class TestConfig(_message.Message):
    __slots__ = ("message_count", "max_messages_per_second", "seed", "message_size", "send_batch_on_register", "dummy_fields")
    class DummyFields(_message.Message):
        __slots__ = ("field_uint32", "field_int32", "field_uint64", "field_int64", "field_fixed64", "field_sfixed64", "field_fixed32", "field_sfixed32", "field_double", "field_float", "field_sint64", "field_sint32", "field_string", "field_bytes")
        FIELD_UINT32_FIELD_NUMBER: _ClassVar[int]
        FIELD_INT32_FIELD_NUMBER: _ClassVar[int]
        FIELD_UINT64_FIELD_NUMBER: _ClassVar[int]
        FIELD_INT64_FIELD_NUMBER: _ClassVar[int]
        FIELD_FIXED64_FIELD_NUMBER: _ClassVar[int]
        FIELD_SFIXED64_FIELD_NUMBER: _ClassVar[int]
        FIELD_FIXED32_FIELD_NUMBER: _ClassVar[int]
        FIELD_SFIXED32_FIELD_NUMBER: _ClassVar[int]
        FIELD_DOUBLE_FIELD_NUMBER: _ClassVar[int]
        FIELD_FLOAT_FIELD_NUMBER: _ClassVar[int]
        FIELD_SINT64_FIELD_NUMBER: _ClassVar[int]
        FIELD_SINT32_FIELD_NUMBER: _ClassVar[int]
        FIELD_STRING_FIELD_NUMBER: _ClassVar[int]
        FIELD_BYTES_FIELD_NUMBER: _ClassVar[int]
        field_uint32: int
        field_int32: int
        field_uint64: int
        field_int64: int
        field_fixed64: int
        field_sfixed64: int
        field_fixed32: int
        field_sfixed32: int
        field_double: float
        field_float: float
        field_sint64: int
        field_sint32: int
        field_string: str
        field_bytes: bytes
        def __init__(self, field_uint32: _Optional[int] = ..., field_int32: _Optional[int] = ..., field_uint64: _Optional[int] = ..., field_int64: _Optional[int] = ..., field_fixed64: _Optional[int] = ..., field_sfixed64: _Optional[int] = ..., field_fixed32: _Optional[int] = ..., field_sfixed32: _Optional[int] = ..., field_double: _Optional[float] = ..., field_float: _Optional[float] = ..., field_sint64: _Optional[int] = ..., field_sint32: _Optional[int] = ..., field_string: _Optional[str] = ..., field_bytes: _Optional[bytes] = ...) -> None: ...
    MESSAGE_COUNT_FIELD_NUMBER: _ClassVar[int]
    MAX_MESSAGES_PER_SECOND_FIELD_NUMBER: _ClassVar[int]
    SEED_FIELD_NUMBER: _ClassVar[int]
    MESSAGE_SIZE_FIELD_NUMBER: _ClassVar[int]
    SEND_BATCH_ON_REGISTER_FIELD_NUMBER: _ClassVar[int]
    DUMMY_FIELDS_FIELD_NUMBER: _ClassVar[int]
    message_count: int
    max_messages_per_second: int
    seed: int
    message_size: int
    send_batch_on_register: bool
    dummy_fields: TestConfig.DummyFields
    def __init__(self, message_count: _Optional[int] = ..., max_messages_per_second: _Optional[int] = ..., seed: _Optional[int] = ..., message_size: _Optional[int] = ..., send_batch_on_register: _Optional[bool] = ..., dummy_fields: _Optional[_Union[TestConfig.DummyFields, _Mapping]] = ...) -> None: ...
