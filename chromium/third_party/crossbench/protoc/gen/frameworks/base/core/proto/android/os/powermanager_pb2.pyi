from frameworks.base.core.proto.android.os import worksource_pb2 as _worksource_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PowerManagerProto(_message.Message):
    __slots__ = ()
    class UserActivityEvent(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        USER_ACTIVITY_EVENT_OTHER: _ClassVar[PowerManagerProto.UserActivityEvent]
        USER_ACTIVITY_EVENT_BUTTON: _ClassVar[PowerManagerProto.UserActivityEvent]
        USER_ACTIVITY_EVENT_TOUCH: _ClassVar[PowerManagerProto.UserActivityEvent]
        USER_ACTIVITY_EVENT_ACCESSIBILITY: _ClassVar[PowerManagerProto.UserActivityEvent]
    USER_ACTIVITY_EVENT_OTHER: PowerManagerProto.UserActivityEvent
    USER_ACTIVITY_EVENT_BUTTON: PowerManagerProto.UserActivityEvent
    USER_ACTIVITY_EVENT_TOUCH: PowerManagerProto.UserActivityEvent
    USER_ACTIVITY_EVENT_ACCESSIBILITY: PowerManagerProto.UserActivityEvent
    class WakeLock(_message.Message):
        __slots__ = ("tag", "package_name", "held", "internal_count", "work_source")
        TAG_FIELD_NUMBER: _ClassVar[int]
        PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
        HELD_FIELD_NUMBER: _ClassVar[int]
        INTERNAL_COUNT_FIELD_NUMBER: _ClassVar[int]
        WORK_SOURCE_FIELD_NUMBER: _ClassVar[int]
        tag: str
        package_name: str
        held: bool
        internal_count: int
        work_source: _worksource_pb2.WorkSourceProto
        def __init__(self, tag: _Optional[str] = ..., package_name: _Optional[str] = ..., held: _Optional[bool] = ..., internal_count: _Optional[int] = ..., work_source: _Optional[_Union[_worksource_pb2.WorkSourceProto, _Mapping]] = ...) -> None: ...
    def __init__(self) -> None: ...

class PowerManagerInternalProto(_message.Message):
    __slots__ = ()
    class Wakefulness(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        WAKEFULNESS_ASLEEP: _ClassVar[PowerManagerInternalProto.Wakefulness]
        WAKEFULNESS_AWAKE: _ClassVar[PowerManagerInternalProto.Wakefulness]
        WAKEFULNESS_DREAMING: _ClassVar[PowerManagerInternalProto.Wakefulness]
        WAKEFULNESS_DOZING: _ClassVar[PowerManagerInternalProto.Wakefulness]
    WAKEFULNESS_ASLEEP: PowerManagerInternalProto.Wakefulness
    WAKEFULNESS_AWAKE: PowerManagerInternalProto.Wakefulness
    WAKEFULNESS_DREAMING: PowerManagerInternalProto.Wakefulness
    WAKEFULNESS_DOZING: PowerManagerInternalProto.Wakefulness
    def __init__(self) -> None: ...
