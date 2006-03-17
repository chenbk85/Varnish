/*
 * $Id$
 *
 * The management process and CLI handling
 */

#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>

#include <event.h>
#include <sbuf.h>

#include <cli.h>
#include <cli_priv.h>
#include <libvarnish.h>

#include "mgt.h"
#include "heritage.h"
#include "cli_event.h"

/*--------------------------------------------------------------------*/

struct heritage heritage;
struct event_base *eb;

/*--------------------------------------------------------------------*/

void
xxx_ccb(unsigned u, const char *r, void *priv)
{
	printf("%s(%u, %s, %p)\n", __func__, u, r, priv);
}

/*--------------------------------------------------------------------*/

static void
cli_func_url_query(struct cli *cli, char **av __unused, void *priv __unused)
{

	mgt_child_request(xxx_ccb, NULL, "url.query %s", av[2]);
}

/*--------------------------------------------------------------------*/

static void
cli_func_server_start(struct cli *cli, char **av __unused, void *priv __unused)
{

	mgt_child_start();
}

/*--------------------------------------------------------------------*/

static void
cli_func_server_stop(struct cli *cli, char **av __unused, void *priv __unused)
{

	mgt_child_stop();
}

/*--------------------------------------------------------------------*/

static void
cli_func_verbose(struct cli *cli, char **av __unused, void *priv)
{

	cli->verbose = !cli->verbose;
}


static void
cli_func_ping(struct cli *cli, char **av, void *priv __unused)
{
	time_t t;

	if (av[2] != NULL) {
		cli_out(cli, "Got your %s\n", av[2]);
	} 
	time(&t);
	cli_out(cli, "PONG %ld\n", t);
}

/*--------------------------------------------------------------------*/

static struct cli_proto cli_proto[] = {
	/* URL manipulation */
	{ CLI_URL_QUERY,	cli_func_url_query, NULL },
	{ CLI_URL_PURGE },
	{ CLI_URL_STATUS },
	{ CLI_CONFIG_LOAD },
	{ CLI_CONFIG_INLINE },
	{ CLI_CONFIG_UNLOAD },
	{ CLI_CONFIG_LIST },
	{ CLI_CONFIG_USE },
	{ CLI_SERVER_FREEZE },
	{ CLI_SERVER_THAW },
	{ CLI_SERVER_SUSPEND },
	{ CLI_SERVER_RESUME },
	{ CLI_SERVER_STOP,	cli_func_server_stop, NULL },
	{ CLI_SERVER_START,	cli_func_server_start, NULL },
	{ CLI_SERVER_RESTART },
	{ CLI_PING,		cli_func_ping, NULL },
	{ CLI_STATS },
	{ CLI_ZERO },
	{ CLI_HELP,		cli_func_help, cli_proto },
	{ CLI_VERBOSE,		cli_func_verbose, NULL },
	{ CLI_EXIT },
	{ CLI_QUIT },
	{ CLI_BYE },
	{ NULL }
};

static void
testme(void)
{
	struct event e_sigchld;
	struct cli *cli;
	int i;

	eb = event_init();
	assert(eb != NULL);

	cli = cli_setup(0, 1, 1, cli_proto);

	signal_set(&e_sigchld, SIGCHLD, mgt_sigchld, NULL);
	signal_add(&e_sigchld, NULL);

	i = event_dispatch();
	if (i != 0)
		printf("event_dispatch() = %d\n", i);

}

/*--------------------------------------------------------------------*/

static void
usage(void)
{
	fprintf(stderr, "usage: varnishd [options]\n");
	fprintf(stderr, "    %-20s # %s\n", "-d", "debug");
	fprintf(stderr, "    %-20s # %s\n", "-p number", "TCP listen port");
#if 0
	-c clusterid@cluster_controller
	-f config_file
	-m memory_limit
	-s kind[,storage-options]
	-l logfile,logsize
	-b backend ip...
	-u uid
	-a CLI_port
#endif
	exit(1);
}

/*--------------------------------------------------------------------*/

int
main(int argc, char *argv[])
{
	int o;
	unsigned portnumber = 8080;
	unsigned dflag = 1;	/* XXX: debug=on for now */

	while ((o = getopt(argc, argv, "dp:")) != -1)
		switch (o) {
		case 'd':
			dflag++;
			break;
		case 'p':
			portnumber = strtoul(optarg, NULL, 0);
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc != 0)
		usage();

	testme();


	exit(0);
}
