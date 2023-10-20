import boto3
import botocore

class ShimS3:
  def __init__(self, bucket, rendezvous_path):
    self.bucket = bucket
    self.s3_client = boto3.client('s3')
    self.rendezvous_path = rendezvous_path

  def _bucket_key_rendezvous(self, bid):
    return f"{self.rendezvous_path}/{bid}"

  def _find_object(self, bid, obj_key, metadata_created_at):
    try:
      response = self.s3_client.head_object(Bucket=self.bucket, Key=obj_key)
      print(f"[DEBUG] Found object: {response}", flush=True)
      return True
      
    except botocore.exceptions.ClientError as e:
      if e.response['Error']['Code'] in ['NoSuchKey','404']:
        pass
      else:
        print(f"[ERROR] [S3] {e}")
        raise e
    
    return False

  def find_metadata(self, bid):
    try:
      response = self.s3_client.get_object(Bucket=self.bucket, Key=self._bucket_key_rendezvous(bid))
      obj_key = str(response['Body'].read().decode('utf-8'))
      # wait until object (post) is available
      if not self._find_object(bid, obj_key, response['LastModified']):
        return False
      return True

    except botocore.exceptions.ClientError as e:
      if e.response['Error']['Code'] in ['NoSuchKey','404']:
        return None
      else:
        print(f"[ERROR] [S3] {e}")
        raise e

  def _parse_metadata(self, item):
    return item['bid']
