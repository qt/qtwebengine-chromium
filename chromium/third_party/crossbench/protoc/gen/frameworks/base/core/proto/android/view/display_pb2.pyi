from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class DisplayProto(_message.Message):
    __slots__ = ()
    class ColorMode(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        COLOR_MODE_INVALID: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_DEFAULT: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_BT601_625: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_BT601_625_UNADJUSTED: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_BT601_525: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_BT601_525_UNADJUSTED: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_BT709: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_DCI_P3: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_SRGB: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_ADOBE_RGB: _ClassVar[DisplayProto.ColorMode]
        COLOR_MODE_DISPLAY_P3: _ClassVar[DisplayProto.ColorMode]
    COLOR_MODE_INVALID: DisplayProto.ColorMode
    COLOR_MODE_DEFAULT: DisplayProto.ColorMode
    COLOR_MODE_BT601_625: DisplayProto.ColorMode
    COLOR_MODE_BT601_625_UNADJUSTED: DisplayProto.ColorMode
    COLOR_MODE_BT601_525: DisplayProto.ColorMode
    COLOR_MODE_BT601_525_UNADJUSTED: DisplayProto.ColorMode
    COLOR_MODE_BT709: DisplayProto.ColorMode
    COLOR_MODE_DCI_P3: DisplayProto.ColorMode
    COLOR_MODE_SRGB: DisplayProto.ColorMode
    COLOR_MODE_ADOBE_RGB: DisplayProto.ColorMode
    COLOR_MODE_DISPLAY_P3: DisplayProto.ColorMode
    def __init__(self) -> None: ...
