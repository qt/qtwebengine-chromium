from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.proto_logging.stats.enums.app_shared import app_enums_pb2 as _app_enums_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ApplicationExitInfoProto(_message.Message):
    __slots__ = ("pid", "real_uid", "package_uid", "defining_uid", "process_name", "connection_group", "reason", "sub_reason", "status", "importance", "pss", "rss", "timestamp", "description", "state", "trace_file")
    PID_FIELD_NUMBER: _ClassVar[int]
    REAL_UID_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_UID_FIELD_NUMBER: _ClassVar[int]
    DEFINING_UID_FIELD_NUMBER: _ClassVar[int]
    PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
    CONNECTION_GROUP_FIELD_NUMBER: _ClassVar[int]
    REASON_FIELD_NUMBER: _ClassVar[int]
    SUB_REASON_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    IMPORTANCE_FIELD_NUMBER: _ClassVar[int]
    PSS_FIELD_NUMBER: _ClassVar[int]
    RSS_FIELD_NUMBER: _ClassVar[int]
    TIMESTAMP_FIELD_NUMBER: _ClassVar[int]
    DESCRIPTION_FIELD_NUMBER: _ClassVar[int]
    STATE_FIELD_NUMBER: _ClassVar[int]
    TRACE_FILE_FIELD_NUMBER: _ClassVar[int]
    pid: int
    real_uid: int
    package_uid: int
    defining_uid: int
    process_name: str
    connection_group: int
    reason: _app_enums_pb2.AppExitReasonCode
    sub_reason: _app_enums_pb2.AppExitSubReasonCode
    status: int
    importance: _app_enums_pb2.Importance
    pss: int
    rss: int
    timestamp: int
    description: str
    state: bytes
    trace_file: str
    def __init__(self, pid: _Optional[int] = ..., real_uid: _Optional[int] = ..., package_uid: _Optional[int] = ..., defining_uid: _Optional[int] = ..., process_name: _Optional[str] = ..., connection_group: _Optional[int] = ..., reason: _Optional[_Union[_app_enums_pb2.AppExitReasonCode, str]] = ..., sub_reason: _Optional[_Union[_app_enums_pb2.AppExitSubReasonCode, str]] = ..., status: _Optional[int] = ..., importance: _Optional[_Union[_app_enums_pb2.Importance, str]] = ..., pss: _Optional[int] = ..., rss: _Optional[int] = ..., timestamp: _Optional[int] = ..., description: _Optional[str] = ..., state: _Optional[bytes] = ..., trace_file: _Optional[str] = ...) -> None: ...
