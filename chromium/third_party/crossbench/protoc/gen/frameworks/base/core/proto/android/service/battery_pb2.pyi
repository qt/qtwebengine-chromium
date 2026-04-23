from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from frameworks.proto_logging.stats.enums.os import enums_pb2 as _enums_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class BatteryServiceDumpProto(_message.Message):
    __slots__ = ("are_updates_stopped", "plugged", "max_charging_current", "max_charging_voltage", "charge_counter", "status", "health", "is_present", "level", "scale", "voltage", "temperature", "technology")
    ARE_UPDATES_STOPPED_FIELD_NUMBER: _ClassVar[int]
    PLUGGED_FIELD_NUMBER: _ClassVar[int]
    MAX_CHARGING_CURRENT_FIELD_NUMBER: _ClassVar[int]
    MAX_CHARGING_VOLTAGE_FIELD_NUMBER: _ClassVar[int]
    CHARGE_COUNTER_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    HEALTH_FIELD_NUMBER: _ClassVar[int]
    IS_PRESENT_FIELD_NUMBER: _ClassVar[int]
    LEVEL_FIELD_NUMBER: _ClassVar[int]
    SCALE_FIELD_NUMBER: _ClassVar[int]
    VOLTAGE_FIELD_NUMBER: _ClassVar[int]
    TEMPERATURE_FIELD_NUMBER: _ClassVar[int]
    TECHNOLOGY_FIELD_NUMBER: _ClassVar[int]
    are_updates_stopped: bool
    plugged: _enums_pb2.BatteryPluggedStateEnum
    max_charging_current: int
    max_charging_voltage: int
    charge_counter: int
    status: _enums_pb2.BatteryStatusEnum
    health: _enums_pb2.BatteryHealthEnum
    is_present: bool
    level: int
    scale: int
    voltage: int
    temperature: int
    technology: str
    def __init__(self, are_updates_stopped: _Optional[bool] = ..., plugged: _Optional[_Union[_enums_pb2.BatteryPluggedStateEnum, str]] = ..., max_charging_current: _Optional[int] = ..., max_charging_voltage: _Optional[int] = ..., charge_counter: _Optional[int] = ..., status: _Optional[_Union[_enums_pb2.BatteryStatusEnum, str]] = ..., health: _Optional[_Union[_enums_pb2.BatteryHealthEnum, str]] = ..., is_present: _Optional[bool] = ..., level: _Optional[int] = ..., scale: _Optional[int] = ..., voltage: _Optional[int] = ..., temperature: _Optional[int] = ..., technology: _Optional[str] = ...) -> None: ...
