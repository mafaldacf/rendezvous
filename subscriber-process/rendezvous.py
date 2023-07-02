import threading

class Rendezvous:
  def __init__(self, shim_layer):
    self.shim_layer = shim_layer
    self.threads = []

  def close_subscribed_branches(self):
    t = threading.Thread(target=self.shim_layer.close_subscribed_branches)
    self.threads.append(t)
    t.start()

  def close_branch(self, bid):
    t = threading.Thread(target=self.shim_layer.close_branch, args=(bid,))
    self.threads.append(t)
    t.start()

  # Specific for this benchmark
  def prevented_inconsistency(self):
    return self.shim_layer.inconsistency
  
  def stop(self):
    self.shim.running = False
    for t in self.threads:
      t.join()