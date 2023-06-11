import boto3
from rendezvous_shim import RendezvousShim
from boto3.dynamodb.conditions import Attr
import time

#DYNAMO_RENDEZVOUS_TABLE = os.environ['DYNAMO_RENDEZVOUS_TABLE']
#DYNAMO_POST_TABLE_NAME = os.environ['DYNAMO_POST_TABLE_NAME']
DYNAMO_RENDEZVOUS_TABLE = 'rendezvous'
DYNAMO_POST_TABLE_NAME = None

class RendezvousDynamo(RendezvousShim):
  def __init__(self, service, region, rendezvous_address, client_config):
    super().__init__(service, region, rendezvous_address, client_config)
    self.conn = None
    self.rendezvous_table = None
    self.table = None
    self.last_evaluated_key = None

  def init_conn(self, region, table):
    self.conn = boto3.resource('dynamodb', region_name=region, endpoint_url=f"http://dynamodb.{region}.amazonaws.com")
    self.table = self.conn.Table(table)
    self.rendezvous_table = self.conn.Table(DYNAMO_RENDEZVOUS_TABLE)

  def _find_object(self, bid, obj_key):
    response = self.table.get_item(Key={'k': obj_key}, AttributesToGet=['rendezvous'])
    if 'Item' in response and bid == response['Item']['rendezvous']:
      return True
    return False

  def find_metadata(self, bid):
    response = self.rendezvous_table.get_item(Key={'bid': bid})
    if 'Item' in response:
      item = response['Item']

      # wait until object (post) is available
      if not self._find_object(bid, item['obj_key']):
        return False

      return True
    
    return False

  def _parse_metadata(self, item):
    return item['bid']

  def read_all_metadata(self):
    print("Reading all metadata...")
    time_ago = int(time.time()) - self.metadata_validity_s

    items = []
    if not self.last_evaluated_key:
      response = self.rendezvous_table.scan(FilterExpression=Attr("ts").gte(time_ago))
    else:
      response = self.rendezvous_table.scan(FilterExpression=Attr("ts").gte(time_ago), ExclusiveStartKey=self.last_evaluated_key)

    print(f"[DEBUG] [DYNAMO] Scanning response: {response}", flush=True)

    # track last evaluated key to continue scanning in the next function call
    self.last_evaluated_key = response.get('LastEvaluatedKey', None)
    
    for item in response.get('Items', []):
      # check if object (post) is available
      if self._find_object(item['bid'], item['obj_key']):
        items.append(item)

    return items