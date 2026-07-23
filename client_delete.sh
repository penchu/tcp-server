#!/bin/bash

for i in {1..5}; do
    curl -s -H 'Content-Type: application/json' -d "{ \"username\":\"penchu$i\", \"password\":\"abcd\" }" -X POST http://localhost:8080/users
done

for i in {1..5}; do
    TOKEN=$(curl -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d "{\"username\":\"penchu$i\", \"password\":\"abcd\"}" | jq -r '.token')
    UUID=$(curl -s http://localhost:8080/users | jq -r ".[]|select(.username==\"penchu$i\")|.uuid")
    curl -X DELETE http://localhost:8080/users/${UUID} -H "Authorization: Bearer $TOKEN" &
done

wait