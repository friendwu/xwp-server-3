all:
	gcc -g -Wall  main.c connection.c vhost.c server.c xml_parser.c xml_builder_tree.c xml_tree.c utils.c -ldl -lpthread -o xwp

clean:
	rm -f xwp
