XWP_PATH=/usr/local/xwp
all:
	gcc -g -Wall buf.c array.c pool.c http_request.c utils.c http.c conf.c main.c connection.c server.c xml_parser.c xml_builder_tree.c xml_tree.c -ldl -lpthread -lzlog -o ../xwp
	gcc -g -Wall module_default.c buf.c array.c pool.c http.c -lzlog -shared -fPIC -o ../modules/libmodule_default.so
	gcc -g -Wall module_uwsgi.c buf.c array.c utils.c pool.c http.c http_request.c xml_tree.c -lpthread -lzlog -shared -fPIC -o ../modules/libmodule_uwsgi.so

clean:
	rm -f ../xwp ../test_xwp ../modules/libmodule_default.so ../modules/libmodule_uwsgi.so
install:
	cp ../xwp $(XWP_PATH)/sbin/
	cp ../modules $(XWP_PATH) -rf
	cp ../conf $(XWP_PATH) -rf
	cp ../static $(XWP_PATH) -rf
	

.PHONY: all clean install
