from abc import abstractmethod
import os
import grpc
import threading
from proto import rendezvous_pb2 as pb
from proto import rendezvous_pb2_grpc as rdv
import time
import datetime

class RendezvousShim:
  def __init__(self, service, region, rendezvous_address, client_config):
    self.service = service
    self.region = region

    #self.rendezvous_address = os.environ[f"RENDEZVOUS_{region.replace('-','_').upper()}"]
    self.channel = grpc.insecure_channel(rendezvous_address)
    self.stub = rdv.ClientServiceStub(self.channel)

    self.running = True

    # Specific for this benchmark evaluation
    # self.inconsistency = False 

    self.metadata_validity_s = client_config['metadata_validity_s']
    self.close_branches_db_sleep_time_s = client_config['close_branches_db_sleep_time_s']
    self.close_branches_db = bool(client_config['close_branches_db'])
    self.server_unavailable_sleep_time_s = client_config['server_unavailable_sleep_time_s']


  @abstractmethod
  def find_metadata(self, item):
    pass

  @abstractmethod
  def _parse_metadata(self, item):
    pass

  @abstractmethod
  def read_all_metadata(self, item):
    pass

  def _handle_grpc_error(self, code, details):
    if code == grpc.StatusCode.UNAVAILABLE:
      print("Rendezvous server is unavailable. Retrying in 5 seconds...", flush=True)
      time.sleep(5)

      # Specific for this benchmark
      #print("[ERROR] Server is unavailable", flush=True)
      #exit(0)
    elif code in [grpc.StatusCode.INVALID_ARGUMENT, grpc.StatusCode.NOT_FOUND, grpc.StatusCode.ALREADY_EXISTS]:
      print("[ERROR] Grpc exception caught:", details, flush=True)
    else:
      print("[ERROR] Unexpected grpc exception caught:", details, flush=True)
      exit(-1)

# -----------------
# Current request
# -----------------

  def close_branch(self, bid):
    while True:
      if self.find_metadata(bid):
        break
      self.inconsistency = True

    try:
      self.stub.CloseBranch(pb.CloseBranchMessage(bid=bid, region=self.region))
      print("[DEBUG] Closed branch with prevented inconsistency =", self.inconsistency)
    except grpc.RpcError as e:
      print(f"[ERROR] Rendezvous exception closing branch: {e.details()}")
      exit(-1)

# -----------------
# Publish Subscribe
# -----------------

  def close_subscribed_branches(self):
      threads = []
      lock = threading.Lock()
      cond = threading.Condition(lock=lock)
      bids = set()

      if self.close_branches_db:
        threads.append(threading.Thread(target=self._close_branches_db))
      threads.append(threading.Thread(target=self._subscribe_branches, args=(bids, lock, cond)))
      threads.append(threading.Thread(target=self._close_subscribed_branches, args=(bids, lock, cond)))

      for t in threads:
        t.start()

      # when running flag is set to false
      for t in threads:
        t.join()

  def _close_branches_db(self):
    closed = set()
    now = datetime.datetime.utcnow()
    final_time = now + datetime.timedelta(seconds=self.metadata_validity_s)

    while self.running and now < final_time:
      items = self.read_all_metadata()

      for item in items:
        bid = self._parse_metadata(item)

        if bid not in closed:
          try:
            self.stub.CloseBranch(pb.CloseBranchMessage(bid=bid, region=self.region))
            closed.add(bid)

          except grpc.RpcError as e:
            print(f"[ERROR] Failure closing previous branches: {e.details()}", flush=True)
            self._handle_grpc_error(e.code(), e.details())

      time.sleep(self.close_branches_db_sleep_time_s)
      now = datetime.datetime.utcnow()

  def _subscribe_branches(self, bids, lock, cond):
    while self.running:
      try: 
        request = pb.SubscribeBranchesMessage(service=self.service)

        print("[DEBUG] Subcription: going to subscribe...", flush=True)
        reader = self.stub.SubscribeBranches(request)
        for response in reader:
          bid = response.bid
          #print("[DEBUG] Subcription: received bid=", bid, flush=True)
          with lock:
            bids.add(bid)
            cond.notify_all()

      except grpc.RpcError as e:
        print(f"[ERROR] Failure subscribing branches: {e.details()}", flush=True)
        self._handle_grpc_error(e.code(), e.details())

    # notify to join second thread at the end
    bids.clear()
    cond.notify_all()

  def _close_subscribed_branches(self, bids, lock, cond):
    while self.running:
      with lock:
        while len(bids) == 0 and self.running:
          print("[INFO] Closure: waiting for bids...", flush=True)
          cond.wait()
        copy = bids.copy()

      closed = []
      for bid in copy:
        if self.find_metadata(bid):
          try:
            #print(f"[DEBUG] Closing branch for bid = {bid}, service = {self.service}, region = {self.region}", flush=True)
            self.stub.CloseBranch(pb.CloseBranchMessage(bid=bid, region=self.region))
            closed.append(bid)

          except grpc.RpcError as e:
            print(f"[ERROR] Failure closing branches: {e.details()}", flush=True)
            self._handle_grpc_error(e.code(), e.details())
        else:
          self.inconsistency = True

      with lock:
        for bid in closed:
          bids.remove(bid)