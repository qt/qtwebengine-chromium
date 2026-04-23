from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WorkSourceProto(_message.Message):
    __slots__ = ("work_source_contents", "work_chains")
    class WorkSourceContentProto(_message.Message):
        __slots__ = ("uid", "name")
        UID_FIELD_NUMBER: _ClassVar[int]
        NAME_FIELD_NUMBER: _ClassVar[int]
        uid: int
        name: str
        def __init__(self, uid: _Optional[int] = ..., name: _Optional[str] = ...) -> None: ...
    class WorkChain(_message.Message):
        __slots__ = ("nodes",)
        NODES_FIELD_NUMBER: _ClassVar[int]
        nodes: _containers.RepeatedCompositeFieldContainer[WorkSourceProto.WorkSourceContentProto]
        def __init__(self, nodes: _Optional[_Iterable[_Union[WorkSourceProto.WorkSourceContentProto, _Mapping]]] = ...) -> None: ...
    WORK_SOURCE_CONTENTS_FIELD_NUMBER: _ClassVar[int]
    WORK_CHAINS_FIELD_NUMBER: _ClassVar[int]
    work_source_contents: _containers.RepeatedCompositeFieldContainer[WorkSourceProto.WorkSourceContentProto]
    work_chains: _containers.RepeatedCompositeFieldContainer[WorkSourceProto.WorkChain]
    def __init__(self, work_source_contents: _Optional[_Iterable[_Union[WorkSourceProto.WorkSourceContentProto, _Mapping]]] = ..., work_chains: _Optional[_Iterable[_Union[WorkSourceProto.WorkChain, _Mapping]]] = ...) -> None: ...
