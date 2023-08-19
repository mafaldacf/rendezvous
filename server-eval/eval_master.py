#!/usr/bin/env python3

from plumbum import FG, local
import yaml

# start off by loading the master config file
with open('configs/master.yml', 'r') as f:
    config = yaml.safe_load(f)
    DURATION = int(config['duration'])
    THREADS = int(config['threads'])
    WORDS23 = bool(config['words23'])
    PARAMS = config['params'] 

eval_worker = local["./eval_worker.py"] 
eval_worker['deploy-clients'] & FG
for i, types in enumerate(PARAMS):
    clients, metadata = types
    eval_worker['restart-server'] & FG

    run_args = ['run-clients', 
        '-d', DURATION,
        '-t', THREADS,
        '-c', clients,
        '-m', metadata]
    if WORDS23:
        run_args.append('-words')
    
    print('\n##### -----------------------------------------------')
    print(f'##### Running eval {i+1}/{len(PARAMS)} for: {clients} clients, {metadata} metadata')
    print('##### -----------------------------------------------\n')

    eval_worker[run_args] & FG
    
