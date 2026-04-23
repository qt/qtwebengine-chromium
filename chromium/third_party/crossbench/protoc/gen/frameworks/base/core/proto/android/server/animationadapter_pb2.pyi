from frameworks.base.core.proto.android.graphics import point_pb2 as _point_pb2
from frameworks.base.core.proto.android.view import remote_animation_target_pb2 as _remote_animation_target_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class AnimationAdapterProto(_message.Message):
    __slots__ = ("local", "remote")
    LOCAL_FIELD_NUMBER: _ClassVar[int]
    REMOTE_FIELD_NUMBER: _ClassVar[int]
    local: LocalAnimationAdapterProto
    remote: RemoteAnimationAdapterWrapperProto
    def __init__(self, local: _Optional[_Union[LocalAnimationAdapterProto, _Mapping]] = ..., remote: _Optional[_Union[RemoteAnimationAdapterWrapperProto, _Mapping]] = ...) -> None: ...

class RemoteAnimationAdapterWrapperProto(_message.Message):
    __slots__ = ("target",)
    TARGET_FIELD_NUMBER: _ClassVar[int]
    target: _remote_animation_target_pb2.RemoteAnimationTargetProto
    def __init__(self, target: _Optional[_Union[_remote_animation_target_pb2.RemoteAnimationTargetProto, _Mapping]] = ...) -> None: ...

class LocalAnimationAdapterProto(_message.Message):
    __slots__ = ("animation_spec",)
    ANIMATION_SPEC_FIELD_NUMBER: _ClassVar[int]
    animation_spec: AnimationSpecProto
    def __init__(self, animation_spec: _Optional[_Union[AnimationSpecProto, _Mapping]] = ...) -> None: ...

class AnimationSpecProto(_message.Message):
    __slots__ = ("window", "move", "alpha", "rotate")
    WINDOW_FIELD_NUMBER: _ClassVar[int]
    MOVE_FIELD_NUMBER: _ClassVar[int]
    ALPHA_FIELD_NUMBER: _ClassVar[int]
    ROTATE_FIELD_NUMBER: _ClassVar[int]
    window: WindowAnimationSpecProto
    move: MoveAnimationSpecProto
    alpha: AlphaAnimationSpecProto
    rotate: RotationAnimationSpecProto
    def __init__(self, window: _Optional[_Union[WindowAnimationSpecProto, _Mapping]] = ..., move: _Optional[_Union[MoveAnimationSpecProto, _Mapping]] = ..., alpha: _Optional[_Union[AlphaAnimationSpecProto, _Mapping]] = ..., rotate: _Optional[_Union[RotationAnimationSpecProto, _Mapping]] = ...) -> None: ...

class WindowAnimationSpecProto(_message.Message):
    __slots__ = ("animation",)
    ANIMATION_FIELD_NUMBER: _ClassVar[int]
    animation: str
    def __init__(self, animation: _Optional[str] = ...) -> None: ...

class MoveAnimationSpecProto(_message.Message):
    __slots__ = ("to", "duration_ms")
    FROM_FIELD_NUMBER: _ClassVar[int]
    TO_FIELD_NUMBER: _ClassVar[int]
    DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    to: _point_pb2.PointProto
    duration_ms: int
    def __init__(self, to: _Optional[_Union[_point_pb2.PointProto, _Mapping]] = ..., duration_ms: _Optional[int] = ..., **kwargs) -> None: ...

class AlphaAnimationSpecProto(_message.Message):
    __slots__ = ("to", "duration_ms")
    FROM_FIELD_NUMBER: _ClassVar[int]
    TO_FIELD_NUMBER: _ClassVar[int]
    DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    to: float
    duration_ms: int
    def __init__(self, to: _Optional[float] = ..., duration_ms: _Optional[int] = ..., **kwargs) -> None: ...

class RotationAnimationSpecProto(_message.Message):
    __slots__ = ("start_luma", "end_luma", "duration_ms")
    START_LUMA_FIELD_NUMBER: _ClassVar[int]
    END_LUMA_FIELD_NUMBER: _ClassVar[int]
    DURATION_MS_FIELD_NUMBER: _ClassVar[int]
    start_luma: float
    end_luma: float
    duration_ms: int
    def __init__(self, start_luma: _Optional[float] = ..., end_luma: _Optional[float] = ..., duration_ms: _Optional[int] = ...) -> None: ...
