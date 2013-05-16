#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "server.h"
#include "zlog.h"

#ifndef LOG_CONF_PATH
#define LOG_CONF_PATH "conf/zlog.conf"
#endif

#ifndef XWP_CONF_PATH
#define XWP_CONF_PATH "conf/xwp.xml"
#endif
#define MAX_CONF_LEN 512

void usage(char* s)
{
	fprintf(stderr, "<usage>: xwp -c <config> -l <log>\n");

	return;
}

int main(int argc, char** argv)
{
	//daemon(1, 0);
	int opt = 0;
	char log_conf_file[MAX_CONF_LEN] = {0};
	char xwp_conf_file[MAX_CONF_LEN] = {0};
	while((opt = getopt(argc, argv, "c:l:")) != -1) 
	{
		switch(opt)
		{
			case 'c': 
			{
				strncpy(xwp_conf_file, optarg, MAX_CONF_LEN);
				break;
			}
			case 'l': 
			{
				strncpy(log_conf_file, optarg, MAX_CONF_LEN);
				break;
			}
			default:
			{
				usage(argv[0]);
				return -1;
			}
		}
	}
	
	if(xwp_conf_file[0] == '\0') strncpy(xwp_conf_file, XWP_CONF_PATH, MAX_CONF_LEN);
	if(log_conf_file[0] == '\0') strncpy(log_conf_file, LOG_CONF_PATH, MAX_CONF_LEN);

	int rc;
	rc = dzlog_init(log_conf_file, "default");
	if (rc) 
	{
		fprintf(stderr, "init failed\n");
		return -1;
	}

	server_t* server = server_create(xwp_conf_file);
	if(server == NULL) return 0; 

	char buf[512] = {0};

	for(;;) 
	{
		fgets(buf, 512, stdin);
		if(strncmp(buf, "quit", strlen("quit")) == 0) break;
	}

	server_destroy(server);
	zlog_fini();

	return 0;
}
