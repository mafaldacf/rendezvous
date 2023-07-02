import boto3
import botocore
import json
from rendezvous_shim import RendezvousShim
import time

class RendezvousS3(RendezvousShim):
  def __init__(self, service, region, rendezvous_address, client_config):
    super().__init__(service, region, rendezvous_address, client_config)
    self.bucket = None
    self.s3_client = None
    self.continuation_token = None
  
  def init_conn(self, bucket, client_path, rendezvous_path):
    self.bucket = bucket
    self.s3_client = boto3.client('s3')
    self.client_path = client_path
    self.rendezvous_path = rendezvous_path

  def _bucket_key_client(self, key):
    return f"{self.client_path}/{key}" if self.client_path else key

  def _bucket_key_rendezvous(self, bid):
    return f"{self.rendezvous_path}/{bid}"
  
  def _bucket_prefix_rendezvous(self):
    return f"{self.rendezvous_path}/"
    
  
  def _find_object(self, bid, obj_key, metadata_created_at):
    try:
      response = self.s3_client.head_object(Bucket=self.bucket, Key=self._bucket_key_client(obj_key))

      # found the object version we were looking for with the correct bid
      if response.get('Metadata') and response['Metadata'].get('rendezvous') == bid:
        return True
      
      # current object corresponds to a newer version
      if response['LastModified'] >= metadata_created_at:
        return True
      
    except botocore.exceptions.ClientError as e:
      if e.response['Error']['Code'] in ['NoSuchKey','404']:
        pass
      else:
        raise
    
    return False

  def find_metadata(self, bid):
    try:
      response = self.s3_client.get_object(Bucket=self.bucket, Key=self._bucket_key_rendezvous(bid))
      obj = json.loads(response['Body'].read())

      # wait until object (post) is available
      if not self._find_object(bid, obj['obj_key'], response['LastModified']):
        self.inconsistency = True
        return None

      return obj['bid']

    except botocore.exceptions.ClientError as e:
      if e.response['Error']['Code'] in ['NoSuchKey','404']:
        self.inconsistency = True
        return None
      else:
        raise

  def _parse_metadata(self, item):
    return item['bid']

  def read_all_metadata(self):
    print("Reading all metadata...")
    
    if not self.continuation_token:
      response = self.s3_client.list_objects_v2(Bucket=self.bucket, Prefix=self._bucket_prefix_rendezvous())
    else:
      response = self.s3_client.list_objects_v2(Bucket=self.bucket, Prefix=self._bucket_prefix_rendezvous(), ContinuationToken=self.continuation_token)

    # track continuation token to continue reading in the next function call
    self.continuation_token = response.get('NextContinuationToken', None)

    print(f"[DEBUG] [S3] List objects response: {response}", flush=True)


    objects = []
    time_ago = time.time() + self.metadata_validity_s
    for obj in response.get('Contents', []):
      response = self.s3_client.get_object(Bucket=self.bucket, Key=obj['Key'])
      metadata = json.loads(response['Body'].read())

      # check if object (post) is available
      if metadata['ts'] >= time_ago and self._find_object(metadata['bid'], metadata['obj_key'], obj['LastModified']):
        objects.append(metadata)
    
    return objects
