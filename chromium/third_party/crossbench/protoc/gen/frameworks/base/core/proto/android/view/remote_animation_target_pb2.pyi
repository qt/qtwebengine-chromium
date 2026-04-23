from frameworks.base.core.proto.android.app import window_configuration_pb2 as _window_configuration_pb2
from frameworks.base.core.proto.android.graphics import point_pb2 as _point_pb2
from frameworks.base.core.proto.android.graphics import rect_pb2 as _rect_pb2
from frameworks.base.core.proto.android.view import surfacecontrol_pb2 as _surfacecontrol_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class RemoteAnimationTargetProto(_message.Message):
    __slots__ = ("task_id", "mode", "leash", "is_translucent", "clip_rect", "content_insets", "prefix_order_index", "position", "source_container_bounds", "window_configuration", "start_leash", "start_bounds", "local_bounds", "screen_space_bounds")
    TASK_ID_FIELD_NUMBER: _ClassVar[int]
    MODE_FIELD_NUMBER: _ClassVar[int]
    LEASH_FIELD_NUMBER: _ClassVar[int]
    IS_TRANSLUCENT_FIELD_NUMBER: _ClassVar[int]
    CLIP_RECT_FIELD_NUMBER: _ClassVar[int]
    CONTENT_INSETS_FIELD_NUMBER: _ClassVar[int]
    PREFIX_ORDER_INDEX_FIELD_NUMBER: _ClassVar[int]
    POSITION_FIELD_NUMBER: _ClassVar[int]
    SOURCE_CONTAINER_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    WINDOW_CONFIGURATION_FIELD_NUMBER: _ClassVar[int]
    START_LEASH_FIELD_NUMBER: _ClassVar[int]
    START_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    LOCAL_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    SCREEN_SPACE_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    task_id: int
    mode: int
    leash: _surfacecontrol_pb2.SurfaceControlProto
    is_translucent: bool
    clip_rect: _rect_pb2.RectProto
    content_insets: _rect_pb2.RectProto
    prefix_order_index: int
    position: _point_pb2.PointProto
    source_container_bounds: _rect_pb2.RectProto
    window_configuration: _window_configuration_pb2.WindowConfigurationProto
    start_leash: _surfacecontrol_pb2.SurfaceControlProto
    start_bounds: _rect_pb2.RectProto
    local_bounds: _rect_pb2.RectProto
    screen_space_bounds: _rect_pb2.RectProto
    def __init__(self, task_id: _Optional[int] = ..., mode: _Optional[int] = ..., leash: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ..., is_translucent: _Optional[bool] = ..., clip_rect: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., content_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., prefix_order_index: _Optional[int] = ..., position: _Optional[_Union[_point_pb2.PointProto, _Mapping]] = ..., source_container_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., window_configuration: _Optional[_Union[_window_configuration_pb2.WindowConfigurationProto, _Mapping]] = ..., start_leash: _Optional[_Union[_surfacecontrol_pb2.SurfaceControlProto, _Mapping]] = ..., start_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., local_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., screen_space_bounds: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ...) -> None: ...
