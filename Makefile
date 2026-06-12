all:
	gcc -o server server.c -lsqlite3 -luuid -lsodium -ljwt && ./server

run:
	./server
	./set_admin
	