#include <signal.h>
#include <unistd.h>
#include <stdio.h>
#include "server.h"

int main(int argc, char** argv)
{
	server_t* server = server_create("config.xml");
	if(server == NULL) return 0; 

	//TODO accept command from console.
	for(;;) sleep(1);

	server_destroy(server);

	return 0;
}
