from frameworks.base.core.proto.android import privacy_pb2 as _privacy_pb2
from google.protobuf.internal import containers as _containers
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from collections.abc import Iterable as _Iterable, Mapping as _Mapping
from typing import ClassVar as _ClassVar, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class PackageItemInfoProto(_message.Message):
    __slots__ = ("name", "package_name", "label_res", "non_localized_label", "icon", "banner", "is_archived")
    NAME_FIELD_NUMBER: _ClassVar[int]
    PACKAGE_NAME_FIELD_NUMBER: _ClassVar[int]
    LABEL_RES_FIELD_NUMBER: _ClassVar[int]
    NON_LOCALIZED_LABEL_FIELD_NUMBER: _ClassVar[int]
    ICON_FIELD_NUMBER: _ClassVar[int]
    BANNER_FIELD_NUMBER: _ClassVar[int]
    IS_ARCHIVED_FIELD_NUMBER: _ClassVar[int]
    name: str
    package_name: str
    label_res: int
    non_localized_label: str
    icon: int
    banner: int
    is_archived: bool
    def __init__(self, name: _Optional[str] = ..., package_name: _Optional[str] = ..., label_res: _Optional[int] = ..., non_localized_label: _Optional[str] = ..., icon: _Optional[int] = ..., banner: _Optional[int] = ..., is_archived: _Optional[bool] = ...) -> None: ...

