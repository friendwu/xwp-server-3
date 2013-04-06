all:
	gcc -g -Wall  buf.c array.c pool.c utils.c http.c conf.c main.c connection.c server.c -ldl -lpthread -o xwp
	gcc -g -I. modules/module_default.c buf.c array.c pool.c http.c conf.c -shared -fPIC -o modules/module_default.so
	gcc -g pool.c  -DPOOL_TEST -o pool_test
clean:
	rm -f xwp
