# -*- coding: utf-8 -*-
# Generated by the protocol buffer compiler.  DO NOT EDIT!
# source: rendezvous/protos/rendezvous.proto
"""Generated protocol buffer code."""
from google.protobuf import descriptor as _descriptor
from google.protobuf import descriptor_pool as _descriptor_pool
from google.protobuf import symbol_database as _symbol_database
from google.protobuf.internal import builder as _builder
# @@protoc_insertion_point(imports)

_sym_db = _symbol_database.Default()




DESCRIPTOR = _descriptor_pool.Default().AddSerializedFile(b'\n\"rendezvous/protos/rendezvous.proto\x12\nrendezvous\"\x07\n\x05\x45mpty\"}\n\x0eRequestContext\x12:\n\x08versions\x18\x01 \x03(\x0b\x32(.rendezvous.RequestContext.VersionsEntry\x1a/\n\rVersionsEntry\x12\x0b\n\x03key\x18\x01 \x01(\t\x12\r\n\x05value\x18\x02 \x01(\x05:\x02\x38\x01\"3\n\x10SubscribeMessage\x12\x0f\n\x07service\x18\x01 \x01(\t\x12\x0e\n\x06region\x18\x02 \x01(\t\"-\n\x11SubscribeResponse\x12\x0b\n\x03\x62id\x18\x01 \x01(\t\x12\x0b\n\x03tag\x18\x02 \x01(\t\"%\n\x16RegisterRequestMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\"S\n\x17RegisterRequestResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12+\n\x07\x63ontext\x18\x02 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"\x80\x01\n\x15RegisterBranchMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0f\n\x07service\x18\x02 \x01(\t\x12\x0b\n\x03tag\x18\x03 \x01(\t\x12\x0f\n\x07regions\x18\x04 \x03(\t\x12+\n\x07\x63ontext\x18\x05 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"_\n\x16RegisterBranchResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0b\n\x03\x62id\x18\x02 \x01(\t\x12+\n\x07\x63ontext\x18\x03 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"E\n\x12\x44\x61tastoreBranching\x12\x11\n\tdatastore\x18\x01 \x01(\t\x12\x0b\n\x03tag\x18\x02 \x01(\t\x12\x0f\n\x07regions\x18\x03 \x03(\t\"\xc2\x01\n!RegisterBranchesDatastoresMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x12\n\ndatastores\x18\x02 \x03(\t\x12\x0f\n\x07regions\x18\x03 \x03(\t\x12\x0c\n\x04tags\x18\x04 \x03(\t\x12\x30\n\x08\x62ranches\x18\x05 \x03(\x0b\x32\x1e.rendezvous.DatastoreBranching\x12+\n\x07\x63ontext\x18\x06 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"l\n\"RegisterBranchesDatastoresResponse\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0c\n\x04\x62ids\x18\x02 \x03(\t\x12+\n\x07\x63ontext\x18\x03 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"^\n\x12\x43loseBranchMessage\x12\x0b\n\x03\x62id\x18\x01 \x01(\t\x12\x0e\n\x06region\x18\x02 \x01(\t\x12+\n\x07\x63ontext\x18\x03 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"\x8d\x01\n\x12WaitRequestMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0f\n\x07service\x18\x02 \x01(\t\x12\x0e\n\x06region\x18\x03 \x01(\t\x12\x0b\n\x03tag\x18\x04 \x01(\t\x12\x0f\n\x07timeout\x18\x05 \x01(\x05\x12+\n\x07\x63ontext\x18\x07 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"I\n\x13WaitRequestResponse\x12\x1f\n\x17prevented_inconsistency\x18\x01 \x01(\x08\x12\x11\n\ttimed_out\x18\x02 \x01(\x08\"\x82\x01\n\x13\x43heckRequestMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0f\n\x07service\x18\x02 \x01(\t\x12\x0e\n\x06region\x18\x03 \x01(\t\x12\x10\n\x08\x64\x65tailed\x18\x04 \x01(\x08\x12+\n\x07\x63ontext\x18\x05 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"A\n\x14\x43heckRequestResponse\x12)\n\x06status\x18\x01 \x01(\x0e\x32\x19.rendezvous.RequestStatus\"{\n\x1c\x43heckRequestByRegionsMessage\x12\x0b\n\x03rid\x18\x01 \x01(\t\x12\x0f\n\x07service\x18\x02 \x01(\t\x12\x10\n\x08\x64\x65tailed\x18\x03 \x01(\x08\x12+\n\x07\x63ontext\x18\x04 \x01(\x0b\x32\x1a.rendezvous.RequestContext\"\xb6\x01\n\x1d\x43heckRequestByRegionsResponse\x12I\n\x08statuses\x18\x01 \x03(\x0b\x32\x37.rendezvous.CheckRequestByRegionsResponse.StatusesEntry\x1aJ\n\rStatusesEntry\x12\x0b\n\x03key\x18\x01 \x01(\t\x12(\n\x05value\x18\x02 \x01(\x0e\x32\x19.rendezvous.RequestStatus:\x02\x38\x01*4\n\rRequestStatus\x12\n\n\x06OPENED\x10\x00\x12\n\n\x06\x43LOSED\x10\x01\x12\x0b\n\x07UNKNOWN\x10\x02\x32\xe0\x05\n\rClientService\x12J\n\tSubscribe\x12\x1c.rendezvous.SubscribeMessage\x1a\x1d.rendezvous.SubscribeResponse0\x01\x12Z\n\x0fRegisterRequest\x12\".rendezvous.RegisterRequestMessage\x1a#.rendezvous.RegisterRequestResponse\x12W\n\x0eRegisterBranch\x12!.rendezvous.RegisterBranchMessage\x1a\".rendezvous.RegisterBranchResponse\x12{\n\x1aRegisterBranchesDatastores\x12-.rendezvous.RegisterBranchesDatastoresMessage\x1a..rendezvous.RegisterBranchesDatastoresResponse\x12@\n\x0b\x43loseBranch\x12\x1e.rendezvous.CloseBranchMessage\x1a\x11.rendezvous.Empty\x12N\n\x0bWaitRequest\x12\x1e.rendezvous.WaitRequestMessage\x1a\x1f.rendezvous.WaitRequestResponse\x12Q\n\x0c\x43heckRequest\x12\x1f.rendezvous.CheckRequestMessage\x1a .rendezvous.CheckRequestResponse\x12l\n\x15\x43heckRequestByRegions\x12(.rendezvous.CheckRequestByRegionsMessage\x1a).rendezvous.CheckRequestByRegionsResponseb\x06proto3')

