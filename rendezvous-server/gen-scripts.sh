#!/bin/bash

# - - - - - - - - - - - - - - - - - - -
#
# Multiple scripts for testing purposes
#
# - - - - - - - - - - - - - - - - - - -

parse_args() {

    if [ "$#" -eq 1 ]; then
        script_name=$1
    else
        echo "Invalid arguments!"
        echo "Usage: ./generateScript.sh <script name>"
        exit 1
    fi
}

prompt_user() {
    # Prompt user for input
    echo "Select one script:"
    echo "1. simple_stress_test"
    echo "2. one_request_multiple_branches"
    printf "> "
    read choice

    # Call the selected function
    if [ "$choice" == "1" ]; then
        simple_stress_test $script_name 1000 100
    elif [ "$choice" == "2" ]; then
        one_request_multiple_branches $script_name 30 1000
    else
        echo "Invalid choice"
    fi
}

simple_stress_test() {

    # Parse Arguments
    filename=$1
    num_requests=$2
    num_branches=$3

    echo "Running simple_stress_test with $num_requests requests and $num_branches branches"
    echo "Saving output to $filename"
    echo

    # Redirect output to file
    exec > "scripts/$filename"

    # Register <num_requests> requests
    echo "(1) writing command: registering $num_requests requests and $num_branches branches for each request..." >&2
    for r in $(seq 0 $(($num_requests-1))); do
        echo "rr $r"
        echo "rbs $r $num_branches"
    done

    # Close <num_branches> branches for all <num_requests> requests
    echo "(2) writing command: closing $num_branches branches for all $num_requests requests..." >&2
    for r in $(seq 0 $(($num_requests-1))); do
        for b in $(seq 0 $(($num_branches-1))); do
            echo "cb $r $b"
        done
    done

    # Check status for <num_requests> requests
    echo "(3) writing command: checking status for $num_requests requests..." >&2
    for r in $(seq 0 $(($num_requests-1))); do
        echo "cr $r"
    done

    echo "exit"

    echo >&2
    echo "done!" >&2
}

one_request_multiple_branches() {

    # Parse Arguments
    filename=$1
    iter_request=$2
    num_branches=$3
    rid=0

    echo "Running one_request_multiple_branches $num_branches branches (assuming request was previously registered)"
    echo "Saving output to $filename"
    echo

    # Redirect output to file
    exec > "scripts/$filename"

    # Register <num_branches> branches with no context
    echo "(1) writing command: registering $num_branches branches with no context..." >&2
    for b in $(seq 0 $(($num_branches-1))); do
        echo "rb $rid"
    done

    # Register <num_branches> branches for 'service1'
    echo "(2) writing command: registering $num_branches branches for 'service1'..." >&2
    for b in $(seq 0 $(($num_branches-1))); do
        echo "rb $rid service1"
    done

    # Register <num_branches> branches for 'service2'
    echo "(3) writing command: registering $num_branches branches for 'service2'..." >&2
    for b in $(seq 0 $(($num_branches-1))); do
        echo "rb $rid service2"
    done

    # Register <num_branches> branches for 'service2' and 'region1'
    echo "(4) writing command: registering $num_branches branches for 'service2' and 'region1'..." >&2
    for b in $(seq 0 $(($num_branches-1))); do
        echo "rb $rid service2 region1"
    done

    # Check status with no context
    echo "(5) writing command: checking status x$iter_request with no context..." >&2
    for r in $(seq 0 $(($iter_request-1))); do
        echo "cr $rid"
    done

    # Check status on 'service1'
    echo "(6) writing command: checking status x$iter_request on 'service1'..." >&2
    for r in $(seq 0 $(($iter_request-1))); do
        echo "cr $rid service1"
    done

    # Check status on 'service2' and 'region1'
    echo "(7) writing command: checking status x$iter_request on 'service2' and 'region1..." >&2
    for r in $(seq 0 $(($iter_request-1))); do
        echo "cr $rid service2 region1"
    done

    # Check status by region with no service
    echo "(7) writing command: checking status by region x$iter_request with no service..." >&2
    for r in $(seq 0 $(($iter_request-1))); do
        echo "crr $rid"
    done

    # Check status by region on 'service2'
    echo "(7) writing command: checking status by region x$iter_request on 'service2'..." >&2
    for r in $(seq 0 $(($iter_request-1))); do
        echo "crr $rid service2"
    done

    echo "exit"

    echo >&2
    echo "done!" >&2
}

parse_args "$@"
prompt_user