class ApplicationInfoProto(_message.Message):
    __slots__ = ("package", "permission", "process_name", "uid", "flags", "private_flags", "theme", "source_dir", "public_source_dir", "split_source_dirs", "split_public_source_dirs", "resource_dirs", "data_dir", "class_loader_name", "split_class_loader_names", "version", "detail", "overlay_paths", "known_activity_embedding_certs")
    class Version(_message.Message):
        __slots__ = ("enabled", "min_sdk_version", "target_sdk_version", "version_code", "target_sandbox_version")
        ENABLED_FIELD_NUMBER: _ClassVar[int]
        MIN_SDK_VERSION_FIELD_NUMBER: _ClassVar[int]
        TARGET_SDK_VERSION_FIELD_NUMBER: _ClassVar[int]
        VERSION_CODE_FIELD_NUMBER: _ClassVar[int]
        TARGET_SANDBOX_VERSION_FIELD_NUMBER: _ClassVar[int]
        enabled: bool
        min_sdk_version: int
        target_sdk_version: int
        version_code: int
        target_sandbox_version: int
        def __init__(self, enabled: _Optional[bool] = ..., min_sdk_version: _Optional[int] = ..., target_sdk_version: _Optional[int] = ..., version_code: _Optional[int] = ..., target_sandbox_version: _Optional[int] = ...) -> None: ...
    class Detail(_message.Message):
        __slots__ = ("class_name", "task_affinity", "requires_smallest_width_dp", "compatible_width_limit_dp", "largest_width_limit_dp", "seinfo", "seinfo_user", "device_protected_data_dir", "credential_protected_data_dir", "shared_library_files", "manage_space_activity_name", "description_res", "ui_options", "supports_rtl", "content", "is_full_backup", "network_security_config_res", "category", "enable_gwp_asan", "enable_memtag", "native_heap_zero_init", "allow_cross_uid_activity_switch_from_below", "enable_page_size_app_compat")
        CLASS_NAME_FIELD_NUMBER: _ClassVar[int]
        TASK_AFFINITY_FIELD_NUMBER: _ClassVar[int]
        REQUIRES_SMALLEST_WIDTH_DP_FIELD_NUMBER: _ClassVar[int]
        COMPATIBLE_WIDTH_LIMIT_DP_FIELD_NUMBER: _ClassVar[int]
        LARGEST_WIDTH_LIMIT_DP_FIELD_NUMBER: _ClassVar[int]
        SEINFO_FIELD_NUMBER: _ClassVar[int]
        SEINFO_USER_FIELD_NUMBER: _ClassVar[int]
        DEVICE_PROTECTED_DATA_DIR_FIELD_NUMBER: _ClassVar[int]
        CREDENTIAL_PROTECTED_DATA_DIR_FIELD_NUMBER: _ClassVar[int]
        SHARED_LIBRARY_FILES_FIELD_NUMBER: _ClassVar[int]
        MANAGE_SPACE_ACTIVITY_NAME_FIELD_NUMBER: _ClassVar[int]
        DESCRIPTION_RES_FIELD_NUMBER: _ClassVar[int]
        UI_OPTIONS_FIELD_NUMBER: _ClassVar[int]
        SUPPORTS_RTL_FIELD_NUMBER: _ClassVar[int]
        CONTENT_FIELD_NUMBER: _ClassVar[int]
        IS_FULL_BACKUP_FIELD_NUMBER: _ClassVar[int]
        NETWORK_SECURITY_CONFIG_RES_FIELD_NUMBER: _ClassVar[int]
        CATEGORY_FIELD_NUMBER: _ClassVar[int]
        ENABLE_GWP_ASAN_FIELD_NUMBER: _ClassVar[int]
        ENABLE_MEMTAG_FIELD_NUMBER: _ClassVar[int]
        NATIVE_HEAP_ZERO_INIT_FIELD_NUMBER: _ClassVar[int]
        ALLOW_CROSS_UID_ACTIVITY_SWITCH_FROM_BELOW_FIELD_NUMBER: _ClassVar[int]
        ENABLE_PAGE_SIZE_APP_COMPAT_FIELD_NUMBER: _ClassVar[int]
        class_name: str
        task_affinity: str
        requires_smallest_width_dp: int
        compatible_width_limit_dp: int
        largest_width_limit_dp: int
        seinfo: str
        seinfo_user: str
        device_protected_data_dir: str
        credential_protected_data_dir: str
        shared_library_files: _containers.RepeatedScalarFieldContainer[str]
        manage_space_activity_name: str
        description_res: int
        ui_options: int
        supports_rtl: bool
        content: str
        is_full_backup: bool
        network_security_config_res: int
        category: int
        enable_gwp_asan: int
        enable_memtag: int
        native_heap_zero_init: bool
        allow_cross_uid_activity_switch_from_below: bool
        enable_page_size_app_compat: int
        def __init__(self, class_name: _Optional[str] = ..., task_affinity: _Optional[str] = ..., requires_smallest_width_dp: _Optional[int] = ..., compatible_width_limit_dp: _Optional[int] = ..., largest_width_limit_dp: _Optional[int] = ..., seinfo: _Optional[str] = ..., seinfo_user: _Optional[str] = ..., device_protected_data_dir: _Optional[str] = ..., credential_protected_data_dir: _Optional[str] = ..., shared_library_files: _Optional[_Iterable[str]] = ..., manage_space_activity_name: _Optional[str] = ..., description_res: _Optional[int] = ..., ui_options: _Optional[int] = ..., supports_rtl: _Optional[bool] = ..., content: _Optional[str] = ..., is_full_backup: _Optional[bool] = ..., network_security_config_res: _Optional[int] = ..., category: _Optional[int] = ..., enable_gwp_asan: _Optional[int] = ..., enable_memtag: _Optional[int] = ..., native_heap_zero_init: _Optional[bool] = ..., allow_cross_uid_activity_switch_from_below: _Optional[bool] = ..., enable_page_size_app_compat: _Optional[int] = ...) -> None: ...
    PACKAGE_FIELD_NUMBER: _ClassVar[int]
    PERMISSION_FIELD_NUMBER: _ClassVar[int]
    PROCESS_NAME_FIELD_NUMBER: _ClassVar[int]
    UID_FIELD_NUMBER: _ClassVar[int]
    FLAGS_FIELD_NUMBER: _ClassVar[int]
    PRIVATE_FLAGS_FIELD_NUMBER: _ClassVar[int]
    THEME_FIELD_NUMBER: _ClassVar[int]
    SOURCE_DIR_FIELD_NUMBER: _ClassVar[int]
    PUBLIC_SOURCE_DIR_FIELD_NUMBER: _ClassVar[int]
    SPLIT_SOURCE_DIRS_FIELD_NUMBER: _ClassVar[int]
    SPLIT_PUBLIC_SOURCE_DIRS_FIELD_NUMBER: _ClassVar[int]
    RESOURCE_DIRS_FIELD_NUMBER: _ClassVar[int]
    DATA_DIR_FIELD_NUMBER: _ClassVar[int]
    CLASS_LOADER_NAME_FIELD_NUMBER: _ClassVar[int]
    SPLIT_CLASS_LOADER_NAMES_FIELD_NUMBER: _ClassVar[int]
    VERSION_FIELD_NUMBER: _ClassVar[int]
    DETAIL_FIELD_NUMBER: _ClassVar[int]
    OVERLAY_PATHS_FIELD_NUMBER: _ClassVar[int]
    KNOWN_ACTIVITY_EMBEDDING_CERTS_FIELD_NUMBER: _ClassVar[int]
    package: PackageItemInfoProto
    permission: str
    process_name: str
    uid: int
    flags: int
    private_flags: int
    theme: int
    source_dir: str
    public_source_dir: str
    split_source_dirs: _containers.RepeatedScalarFieldContainer[str]
    split_public_source_dirs: _containers.RepeatedScalarFieldContainer[str]
    resource_dirs: _containers.RepeatedScalarFieldContainer[str]
    data_dir: str
    class_loader_name: str
    split_class_loader_names: _containers.RepeatedScalarFieldContainer[str]
    version: ApplicationInfoProto.Version
    detail: ApplicationInfoProto.Detail
    overlay_paths: _containers.RepeatedScalarFieldContainer[str]
    known_activity_embedding_certs: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, package: _Optional[_Union[PackageItemInfoProto, _Mapping]] = ..., permission: _Optional[str] = ..., process_name: _Optional[str] = ..., uid: _Optional[int] = ..., flags: _Optional[int] = ..., private_flags: _Optional[int] = ..., theme: _Optional[int] = ..., source_dir: _Optional[str] = ..., public_source_dir: _Optional[str] = ..., split_source_dirs: _Optional[_Iterable[str]] = ..., split_public_source_dirs: _Optional[_Iterable[str]] = ..., resource_dirs: _Optional[_Iterable[str]] = ..., data_dir: _Optional[str] = ..., class_loader_name: _Optional[str] = ..., split_class_loader_names: _Optional[_Iterable[str]] = ..., version: _Optional[_Union[ApplicationInfoProto.Version, _Mapping]] = ..., detail: _Optional[_Union[ApplicationInfoProto.Detail, _Mapping]] = ..., overlay_paths: _Optional[_Iterable[str]] = ..., known_activity_embedding_certs: _Optional[_Iterable[str]] = ...) -> None: ...
