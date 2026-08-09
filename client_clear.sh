#!/bin/bash

UUIDS=($(curl -s http://localhost:8080/users | jq -r '.[].uuid'))

TOKEN=$(curl -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d "{\"username\":\"admin\", \"password\":\"admin\"}" | jq -r '.token')
    
for i in {0..4}; do
    curl -X DELETE http://localhost:8080/users/${UUIDS[$i]} -H "Authorization: Bearer $TOKEN" &
done

wait