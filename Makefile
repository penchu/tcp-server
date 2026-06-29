all:
	gcc -o server server.c -lsqlite3 -luuid -lsodium -ljwt && ./server

admin:
	gcc -o set_admin set_admin.c -lsqlite3 -luuid -lsodium -ljwt && ./set_admin


	