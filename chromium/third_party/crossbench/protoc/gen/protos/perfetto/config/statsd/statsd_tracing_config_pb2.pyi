from protos.perfetto.config.statsd import atom_ids_pb2 as _atom_ids_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class StatsdTracingConfig(_message.Message):
    __slots__ = ("push_atom_id", "raw_push_atom_id", "pull_config")
    PUSH_ATOM_ID_FIELD_NUMBER: _ClassVar[int]
    RAW_PUSH_ATOM_ID_FIELD_NUMBER: _ClassVar[int]
    PULL_CONFIG_FIELD_NUMBER: _ClassVar[int]
    push_atom_id: _containers.RepeatedScalarFieldContainer[_atom_ids_pb2.AtomId]
    raw_push_atom_id: _containers.RepeatedScalarFieldContainer[int]
    pull_config: _containers.RepeatedCompositeFieldContainer[StatsdPullAtomConfig]
    def __init__(self, push_atom_id: _Optional[_Iterable[_Union[_atom_ids_pb2.AtomId, str]]] = ..., raw_push_atom_id: _Optional[_Iterable[int]] = ..., pull_config: _Optional[_Iterable[_Union[StatsdPullAtomConfig, _Mapping]]] = ...) -> None: ...

class StatsdPullAtomConfig(_message.Message):
    __slots__ = ("pull_atom_id", "raw_pull_atom_id", "pull_frequency_ms", "packages")
    PULL_ATOM_ID_FIELD_NUMBER: _ClassVar[int]
    RAW_PULL_ATOM_ID_FIELD_NUMBER: _ClassVar[int]
    PULL_FREQUENCY_MS_FIELD_NUMBER: _ClassVar[int]
    PACKAGES_FIELD_NUMBER: _ClassVar[int]
    pull_atom_id: _containers.RepeatedScalarFieldContainer[_atom_ids_pb2.AtomId]
    raw_pull_atom_id: _containers.RepeatedScalarFieldContainer[int]
    pull_frequency_ms: int
    packages: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, pull_atom_id: _Optional[_Iterable[_Union[_atom_ids_pb2.AtomId, str]]] = ..., raw_pull_atom_id: _Optional[_Iterable[int]] = ..., pull_frequency_ms: _Optional[int] = ..., packages: _Optional[_Iterable[str]] = ...) -> None: ...
