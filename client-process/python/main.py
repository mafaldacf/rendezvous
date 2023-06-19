import argparse
import yaml
from rendezvous_dynamo import RendezvousDynamo
from rendezvous_mysql import RendezvousMysql
from rendezvous_cache import RendezvousCache
from rendezvous_s3 import RendezvousS3
from rendezvous_shim import RendezvousShim
from rendezvous import Rendezvous

DATASTORE_SHIMS = {
    'aws': {
        'cache': RendezvousCache,
        'dynamo': RendezvousDynamo,
        's3': RendezvousS3,
        'mysql': RendezvousMysql,
    }
}

def load_client_config(region):
    with open(f'config/client-{region}.yaml', 'r') as file:
        config = yaml.safe_load(file)
        return config

def load_connections_config(region, cloud_provider, datastore):
    with open(f'config/connections-{region}.yaml', 'r') as file:
        info = yaml.safe_load(file)
        return info[cloud_provider][datastore], info['rendezvous']

def init_process(cloud_provider, region, datastore):
    datastore_info, rendezvous_info = load_connections_config(region, cloud_provider, datastore)
    client_config = load_client_config(region)

    shim_layer = DATASTORE_SHIMS[cloud_provider][datastore]
    shim_layer = shim_layer(client_config['service'], client_config['region'], rendezvous_info['address'], client_config)
    shim_layer.init_conn(**datastore_info)

    rendezvous_api = Rendezvous(shim_layer)
    rendezvous_api.close_subscribed_branches()

# Usage: python3 main.py -cp aws -r eu -d dynamo

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    cloud_provider_group = parser.add_argument_group('Cloud Provider')
    cloud_provider_group.add_argument('-cp', '--cloud-provider', choices=['aws'], help='Specify the cloud provider')

    region_group = parser.add_argument_group('Region')
    region_group.add_argument('-r', '--region', choices=['eu', 'us'], help='Specify the region')

    args, cloud_provider_args = parser.parse_known_args()
    if args.cloud_provider == 'aws':
        datastore_group = parser.add_argument_group('Datastore')
        datastore_group.add_argument('-d', '--datastore', choices=['cache', 'dynamo', 'mysql', 's3'], required=True, help='Specify the type of datastore')

    cloud_provider_args = parser.parse_args(cloud_provider_args)
    init_process(args.cloud_provider, args.region, cloud_provider_args.datastore)
