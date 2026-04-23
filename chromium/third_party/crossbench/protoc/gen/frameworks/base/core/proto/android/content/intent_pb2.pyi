from frameworks.base.core.proto.android.content import component_name_pb2 as _component_name_pb2
from frameworks.base.core.proto.android.os import patternmatcher_pb2 as _patternmatcher_pb2
from frameworks.base.core.proto.android.os import persistablebundle_pb2 as _persistablebundle_pb2
from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class IntentProto(_message.Message):
    __slots__ = ("action", "categories", "data", "type", "flag", "extended_flag", "package", "component", "source_bounds", "clip_data", "extras", "content_user_hint", "selector", "identifier")
    class DockState(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        DOCK_STATE_UNDOCKED: _ClassVar[IntentProto.DockState]
        DOCK_STATE_DESK: _ClassVar[IntentProto.DockState]
        DOCK_STATE_CAR: _ClassVar[IntentProto.DockState]
        DOCK_STATE_LE_DESK: _ClassVar[IntentProto.DockState]
        DOCK_STATE_HE_DESK: _ClassVar[IntentProto.DockState]
    DOCK_STATE_UNDOCKED: IntentProto.DockState
    DOCK_STATE_DESK: IntentProto.DockState
    DOCK_STATE_CAR: IntentProto.DockState
    DOCK_STATE_LE_DESK: IntentProto.DockState
    DOCK_STATE_HE_DESK: IntentProto.DockState
    ACTION_FIELD_NUMBER: _ClassVar[int]
    CATEGORIES_FIELD_NUMBER: _ClassVar[int]
    DATA_FIELD_NUMBER: _ClassVar[int]
    TYPE_FIELD_NUMBER: _ClassVar[int]
    FLAG_FIELD_NUMBER: _ClassVar[int]
    EXTENDED_FLAG_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_FIELD_NUMBER: _ClassVar[int]
    COMPONENT_FIELD_NUMBER: _ClassVar[int]
    SOURCE_BOUNDS_FIELD_NUMBER: _ClassVar[int]
    CLIP_DATA_FIELD_NUMBER: _ClassVar[int]
    EXTRAS_FIELD_NUMBER: _ClassVar[int]
    CONTENT_USER_HINT_FIELD_NUMBER: _ClassVar[int]
    SELECTOR_FIELD_NUMBER: _ClassVar[int]
    IDENTIFIER_FIELD_NUMBER: _ClassVar[int]
    action: str
    categories: _containers.RepeatedScalarFieldContainer[str]
    data: str
    type: str
    flag: str
    extended_flag: str
    package: str
    component: _component_name_pb2.ComponentNameProto
    source_bounds: str
    clip_data: str
    extras: str
    content_user_hint: int
    selector: str
    identifier: str
    def __init__(self, action: _Optional[str] = ..., categories: _Optional[_Iterable[str]] = ..., data: _Optional[str] = ..., type: _Optional[str] = ..., flag: _Optional[str] = ..., extended_flag: _Optional[str] = ..., package: _Optional[str] = ..., component: _Optional[_Union[_component_name_pb2.ComponentNameProto, _Mapping]] = ..., source_bounds: _Optional[str] = ..., clip_data: _Optional[str] = ..., extras: _Optional[str] = ..., content_user_hint: _Optional[int] = ..., selector: _Optional[str] = ..., identifier: _Optional[str] = ...) -> None: ...

class IntentFilterProto(_message.Message):
    __slots__ = ("actions", "categories", "data_schemes", "data_scheme_specs", "data_authorities", "data_paths", "data_types", "priority", "has_partial_types", "get_auto_verify", "mime_groups", "extras", "uri_relative_filter_groups")
    ACTIONS_FIELD_NUMBER: _ClassVar[int]
    CATEGORIES_FIELD_NUMBER: _ClassVar[int]
    DATA_SCHEMES_FIELD_NUMBER: _ClassVar[int]
    DATA_SCHEME_SPECS_FIELD_NUMBER: _ClassVar[int]
    DATA_AUTHORITIES_FIELD_NUMBER: _ClassVar[int]
    DATA_PATHS_FIELD_NUMBER: _ClassVar[int]
    DATA_TYPES_FIELD_NUMBER: _ClassVar[int]
    PRIORITY_FIELD_NUMBER: _ClassVar[int]
    HAS_PARTIAL_TYPES_FIELD_NUMBER: _ClassVar[int]
    GET_AUTO_VERIFY_FIELD_NUMBER: _ClassVar[int]
    MIME_GROUPS_FIELD_NUMBER: _ClassVar[int]
    EXTRAS_FIELD_NUMBER: _ClassVar[int]
    URI_RELATIVE_FILTER_GROUPS_FIELD_NUMBER: _ClassVar[int]
    actions: _containers.RepeatedScalarFieldContainer[str]
    categories: _containers.RepeatedScalarFieldContainer[str]
    data_schemes: _containers.RepeatedScalarFieldContainer[str]
    data_scheme_specs: _containers.RepeatedCompositeFieldContainer[_patternmatcher_pb2.PatternMatcherProto]
    data_authorities: _containers.RepeatedCompositeFieldContainer[AuthorityEntryProto]
    data_paths: _containers.RepeatedCompositeFieldContainer[_patternmatcher_pb2.PatternMatcherProto]
    data_types: _containers.RepeatedScalarFieldContainer[str]
    priority: int
    has_partial_types: bool
    get_auto_verify: bool
    mime_groups: _containers.RepeatedScalarFieldContainer[str]
    extras: _persistablebundle_pb2.PersistableBundleProto
    uri_relative_filter_groups: _containers.RepeatedCompositeFieldContainer[UriRelativeFilterGroupProto]
    def __init__(self, actions: _Optional[_Iterable[str]] = ..., categories: _Optional[_Iterable[str]] = ..., data_schemes: _Optional[_Iterable[str]] = ..., data_scheme_specs: _Optional[_Iterable[_Union[_patternmatcher_pb2.PatternMatcherProto, _Mapping]]] = ..., data_authorities: _Optional[_Iterable[_Union[AuthorityEntryProto, _Mapping]]] = ..., data_paths: _Optional[_Iterable[_Union[_patternmatcher_pb2.PatternMatcherProto, _Mapping]]] = ..., data_types: _Optional[_Iterable[str]] = ..., priority: _Optional[int] = ..., has_partial_types: _Optional[bool] = ..., get_auto_verify: _Optional[bool] = ..., mime_groups: _Optional[_Iterable[str]] = ..., extras: _Optional[_Union[_persistablebundle_pb2.PersistableBundleProto, _Mapping]] = ..., uri_relative_filter_groups: _Optional[_Iterable[_Union[UriRelativeFilterGroupProto, _Mapping]]] = ...) -> None: ...

class AuthorityEntryProto(_message.Message):
    __slots__ = ("host", "wild", "port")
    HOST_FIELD_NUMBER: _ClassVar[int]
    WILD_FIELD_NUMBER: _ClassVar[int]
    PORT_FIELD_NUMBER: _ClassVar[int]
    host: str
    wild: bool
    port: int
    def __init__(self, host: _Optional[str] = ..., wild: _Optional[bool] = ..., port: _Optional[int] = ...) -> None: ...

class UriRelativeFilterGroupProto(_message.Message):
    __slots__ = ("action", "uri_relative_filters")
    class Action(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        ACTION_ALLOW: _ClassVar[UriRelativeFilterGroupProto.Action]
        ACTION_BLOCK: _ClassVar[UriRelativeFilterGroupProto.Action]
    ACTION_ALLOW: UriRelativeFilterGroupProto.Action
    ACTION_BLOCK: UriRelativeFilterGroupProto.Action
    ACTION_FIELD_NUMBER: _ClassVar[int]
    URI_RELATIVE_FILTERS_FIELD_NUMBER: _ClassVar[int]
    action: UriRelativeFilterGroupProto.Action
    uri_relative_filters: _containers.RepeatedCompositeFieldContainer[UriRelativeFilterProto]
    def __init__(self, action: _Optional[_Union[UriRelativeFilterGroupProto.Action, str]] = ..., uri_relative_filters: _Optional[_Iterable[_Union[UriRelativeFilterProto, _Mapping]]] = ...) -> None: ...

class UriRelativeFilterProto(_message.Message):
    __slots__ = ("uri_part", "pattern_type", "filter")
    URI_PART_FIELD_NUMBER: _ClassVar[int]
    PATTERN_TYPE_FIELD_NUMBER: _ClassVar[int]
    FILTER_FIELD_NUMBER: _ClassVar[int]
    uri_part: int
    pattern_type: int
    filter: str
    def __init__(self, uri_part: _Optional[int] = ..., pattern_type: _Optional[int] = ..., filter: _Optional[str] = ...) -> None: ...
