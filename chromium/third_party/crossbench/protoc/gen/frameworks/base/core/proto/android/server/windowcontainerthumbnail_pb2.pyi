from frameworks.base.core.proto.android.server import surfaceanimator_pb2 as _surfaceanimator_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class WindowContainerThumbnailProto(_message.Message):
    __slots__ = ("width", "height", "surface_animator")
    WIDTH_FIELD_NUMBER: _ClassVar[int]
    HEIGHT_FIELD_NUMBER: _ClassVar[int]
    SURFACE_ANIMATOR_FIELD_NUMBER: _ClassVar[int]
    width: int
    height: int
    surface_animator: _surfaceanimator_pb2.SurfaceAnimatorProto
    def __init__(self, width: _Optional[int] = ..., height: _Optional[int] = ..., surface_animator: _Optional[_Union[_surfaceanimator_pb2.SurfaceAnimatorProto, _Mapping]] = ...) -> None: ...
