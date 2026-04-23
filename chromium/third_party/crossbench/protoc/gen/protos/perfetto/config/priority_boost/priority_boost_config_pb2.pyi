from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PriorityBoostConfig(_message.Message):
    __slots__ = ("policy", "priority")
    class BoostPolicy(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        POLICY_UNSPECIFIED: _ClassVar[PriorityBoostConfig.BoostPolicy]
        POLICY_SCHED_OTHER: _ClassVar[PriorityBoostConfig.BoostPolicy]
        POLICY_SCHED_FIFO: _ClassVar[PriorityBoostConfig.BoostPolicy]
    POLICY_UNSPECIFIED: PriorityBoostConfig.BoostPolicy
    POLICY_SCHED_OTHER: PriorityBoostConfig.BoostPolicy
    POLICY_SCHED_FIFO: PriorityBoostConfig.BoostPolicy
    POLICY_FIELD_NUMBER: _ClassVar[int]
    PRIORITY_FIELD_NUMBER: _ClassVar[int]
    policy: PriorityBoostConfig.BoostPolicy
    priority: int
    def __init__(self, policy: _Optional[_Union[PriorityBoostConfig.BoostPolicy, str]] = ..., priority: _Optional[int] = ...) -> None: ...
