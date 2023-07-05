import boto3
from rendezvous_shim import RendezvousShim
from boto3.dynamodb.conditions import Attr, Key
import time

class RendezvousDynamo(RendezvousShim):
  def __init__(self, service, region, rendezvous_address, client_config):
    super().__init__(service, region, rendezvous_address, client_config)
    self.conn = None
    self.client_table = None
    self.rendezvous_table = None
    self.last_evaluated_key = None

  def init_conn(self, region, client_table, rendezvous_table):
    self.conn = boto3.resource('dynamodb', region_name=region, endpoint_url=f"http://dynamodb.{region}.amazonaws.com")
    self.client_table = self.conn.Table(client_table)
    self.rendezvous_table = self.conn.Table(rendezvous_table)

  def _find_object(self, bid, obj_key):
    response = self.client_table.get_item(Key={'k': obj_key}, AttributesToGet=['rdv_bid'])
    if 'Item' in response and bid == response['Item']['rdv_bid']:
      return True
    return False

  def find_metadata(self, bid):
    try:
      #response = self.rendezvous_table.get_item(Key={'bid': bid})
      response = self.client_table.query(            
        IndexName='rdv_bid-index',
        KeyConditionExpression=Key('rdv_bid').eq(bid),     
      )

      if 'Items' in response and len(response['Items']) > 0:
        #print("Found item!", response['Items'][0], flush=True)
        return True
      #if 'Item' in response:
      #  item = response['Item']

        # wait until object (post) is available
      #  if self._find_object(bid, item['obj_key']):
      #    return True
    except Exception as e:
      print(f"[ERROR] [Dynamo] {e}")
    #print("[DynamoDB] Item not found :(", response)
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