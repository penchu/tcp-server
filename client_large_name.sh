#!/bin/bash

VALUE=$(head -c 10000 /dev/zero | tr '\0' 'A')

curl -s -H 'Content-Type: application/json' -d "{ \"username\":\"penchu$VALUE\", \"password\":\"abcd\" }" -X POST http://localhost:8080/users