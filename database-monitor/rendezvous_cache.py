import json
import redis
from rendezvous_shim import RendezvousShim
import time

class RendezvousCache(RendezvousShim):
  def __init__(self, service, region, rendezvous_address, client_config):
    super().__init__(service, region, rendezvous_address, client_config)
    self.conn = None
    self.cursor = 0

  def init_conn(self, host, port, rendezvous_prefix):
    self.conn = redis.Redis(
      host=host, port=port, db=0, 
      charset="utf-8", decode_responses=True, 
      socket_connect_timeout=5, socket_timeout=5)
    self.rendezvous_prefix = rendezvous_prefix

  def _cache_key_rendezvous(self, bid):
    return f"{self.rendezvous_prefix}:{bid}"
  
  def _cache_prefix_rendezvous(self):
    return f"{self.rendezvous_prefix}:*"

  def find_metadata(self, bid):
    item = self.conn.get(self._cache_key_rendezvous(bid))
    print("[DEBUG] Got item: ", item, flush=True)
    if item:
      return True
    return False

  def _parse_metadata(self, item):
    metadata = json.loads(item)
    return metadata['bid']

  def read_all_metadata(self):
    result = []
    pipe = self.conn.pipeline()

    # track cursor to continue reading in the next function call
    self.cursor, keys = self.conn.scan(cursor=self.cursor, match=self._cache_prefix_rendezvous(), count=10000)
    for key in keys:
      pipe.get(key)

    items = pipe.execute()

    #hardcoded filter
    time_ago = time.time() + self.metadata_validity_s
    for item in items:
      metadata = json.loads(item)
      if metadata['ts'] >= time_ago:
        result.append(item)
    return result
