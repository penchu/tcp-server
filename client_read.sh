#!/bin/bash

# UUIDS=($(curl -s http://localhost:8080/users | jq -r '.[].uuid'))
UUIDS=($(jq -rs '.[].uuid' < read.txt))

for i in {0..4}; do
    timestamp=$(date +%s%N)
    curl -s http://localhost:8080/users/${UUIDS[$i]} &
done

wait

    

