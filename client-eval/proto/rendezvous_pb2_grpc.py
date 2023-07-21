# Generated by the gRPC Python protocol compiler plugin. DO NOT EDIT!
"""Client and server classes corresponding to protobuf-defined services."""
import grpc

from proto import rendezvous_pb2 as rendezvous_dot_protos_dot_rendezvous__pb2


class ClientServiceStub(object):
    """Missing associated documentation comment in .proto file."""

    def __init__(self, channel):
        """Constructor.

        Args:
            channel: A grpc.Channel.
        """
        self.SubscribeBranches = channel.unary_stream(
                '/rendezvous.ClientService/SubscribeBranches',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesResponse.FromString,
                )
        self.CloseBranches = channel.stream_unary(
                '/rendezvous.ClientService/CloseBranches',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.Empty.FromString,
                )
        self.RegisterRequest = channel.unary_unary(
                '/rendezvous.ClientService/RegisterRequest',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestResponse.FromString,
                )
        self.RegisterBranch = channel.unary_unary(
                '/rendezvous.ClientService/RegisterBranch',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchResponse.FromString,
                )
        self.RegisterBranches = channel.unary_unary(
                '/rendezvous.ClientService/RegisterBranches',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesResponse.FromString,
                )
        self.RegisterBranchesDatastores = channel.unary_unary(
                '/rendezvous.ClientService/RegisterBranchesDatastores',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresResponse.FromString,
                )
        self.CloseBranch = channel.unary_unary(
                '/rendezvous.ClientService/CloseBranch',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.Empty.FromString,
                )
        self.WaitRequest = channel.unary_unary(
                '/rendezvous.ClientService/WaitRequest',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestResponse.FromString,
                )
        self.CheckRequest = channel.unary_unary(
                '/rendezvous.ClientService/CheckRequest',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestResponse.FromString,
                )
        self.CheckRequestByRegions = channel.unary_unary(
                '/rendezvous.ClientService/CheckRequestByRegions',
                request_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsMessage.SerializeToString,
                response_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsResponse.FromString,
                )


class ClientServiceServicer(object):
    """Missing associated documentation comment in .proto file."""

    def SubscribeBranches(self, request, context):
        """Streaming 
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CloseBranches(self, request_iterator, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def RegisterRequest(self, request, context):
        """Unary RPCs 
        """
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def RegisterBranch(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def RegisterBranches(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def RegisterBranchesDatastores(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CloseBranch(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def WaitRequest(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CheckRequest(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')

    def CheckRequestByRegions(self, request, context):
        """Missing associated documentation comment in .proto file."""
        context.set_code(grpc.StatusCode.UNIMPLEMENTED)
        context.set_details('Method not implemented!')
        raise NotImplementedError('Method not implemented!')


def add_ClientServiceServicer_to_server(servicer, server):
    rpc_method_handlers = {
            'SubscribeBranches': grpc.unary_stream_rpc_method_handler(
                    servicer.SubscribeBranches,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesResponse.SerializeToString,
            ),
            'CloseBranches': grpc.stream_unary_rpc_method_handler(
                    servicer.CloseBranches,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.Empty.SerializeToString,
            ),
            'RegisterRequest': grpc.unary_unary_rpc_method_handler(
                    servicer.RegisterRequest,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestResponse.SerializeToString,
            ),
            'RegisterBranch': grpc.unary_unary_rpc_method_handler(
                    servicer.RegisterBranch,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchResponse.SerializeToString,
            ),
            'RegisterBranches': grpc.unary_unary_rpc_method_handler(
                    servicer.RegisterBranches,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesResponse.SerializeToString,
            ),
            'RegisterBranchesDatastores': grpc.unary_unary_rpc_method_handler(
                    servicer.RegisterBranchesDatastores,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresResponse.SerializeToString,
            ),
            'CloseBranch': grpc.unary_unary_rpc_method_handler(
                    servicer.CloseBranch,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.Empty.SerializeToString,
            ),
            'WaitRequest': grpc.unary_unary_rpc_method_handler(
                    servicer.WaitRequest,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestResponse.SerializeToString,
            ),
            'CheckRequest': grpc.unary_unary_rpc_method_handler(
                    servicer.CheckRequest,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestResponse.SerializeToString,
            ),
            'CheckRequestByRegions': grpc.unary_unary_rpc_method_handler(
                    servicer.CheckRequestByRegions,
                    request_deserializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsMessage.FromString,
                    response_serializer=rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsResponse.SerializeToString,
            ),
    }
    generic_handler = grpc.method_handlers_generic_handler(
            'rendezvous.ClientService', rpc_method_handlers)
    server.add_generic_rpc_handlers((generic_handler,))


 # This class is part of an EXPERIMENTAL API.
class ClientService(object):
    """Missing associated documentation comment in .proto file."""

    @staticmethod
    def SubscribeBranches(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_stream(request, target, '/rendezvous.ClientService/SubscribeBranches',
            rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.SubscribeBranchesResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CloseBranches(request_iterator,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.stream_unary(request_iterator, target, '/rendezvous.ClientService/CloseBranches',
            rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.Empty.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def RegisterRequest(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/RegisterRequest',
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterRequestResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def RegisterBranch(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/RegisterBranch',
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def RegisterBranches(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/RegisterBranches',
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def RegisterBranchesDatastores(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/RegisterBranchesDatastores',
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.RegisterBranchesDatastoresResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CloseBranch(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/CloseBranch',
            rendezvous_dot_protos_dot_rendezvous__pb2.CloseBranchMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.Empty.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def WaitRequest(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/WaitRequest',
            rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.WaitRequestResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CheckRequest(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/CheckRequest',
            rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)

    @staticmethod
    def CheckRequestByRegions(request,
            target,
            options=(),
            channel_credentials=None,
            call_credentials=None,
            insecure=False,
            compression=None,
            wait_for_ready=None,
            timeout=None,
            metadata=None):
        return grpc.experimental.unary_unary(request, target, '/rendezvous.ClientService/CheckRequestByRegions',
            rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsMessage.SerializeToString,
            rendezvous_dot_protos_dot_rendezvous__pb2.CheckRequestByRegionsResponse.FromString,
            options, channel_credentials,
            insecure, call_credentials, compression, wait_for_ready, timeout, metadata)
