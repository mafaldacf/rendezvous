# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: rendezvous/protos/rendezvous.proto
"""Generated protocol buffer code."""
from google.protobuf.internal import builder as _builder
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\"rendezvous/protos/rendezvous.proto\x12\nrendezvous\"I\n\x0cRegionStatus\x12\x0e\n\x06region\x18\x01 \x01(\t\x12)\n\x06status\x18\x02 \x01(\x0e\x32\x19.rendezvous.RequestStatus\"\x07\n\x05\x45mpty\"2\n\x16RegisterRequestMessage\x12\x10\n\x03rid\x18\x01 \x01(\tH\x00\x88\x01\x01\x42\x06\n\x04_rid\"&\n\x17RegisterRequestResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\"c\n\x15RegisterBranchMessage\x12\x10\n\x03rid\x18\x01 \x01(\tH\x00\x88\x01\x01\x12\x14\n\x07service\x18\x02 \x01(\tH\x01\x88\x01\x01\x12\x0e\n\x06region\x18\x03 \x01(\tB\x06\n\x04_ridB\n\n\x08_service\"2\n\x16RegisterBranchResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0b\n\x03\x62id\x18\x02 \x01(\t\"f\n\x17RegisterBranchesMessage\x12\x10\n\x03rid\x18\x01 \x01(\tH\x00\x88\x01\x01\x12\x14\n\x07service\x18\x02 \x01(\tH\x01\x88\x01\x01\x12\x0f\n\x07regions\x18\x03 \x03(\tB\x06\n\x04_ridB\n\n\x08_service\"4\n\x18RegisterBranchesResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0b\n\x03\x62id\x18\x02 \x01(\t\"S\n\x12\x43loseBranchMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x14\n\x07service\x18\x02 \x01(\tH\x00\x88\x01\x01\x12\x0e\n\x06region\x18\x03 \x01(\tB\n\n\x08_service\"c\n\x12WaitRequestMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x14\n\x07service\x18\x02 \x01(\tH\x00\x88\x01\x01\x12\x13\n\x06region\x18\x03 \x01(\tH\x01\x88\x01\x01\x42\n\n\x08_serviceB\t\n\x07_region\"5\n\x13WaitRequestResponse\x12\x1e\n\x16preventedInconsistency\x18\x01 \x01(\x08\"d\n\x13\x43heckRequestMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x14\n\x07service\x18\x02 \x01(\tH\x00\x88\x01\x01\x12\x13\n\x06region\x18\x03 \x01(\tH\x01\x88\x01\x01\x42\n\n\x08_serviceB\t\n\x07_region\"A\n\x14\x43heckRequestResponse\x12)\n\x06status\x18\x01 \x01(\x0e\x32\x19.rendezvous.RequestStatus\"M\n\x1c\x43heckRequestByRegionsMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x14\n\x07service\x18\x02 \x01(\tH\x00\x88\x01\x01\x42\n\n\x08_service\"O\n\x1d\x43heckRequestByRegionsResponse\x12.\n\x0cregionStatus\x18\x01 \x03(\x0b\x32\x18.rendezvous.RegionStatus\">\n#GetPreventedInconsistenciesResponse\x12\x17\n\x0finconsistencies\x18\x01 \x01(\x03*\'\n\rRequestStatus\x12\n\n\x06OPENED\x10\x00\x12\n\n\x06\x43LOSED\x10\x01\x32\xdd\x05\n\x11RendezvousService\x12Z\n\x0fregisterRequest\x12\".rendezvous.RegisterRequestMessage\x1a#.rendezvous.RegisterRequestResponse\x12W\n\x0eregisterBranch\x12!.rendezvous.RegisterBranchMessage\x1a\".rendezvous.RegisterBranchResponse\x12]\n\x10registerBranches\x12#.rendezvous.RegisterBranchesMessage\x1a$.rendezvous.RegisterBranchesResponse\x12@\n\x0b\x63loseBranch\x12\x1e.rendezvous.CloseBranchMessage\x1a\x11.rendezvous.Empty\x12N\n\x0bwaitRequest\x12\x1e.rendezvous.WaitRequestMessage\x1a\x1f.rendezvous.WaitRequestResponse\x12Q\n\x0c\x63heckRequest\x12\x1f.rendezvous.CheckRequestMessage\x1a .rendezvous.CheckRequestResponse\x12l\n\x15\x63heckRequestByRegions\x12(.rendezvous.CheckRequestByRegionsMessage\x1a).rendezvous.CheckRequestByRegionsResponse\x12\x61\n\x1bgetPreventedInconsistencies\x12\x11.rendezvous.Empty\x1a/.rendezvous.GetPreventedInconsistenciesResponseb\x06proto3')

_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, globals())
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'rendezvous.protos.rendezvous_pb2', globals())
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  _REQUESTSTATUS._serialized_start=1171
  _REQUESTSTATUS._serialized_end=1210
  _REGIONSTATUS._serialized_start=50
  _REGIONSTATUS._serialized_end=123
  _EMPTY._serialized_start=125
  _EMPTY._serialized_end=132
  _REGISTERREQUESTMESSAGE._serialized_start=134
  _REGISTERREQUESTMESSAGE._serialized_end=184
  _REGISTERREQUESTRESPONSE._serialized_start=186
  _REGISTERREQUESTRESPONSE._serialized_end=224
  _REGISTERBRANCHMESSAGE._serialized_start=226
  _REGISTERBRANCHMESSAGE._serialized_end=325
  _REGISTERBRANCHRESPONSE._serialized_start=327
  _REGISTERBRANCHRESPONSE._serialized_end=377
  _REGISTERBRANCHESMESSAGE._serialized_start=379
  _REGISTERBRANCHESMESSAGE._serialized_end=481
  _REGISTERBRANCHESRESPONSE._serialized_start=483
  _REGISTERBRANCHESRESPONSE._serialized_end=535
  _CLOSEBRANCHMESSAGE._serialized_start=537
  _CLOSEBRANCHMESSAGE._serialized_end=620
  _WAITREQUESTMESSAGE._serialized_start=622
  _WAITREQUESTMESSAGE._serialized_end=721
  _WAITREQUESTRESPONSE._serialized_start=723
  _WAITREQUESTRESPONSE._serialized_end=776
  _CHECKREQUESTMESSAGE._serialized_start=778
  _CHECKREQUESTMESSAGE._serialized_end=878
  _CHECKREQUESTRESPONSE._serialized_start=880
  _CHECKREQUESTRESPONSE._serialized_end=945
  _CHECKREQUESTBYREGIONSMESSAGE._serialized_start=947
  _CHECKREQUESTBYREGIONSMESSAGE._serialized_end=1024
  _CHECKREQUESTBYREGIONSRESPONSE._serialized_start=1026
  _CHECKREQUESTBYREGIONSRESPONSE._serialized_end=1105
  _GETPREVENTEDINCONSISTENCIESRESPONSE._serialized_start=1107
  _GETPREVENTEDINCONSISTENCIESRESPONSE._serialized_end=1169
  _RENDEZVOUSSERVICE._serialized_start=1213
  _RENDEZVOUSSERVICE._serialized_end=1946
# @@protoc_insertion_point(module_scope)
