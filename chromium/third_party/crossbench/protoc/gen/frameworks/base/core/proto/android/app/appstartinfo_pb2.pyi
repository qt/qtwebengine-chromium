from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.proto_logging.stats.enums.app_shared import app_enums_pb2 as _app_enums_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ApplicationStartInfoProto(_message.Message):
    __slots__ = ("pid", "real_uid", "package_uid", "defining_uid", "process_name", "startup_state", "reason", "startup_timestamps", "start_type", "start_intent", "launch_mode", "was_force_stopped", "monotonic_creation_time_ms", "start_component")
    PID_FIELD_NUMBER: _ClassVar[int]
    REAL_UID_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_UID_FIELD_NUMBER: _ClassVar[int]
    DEFINING_UID_FIELD_NUMBER: _ClassVar[int]
    PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
    STARTUP_STATE_FIELD_NUMBER: _ClassVar[int]
    REASON_FIELD_NUMBER: _ClassVar[int]
    STARTUP_TIMESTAMPS_FIELD_NUMBER: _ClassVar[int]
    START_TYPE_FIELD_NUMBER: _ClassVar[int]
    START_INTENT_FIELD_NUMBER: _ClassVar[int]
    LAUNCH_MODE_FIELD_NUMBER: _ClassVar[int]
    WAS_FORCE_STOPPED_FIELD_NUMBER: _ClassVar[int]
    MONOTONIC_CREATION_TIME_MS_FIELD_NUMBER: _ClassVar[int]
    START_COMPONENT_FIELD_NUMBER: _ClassVar[int]
    pid: int
    real_uid: int
    package_uid: int
    defining_uid: int
    process_name: str
    startup_state: _app_enums_pb2.AppStartStartupState
    reason: _app_enums_pb2.AppStartReasonCode
    startup_timestamps: bytes
    start_type: _app_enums_pb2.AppStartStartType
    start_intent: bytes
    launch_mode: _app_enums_pb2.AppStartLaunchMode
    was_force_stopped: bool
    monotonic_creation_time_ms: int
    start_component: int
    def __init__(self, pid: _Optional[int] = ..., real_uid: _Optional[int] = ..., package_uid: _Optional[int] = ..., defining_uid: _Optional[int] = ..., process_name: _Optional[str] = ..., startup_state: _Optional[_Union[_app_enums_pb2.AppStartStartupState, str]] = ..., reason: _Optional[_Union[_app_enums_pb2.AppStartReasonCode, str]] = ..., startup_timestamps: _Optional[bytes] = ..., start_type: _Optional[_Union[_app_enums_pb2.AppStartStartType, str]] = ..., start_intent: _Optional[bytes] = ..., launch_mode: _Optional[_Union[_app_enums_pb2.AppStartLaunchMode, str]] = ..., was_force_stopped: _Optional[bool] = ..., monotonic_creation_time_ms: _Optional[int] = ..., start_component: _Optional[int] = ...) -> None: ...
