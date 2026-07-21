#!/bin/bash

TOKEN=$(curl -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d '{"username":"penchu_test","password":"abcd"}' | jq -r '.token')

UUID=$(curl -s http://localhost:8080/users | jq -r '.[]|select(.username=="penchu_test")|.uuid')

for i in {0..4}; do
    body="{\"username\":\"penchu$i\"}"
    curl -X PUT -H "Content-Type: application/json" -d "$body" http://localhost:8080/users/${UUID} -H "Authorization: Bearer $TOKEN" &
done

wait

# curl -X PUT -H "Content-Type: application/json" -d '{"username":"penchu"}' http://localhost:8080/users/${UUID} -H "Authorization: Bearer $TOKEN"
