import threading
import grpc
import threading
from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rv
import time

class DatastoreMonitor:
  def __init__(self, shim_layers, rendezvous_address, service, region):
    self.running = True
    self.threads = []
    self.shim_layers = shim_layers

    # rendezvous
    self.channel = grpc.insecure_channel(rendezvous_address)
    self.stub = rv.ClientServiceStub(self.channel)
    self.service = service
    self.region = region

    # config values
    self.server_unavailable_sleep_time_s = 5

  def monitor_branches(self):
    lock = threading.Lock()
    cond = threading.Condition(lock=lock)
    bids = set()
    self.threads.append(threading.Thread(target=self._subscribe_branches, args=(bids, lock, cond)))
    self.threads.append(threading.Thread(target=self._close_branches, args=(bids, lock, cond)))
    for t in self.threads:
      t.start()

  def stop(self):
    self.running = False
    for t in self.threads:
      t.join()

# -------
# Helpers
# -------

  def _handle_grpc_error(self, code, details):
    if code == grpc.StatusCode.UNAVAILABLE:
      print(f"Rendezvous server is unavailable. Retrying in {self.server_unavailable_sleep_time_s} seconds...", flush=True)
      time.sleep(self.server_unavailable_sleep_time_s)
      return True
    elif code in [grpc.StatusCode.INVALID_ARGUMENT, grpc.StatusCode.NOT_FOUND, grpc.StatusCode.ALREADY_EXISTS]:
      print("[ERROR] Grpc exception caught:", details, flush=True)
      return False
    print("[ERROR] Unexpected grpc exception caught:", details, flush=True)
    exit(-1)

  def _subscribe_branches(self, bids, lock, cond):
    while self.running:
      try: 
        request = pb.SubscribeMessage(service=self.service, region=self.region)
        print("[INFO] Subcription: going to subscribe...", flush=True)
        reader = self.stub.Subscribe(request)
        for response in reader:
          bid = response.bid
          tag = response.tag
          print(f"[DEBUG] Subcription: received bid: {bid}", flush=True)
          with lock:
            bids.add((bid, tag))
            cond.notify_all()

      except grpc.RpcError as e:
        print(f"[ERROR] Failure subscribing branches: {e.details()}", flush=True)
        self._handle_grpc_error(e.code(), e.details())

    # notify to join second thread at the end
    bids.clear()
    cond.notify_all()

  def _close_branches(self, bids, lock, cond):
    while self.running:
      with lock:
        while len(bids) == 0 and self.running:
          print(f"[DEBUG] Closure: waiting for bids...", flush=True)
          cond.wait()
        print(f"[DEBUG] Got {len(bids)} branches to close", flush=True)
        copy = bids.copy()

      closed = []
      for bid, tag in copy:
        try:
          if self.shim_layers[tag].find_metadata(bid):
            print(f"[DEBUG] Closing branch for bid = {bid}, service = {self.service}, region = {self.region}", flush=True)
            self.stub.CloseBranch(pb.CloseBranchMessage(bid=bid, region=self.region))
            closed.append((bid, tag))
          else:
            time.sleep(2)

        except grpc.RpcError as e:
          print(f"[ERROR] Failure closing branches: {e.details()}", flush=True)
          if not self._handle_grpc_error(e.code(), e.details()):
            closed.append((bid, tag)) # unexpected error occured and so we need to remove this (invalid) bid
        except Exception as e:
          print(f"[ERROR] Unexpected error while monitoring datastore: {e}", flush=True)
          closed.append((bid, tag)) # unexpected error occured and so we need to remove this (invalid) bid

      with lock:
        for bid, tag in closed:
          bids.remove((bid, tag))