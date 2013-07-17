XWP_PATH=/usr/local/xwp

all:
	cd zlog && $(MAKE) $@	
	gcc -g -O2 -Wall buf.c array.c pool.c http_request.c utils.c http.c conf.c main.c connection.c server.c xml_parser.c xml_builder_tree.c xml_tree.c -ldl -lpthread  -Wl,-rpath zlog/src -lzlog -o xwp
	gcc -g -O2 -Wall module_default.c buf.c array.c pool.c http.c -Wl,-rpath zlog/src -lzlog -shared -fPIC -o ../lib/libmodule_default.so
	gcc -g -O2 -Wall module_uwsgi.c buf.c array.c utils.c pool.c http.c http_request.c xml_tree.c -lpthread -Wl,-rpath zlog/src -lzlog -shared -fPIC -o ../lib/libmodule_uwsgi.so

clean:
	rm -f xwp ../lib/*
install:
	cp xwp $(XWP_PATH)/sbin/
	cp ../lib $(XWP_PATH) -rf
	cp zlog/src/libzlog.* /usr/local/lib/
	cp ../conf $(XWP_PATH) -rf
	mv $(XWP_PATH)/conf/xwp.xml.system $(XWP_PATH)/conf/xwp.xml
	cp ../static $(XWP_PATH) -rf


#cp ../zlog.h /usr/local/include/
	

.PHONY: all clean install
