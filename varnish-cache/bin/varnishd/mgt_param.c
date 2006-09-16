/*
 * $Id$
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>

#include "cli.h"
#include "cli_priv.h"
#include "cli_common.h"
#include "mgt.h"
#include "mgt_cli.h"

#include "vsb.h"
#include "heritage.h"

struct parspec;

typedef void tweak_t(struct cli *, struct parspec *, const char *arg);

struct parspec {
	const char	*name;
	tweak_t		*func;
	const char	*expl;
	const char	*def;
	const char	*units;
};

/*--------------------------------------------------------------------*/

static void
tweak_generic_timeout(struct cli *cli, unsigned *dst, const char *arg)
{
	unsigned u;

	if (arg != NULL) {
		u = strtoul(arg, NULL, 0);
		if (u == 0) {
			cli_out(cli, "Timeout must be greater than zero\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		*dst = u;
	} else
		cli_out(cli, "%u", *dst);
}

/*--------------------------------------------------------------------*/

static void
tweak_generic_bool(struct cli *cli, unsigned *dest, const char *arg)
{
	if (arg != NULL) {
		if (!strcasecmp(arg, "off"))
			*dest = 0;
		else if (!strcasecmp(arg, "disable"))
			*dest = 0;
		else if (!strcasecmp(arg, "no"))
			*dest = 0;
		else if (!strcasecmp(arg, "on"))
			*dest = 1;
		else if (!strcasecmp(arg, "enable"))
			*dest = 1;
		else if (!strcasecmp(arg, "yes"))
			*dest = 1;
		else {
			cli_out(cli, "use \"on\" or \"off\"\n");
			cli_result(cli, CLIS_PARAM);
			return;
		}
	} else
		cli_out(cli, *dest ? "on" : "off");
}

/*--------------------------------------------------------------------*/

static void
tweak_generic_uint(struct cli *cli, unsigned *dest, const char *arg, unsigned min, unsigned max)
{
	unsigned u;

	if (arg != NULL) {
		if (!strcasecmp(arg, "unlimited"))
			u = UINT_MAX;
		else
			u = strtoul(arg, NULL, 0);
		if (u < min) {
			cli_out(cli, "Must be at least %u", min);
			cli_result(cli, CLIS_PARAM);	
			return;
		}
		if (u > max) {
			cli_out(cli, "Must be no more than %u", max);
			cli_result(cli, CLIS_PARAM);	
			return;
		}
		*dest = u;
	} else if (*dest == UINT_MAX) {
		cli_out(cli, "unlimited", *dest);
	} else {
		cli_out(cli, "%u", *dest);
	}
}

/*--------------------------------------------------------------------*/

static void
tweak_default_ttl(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->default_ttl, arg, 0, UINT_MAX);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pools(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->wthread_pools, arg,
	    1, UINT_MAX);
}


/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_min(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->wthread_min, arg,
	    0, params->wthread_max);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_max(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->wthread_max, arg,
	    params->wthread_min, UINT_MAX);
}

/*--------------------------------------------------------------------*/

