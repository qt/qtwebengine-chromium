from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional

DESCRIPTOR: _descriptor.FileDescriptor

class PackagesListConfig(_message.Message):
    __slots__ = ("package_name_filter", "only_write_on_cpu_use_every_ms")
    PACKAGE_NAME_FILTER_FIELD_NUMBER: _ClassVar[int]
    ONLY_WRITE_ON_CPU_USE_EVERY_MS_FIELD_NUMBER: _ClassVar[int]
    package_name_filter: _containers.RepeatedScalarFieldContainer[str]
    only_write_on_cpu_use_every_ms: int
    def __init__(self, package_name_filter: _Optional[_Iterable[str]] = ..., only_write_on_cpu_use_every_ms: _Optional[int] = ...) -> None: ...
