#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "server.h"
#include "log.h"
#include "pool.h"
#include "conf.h"
#include "utils.h"

#ifndef LOG_CONF_PATH
#define LOG_CONF_PATH "../conf/zlog.conf"
#endif

#ifndef XWP_CONF_PATH
#define XWP_CONF_PATH "../conf/xwp.xml"
#endif
#define MAX_CONF_LEN 512
#define MAX_WORKER_NUM 32

int s_listen_fd = -1;
int s_worker_count = 0;
pid_t s_worker_processes[MAX_WORKER_NUM] = {0};
int s_worker_id = 0;
static int s_exit_flag = 0;
static int s_child_exited = 0;
conf_t* s_conf = NULL;

void usage(char* s)
{
	fprintf(stderr, "<usage>: xwp -c <config> -l <log>\n");

	return;
}

static void xwp_signal_handler(int signo)
{
    switch(signo)
    {
        case SIGINT:
        case SIGTERM:
        {
            s_exit_flag = 1;
            break;
        }
        case SIGCHLD:
        {
            s_child_exited = 1;
            break;
        }
    }
}

static void worker_main()
{
    server_t* server = server_create(s_conf);
	if(server == NULL) return;

	while(!s_exit_flag)
    {
        sleep(1);
    }

	server_destroy(server);
	zlog_fini();

    exit(0);
}

static void master_main()
{
    int pid, status;

	while(!s_exit_flag)
	{
	    sleep(1);

        if(s_child_exited == 0) continue;
        s_child_exited = 0;

        int i = 0;
        for(; i<s_worker_count; i++)
        {
            pid = waitpid(s_worker_processes[i], &status, WNOHANG);
            if(!s_exit_flag && pid > 0)
            {
                log_error("worker %d exited, pid %d.", i, pid);
                int pid = fork();
                if(pid == 0)
                {
                    /*child*/
                    s_worker_id = i;
                    worker_main();
                }
                else if(pid > 0)
                {
                    s_worker_processes[i] = pid;
                }
            }
        }

	}
    log_error("master exited.");

	zlog_fini();

    kill(0, SIGTERM);
	exit(0);
}

int main(int argc, char** argv)
{
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

    int rc = dzlog_init(log_conf_file, "default");
	if(rc)
	{
		fprintf(stderr, "init failed\n");
		return -1;
	}

	daemon(1, 1);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, xwp_signal_handler);
    signal(SIGTERM, xwp_signal_handler);
    signal(SIGINT, xwp_signal_handler);

	pool_t* pool = pool_create(2 * 1024);
	s_conf = conf_parse(xwp_conf_file, pool);
	if(s_conf == NULL)
	{
		pool_destroy(pool);
		return 0;
	}

	s_listen_fd = open_listen_fd(s_conf->ip.data, s_conf->port, s_conf->max_threads);
	if(s_listen_fd < 0)
	{
		log_error("open listen fd failed.");
		pool_destroy(pool);
		zlog_fini();
		return 0;
	}

    int i = 0;

    s_conf->worker_num = 1;
    for(; i<s_conf->worker_num; i++)
    {
        int pid = fork();
        if(pid == 0)
        {
            /*child*/
            s_worker_id = i;
            worker_main();
        }
        else if(pid > 0)
        {
            s_worker_processes[i] = pid;
            s_worker_count++;
        }
    }

    master_main();

	return 0;
}
