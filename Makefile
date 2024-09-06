
ALL: ./c-output/client.out ./c-output/server.out

./c-output/server.out: ./server.c
	gcc $< -o $@ -O2 -g

./c-output/client.out: ./client.c
	gcc $< -o $@ -O2 -g
