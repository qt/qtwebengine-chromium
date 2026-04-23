from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from typing import ClassVar as _ClassVar

DESCRIPTOR: _descriptor.FileDescriptor

class DisplayStateEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    DISPLAY_STATE_UNKNOWN: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_OFF: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_ON: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_DOZE: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_DOZE_SUSPEND: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_VR: _ClassVar[DisplayStateEnum]
    DISPLAY_STATE_ON_SUSPEND: _ClassVar[DisplayStateEnum]

class DisplayStateReason(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    DISPLAY_STATE_REASON_UNKNOWN: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_DEFAULT_POLICY: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_DRAW_WAKE_LOCK: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_OFFLOAD: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_TILT: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_DREAM_MANAGER: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_KEY: _ClassVar[DisplayStateReason]
    DISPLAY_STATE_REASON_MOTION: _ClassVar[DisplayStateReason]

class TransitionTypeEnum(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = ()
    TRANSIT_NONE: _ClassVar[TransitionTypeEnum]
    TRANSIT_UNSET: _ClassVar[TransitionTypeEnum]
    TRANSIT_ACTIVITY_OPEN: _ClassVar[TransitionTypeEnum]
    TRANSIT_ACTIVITY_CLOSE: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_OPEN: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_CLOSE: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_TO_FRONT: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_TO_BACK: _ClassVar[TransitionTypeEnum]
    TRANSIT_WALLPAPER_CLOSE: _ClassVar[TransitionTypeEnum]
    TRANSIT_WALLPAPER_OPEN: _ClassVar[TransitionTypeEnum]
    TRANSIT_WALLPAPER_INTRA_OPEN: _ClassVar[TransitionTypeEnum]
    TRANSIT_WALLPAPER_INTRA_CLOSE: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_OPEN_BEHIND: _ClassVar[TransitionTypeEnum]
    TRANSIT_TASK_IN_PLACE: _ClassVar[TransitionTypeEnum]
    TRANSIT_ACTIVITY_RELAUNCH: _ClassVar[TransitionTypeEnum]
    TRANSIT_DOCK_TASK_FROM_RECENTS: _ClassVar[TransitionTypeEnum]
    TRANSIT_KEYGUARD_GOING_AWAY: _ClassVar[TransitionTypeEnum]
    TRANSIT_KEYGUARD_GOING_AWAY_ON_WALLPAPER: _ClassVar[TransitionTypeEnum]
    TRANSIT_KEYGUARD_OCCLUDE: _ClassVar[TransitionTypeEnum]
    TRANSIT_KEYGUARD_UNOCCLUDE: _ClassVar[TransitionTypeEnum]
    TRANSIT_TRANSLUCENT_ACTIVITY_OPEN: _ClassVar[TransitionTypeEnum]
    TRANSIT_TRANSLUCENT_ACTIVITY_CLOSE: _ClassVar[TransitionTypeEnum]
    TRANSIT_CRASHING_ACTIVITY_CLOSE: _ClassVar[TransitionTypeEnum]
DISPLAY_STATE_UNKNOWN: DisplayStateEnum
DISPLAY_STATE_OFF: DisplayStateEnum
DISPLAY_STATE_ON: DisplayStateEnum
DISPLAY_STATE_DOZE: DisplayStateEnum
DISPLAY_STATE_DOZE_SUSPEND: DisplayStateEnum
DISPLAY_STATE_VR: DisplayStateEnum
DISPLAY_STATE_ON_SUSPEND: DisplayStateEnum
DISPLAY_STATE_REASON_UNKNOWN: DisplayStateReason
DISPLAY_STATE_REASON_DEFAULT_POLICY: DisplayStateReason
DISPLAY_STATE_REASON_DRAW_WAKE_LOCK: DisplayStateReason
DISPLAY_STATE_REASON_OFFLOAD: DisplayStateReason
DISPLAY_STATE_REASON_TILT: DisplayStateReason
DISPLAY_STATE_REASON_DREAM_MANAGER: DisplayStateReason
DISPLAY_STATE_REASON_KEY: DisplayStateReason
DISPLAY_STATE_REASON_MOTION: DisplayStateReason
TRANSIT_NONE: TransitionTypeEnum
TRANSIT_UNSET: TransitionTypeEnum
TRANSIT_ACTIVITY_OPEN: TransitionTypeEnum
TRANSIT_ACTIVITY_CLOSE: TransitionTypeEnum
TRANSIT_TASK_OPEN: TransitionTypeEnum
TRANSIT_TASK_CLOSE: TransitionTypeEnum
TRANSIT_TASK_TO_FRONT: TransitionTypeEnum
TRANSIT_TASK_TO_BACK: TransitionTypeEnum
TRANSIT_WALLPAPER_CLOSE: TransitionTypeEnum
TRANSIT_WALLPAPER_OPEN: TransitionTypeEnum
TRANSIT_WALLPAPER_INTRA_OPEN: TransitionTypeEnum
TRANSIT_WALLPAPER_INTRA_CLOSE: TransitionTypeEnum
TRANSIT_TASK_OPEN_BEHIND: TransitionTypeEnum
TRANSIT_TASK_IN_PLACE: TransitionTypeEnum
TRANSIT_ACTIVITY_RELAUNCH: TransitionTypeEnum
TRANSIT_DOCK_TASK_FROM_RECENTS: TransitionTypeEnum
TRANSIT_KEYGUARD_GOING_AWAY: TransitionTypeEnum
TRANSIT_KEYGUARD_GOING_AWAY_ON_WALLPAPER: TransitionTypeEnum
TRANSIT_KEYGUARD_OCCLUDE: TransitionTypeEnum
TRANSIT_KEYGUARD_UNOCCLUDE: TransitionTypeEnum
TRANSIT_TRANSLUCENT_ACTIVITY_OPEN: TransitionTypeEnum
TRANSIT_TRANSLUCENT_ACTIVITY_CLOSE: TransitionTypeEnum
TRANSIT_CRASHING_ACTIVITY_CLOSE: TransitionTypeEnum
