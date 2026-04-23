from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class PixelFormatProto(_message.Message):
    __slots__ = ()
    class Format(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNKNOWN: _ClassVar[PixelFormatProto.Format]
        TRANSLUCENT: _ClassVar[PixelFormatProto.Format]
        TRANSPARENT: _ClassVar[PixelFormatProto.Format]
        OPAQUE: _ClassVar[PixelFormatProto.Format]
        RGBA_8888: _ClassVar[PixelFormatProto.Format]
        RGBX_8888: _ClassVar[PixelFormatProto.Format]
        RGB_888: _ClassVar[PixelFormatProto.Format]
        RGB_565: _ClassVar[PixelFormatProto.Format]
        RGBA_F16: _ClassVar[PixelFormatProto.Format]
        RGBA_1010102: _ClassVar[PixelFormatProto.Format]
    UNKNOWN: PixelFormatProto.Format
    TRANSLUCENT: PixelFormatProto.Format
    TRANSPARENT: PixelFormatProto.Format
    OPAQUE: PixelFormatProto.Format
    RGBA_8888: PixelFormatProto.Format
    RGBX_8888: PixelFormatProto.Format
    RGB_888: PixelFormatProto.Format
    RGB_565: PixelFormatProto.Format
    RGBA_F16: PixelFormatProto.Format
    RGBA_1010102: PixelFormatProto.Format
    def __init__(self) -> None: ...
