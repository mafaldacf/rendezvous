#!/usr/bin/env python3

from __future__ import print_function
import argparse
import time
import pandas as pd
from pprint import pprint as pp
from matplotlib import pyplot as plt
import glob
import seaborn as sns
import time
import sys
import yaml

# start off by loading the configuration file
with open('config.yaml', 'r') as file:
    config = yaml.safe_load(file)
    RESULTS_DIR = str(config['results_dir'])

# ---------------------------
# (Hardcoded) plot annotation
# ---------------------------

ANNOTATION_CLIENTS_DPS = [
    {
        '#': 5,
        'pos': [(5, 8), (-7, 5), (-14, 3), (-15, 0), (-17, -1)]
    }
]

ANNOTATION_DATASTORES_DPS = [
    {
        '#': 5,
        'combinations': [20, 15, 10, 5, 1],
        'pos': [(1, 5), (0, 5), (0, 5), (0, 5), (0, 5)]
    },
    {
        '#': 7,
        'combinations': [100, 80, 70, 60, 50, 30, 1],
        'pos': [(10, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5), (5, 5)]
    }
]

# -------
# Helpers
# -------

def _annotate_clients(plt, datapoints):
    pos = None
    for annotation in ANNOTATION_CLIENTS_DPS:
        if len(datapoints) == annotation['#']:
            pos = annotation['pos']

    if not pos:
        print("[WARNING] Not enough datapoints to annotate!")

    for i, (throughput, latency) in enumerate(datapoints):
        plt.annotate(f"{(i+1)*200}", (throughput, latency), xytext=pos[i], textcoords='offset points', ha='center', size=10)

def _annotate_datastores(plt, datapoints):
    pos, combinations = None, None
    for annotation in ANNOTATION_DATASTORES_DPS:
        if len(datapoints) == annotation['#']:
            pos = annotation['pos']
            combinations = annotation['combinations']
    if not pos or not combinations:
        print("[WARNING] Not enough datapoints to annotate!")

    for i, (throughput, latency) in enumerate(datapoints):
        plt.annotate(f"{combinations[i]}", (throughput, latency), xytext=pos[i], textcoords='offset points', ha='center', size=10)


def _get_datapoints_info(subdir_name=None, prefix=''):
    latencies = []
    throughputs = []
    if subdir_name:
        pattern = f"{RESULTS_DIR}/{subdir_name}/{prefix}*.info"
    else:
        pattern = f"{RESULTS_DIR}/{prefix}*.info"
    for file_path in glob.glob(pattern):
        with open(file_path, "r") as file:
            for line in file:
                if "Throughput" in line:
                    throughput = line.split(":")[1].strip()
                    throughputs.append(int(float(throughput)))
                elif "Latency" in line:
                    latency = line.split(":")[1].strip()
                    latencies.append(int(float(latency)))
    return sorted(zip(throughputs, latencies))

def _get_datapoints_csv(subdir_name=None, prefix=''):
    latencies = []
    throughputs = []
    df_latencies_list = []
    if subdir_name:
        pattern = f"{RESULTS_DIR}/{subdir_name}/{prefix}*"
    else:
        pattern = f"{RESULTS_DIR}/{prefix}*"
    for file_path in glob.glob(f'{pattern}.csv'):
        df = pd.read_csv(file_path, sep=';', index_col=0, low_memory=False)
        df_latencies_list.append(df)

    for file_path in glob.glob(f'{pattern}.info'):
        with open(file_path, "r") as file:
                # each file represents a different number of clients (1 to 5)
                for line in file:
                    if "Throughput" in line:
                        throughput = line.split(":")[1].strip()
                        throughputs.append(int(float(throughput)))

    for df in df_latencies_list:
        df['latency'] = pd.to_numeric(df['latency'], errors='coerce')
        latency_median = df['latency'].mean()
        latencies.append(int(float(latency_median)))
    return sorted(zip(throughputs, latencies))

