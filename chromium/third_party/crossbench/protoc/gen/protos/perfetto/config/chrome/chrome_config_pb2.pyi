from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class ChromeConfig(_message.Message):
    __slots__ = ("trace_config", "privacy_filtering_enabled", "convert_to_legacy_json", "client_priority", "json_agent_label_filter", "event_package_name_filter_enabled")
    class ClientPriority(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
        __slots__ = ()
        UNKNOWN: _ClassVar[ChromeConfig.ClientPriority]
        BACKGROUND: _ClassVar[ChromeConfig.ClientPriority]
        USER_INITIATED: _ClassVar[ChromeConfig.ClientPriority]
    UNKNOWN: ChromeConfig.ClientPriority
    BACKGROUND: ChromeConfig.ClientPriority
    USER_INITIATED: ChromeConfig.ClientPriority
    TRACE_CONFIG_FIELD_NUMBER: _ClassVar[int]
    PRIVACY_FILTERING_ENABLED_FIELD_NUMBER: _ClassVar[int]
    CONVERT_TO_LEGACY_JSON_FIELD_NUMBER: _ClassVar[int]
    CLIENT_PRIORITY_FIELD_NUMBER: _ClassVar[int]
    JSON_AGENT_LABEL_FILTER_FIELD_NUMBER: _ClassVar[int]
    EVENT_PACKAGE_NAME_FILTER_ENABLED_FIELD_NUMBER: _ClassVar[int]
    trace_config: str
    privacy_filtering_enabled: bool
    convert_to_legacy_json: bool
    client_priority: ChromeConfig.ClientPriority
    json_agent_label_filter: str
    event_package_name_filter_enabled: bool
    def __init__(self, trace_config: _Optional[str] = ..., privacy_filtering_enabled: _Optional[bool] = ..., convert_to_legacy_json: _Optional[bool] = ..., client_priority: _Optional[_Union[ChromeConfig.ClientPriority, str]] = ..., json_agent_label_filter: _Optional[str] = ..., event_package_name_filter_enabled: _Optional[bool] = ...) -> None: ...
