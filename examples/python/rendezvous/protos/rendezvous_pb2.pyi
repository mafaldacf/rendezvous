from google.protobuf.internal import containers as _containers
from google.protobuf.internal import enum_type_wrapper as _enum_type_wrapper
from google.protobuf import descriptor as _descriptor
from google.protobuf import message as _message
from typing import ClassVar as _ClassVar, Iterable as _Iterable, Mapping as _Mapping, Optional as _Optional, Union as _Union

CLOSED: RequestStatus
DESCRIPTOR: _descriptor.FileDescriptor
NOT_FOUND: RequestStatus
OPENED: RequestStatus

class CheckRequestByRegionsMessage(_message.Message):
    __slots__ = ["context", "rid", "service"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    rid: str
    service: str
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

class CheckRequestMessage(_message.Message):
    __slots__ = ["context", "region", "rid", "service"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    region: str
    rid: str
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class CheckRequestResponse(_message.Message):
    __slots__ = ["status"]
    STATUS_FIELD_NUMBER: _ClassVar[int]
    status: RequestStatus
    def __init__(self, status: _Optional[_Union[RequestStatus, str]] = ...) -> None: ...

class CloseBranchMessage(_message.Message):
    __slots__ = ["bid", "region", "rid"]
    BID_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    bid: str
    region: str
    rid: str
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., region: _Optional[str] = ...) -> None: ...

class Empty(_message.Message):
    __slots__ = []
    def __init__(self) -> None: ...

class GetPreventedInconsistenciesResponse(_message.Message):
    __slots__ = ["inconsistencies"]
    INCONSISTENCIES_FIELD_NUMBER: _ClassVar[int]
    inconsistencies: int
    def __init__(self, inconsistencies: _Optional[int] = ...) -> None: ...

class RegisterBranchMessage(_message.Message):
    __slots__ = ["context", "region", "rid", "service"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    region: str
    rid: str
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchResponse(_message.Message):
    __slots__ = ["bid", "context", "rid"]
    BID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    bid: str
    context: RequestContext
    rid: str
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchesMessage(_message.Message):
    __slots__ = ["context", "regions", "rid", "service"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    REGIONS_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    regions: _containers.RepeatedScalarFieldContainer[str]
    rid: str
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., regions: _Optional[_Iterable[str]] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterBranchesResponse(_message.Message):
    __slots__ = ["bid", "context", "rid"]
    BID_FIELD_NUMBER: _ClassVar[int]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    bid: str
    context: RequestContext
    rid: str
    def __init__(self, rid: _Optional[str] = ..., bid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class RegisterRequestMessage(_message.Message):
    __slots__ = ["rid"]
    RID_FIELD_NUMBER: _ClassVar[int]
    rid: str
    def __init__(self, rid: _Optional[str] = ...) -> None: ...

class RegisterRequestResponse(_message.Message):
    __slots__ = ["context", "rid"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    rid: str
    def __init__(self, rid: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

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

class WaitRequestMessage(_message.Message):
    __slots__ = ["context", "region", "rid", "service"]
    CONTEXT_FIELD_NUMBER: _ClassVar[int]
    REGION_FIELD_NUMBER: _ClassVar[int]
    RID_FIELD_NUMBER: _ClassVar[int]
    SERVICE_FIELD_NUMBER: _ClassVar[int]
    context: RequestContext
    region: str
    rid: str
    service: str
    def __init__(self, rid: _Optional[str] = ..., service: _Optional[str] = ..., region: _Optional[str] = ..., context: _Optional[_Union[RequestContext, _Mapping]] = ...) -> None: ...

class WaitRequestResponse(_message.Message):
    __slots__ = ["prevented_inconsistency"]
    PREVENTED_INCONSISTENCY_FIELD_NUMBER: _ClassVar[int]
    prevented_inconsistency: bool
    def __init__(self, prevented_inconsistency: bool = ...) -> None: ...

class RequestStatus(int, metaclass=_enum_type_wrapper.EnumTypeWrapper):
    __slots__ = []
