all:
	gcc -g -Wall  buf.c array.c pool.c utils.c http.c conf.c main.c connection.c server.c -ldl -lpthread -o xwp

clean:
	rm -f xwp
