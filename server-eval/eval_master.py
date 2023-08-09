#!/usr/bin/env python3

from plumbum import FG, local

DURATION = 150
THREADS = 200
WORDS23 = False 
EVAL_THESIS = [
    # (clients, metadata)
    #(1, 1),    (2, 1),    (3, 1),    (4, 1),    (5, 1),
    (1, 10),   (2, 10),   (3, 10),   (4, 10),   (5, 10),
    #(1, 100),  (2, 100),  (3, 100),  (4, 100),  (5, 100)
]

server_eval = local["./eval.py"] 
server_eval['deploy-clients'] & FG
for i, types in enumerate(EVAL_THESIS):
    clients, metadata = types
    server_eval['restart-server'] & FG

    run_args = ['run-clients', 
        '-d', DURATION,
        '-t', THREADS,
        '-c', clients,
        '-m', metadata]
    if WORDS23:
        run_args.append('-words')

    print("\n##### -----------------------")
    print(f"##### Running eval {i+1}/{len(EVAL_THESIS)} for: {clients} clients, {metadata} metadata")
    print("##### -----------------------\n")
    server_eval[run_args] & FG
    
