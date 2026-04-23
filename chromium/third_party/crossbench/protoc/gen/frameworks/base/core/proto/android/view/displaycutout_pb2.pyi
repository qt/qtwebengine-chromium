from frameworks.base.core.proto.android.graphics import rect_pb2 as _rect_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class DisplayCutoutProto(_message.Message):
    __slots__ = ("insets", "bound_left", "bound_top", "bound_right", "bound_bottom", "waterfall_insets", "side_overrides")
    INSETS_FIELD_NUMBER: _ClassVar[int]
    BOUND_LEFT_FIELD_NUMBER: _ClassVar[int]
    BOUND_TOP_FIELD_NUMBER: _ClassVar[int]
    BOUND_RIGHT_FIELD_NUMBER: _ClassVar[int]
    BOUND_BOTTOM_FIELD_NUMBER: _ClassVar[int]
    WATERFALL_INSETS_FIELD_NUMBER: _ClassVar[int]
    SIDE_OVERRIDES_FIELD_NUMBER: _ClassVar[int]
    insets: _rect_pb2.RectProto
    bound_left: _rect_pb2.RectProto
    bound_top: _rect_pb2.RectProto
    bound_right: _rect_pb2.RectProto
    bound_bottom: _rect_pb2.RectProto
    waterfall_insets: _rect_pb2.RectProto
    side_overrides: _containers.RepeatedScalarFieldContainer[int]
    def __init__(self, insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., bound_left: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., bound_top: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., bound_right: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., bound_bottom: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., waterfall_insets: _Optional[_Union[_rect_pb2.RectProto, _Mapping]] = ..., side_overrides: _Optional[_Iterable[int]] = ...) -> None: ...
