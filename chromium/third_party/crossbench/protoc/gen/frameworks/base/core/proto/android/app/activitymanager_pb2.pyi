from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class UidObserverFlag(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    UID_OBSERVER_FLAG_PROCSTATE: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_GONE: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_IDLE: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_ACTIVE: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_CACHED: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_CAPABILITY: _ClassVar[UidObserverFlag]
    UID_OBSERVER_FLAG_PROC_OOM_ADJ: _ClassVar[UidObserverFlag]
UID_OBSERVER_FLAG_PROCSTATE: UidObserverFlag
UID_OBSERVER_FLAG_GONE: UidObserverFlag
UID_OBSERVER_FLAG_IDLE: UidObserverFlag
UID_OBSERVER_FLAG_ACTIVE: UidObserverFlag
UID_OBSERVER_FLAG_CACHED: UidObserverFlag
UID_OBSERVER_FLAG_CAPABILITY: UidObserverFlag
UID_OBSERVER_FLAG_PROC_OOM_ADJ: UidObserverFlag