def plot_words23(annotate):
    sns.set_theme(style='ticks')
    plt.rcParams["figure.figsize"] = [6, 2.25] 
    plt.rcParams["figure.dpi"] = 600
    plt.rcParams['axes.labelsize'] = 'small'

    dps_clients = _get_datapoints_info('clients')
    data_clients = [
        {
        'throughput': dp[0],
        'latency': dp[1],
        'variation': '# clients'
        } for dp in dps_clients
    ]

    dps_datastores = _get_datapoints_info('datastores')
    data_datastores = [
        {
        'throughput': dp[0],
        'latency': dp[1],
        'variation': '# datastores'
        } for dp in dps_datastores
    ]

    df_clients = pd.DataFrame.from_records(data_clients)
    df_datastores = pd.DataFrame.from_records(data_datastores)
    pp(df_clients)
    pp(df_datastores)

    # create figure for both plots
    fig, axes = plt.subplots(1, 2)

    # plot datapoints with datastore variation and force secondary color of searborn palette
    ax_datastores = sns.lineplot(data=df_datastores, x="throughput", y="latency", hue='variation', 
        style='variation', markers=['o'], ax=axes[0])
    ax_datastores.set_xlabel(None)
    ax_datastores.set_ylabel(None)
    ax_datastores.legend_.set_title(None)
    # use same y limits for both subplots
    #ax_datastores.set_xlim(right=5100)
    ax_datastores.set_ylim(top=138, bottom=35)
    if annotate:
        _annotate_datastores(ax_datastores, dps_datastores)

    # plot datapoints with client variation
    ax_clients = sns.lineplot(data=df_clients, x="throughput", y="latency", hue='variation', 
        style='variation', markers=['o'], ax=axes[1], palette=[sns.color_palette()[1]])
    ax_clients.set_xlabel(None)
    ax_clients.set_ylabel(None)
    ax_clients.legend_.set_title(None)
    # use same y limits for both subplots
    #ax_clients.set_xlim(right=10000)
    ax_clients.set_ylim(top=138, bottom=35) # use same y limits for both subplots
    if annotate:
        _annotate_clients(ax_clients, dps_clients)
    # remove y numeration since it is already present in the first subplot on the left side
    ax_clients.set_yticklabels([])

    # set common x and y labels
    fig.text(0.5, -0.10, 'Throughput (req/s)', ha='center')
    fig.text(0.02, 0.5, 'Latency (ms)', va='center', rotation='vertical')

    # reverse legend for both subplots
    handles, labels = ax_clients.get_legend_handles_labels()
    ax_clients.legend(handles[::-1], labels[::-1])
    handles, labels = ax_datastores.get_legend_handles_labels()
    ax_datastores.legend(handles[::-1], labels[::-1])

    plot_name = f'plots/throughput_latency_datastores_clients_{time.time()}.png'
    plt.savefig(plot_name, bbox_inches='tight', pad_inches=0.1)
    print(f"Successfuly saved plot figure in {plot_name}!")

def plot_thesis():
    sns.set_theme(style='ticks')
    plt.rcParams["figure.figsize"] = [6, 4.25] 
    plt.rcParams["figure.dpi"] = 600
    plt.rcParams['axes.labelsize'] = 'small'

    results = {
        '1 region': _get_datapoints_info(prefix='regions_1_'),
        '10 regions': _get_datapoints_info(prefix='regions_10_'),
        '100 regions': _get_datapoints_info(prefix='regions_100_')
    }

    data = [
        {
        'throughput': dp[0],
        'latency': dp[1],
        'regions': k,
        } for k, v in results.items() for dp in v
    ]
    
    df = pd.DataFrame.from_records(data)
    pp(df)
    ax = sns.lineplot(data=df, x="throughput", y="latency", hue='regions', 
        style='regions', markers=True, dashes=False, linewidth = 3)

    ax.set_xlabel('Throughput (req/s)')
    ax.set_ylabel('Latency (ms)')
    ax.legend_.set_title('metadata')

    plot_name = f'plots/throughput_latency_thesis_{time.time()}.png'
    plt.savefig(plot_name, bbox_inches = 'tight', pad_inches = 0.1)
    print(f"Successfuly saved plot figure in {plot_name}!")

def plot_thesis_clients(annotate):
    sns.set_theme(style='ticks')
    plt.rcParams["figure.figsize"] = [6, 4.5] 
    plt.rcParams["figure.dpi"] = 600
    plt.rcParams['axes.labelsize'] = 'small'

    dps = _get_datapoints_info('clients')
    data = [
        {
        'throughput': dp[0],
        'latency': dp[1],
        'variation': '# clients'
        } for dp in dps
    ]

    df = pd.DataFrame.from_records(data)
    pp(df)

    ax = sns.lineplot(data=df, x="throughput", y="latency", hue='variation', style='variation', markers=['o'])
    ax.set_xlabel('Throughput (req/s)')
    ax.set_ylabel('Latency (ms)')
    ax.set_ylim(top=138, bottom=35)
    # remove variation title
    ax.legend_.set_title(None)

    if annotate:
        _annotate_clients(ax, dps)

    plot_name = f'plots/throughput_latency_thesis_clients_{time.time()}.png'
    plt.savefig(plot_name, bbox_inches='tight', pad_inches=0.1)
    print(f"Successfuly saved plot figure in {plot_name}!")

if __name__ == '__main__':
    main_parser = argparse.ArgumentParser()
    command_parser = main_parser.add_subparsers(help='commands', dest='command')

    words_parser = command_parser.add_parser('words23', help="Plot for words23 workshop")
    words_parser.add_argument('-a', '--annotate', action='store_true', help="Annotate datapoints")

    thesis_parser = command_parser.add_parser('thesis', help="Plot for thesis")
    #thesis_parser.add_argument('-t', '--plot-type', type=str, choices=['clients', 'all'], help="Variation type")
    #thesis_parser.add_argument('-a', '--annotate', action='store_true', help="Annotate datapoints")

    args = vars(main_parser.parse_args())
    print("Arguments:", args, flush=True)
    command = args.pop('command')
    
    function_name = f'plot_{command}'
    if command == 'thesis' and args.get('plot_type'):
        function_name += f"_{args['plot_type']}"
        args.pop('plot_type')
    function = getattr(sys.modules[__name__], function_name, None)
    function(**args)