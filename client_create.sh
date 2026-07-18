#!/bin/bash

# timestamp=$(date +%s%N)

for i in {1..5}; do
    timestamp=$(date +%s%N)
    (curl -s -H 'Content-Type: application/json' -d "{ \"username\":\"penchu$timestamp\", \"password\":\"abcd\" }" -X POST http://localhost:8080/users &) >> read.txt
done

wait