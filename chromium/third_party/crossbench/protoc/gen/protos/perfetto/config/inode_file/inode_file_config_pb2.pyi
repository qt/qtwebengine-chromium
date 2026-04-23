from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class InodeFileConfig(_message.Message):
    __slots__ = ("scan_interval_ms", "scan_delay_ms", "scan_batch_size", "do_not_scan", "scan_mount_points", "mount_point_mapping")
    class MountPointMappingEntry(_message.Message):
        __slots__ = ("mountpoint", "scan_roots")
        MOUNTPOINT_FIELD_NUMBER: _ClassVar[int]
        SCAN_ROOTS_FIELD_NUMBER: _ClassVar[int]
        mountpoint: str
        scan_roots: _containers.RepeatedScalarFieldContainer[str]
        def __init__(self, mountpoint: _Optional[str] = ..., scan_roots: _Optional[_Iterable[str]] = ...) -> None: ...
    SCAN_INTERVAL_MS_FIELD_NUMBER: _ClassVar[int]
    SCAN_DELAY_MS_FIELD_NUMBER: _ClassVar[int]
    SCAN_BATCH_SIZE_FIELD_NUMBER: _ClassVar[int]
    DO_NOT_SCAN_FIELD_NUMBER: _ClassVar[int]
    SCAN_MOUNT_POINTS_FIELD_NUMBER: _ClassVar[int]
    MOUNT_POINT_MAPPING_FIELD_NUMBER: _ClassVar[int]
    scan_interval_ms: int
    scan_delay_ms: int
    scan_batch_size: int
    do_not_scan: bool
    scan_mount_points: _containers.RepeatedScalarFieldContainer[str]
    mount_point_mapping: _containers.RepeatedCompositeFieldContainer[InodeFileConfig.MountPointMappingEntry]
    def __init__(self, scan_interval_ms: _Optional[int] = ..., scan_delay_ms: _Optional[int] = ..., scan_batch_size: _Optional[int] = ..., do_not_scan: _Optional[bool] = ..., scan_mount_points: _Optional[_Iterable[str]] = ..., mount_point_mapping: _Optional[_Iterable[_Union[InodeFileConfig.MountPointMappingEntry, _Mapping]]] = ...) -> None: ...
