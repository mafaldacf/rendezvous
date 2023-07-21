import redis

class ShimCache:
  def __init__(self, host, port, rendezvous_prefix):
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