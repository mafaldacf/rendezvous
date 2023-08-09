from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

DESCRIPTOR: _descriptor.FileDescriptor

class RequestStatus(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
    OPENED: _ClassVar[RequestStatus]
    CLOSED: _ClassVar[RequestStatus]
    UNKNOWN: _ClassVar[RequestStatus]
OPENED: RequestStatus
CLOSED: RequestStatus
UNKNOWN: RequestStatus

class Empty(_message.Message):
    __slots__ = []
    def __init__(self) -> None: ...

class RequestContext(_message.Message):
    __slots__ = ["versions"]
    class VersionsEntry(_message.Message):
        __slots__ = ["key", "value"]
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: int
        def __init__(self, key: _Optional[str] = ..., value: _Optional[int] = ...) -> None: ...
    VERSIONS_FIELD_NUMBER: _ClassVar[int]
    versions: _containers.ScalarMap[str, int]
    def __init__(self, versions: _Optional[_Mapping[str, int]] = ...) -> None: ...

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
    __slots__ = ["rid", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchMessage(_message.Message):
    __slots__ = ["rid", "service", "tag", "regions", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    tag: str
    regions: _containers.RepeatedScalarFieldContainer[str]
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., tag: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchResponse(_message.Message):
    __slots__ = ["rid", "bid", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bid: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class DatastoreBranching(_message.Message):
    __slots__ = ["datastore", "tag", "regions"]
    DATASTORE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    datastore: str
    tag: str
    regions: _containers.RepeatedScalarFieldContainer[str]
    def __init__(self, datastore: _Optional[str] = ..., tag: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ...) -> None: ...

class RegisterBranchesDatastoresMessage(_message.Message):
    __slots__ = ["rid", "datastores", "regions", "tags", "branches", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    DATASTORES_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    TAGS_FIELD_NUMBER: _ClassVar[int]
    BRANCHES_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    datastores: _containers.RepeatedScalarFieldContainer[str]
    regions: _containers.RepeatedScalarFieldContainer[str]
    tags: _containers.RepeatedScalarFieldContainer[str]
    branches: _containers.RepeatedCompositeFieldContainer[DatastoreBranching]
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., datastores: _Optional[_Iterable[str]] = ..., regions: _Optional[_Iterable[str]] = ..., tags: _Optional[_Iterable[str]] = ..., branches: _Optional[_Iterable[_Union[DatastoreBranching, _Mapping]]] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchesDatastoresResponse(_message.Message):
    __slots__ = ["rid", "bids", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BIDS_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bids: _containers.RepeatedScalarFieldContainer[str]
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., bids: _Optional[_Iterable[str]] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class CloseBranchMessage(_message.Message):
    __slots__ = ["bid", "region", "context"]
    BID_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    bid: str
    region: str
    context: RequestContext
    def __init__(self, bid: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class WaitRequestMessage(_message.Message):
    __slots__ = ["rid", "service", "region", "tag", "timeout", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    TIMEOUT_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    region: str
    tag: str
    timeout: int
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., tag: _Optional[str] = ..., timeout: _Optional[int] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class WaitRequestResponse(_message.Message):
    __slots__ = ["prevented_inconsistency", "timed_out"]
    PREVENTED_INCONSISTENCY_FIELD_NUMBER: _ClassVar[int]
    TIMED_OUT_FIELD_NUMBER: _ClassVar[int]
    prevented_inconsistency: bool
    timed_out: bool
    def __init__(self, prevented_inconsistency: bool = ..., timed_out: bool = ...) -> None: ...

class CheckRequestMessage(_message.Message):
    __slots__ = ["rid", "service", "region", "detailed", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    DETAILED_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    region: str
    detailed: bool
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., detailed: bool = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class CheckRequestResponse(_message.Message):
    __slots__ = ["status"]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    status: RequestStatus
    def __init__(self, status: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...

class CheckRequestByRegionsMessage(_message.Message):
    __slots__ = ["rid", "service", "detailed", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    DETAILED_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    detailed: bool
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., detailed: bool = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class CheckRequestByRegionsResponse(_message.Message):
    __slots__ = ["statuses"]
    class StatusesEntry(_message.Message):
        __slots__ = ["key", "value"]
        KEY_FIELD_NUMBER: _ClassVar[int]
        VALUE_FIELD_NUMBER: _ClassVar[int]
        key: str
        value: RequestStatus
        def __init__(self, key: _Optional[str] = ..., value: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...
    STATUSES_FIELD_NUMBER: _ClassVar[int]
    statuses: _containers.ScalarMap[str, RequestStatus]
    def __init__(self, statuses: _Optional[_Mapping[str, RequestStatus]] = ...) -> None: ...
