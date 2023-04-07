from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

CLOSED: RequestStatus
DESCRIPTOR: _descriptor.FileDescriptor
OPENED: RequestStatus

class CheckRequestByRegionsMessage(_message.Message):
    __slots__ = ["rid", "serverInfo", "service"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    rid: str
    serverInfo: ServerInfo
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class CheckRequestByRegionsResponse(_message.Message):
    __slots__ = ["regionStatus"]
    REGIONSTATUS_FIELD_NUMBER: _ClassVar[int]
    regionStatus: _containers.RepeatedCompositeFieldContainer[RegionStatus]
    def __init__(self, regionStatus: _Optional[_Iterable[_Union[RegionStatus, _Mapping]]] = ...) -> None: ...

class CheckRequestMessage(_message.Message):
    __slots__ = ["region", "rid", "serverInfo", "service"]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    region: str
    rid: str
    serverInfo: ServerInfo
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class CheckRequestResponse(_message.Message):
    __slots__ = ["status"]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    status: RequestStatus
    def __init__(self, status: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...

class CloseBranchMessage(_message.Message):
    __slots__ = ["bid", "region", "rid", "service"]
    BID_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    bid: str
    region: str
    rid: str
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., bid: _Optional[str] = ...) -> None: ...

class Empty(_message.Message):
    __slots__ = []
    def __init__(self) -> None: ...

class GetPreventedInconsistenciesResponse(_message.Message):
    __slots__ = ["inconsistencies"]
    INCONSISTENCIES_FIELD_NUMBER: _ClassVar[int]
    inconsistencies: int
    def __init__(self, inconsistencies: _Optional[int] = ...) -> None: ...

class RegionStatus(_message.Message):
    __slots__ = ["region", "status"]
    REGION_FIELD_NUMBER: _ClassVar[int]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    region: str
    status: RequestStatus
    def __init__(self, region: _Optional[str] = ..., status: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...

class RegisterBranchMessage(_message.Message):
    __slots__ = ["region", "rid", "serverInfo", "service"]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    region: str
    rid: str
    serverInfo: ServerInfo
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class RegisterBranchResponse(_message.Message):
    __slots__ = ["bid", "rid", "serverInfo"]
    BID_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    bid: str
    rid: str
    serverInfo: ServerInfo
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class RegisterBranchesMessage(_message.Message):
    __slots__ = ["regions", "rid", "serverInfo", "service"]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    regions: _containers.RepeatedScalarFieldContainer[str]
    rid: str
    serverInfo: ServerInfo
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class RegisterBranchesResponse(_message.Message):
    __slots__ = ["bid", "rid", "serverInfo"]
    BID_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    bid: str
    rid: str
    serverInfo: ServerInfo
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class RegisterRequestMessage(_message.Message):
    __slots__ = ["rid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    def __init__(self, rid: _Optional[str] = ...) -> None: ...

class RegisterRequestResponse(_message.Message):
    __slots__ = ["rid", "serverInfo"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    rid: str
    serverInfo: ServerInfo
    def __init__(self, rid: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class ServerInfo(_message.Message):
    __slots__ = ["metadata"]
    class MetadataEntry(_message.Message):
        __slots__ = ["key", "value"]
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: int
        def __init__(self, key: _Optional[str] = ..., value: _Optional[int] = ...) -> None: ...
    METADATA_FIELD_NUMBER: _ClassVar[int]
    metadata: _containers.ScalarMap[str, int]
    def __init__(self, metadata: _Optional[_Mapping[str, int]] = ...) -> None: ...

class WaitRequestMessage(_message.Message):
    __slots__ = ["region", "rid", "serverInfo", "service"]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVERINFO_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    region: str
    rid: str
    serverInfo: ServerInfo
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., serverInfo: _Optional[_Union[ServerInfo, _Mapping]] = ...) -> None: ...

class WaitRequestResponse(_message.Message):
    __slots__ = ["preventedInconsistency"]
    PREVENTEDINCONSISTENCY_FIELD_NUMBER: _ClassVar[int]
    preventedInconsistency: bool
    def __init__(self, preventedInconsistency: bool = ...) -> None: ...

class RequestStatus(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
