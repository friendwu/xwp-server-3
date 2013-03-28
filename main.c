#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "server.h"

int main(int argc, char** argv)
{
	signal(SIGPIPE, SIG_IGN);

	server_t* server = server_create("config.xml");

	if(server) 
	{
		//TODO accept command from console.
		for(;;) sleep(1);
	}

	server_destroy(server);

	return 0;
}
