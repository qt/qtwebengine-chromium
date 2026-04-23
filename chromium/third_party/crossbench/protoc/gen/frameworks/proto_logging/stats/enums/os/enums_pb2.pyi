from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class BackportedFixStatus(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BACKPORTED_FIX_STATUS_UNKNOWN: _ClassVar[BackportedFixStatus]
    BACKPORTED_FIX_STATUS_FIXED: _ClassVar[BackportedFixStatus]
    BACKPORTED_FIX_STATUS_NOT_APPLICABLE: _ClassVar[BackportedFixStatus]
    BACKPORTED_FIX_STATUS_NOT_FIXED: _ClassVar[BackportedFixStatus]

class BatteryHealthEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BATTERY_HEALTH_INVALID: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_UNKNOWN: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_GOOD: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_OVERHEAT: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_DEAD: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_OVER_VOLTAGE: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_UNSPECIFIED_FAILURE: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_COLD: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_FAIR: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_NOT_AVAILABLE: _ClassVar[BatteryHealthEnum]
    BATTERY_HEALTH_INCONSISTENT: _ClassVar[BatteryHealthEnum]

class BatteryPluggedStateEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BATTERY_PLUGGED_NONE: _ClassVar[BatteryPluggedStateEnum]
    BATTERY_PLUGGED_AC: _ClassVar[BatteryPluggedStateEnum]
    BATTERY_PLUGGED_USB: _ClassVar[BatteryPluggedStateEnum]
    BATTERY_PLUGGED_WIRELESS: _ClassVar[BatteryPluggedStateEnum]
    BATTERY_PLUGGED_DOCK: _ClassVar[BatteryPluggedStateEnum]

class BatteryStatusEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BATTERY_STATUS_INVALID: _ClassVar[BatteryStatusEnum]
    BATTERY_STATUS_UNKNOWN: _ClassVar[BatteryStatusEnum]
    BATTERY_STATUS_CHARGING: _ClassVar[BatteryStatusEnum]
    BATTERY_STATUS_DISCHARGING: _ClassVar[BatteryStatusEnum]
    BATTERY_STATUS_NOT_CHARGING: _ClassVar[BatteryStatusEnum]
    BATTERY_STATUS_FULL: _ClassVar[BatteryStatusEnum]

class PowerComponentEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    POWER_COMPONENT_SCREEN: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_CPU: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_BLUETOOTH: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_CAMERA: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_AUDIO: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_VIDEO: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_FLASHLIGHT: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_SYSTEM_SERVICES: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_MOBILE_RADIO: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_SENSORS: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_GNSS: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_WIFI: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_WAKELOCK: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_MEMORY: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_PHONE: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_AMBIENT_DISPLAY: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_IDLE: _ClassVar[PowerComponentEnum]
    POWER_COMPONENT_REATTRIBUTED_TO_OTHER_CONSUMERS: _ClassVar[PowerComponentEnum]

class TemperatureTypeEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TEMPERATURE_TYPE_UNKNOWN: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_CPU: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_GPU: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_BATTERY: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_SKIN: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_USB_PORT: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_POWER_AMPLIFIER: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_BCL_VOLTAGE: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_BCL_CURRENT: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_BCL_PERCENTAGE: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_NPU: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_TPU: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_DISPLAY: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_MODEM: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_SOC: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_WIFI: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_CAMERA: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_FLASHLIGHT: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_SPEAKER: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_AMBIENT: _ClassVar[TemperatureTypeEnum]
    TEMPERATURE_TYPE_POGO: _ClassVar[TemperatureTypeEnum]

class ThrottlingSeverityEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    NONE: _ClassVar[ThrottlingSeverityEnum]
    LIGHT: _ClassVar[ThrottlingSeverityEnum]
    MODERATE: _ClassVar[ThrottlingSeverityEnum]
    SEVERE: _ClassVar[ThrottlingSeverityEnum]
    CRITICAL: _ClassVar[ThrottlingSeverityEnum]
    EMERGENCY: _ClassVar[ThrottlingSeverityEnum]
    SHUTDOWN: _ClassVar[ThrottlingSeverityEnum]

class CoolingTypeEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    FAN: _ClassVar[CoolingTypeEnum]
    BATTERY: _ClassVar[CoolingTypeEnum]
    CPU: _ClassVar[CoolingTypeEnum]
    GPU: _ClassVar[CoolingTypeEnum]
    MODEM: _ClassVar[CoolingTypeEnum]
    NPU: _ClassVar[CoolingTypeEnum]
    COMPONENT: _ClassVar[CoolingTypeEnum]
    TPU: _ClassVar[CoolingTypeEnum]
    POWER_AMPLIFIER: _ClassVar[CoolingTypeEnum]
    DISPLAY: _ClassVar[CoolingTypeEnum]
    SPEAKER: _ClassVar[CoolingTypeEnum]
    WIFI: _ClassVar[CoolingTypeEnum]
    CAMERA: _ClassVar[CoolingTypeEnum]
    FLASHLIGHT: _ClassVar[CoolingTypeEnum]
    USB_PORT: _ClassVar[CoolingTypeEnum]

class WakeLockLevelEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    PARTIAL_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    SCREEN_DIM_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    SCREEN_BRIGHT_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    FULL_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    PROXIMITY_SCREEN_OFF_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    DOZE_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    DRAW_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]
    SCREEN_TIMEOUT_OVERRIDE_WAKE_LOCK: _ClassVar[WakeLockLevelEnum]

class BatteryChargingStatusEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    BATTERY_STATUS_NORMAL: _ClassVar[BatteryChargingStatusEnum]
    BATTERY_STATUS_TOO_COLD: _ClassVar[BatteryChargingStatusEnum]
    BATTERY_STATUS_TOO_HOT: _ClassVar[BatteryChargingStatusEnum]
    BATTERY_STATUS_LONG_LIFE: _ClassVar[BatteryChargingStatusEnum]
    BATTERY_STATUS_ADAPTIVE: _ClassVar[BatteryChargingStatusEnum]

class BatteryChargingPolicyEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    CHARGING_POLICY_DEFAULT: _ClassVar[BatteryChargingPolicyEnum]
    CHARGING_POLICY_ADAPTIVE_AON: _ClassVar[BatteryChargingPolicyEnum]
    CHARGING_POLICY_ADAPTIVE_AC: _ClassVar[BatteryChargingPolicyEnum]
    CHARGING_POLICY_ADAPTIVE_LONGLIFE: _ClassVar[BatteryChargingPolicyEnum]
BACKPORTED_FIX_STATUS_UNKNOWN: BackportedFixStatus
BACKPORTED_FIX_STATUS_FIXED: BackportedFixStatus
BACKPORTED_FIX_STATUS_NOT_APPLICABLE: BackportedFixStatus
BACKPORTED_FIX_STATUS_NOT_FIXED: BackportedFixStatus
BATTERY_HEALTH_INVALID: BatteryHealthEnum
BATTERY_HEALTH_UNKNOWN: BatteryHealthEnum
BATTERY_HEALTH_GOOD: BatteryHealthEnum
BATTERY_HEALTH_OVERHEAT: BatteryHealthEnum
BATTERY_HEALTH_DEAD: BatteryHealthEnum
BATTERY_HEALTH_OVER_VOLTAGE: BatteryHealthEnum
BATTERY_HEALTH_UNSPECIFIED_FAILURE: BatteryHealthEnum
BATTERY_HEALTH_COLD: BatteryHealthEnum
BATTERY_HEALTH_FAIR: BatteryHealthEnum
BATTERY_HEALTH_NOT_AVAILABLE: BatteryHealthEnum
BATTERY_HEALTH_INCONSISTENT: BatteryHealthEnum
BATTERY_PLUGGED_NONE: BatteryPluggedStateEnum
BATTERY_PLUGGED_AC: BatteryPluggedStateEnum
BATTERY_PLUGGED_USB: BatteryPluggedStateEnum
BATTERY_PLUGGED_WIRELESS: BatteryPluggedStateEnum
BATTERY_PLUGGED_DOCK: BatteryPluggedStateEnum
BATTERY_STATUS_INVALID: BatteryStatusEnum
BATTERY_STATUS_UNKNOWN: BatteryStatusEnum
BATTERY_STATUS_CHARGING: BatteryStatusEnum
BATTERY_STATUS_DISCHARGING: BatteryStatusEnum
BATTERY_STATUS_NOT_CHARGING: BatteryStatusEnum
BATTERY_STATUS_FULL: BatteryStatusEnum
POWER_COMPONENT_SCREEN: PowerComponentEnum
POWER_COMPONENT_CPU: PowerComponentEnum
POWER_COMPONENT_BLUETOOTH: PowerComponentEnum
POWER_COMPONENT_CAMERA: PowerComponentEnum
POWER_COMPONENT_AUDIO: PowerComponentEnum
POWER_COMPONENT_VIDEO: PowerComponentEnum
POWER_COMPONENT_FLASHLIGHT: PowerComponentEnum
POWER_COMPONENT_SYSTEM_SERVICES: PowerComponentEnum
POWER_COMPONENT_MOBILE_RADIO: PowerComponentEnum
POWER_COMPONENT_SENSORS: PowerComponentEnum
POWER_COMPONENT_GNSS: PowerComponentEnum
POWER_COMPONENT_WIFI: PowerComponentEnum
POWER_COMPONENT_WAKELOCK: PowerComponentEnum
POWER_COMPONENT_MEMORY: PowerComponentEnum
POWER_COMPONENT_PHONE: PowerComponentEnum
POWER_COMPONENT_AMBIENT_DISPLAY: PowerComponentEnum
POWER_COMPONENT_IDLE: PowerComponentEnum
POWER_COMPONENT_REATTRIBUTED_TO_OTHER_CONSUMERS: PowerComponentEnum
TEMPERATURE_TYPE_UNKNOWN: TemperatureTypeEnum
TEMPERATURE_TYPE_CPU: TemperatureTypeEnum
TEMPERATURE_TYPE_GPU: TemperatureTypeEnum
TEMPERATURE_TYPE_BATTERY: TemperatureTypeEnum
TEMPERATURE_TYPE_SKIN: TemperatureTypeEnum
TEMPERATURE_TYPE_USB_PORT: TemperatureTypeEnum
TEMPERATURE_TYPE_POWER_AMPLIFIER: TemperatureTypeEnum
TEMPERATURE_TYPE_BCL_VOLTAGE: TemperatureTypeEnum
TEMPERATURE_TYPE_BCL_CURRENT: TemperatureTypeEnum
TEMPERATURE_TYPE_BCL_PERCENTAGE: TemperatureTypeEnum
TEMPERATURE_TYPE_NPU: TemperatureTypeEnum
TEMPERATURE_TYPE_TPU: TemperatureTypeEnum
TEMPERATURE_TYPE_DISPLAY: TemperatureTypeEnum
TEMPERATURE_TYPE_MODEM: TemperatureTypeEnum
TEMPERATURE_TYPE_SOC: TemperatureTypeEnum
TEMPERATURE_TYPE_WIFI: TemperatureTypeEnum
TEMPERATURE_TYPE_CAMERA: TemperatureTypeEnum
TEMPERATURE_TYPE_FLASHLIGHT: TemperatureTypeEnum
TEMPERATURE_TYPE_SPEAKER: TemperatureTypeEnum
TEMPERATURE_TYPE_AMBIENT: TemperatureTypeEnum
TEMPERATURE_TYPE_POGO: TemperatureTypeEnum
NONE: ThrottlingSeverityEnum
LIGHT: ThrottlingSeverityEnum
MODERATE: ThrottlingSeverityEnum
SEVERE: ThrottlingSeverityEnum
CRITICAL: ThrottlingSeverityEnum
EMERGENCY: ThrottlingSeverityEnum
SHUTDOWN: ThrottlingSeverityEnum
FAN: CoolingTypeEnum
BATTERY: CoolingTypeEnum
CPU: CoolingTypeEnum
GPU: CoolingTypeEnum
MODEM: CoolingTypeEnum
NPU: CoolingTypeEnum
COMPONENT: CoolingTypeEnum
TPU: CoolingTypeEnum
POWER_AMPLIFIER: CoolingTypeEnum
DISPLAY: CoolingTypeEnum
SPEAKER: CoolingTypeEnum
WIFI: CoolingTypeEnum
CAMERA: CoolingTypeEnum
FLASHLIGHT: CoolingTypeEnum
USB_PORT: CoolingTypeEnum
PARTIAL_WAKE_LOCK: WakeLockLevelEnum
SCREEN_DIM_WAKE_LOCK: WakeLockLevelEnum
SCREEN_BRIGHT_WAKE_LOCK: WakeLockLevelEnum
FULL_WAKE_LOCK: WakeLockLevelEnum
PROXIMITY_SCREEN_OFF_WAKE_LOCK: WakeLockLevelEnum
DOZE_WAKE_LOCK: WakeLockLevelEnum
DRAW_WAKE_LOCK: WakeLockLevelEnum
SCREEN_TIMEOUT_OVERRIDE_WAKE_LOCK: WakeLockLevelEnum
BATTERY_STATUS_NORMAL: BatteryChargingStatusEnum
BATTERY_STATUS_TOO_COLD: BatteryChargingStatusEnum
BATTERY_STATUS_TOO_HOT: BatteryChargingStatusEnum
BATTERY_STATUS_LONG_LIFE: BatteryChargingStatusEnum
BATTERY_STATUS_ADAPTIVE: BatteryChargingStatusEnum
CHARGING_POLICY_DEFAULT: BatteryChargingPolicyEnum
CHARGING_POLICY_ADAPTIVE_AON: BatteryChargingPolicyEnum
CHARGING_POLICY_ADAPTIVE_AC: BatteryChargingPolicyEnum
CHARGING_POLICY_ADAPTIVE_LONGLIFE: BatteryChargingPolicyEnum
