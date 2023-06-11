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

class SubscribeBranchesMessage(_message.Message):
    __slots__ = ["service", "tag", "region"]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    service: str
    tag: str
    region: str
    def __init__(self, service: _Optional[str] = ..., tag: _Optional[str] = ..., region: _Optional[str] = ...) -> None: ...

class SubscribeBranchesResponse(_message.Message):
    __slots__ = ["bid"]
    BID_FIELD_NUMBER: _ClassVar[int]
    bid: str
    def __init__(self, bid: _Optional[str] = ...) -> None: ...

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
    __slots__ = ["rid", "service", "tag", "region", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    TAG_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    tag: str
    region: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., tag: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchResponse(_message.Message):
    __slots__ = ["rid", "bid", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bid: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchesMessage(_message.Message):
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

class RegisterBranchesResponse(_message.Message):
    __slots__ = ["rid", "bid", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    BID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    bid: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

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
    __slots__ = ["rid", "service", "region", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    region: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class WaitRequestResponse(_message.Message):
    __slots__ = ["prevented_inconsistency"]
    PREVENTED_INCONSISTENCY_FIELD_NUMBER: _ClassVar[int]
    prevented_inconsistency: bool
    def __init__(self, prevented_inconsistency: bool = ...) -> None: ...

class CheckRequestMessage(_message.Message):
    __slots__ = ["rid", "service", "region", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    region: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class CheckRequestResponse(_message.Message):
    __slots__ = ["status"]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    status: RequestStatus
    def __init__(self, status: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...

class CheckRequestByRegionsMessage(_message.Message):
    __slots__ = ["rid", "service", "context"]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    rid: str
    service: str
    context: RequestContext
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

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

class GetNumPreventedInconsistenciesResponse(_message.Message):
    __slots__ = ["inconsistencies"]
    INCONSISTENCIES_FIELD_NUMBER: _ClassVar[int]
    inconsistencies: int
    def __init__(self, inconsistencies: _Optional[int] = ...) -> None: ...
