from rendezvous.protos import rendezvous_pb2 as pb
from typing import List

# -------------------
# context propagation 
# -------------------

def context_msg_to_bytes(context: pb.RequestContext):
  return context.SerializeToString()

def context_bytes_to_msg(context: pb.RequestContext):
  message = pb.RequestContext()
  message.ParseFromString(context)
  return message

def context_msg_to_string(context: pb.RequestContext):
  return context.SerializeToString().decode('utf-8')

def context_string_to_msg(context: pb.RequestContext):
  message = pb.RequestContext()
  message.ParseFromString(context.encode('utf-8'))
  return message

# -----------
# async zones
# -----------

def next_async_context(context: pb.RequestContext) -> pb.RequestContext:
    if context.async_zone == "":
      context.async_zone = "r"
    
    new_context = pb.RequestContext()
    new_context.CopyFrom(context)
    
    new_context.async_zone += ":" + str(context.num_sub_zones)
    context.num_sub_zones += 1
    return new_context

def next_async_contexts(context: pb.RequestContext, num: int) -> List[pb.RequestContext]:
    lst = []

    if context.async_zone == "":
      context.async_zone = "r"

    for i in range (context.num_sub_zones, context.num_sub_zones + num):
        new_context = pb.RequestContext()
        new_context.CopyFrom(context)
        new_context.async_zone += ":" + str(i)
        new_context.num_sub_zones = 0
        lst.append(new_context)

    context.num_sub_zones += num
    return lst