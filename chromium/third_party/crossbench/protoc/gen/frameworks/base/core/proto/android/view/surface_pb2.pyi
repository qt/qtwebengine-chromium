from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class SurfaceProto(_message.Message):
    __slots__ = ()
    class Rotation(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        ROTATION_0: _ClassVar[SurfaceProto.Rotation]
        ROTATION_90: _ClassVar[SurfaceProto.Rotation]
        ROTATION_180: _ClassVar[SurfaceProto.Rotation]
        ROTATION_270: _ClassVar[SurfaceProto.Rotation]
    ROTATION_0: SurfaceProto.Rotation
    ROTATION_90: SurfaceProto.Rotation
    ROTATION_180: SurfaceProto.Rotation
    ROTATION_270: SurfaceProto.Rotation
    def __init__(self) -> None: ...
