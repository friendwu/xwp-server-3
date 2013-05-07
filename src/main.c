#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "server.h"

int main(int argc, char** argv)
{
	server_t* server = server_create("config.xml");
	if(server == NULL) return 0; 

	char buf[512] = {0};

	for(;;) 
	{
		fgets(buf, 512, stdin);
		if(strncmp(buf, "quit", strlen("quit")) == 0) break;

	}

	server_destroy(server);

	return 0;
}