_globals = globals()
_builder.BuildMessageAndEnumDescriptors(DESCRIPTOR, _globals)
_builder.BuildTopDescriptorsAndMessages(DESCRIPTOR, 'rendezvous.protos.rendezvous_pb2', _globals)
if _descriptor._USE_C_DESCRIPTORS == False:

  DESCRIPTOR._options = None
  _REQUESTCONTEXT_VERSIONSENTRY._options = None
  _REQUESTCONTEXT_VERSIONSENTRY._serialized_options = b'8\001'
  _CHECKREQUESTBYREGIONSRESPONSE_STATUSESENTRY._options = None
  _CHECKREQUESTBYREGIONSRESPONSE_STATUSESENTRY._serialized_options = b'8\001'
  _globals['_REQUESTSTATUS']._serialized_start=1841
  _globals['_REQUESTSTATUS']._serialized_end=1893
  _globals['_EMPTY']._serialized_start=50
  _globals['_EMPTY']._serialized_end=57
  _globals['_REQUESTCONTEXT']._serialized_start=59
  _globals['_REQUESTCONTEXT']._serialized_end=184
  _globals['_REQUESTCONTEXT_VERSIONSENTRY']._serialized_start=137
  _globals['_REQUESTCONTEXT_VERSIONSENTRY']._serialized_end=184
  _globals['_SUBSCRIBEMESSAGE']._serialized_start=186
  _globals['_SUBSCRIBEMESSAGE']._serialized_end=237
  _globals['_SUBSCRIBERESPONSE']._serialized_start=239
  _globals['_SUBSCRIBERESPONSE']._serialized_end=284
  _globals['_REGISTERREQUESTMESSAGE']._serialized_start=286
  _globals['_REGISTERREQUESTMESSAGE']._serialized_end=323
  _globals['_REGISTERREQUESTRESPONSE']._serialized_start=325
  _globals['_REGISTERREQUESTRESPONSE']._serialized_end=408
  _globals['_REGISTERBRANCHMESSAGE']._serialized_start=411
  _globals['_REGISTERBRANCHMESSAGE']._serialized_end=539
  _globals['_REGISTERBRANCHRESPONSE']._serialized_start=541
  _globals['_REGISTERBRANCHRESPONSE']._serialized_end=636
  _globals['_DATASTOREBRANCHING']._serialized_start=638
  _globals['_DATASTOREBRANCHING']._serialized_end=707
  _globals['_REGISTERBRANCHESDATASTORESMESSAGE']._serialized_start=710
  _globals['_REGISTERBRANCHESDATASTORESMESSAGE']._serialized_end=904
  _globals['_REGISTERBRANCHESDATASTORESRESPONSE']._serialized_start=906
  _globals['_REGISTERBRANCHESDATASTORESRESPONSE']._serialized_end=1014
  _globals['_CLOSEBRANCHMESSAGE']._serialized_start=1016
  _globals['_CLOSEBRANCHMESSAGE']._serialized_end=1110
  _globals['_WAITREQUESTMESSAGE']._serialized_start=1113
  _globals['_WAITREQUESTMESSAGE']._serialized_end=1254
  _globals['_WAITREQUESTRESPONSE']._serialized_start=1256
  _globals['_WAITREQUESTRESPONSE']._serialized_end=1329
  _globals['_CHECKREQUESTMESSAGE']._serialized_start=1332
  _globals['_CHECKREQUESTMESSAGE']._serialized_end=1462
  _globals['_CHECKREQUESTRESPONSE']._serialized_start=1464
  _globals['_CHECKREQUESTRESPONSE']._serialized_end=1529
  _globals['_CHECKREQUESTBYREGIONSMESSAGE']._serialized_start=1531
  _globals['_CHECKREQUESTBYREGIONSMESSAGE']._serialized_end=1654
  _globals['_CHECKREQUESTBYREGIONSRESPONSE']._serialized_start=1657
  _globals['_CHECKREQUESTBYREGIONSRESPONSE']._serialized_end=1839
  _globals['_CHECKREQUESTBYREGIONSRESPONSE_STATUSESENTRY']._serialized_start=1765
  _globals['_CHECKREQUESTBYREGIONSRESPONSE_STATUSESENTRY']._serialized_end=1839
  _globals['_CLIENTSERVICE']._serialized_start=1896
  _globals['_CLIENTSERVICE']._serialized_end=2632
# @@protoc_insertion_point(module_scope)