import pymysql

class ShimMysql:
  def __init__(self, host, port, user, password, db, client_table):
    self.conn = pymysql.connect(
      host=host,
      port=port,
      user=user,
      password=password,
      db=db,
      connect_timeout=30,
      autocommit=True
    )
    self.client_table = client_table

  def find_metadata(self, bid):
    with self.conn.cursor() as cursor:
      sql = f"SELECT COUNT(*) FROM `{self.client_table}` WHERE `rdv_bid` = %s"
      cursor.execute(sql, (bid,))
      count = cursor.fetchone()[0]
      return count > 0