static void
tweak_thread_pool_timeout(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_timeout(cli, &params->wthread_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_http_workspace(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->mem_workspace, arg,
	    1024, UINT_MAX);
}

/*--------------------------------------------------------------------*/

static void
tweak_sess_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->sess_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_pipe_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->pipe_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_send_timeout(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_timeout(cli, &params->send_timeout, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_auto_restart(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_bool(cli, &params->auto_restart, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_fetch_chunksize(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->fetch_chunksize, arg,
	    4, UINT_MAX / 1024);
}

#ifdef HAVE_SENDFILE
/*--------------------------------------------------------------------*/

static void
tweak_sendfile_threshold(struct cli *cli, struct parspec *par, const char *arg)
{

	(void)par;
	tweak_generic_uint(cli, &params->sendfile_threshold, arg, 0, UINT_MAX);
}
#endif /* HAVE_SENDFILE */

/*--------------------------------------------------------------------*/

static void
tweak_vcl_trace(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_bool(cli, &params->vcl_trace, arg);
}

/*--------------------------------------------------------------------*/

static void
tweak_listen_address(struct cli *cli, struct parspec *par, const char *arg)
{
	char *a, *p;

	(void)par;
	if (arg != NULL) {
		if (TCP_parse(arg, &a, &p) != 0) {
			cli_out(cli, "Invalid listen address");
			cli_result(cli, CLIS_PARAM);
			return;
		}
		if (p == NULL) {
			p = strdup("http");
			AN(p);
		}
		TCP_check(cli, a, p);
		if (cli->result != CLIS_OK)
			return;
		free(params->listen_address);
		free(params->listen_host);
		free(params->listen_port);
		params->listen_address = strdup(arg);
		AN(params->listen_address);
		params->listen_host = a;
		params->listen_port = p;
	} else 
		cli_out(cli, "%s", params->listen_address);
}

/*--------------------------------------------------------------------*/

static void
tweak_listen_depth(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_uint(cli, &params->listen_depth, arg, 0, UINT_MAX);
}

/*--------------------------------------------------------------------*/

static void
tweak_srcaddr_hash(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_uint(cli, &params->srcaddr_hash, arg, 63, UINT_MAX);
}

/*--------------------------------------------------------------------*/

static void
tweak_srcaddr_ttl(struct cli *cli, struct parspec *par, const char *arg)
{
	(void)par;
	tweak_generic_uint(cli, &params->srcaddr_ttl, arg, 0, UINT_MAX);
}

/*--------------------------------------------------------------------*/

/*
 * Make sure to end all lines with either a space or newline of the
 * formatting will go haywire.
 */

#define DELAYED_EFFECT \
	"\nNB: This parameter will take some time to take effect.\n"

#define SHOULD_RESTART \
	"\nNB: This parameter will not take full effect until the " \
	"child process has been restarted.\n"

#define MUST_RESTART \
	"\nNB: This parameter will not take any effect until the " \
	"child process has been restarted.\n"


static struct parspec parspec[] = {
	{ "default_ttl", tweak_default_ttl,
		"The TTL assigned to objects if neither the backend nor "
		"the VCL code assigns one.\n"
		"Objects already cached will not be affected by changes "
		"made until they are fetched from the backend again.\n"
		"To force an immediate effect at the expense of a total "
		"flush of the cache use \"url.purge .\"",
		"120", "seconds" },
	{ "thread_pools", tweak_thread_pools,
		"Number of thread pools.\n",
		"1", "pools" },
	{ "thread_pool_max", tweak_thread_pool_max,
		"The maximum number of threads in the worker pool.\n"
		"-1 is unlimited.\n"
		DELAYED_EFFECT,
		"-1", "threads" },
	{ "thread_pool_min", tweak_thread_pool_min,
		"The minimum number of threads in the worker pool.\n"
		DELAYED_EFFECT
		"Minimum is 1 thread. ",
		"1", "threads" },
	{ "thread_pool_timeout", tweak_thread_pool_timeout,
		"Thread dies after this many seconds of inactivity.\n"
		"Minimum is 1 second. ",
		"120", "seconds" },
	{ "http_workspace", tweak_http_workspace,
		"Bytes of HTTP protocol workspace allocated. "
		"This space must be big enough for the entire HTTP protocol "
		"header and any edits done to it in the VCL code.\n"
		SHOULD_RESTART
		"Minimum is 1024 bytes. ",
		"8192", "bytes" },
	{ "sess_timeout", tweak_sess_timeout,
		"Idle timeout for persistent sessions. "
		"If a HTTP request has not been received in this many "
		"seconds, the session is closed.\n",
		"5", "seconds" },
	{ "pipe_timeout", tweak_pipe_timeout,
		"Idle timeout for PIPE sessions. "
		"If nothing have been received in either directoin for "
	        "this many seconds, the session is closed.\n",
		"60", "seconds" },
	{ "send_timeout", tweak_send_timeout,
		"Send timeout for client connections. "
		"If no data has been sent to the client in this many seconds, "
		"the session is closed.\n"
		DELAYED_EFFECT
		"See getopt(3) under SO_SNDTIMEO for more information.\n",
		"600", "seconds" },
	{ "auto_restart", tweak_auto_restart,
		"Restart child process automatically if it dies.\n"
		"Minimum is 4 kilobytes.\n",
		"on", "bool" },
	{ "fetch_chunksize", tweak_fetch_chunksize,
		"The default chunksize used by fetcher.\n",
		"128", "kilobytes" },
#ifdef HAVE_SENDFILE
	{ "sendfile_threshold", tweak_sendfile_threshold,
		"The minimum size of objects transmitted with sendfile.\n",
		"8192", "bytes" },
#endif /* HAVE_SENDFILE */
	{ "vcl_trace", tweak_vcl_trace,
		"Trace VCL execution in the shmlog\n",
		"off", "bool" },
	{ "listen_address", tweak_listen_address,
		"The network address/port where Varnish services requests.\n"
		MUST_RESTART,
		"0.0.0.0:80" },
	{ "listen_depth", tweak_listen_depth,
		"Listen(2) queue depth.\n"
		MUST_RESTART,
		"1024", "connections" },
	{ "srcaddr_hash", tweak_srcaddr_hash,
		"Number of source address hash buckets.\n"
		"Powers of two are bad, prime numbers are good.\n"
		MUST_RESTART,
		"1049", "buckets" },
	{ "srcaddr_ttl", tweak_srcaddr_ttl,
		"Lifetime of srcaddr entries.\n"
		"Zero will disable srcaddr accounting.\n",
		"30", "seconds" },
	{ NULL, NULL, NULL }
};

/*--------------------------------------------------------------------*/

void
mcf_param_show(struct cli *cli, char **av, void *priv)
{
	struct parspec *pp;
	const char *p, *q;
	int lfmt;

	(void)priv;
	if (av[2] == NULL || strcmp(av[2], "-l"))
		lfmt = 0;
	else
		lfmt = 1;
	for (pp = parspec; pp->name != NULL; pp++) {
		if (av[2] != NULL && !lfmt && strcmp(pp->name, av[2]))
			continue;
		cli_out(cli, "%-20s ", pp->name);
		if (pp->func == NULL) {
			cli_out(cli, "Not implemented.\n");
			if (av[2] != NULL && !lfmt) 
				return;
			else
				continue;
		}
		pp->func(cli, pp, NULL);
		if (pp->units != NULL)
			cli_out(cli, " [%s]\n", pp->units);
		else
			cli_out(cli, "\n");
		if (av[2] != NULL) {
			cli_out(cli, "%-20s Default is %s\n", "", pp->def);
			/* Format text to 72 col width */
			for (p = pp->expl; *p != '\0'; ) {
				q = strchr(p, '\n');
				if (q == NULL)
					q = strchr(p, '\0');
				if (q > p + 52) {
					q = p + 52;
					while (q > p && *q != ' ')
						q--;
					AN(q);
				}
				cli_out(cli, "%20s %.*s\n", "", q - p, p);
				p = q;
				if (*p == ' ' || *p == '\n')
					p++;
			}
			if (!lfmt)
				return;
			else
				cli_out(cli, "\n");
		}
	}
	if (av[2] != NULL && !lfmt) {
		cli_result(cli, CLIS_PARAM);
		cli_out(cli, "Unknown paramter \"%s\".", av[2]);
	}
}

/*--------------------------------------------------------------------*/

void
MCF_ParamSet(struct cli *cli, const char *param, const char *val)
{
	struct parspec *pp;

	for (pp = parspec; pp->name != NULL; pp++) {
		if (!strcmp(pp->name, param)) {
			pp->func(cli, pp, val);
			return;
		}
	}
	cli_result(cli, CLIS_PARAM);
	cli_out(cli, "Unknown paramter \"%s\".", param);
}


/*--------------------------------------------------------------------*/

void
mcf_param_set(struct cli *cli, char **av, void *priv)
{

	(void)priv;
	MCF_ParamSet(cli, av[2], av[3]);
}

/*--------------------------------------------------------------------*/

void
MCF_ParamInit(struct cli *cli)
{
	struct parspec *pp;

	for (pp = parspec; pp->name != NULL; pp++) {
		cli_out(cli, "Set Default for %s = %s\n", pp->name, pp->def);
		pp->func(cli, pp, pp->def);
		if (cli->result != CLIS_OK)
			return;
	}
}
