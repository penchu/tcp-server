#!/bin/bash

# GET /users:
curl -v http://localhost:8080/users

# Add new user:
curl -H 'Content-Type: application/json' -d '{ "username":"penchu", "password":"abcd" }' -X POST -v http://localhost:8080/users

# Update user:
curl -v -X PUT -H "Content-Type: application/json" -d '{"penchu_"}' http://localhost:8080/users/UUID

# Delete user:
curl -X DELETE http://localhost:8080/users/{uuid} -H "Authorization: Bearer $TOKEN"

# Login user:
curl -H 'Content-Type: application/json' -d '{ "username":"penchu", "password":"abcd" }' -X POST -v http://localhost:8080/login

# Token saving login, need to use it for capturing the token:
TOKEN=$(curl -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d '{"username":"user","password":"pass"}' | jq -r '.token')



