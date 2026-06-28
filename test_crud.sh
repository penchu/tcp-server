#!/bin/bash

# GET /users:
curl -v http://localhost:8080/users

# Add new user POST:
curl -H 'Content-Type: application/json' -d '{ "username":"penchu", "password":"abcd" }' -X POST -v http://localhost:8080/users

# Update user:
curl -v -X PUT -H "Content-Type: application/json" -d '{"username":"penchu","password":"abcd"}' http://localhost:8080/users/UUID -H "Authorization: Bearer $TOKEN"

# Delete user:
curl -v -X DELETE http://localhost:8080/users/{uuid} -H "Authorization: Bearer $TOKEN"

# Login user:
curl -H 'Content-Type: application/json' -d '{"username":"penchu", "password":"abcd"}' -X POST -v http://localhost:8080/login

# LOGIN with capturing TOKEN:
TOKEN=$(curl -v -s -X POST http://localhost:8080/login -H 'Content-Type: application/json' -d '{"username":"penchu","password":"abcd"}' | jq -r '.token')



