from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class AndroidPowerConfig(_message.Message):
    __slots__ = ("battery_poll_ms", "battery_counters", "collect_power_rails", "collect_energy_estimation_breakdown", "collect_entity_state_residency")
    class BatteryCounters(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        BATTERY_COUNTER_UNSPECIFIED: _ClassVar[AndroidPowerConfig.BatteryCounters]
        BATTERY_COUNTER_CHARGE: _ClassVar[AndroidPowerConfig.BatteryCounters]
        BATTERY_COUNTER_CAPACITY_PERCENT: _ClassVar[AndroidPowerConfig.BatteryCounters]
        BATTERY_COUNTER_CURRENT: _ClassVar[AndroidPowerConfig.BatteryCounters]
        BATTERY_COUNTER_CURRENT_AVG: _ClassVar[AndroidPowerConfig.BatteryCounters]
        BATTERY_COUNTER_VOLTAGE: _ClassVar[AndroidPowerConfig.BatteryCounters]
    BATTERY_COUNTER_UNSPECIFIED: AndroidPowerConfig.BatteryCounters
    BATTERY_COUNTER_CHARGE: AndroidPowerConfig.BatteryCounters
    BATTERY_COUNTER_CAPACITY_PERCENT: AndroidPowerConfig.BatteryCounters
    BATTERY_COUNTER_CURRENT: AndroidPowerConfig.BatteryCounters
    BATTERY_COUNTER_CURRENT_AVG: AndroidPowerConfig.BatteryCounters
    BATTERY_COUNTER_VOLTAGE: AndroidPowerConfig.BatteryCounters
    BATTERY_POLL_MS_FIELD_NUMBER: _ClassVar[int]
    BATTERY_COUNTERS_FIELD_NUMBER: _ClassVar[int]
    COLLECT_POWER_RAILS_FIELD_NUMBER: _ClassVar[int]
    COLLECT_ENERGY_ESTIMATION_BREAKDOWN_FIELD_NUMBER: _ClassVar[int]
    COLLECT_ENTITY_STATE_RESIDENCY_FIELD_NUMBER: _ClassVar[int]
    battery_poll_ms: int
    battery_counters: _containers.RepeatedScalarFieldContainer[AndroidPowerConfig.BatteryCounters]
    collect_power_rails: bool
    collect_energy_estimation_breakdown: bool
    collect_entity_state_residency: bool
    def __init__(self, battery_poll_ms: _Optional[int] = ..., battery_counters: _Optional[_Iterable[_Union[AndroidPowerConfig.BatteryCounters, str]]] = ..., collect_power_rails: _Optional[bool] = ..., collect_energy_estimation_breakdown: _Optional[bool] = ..., collect_entity_state_residency: _Optional[bool] = ...) -> None: ...
