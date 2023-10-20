from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class RequestStatus(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    CLOSED: _ClassVar[RequestStatus]
    OPENED: _ClassVar[RequestStatus]
    UNKNOWN: _ClassVar[RequestStatus]
CLOSED: RequestStatus
OPENED: RequestStatus
UNKNOWN: RequestStatus

class Empty(_message.Message):
    __slots__ = []
    def __init__(self) -> None: ...

class RequestContext(_message.Message):
    __slots__ = ["current_service", "async_zone", "num_sub_zones"]
    CURRENT_SERVICE_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    NUM_SUB_ZONES_FIELD_NUMBER: _ClassVar[int]
    current_service: str
    async_zone: str
    num_sub_zones: int
    def __init__(self, current_service: _Optional[str] = ..., async_zone: _Optional[str] = ..., num_sub_zones: _Optional[int] = ...) -> None: ...

class SubscribeMessage(_message.Message):
    __slots__ = ["service", "region"]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    service: str
    region: str
    def __init__(self, service: _Optional[str] = ..., region: _Optional[str] = ...) -> None: ...

class SubscribeResponse(_message.Message):
    __slots__ = ["bid", "tag"]
    BID_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    bid: str
    tag: str
    def __init__(self, bid: _Optional[str] = ..., tag: _Optional[str] = ...) -> None: ...

class RegisterRequestMessage(_message.Message):
    __slots__ = ["rid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    def __init__(self, rid: _Optional[str] = ...) -> None: ...

class RegisterRequestResponse(_message.Message):
    __slots__ = ["rid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    def __init__(self, rid: _Optional[str] = ...) -> None: ...

class RegisterBranchMessage(_message.Message):
    __slots__ = ["rid", "bid", "service", "tag", "regions", "monitor", "async_zone", "current_service_bid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    MONITOR_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    CURRENT_SERVICE_BID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bid: str
    service: str
    tag: str
    regions: _containers.RepeatedScalarFieldContainer[str]
    monitor: bool
    async_zone: str
    current_service_bid: str
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., service: _Optional[str] = ..., tag: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ..., monitor: bool = ..., async_zone: _Optional[str] = ..., current_service_bid: _Optional[str] = ...) -> None: ...

class RegisterBranchResponse(_message.Message):
    __slots__ = ["rid", "bid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bid: str
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ...) -> None: ...

class Branch(_message.Message):
    __slots__ = ["service", "tag", "regions", "async_zone", "current_service_bid"]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    CURRENT_SERVICE_BID_FIELD_NUMBER: _ClassVar[int]
    service: str
    tag: str
    regions: _containers.RepeatedScalarFieldContainer[str]
    async_zone: str
    current_service_bid: str
    def __init__(self, service: _Optional[str] = ..., tag: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ..., async_zone: _Optional[str] = ..., current_service_bid: _Optional[str] = ...) -> None: ...

class RegisterBranchesMessage(_message.Message):
    __slots__ = ["rid", "branches", "current_service"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BRANCHES_FIELD_NUMBER: _ClassVar[int]
    CURRENT_SERVICE_FIELD_NUMBER: _ClassVar[int]
    rid: str
    branches: _containers.RepeatedCompositeFieldContainer[Branch]
    current_service: str
    def __init__(self, rid: _Optional[str] = ..., branches: _Optional[_Iterable[_Union[Branch, _Mapping]]] = ..., current_service: _Optional[str] = ...) -> None: ...

class RegisterBranchesResponse(_message.Message):
    __slots__ = ["rid", "bids"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BIDS_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bids: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, rid: _Optional[str] = ..., bids: _Optional[_Iterable[str]] = ...) -> None: ...

class DatastoreBranching(_message.Message):
    __slots__ = ["datastore", "tag", "regions"]
    DATASTORE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    datastore: str
    tag: str
    regions: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, datastore: _Optional[str] = ..., tag: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ...) -> None: ...

class CloseBranchMessage(_message.Message):
    __slots__ = ["bid", "region"]
    BID_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    bid: str
    region: str
    def __init__(self, bid: _Optional[str] = ..., region: _Optional[str] = ...) -> None: ...

class WaitRequestMessage(_message.Message):
    __slots__ = ["rid", "service", "services", "region", "tag", "timeout", "wait_deps", "current_service", "async_zone"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    SERVICES_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    TIMEOUT_FIELD_NUMBER: _ClassVar[int]
    WAIT_DEPS_FIELD_NUMBER: _ClassVar[int]
    CURRENT_SERVICE_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    services: _containers.RepeatedScalarFieldContainer[str]
    region: str
    tag: str
    timeout: int
    wait_deps: bool
    current_service: str
    async_zone: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., services: _Optional[_Iterable[str]] = ..., region: _Optional[str] = ..., tag: _Optional[str] = ..., timeout: _Optional[int] = ..., wait_deps: bool = ..., current_service: _Optional[str] = ..., async_zone: _Optional[str] = ...) -> None: ...

class WaitRequestResponse(_message.Message):
    __slots__ = ["prevented_inconsistency", "timed_out"]
    PREVENTED_INCONSISTENCY_FIELD_NUMBER: _ClassVar[int]
    TIMED_OUT_FIELD_NUMBER: _ClassVar[int]
    prevented_inconsistency: bool
    timed_out: bool
    def __init__(self, prevented_inconsistency: bool = ..., timed_out: bool = ...) -> None: ...

class CheckStatusMessage(_message.Message):
    __slots__ = ["rid", "service", "region", "detailed", "async_zone"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    DETAILED_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    region: str
    detailed: bool
    async_zone: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., detailed: bool = ..., async_zone: _Optional[str] = ...) -> None: ...

class CheckStatusResponse(_message.Message):
    __slots__ = ["status", "tagged", "regions"]
    class TaggedEntry(_message.Message):
        __slots__ = ["key", "value"]
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: RequestStatus
        def __init__(self, key: _Optional[str] = ..., value: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...
    class RegionsEntry(_message.Message):
        __slots__ = ["key", "value"]
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: RequestStatus
        def __init__(self, key: _Optional[str] = ..., value: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...
    STATUS_FIELD_NUMBER: _ClassVar[int]
    TAGGED_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    status: RequestStatus
    tagged: _containers.ScalarMap[str, RequestStatus]
    regions: _containers.ScalarMap[str, RequestStatus]
    def __init__(self, status: _Optional[_Union[RequestStatus, str]] = ..., tagged: _Optional[_Mapping[str, RequestStatus]] = ..., regions: _Optional[_Mapping[str, RequestStatus]] = ...) -> None: ...

class FetchDependenciesMessage(_message.Message):
    __slots__ = ["rid", "service", "async_zone"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    ASYNC_ZONE_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    async_zone: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., async_zone: _Optional[str] = ...) -> None: ...

class FetchDependenciesResponse(_message.Message):
    __slots__ = ["deps", "indirect_deps"]
    DEPS_FIELD_NUMBER: _ClassVar[int]
    INDIRECT_DEPS_FIELD_NUMBER: _ClassVar[int]
    deps: _containers.RepeatedScalarFieldContainer[str]
    indirect_deps: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, deps: _Optional[_Iterable[str]] = ..., indirect_deps: _Optional[_Iterable[str]] = ...) -> None: ...
