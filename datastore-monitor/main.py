import argparse
import yaml
from shim_dynamo import ShimDynamo
from shim_mysql import ShimMysql
from shim_cache import ShimCache
from shim_s3 import ShimS3
from monitor import DatastoreMonitor

REGIONS_FULL_NAMES = {
    'eu': 'eu-central-1',
    'us': 'us-east-1'
}

SHIM_LAYERS = {
    'cache': ShimCache,
    'dynamo': ShimDynamo,
    's3': ShimS3,
    'mysql': ShimMysql,
}

def load_client_config():
    with open(f'config/client.yaml', 'r') as file:
        config = yaml.safe_load(file)
        return config

def load_connections_config(datastore, region):
    with open(f'config/connections.yaml', 'r') as file:
        info = yaml.safe_load(file)
        info_datastore = info[datastore]
    
        # workaround to parse datastore hostnames specific to current region
        # there are probably cleaner ways to do this
        keys_to_remove = []
        key_to_insert = None
        value_to_insert = None
        for key in info_datastore:
            # keep hostname in current region by removing the region name from the key
            if '_region_' in key:
                if region in key:
                    key_to_insert = key.split('_region_')[0]
                    value_to_insert = info_datastore[key]
                keys_to_remove.append(key)
        # remove hostnames with keys for all regions
        for key in keys_to_remove:
            info_datastore.pop(key)
        # insert the new hostname key for the current region
        if key_to_insert and value_to_insert:
            info_datastore[key_to_insert] = value_to_insert

        # parse rendezvous address specific to current region
        rendezvous_address = info['rendezvous'][region]
        return info_datastore, rendezvous_address 

def init_monitor(datastore, region, no_consistency_checks):
    region = REGIONS_FULL_NAMES[region]
    client_config = load_client_config()
    info_datastore, rendezvous_address = load_connections_config(datastore, region)
    shim_layers = {
        '': SHIM_LAYERS[datastore](**info_datastore) # empty tag for testing purposes
    }
    monitor = DatastoreMonitor(shim_layers, rendezvous_address, client_config['service'], region, no_consistency_checks)

    if no_consistency_checks:
        print(f'> Starting datastore monitor for {datastore} @ {region} (no consistency checks)')
    else:
        print(f'> Starting datastore monitor for {datastore} @ {region}')

    monitor.monitor_branches()

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    datastore_group = parser.add_argument_group('Datastore')
    datastore_group.add_argument('-d', '--datastore', choices=SHIM_LAYERS.keys(), help='Specify the type of datastore', required=True)

    region_group = parser.add_argument_group('Region')
    region_group.add_argument('-r', '--region', choices=REGIONS_FULL_NAMES.keys(), help='Specify the region', required=True)

    # disable consistency checks
    parser.add_argument('-ncc', '--no-consistency-checks', action='store_true', help="Disables consistency checks")

    args = vars(parser.parse_args())
    init_monitor(**args)
