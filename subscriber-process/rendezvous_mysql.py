import os
import pymysql
from rendezvous_shim import RendezvousShim

#MYSQL_RENDEZVOUS_TABLE = os.environ['MYSQL_RENDEZVOUS_TABLE']
#RENDEZVOUS_ADDRESS = os.environ['RENDEZVOUS_ADDRESS']
MYSQL_RENDEZVOUS_TABLE = 'blobs'

class RendezvousMysql(RendezvousShim):
  def __init__(self, service, region, rendezvous_address, client_config):
    super().__init__(service, region, rendezvous_address, client_config)
    self.conn = None
    self.offset = 0
    self.max_records = 10000

  def init_conn(self, host, port, user, password, db, rendezvous_table):
    self.conn = pymysql.connect(
      host=host,
      port=port,
      user=user,
      password=password,
      db=db,
      connect_timeout=30,
      autocommit=True
    )
    self.rendezvous_table = rendezvous_table


  def find_metadata(self, bid):
    with self.conn.cursor() as cursor:
      sql = f"SELECT COUNT(*) FROM `{self.rendezvous_table}` WHERE `bid` = %s"
      cursor.execute(sql, (bid,))
      count = cursor.fetchone()[0]
      return count > 0

  def _parse_metadata(self, record):
    return record
  
  def read_all_metadata(self):
    with self.conn.cursor() as cursor:
      # fetch non-expired metadata
      # is it worth ordering just to control an offset??
      sql = f"SELECT `bid` FROM `{self.rendezvous_table}` WHERE `ts` >= DATE_SUB(NOW(), INTERVAL %s SECOND) ORDER BY `ts` LIMIT %s,%s"
      cursor.execute(sql, (self.metadata_validity_s, self.offset, self.max_records))
      records = cursor.fetchall()
      num_records = cursor.rowcount

      if cursor.rowcount == self.max_records:
        # track next offset to continue reading in the next function call
        self.offset += num_records
      else:
        # all records were read
        # reset offset to make sure we do not miss any 'old' records inserted in the meantime
        self.offset = 0
      return records
