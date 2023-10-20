import boto3
from boto3.dynamodb.conditions import Key
import botocore.exceptions
import time

class ShimDynamo:
  def __init__(self, region, client_table):
    self.conn = boto3.resource('dynamodb', region_name=region, endpoint_url=f"http://dynamodb.{region}.amazonaws.com")
    self.client_table = self.conn.Table(client_table)

  def find_metadata(self, bid):
    response = self.client_table.query(            
      IndexName='rv_bid-index',
      KeyConditionExpression=Key('rv_bid').eq(bid),
      
    )
    if 'Items' in response and len(response['Items']) > 0:
      return True
    
    #print(f"[DEBUG] [DynamoDB] Item not found: {response}", response)
    return False