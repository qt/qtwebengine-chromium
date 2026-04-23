from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class StatusBarManagerProto(_message.Message):
    __slots__ = ()
    class WindowState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        WINDOW_STATE_SHOWING: _ClassVar[StatusBarManagerProto.WindowState]
        WINDOW_STATE_HIDING: _ClassVar[StatusBarManagerProto.WindowState]
        WINDOW_STATE_HIDDEN: _ClassVar[StatusBarManagerProto.WindowState]
    WINDOW_STATE_SHOWING: StatusBarManagerProto.WindowState
    WINDOW_STATE_HIDING: StatusBarManagerProto.WindowState
    WINDOW_STATE_HIDDEN: StatusBarManagerProto.WindowState
    class TransientWindowState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        TRANSIENT_BAR_NONE: _ClassVar[StatusBarManagerProto.TransientWindowState]
        TRANSIENT_BAR_SHOW_REQUESTED: _ClassVar[StatusBarManagerProto.TransientWindowState]
        TRANSIENT_BAR_SHOWING: _ClassVar[StatusBarManagerProto.TransientWindowState]
        TRANSIENT_BAR_HIDING: _ClassVar[StatusBarManagerProto.TransientWindowState]
    TRANSIENT_BAR_NONE: StatusBarManagerProto.TransientWindowState
    TRANSIENT_BAR_SHOW_REQUESTED: StatusBarManagerProto.TransientWindowState
    TRANSIENT_BAR_SHOWING: StatusBarManagerProto.TransientWindowState
    TRANSIENT_BAR_HIDING: StatusBarManagerProto.TransientWindowState
    def __init__(self) -> None: ...
