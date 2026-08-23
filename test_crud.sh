#!/bin/bash

# GET /users:
curl -v -w '\n' http://localhost:8080/users

#GET /users/uuid:
curl -v -w '\n' http://localhost:8080/users/uuid

# Add new user POST:
curl -H 'Content-Type: application/json' -w '\n' -d '{ "username":"penchu", "password":"abcd" }' POST -v http://localhost:8080/users

# Update user:
curl -v -w '\n' -X PUT -H "Content-Type: application/json" -d '{"username":"penchu","password":"abcd"}' http://localhost:8080/users/UUID -H "Authorization: Bearer $TOKEN"

# Delete user:
curl -v -w '\n' -X DELETE http://localhost:8080/users/{uuid} -H "Authorization: Bearer $TOKEN"

# Login user:
curl -w '\n' -H 'Content-Type: application/json' -d '{"username":"penchu", "password":"abcd"}' -X POST -v http://localhost:8080/login

# LOGIN with capturing TOKEN:
TOKEN=$(curl -v -w '\n' -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d '{"username":"penchu","password":"abcd"}' | jq -r '.token')



