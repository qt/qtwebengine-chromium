from frameworks.base.core.proto.android.server import animationadapter_pb2 as _animationadapter_pb2
from frameworks.base.core.proto.android.view import surfacecontrol_pb2 as _surfacecontrol_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class SurfaceAnimatorProto(_message.Message):
    __slots__ = ("leash", "animation_start_delayed", "animation_adapter")
    LEASH_FIELD_NUMBER: _ClassVar[int]
    ANIMATION_START_DELAYED_FIELD_NUMBER: _ClassVar[int]
    ANIMATION_ADAPTER_FIELD_NUMBER: _ClassVar[int]
    leash: _surfacecontrol_pb2.SurfaceControlProto
    animation_start_delayed: bool
    animation_adapter: _animationadapter_pb2.AnimationAdapterProto
    def __init__(self, leash: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ..., animation_start_delayed: _Optional[bool] = ..., animation_adapter: _Optional[_Union[_animationadapter_pb2.AnimationAdapterProto, _Mapping]] = ...) -> None: ...
