/*
 * $Id$
 *
 * The management process' CLI handling
 */

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>

#include "cli_priv.h"
#include "cli.h"
#include "sbuf.h"
#include "common_cli.h"
#include "mgt.h"
#include "mgt_cli.h"
#include "mgt_event.h"
#include "shmlog.h"

static int		cli_i = -1, cli_o = -1;

/*--------------------------------------------------------------------*/

static void
mcf_stats(struct cli *cli, char **av, void *priv)
{

	(void)av;
	(void)priv;

	assert (VSL_stats != NULL);
#define MAC_STAT(n,t,f,d) \
    cli_out(cli, "%12ju  " d "\n", (VSL_stats->n));
#include "stat_field.h"
#undef MAC_STAT
}


/*--------------------------------------------------------------------
 * Passthru of cli commands.  It is more or less just undoing what
 * the cli parser did, but such is life...
 */

static void
mcf_passthru(struct cli *cli, char **av, void *priv)
{
	char *p, *q, *r;
	unsigned u, v;
	int i;

	(void)priv;

	/* Request */
	if (cli_o <= 0) {
		cli_result(cli, CLIS_CANT);
		cli_out(cli, "Cache process not running");
		return;
	}
	v = 0;
	for (u = 1; av[u] != NULL; u++)
		v += strlen(av[u]) + 3;
	p = malloc(v);
	assert(p != NULL);
	q = p;
	for (u = 1; av[u] != NULL; u++) {
		*q++ = '"';
		for (r = av[u]; *r; r++) {
			switch (*r) {
			case '\\':	*q++ = '\\'; *q++ = '\\'; break;
			case '\n':	*q++ = '\\'; *q++ = 'n'; break;
			case '"':	*q++ = '\\'; *q++ = '"'; break;
			default:	*q++ = *r; break;
			}
		}
		*q++ = '"';
		*q++ = ' ';
	}
	*q++ = '\n';
	v = q - p;
	i = write(cli_o, p, v);
	assert(i == v);
	free(p);

	i = cli_readres(cli_i, &u, &p, 3.0);
	assert(i == 0);
	cli_result(cli, u);
	cli_out(cli, "%s", p);
	free(p);

}

/*--------------------------------------------------------------------*/

static struct cli_proto *cli_proto;

static struct cli_proto mgt_cli_proto[] = {
	{ CLI_HELP,		cli_func_help, NULL },	/* must be first */
	{ CLI_PING,		cli_func_ping },
	{ CLI_SERVER_START,	mcf_server_startstop, NULL },
	{ CLI_SERVER_STOP,	mcf_server_startstop, &cli_proto },
	{ CLI_STATS,		mcf_stats, NULL },
	{ CLI_CONFIG_LOAD,	mcf_config_load, NULL },
	{ CLI_CONFIG_INLINE,	mcf_config_inline, NULL },
	{ CLI_CONFIG_USE,	mcf_config_use, NULL },
	{ CLI_CONFIG_DISCARD,	mcf_config_discard, NULL },
	{ CLI_CONFIG_LIST,	mcf_config_list, NULL },
#if 0
	{ CLI_SERVER_STOP,	m_cli_func_server_stop, NULL },
	{ CLI_SERVER_RESTART },
	{ CLI_PING,		m_cli_func_ping, NULL },
	{ CLI_ZERO },
	{ CLI_VERBOSE,		m_cli_func_verbose, NULL },
	{ CLI_EXIT, 		m_cli_func_exit, NULL},
#endif
	{ CLI_QUIT },
	{ CLI_BYE },
	{ NULL }
};


/*--------------------------------------------------------------------*/

void
mgt_cli_init(void)
{
	struct cli_proto *cp;
	unsigned u, v;


	/*
	 * Build the joint cli_proto by combining the manager process
	 * entries with with the cache process entries.  The latter
	 * get a "passthough" function in the joint list
	 */
	u = 0;
	for (cp = mgt_cli_proto; cp->request != NULL; cp++)
		u++;
	for (cp = CLI_cmds; cp->request != NULL; cp++)
		u++;
	cli_proto = calloc(sizeof *cli_proto, u + 1);
	assert(cli_proto != NULL);
	u = 0;
	for (cp = mgt_cli_proto; cp->request != NULL; cp++)
		cli_proto[u++] = *cp;
	for (cp = CLI_cmds; cp->request != NULL; cp++) {
		/* Skip any cache commands we already have in the manager */
		for (v = 0; v < u; v++)
			if (!strcmp(cli_proto[v].request, cp->request))
				break;
		if (v < u)
			continue;
		cli_proto[u] = *cp;
		cli_proto[u].func = mcf_passthru;
		u++;
	}

	/* Fixup the entry for 'help' entry */
	assert(!strcmp(cli_proto[0].request, "help"));
	cli_proto[0].priv = cli_proto;

	/* XXX: open listening sockets, contact cluster server etc */
}

/*--------------------------------------------------------------------
 * Ask the child something over CLI, return zero only if everything is
 * happy happy.
 */

int
mgt_cli_askchild(int *status, char **resp, const char *fmt, ...)
{
	char *p;
	int i, j;
	va_list ap;

	va_start(ap, fmt);
	i = vasprintf(&p, fmt, ap);
	va_end(ap);
	if (i < 0)
		return (i);
	assert(p[i - 1] == '\n');
	i = write(cli_o, p, strlen(p));
	assert(i == strlen(p));
	free(p);

	i = cli_readres(cli_i, &j, resp, 3.0);
	assert(i == 0);
	if (status != NULL)
		*status = j;
	return (j == CLIS_OK ? 0 : j);
}

/*--------------------------------------------------------------------*/

void
mgt_cli_start_child(int fdi, int fdo)
{

	cli_i = fdi;
	cli_o = fdo;
}

/*--------------------------------------------------------------------*/

void
mgt_cli_stop_child(void)
{

	cli_i = -1;
	cli_o = -1;
	/* XXX: kick any users */
}

/*--------------------------------------------------------------------*/

struct cli_port {
	unsigned		magic;
#define CLI_PORT_MAGIC		0x5791079f
	struct ev		*ev;
	int			fdi;
	int			fdo;
	int			verbose;
	char			*buf;
	unsigned		nbuf;
	unsigned		lbuf;
	struct cli		cli[1];
	char			name[30];
};

static int
mgt_cli_callback(struct ev *e, int what)
{
	struct cli_port *cp;
	char *p;
	int i;

	CAST_OBJ_NOTNULL(cp, e->priv, CLI_PORT_MAGIC);

	while (!(what & (EV_ERR | EV_HUP))) {
		if (cp->nbuf == cp->lbuf) {
			cp->lbuf += cp->lbuf;
			cp->buf = realloc(cp->buf, cp->lbuf);
			assert(cp->buf != NULL);
		}
		i = read(cp->fdi, cp->buf + cp->nbuf, cp->lbuf - cp->nbuf);
		if (i <= 0)
			break;
		cp->nbuf += i;
		p = strchr(cp->buf, '\n');
		if (p == NULL)
			return (0);
		*p = '\0';
		sbuf_clear(cp->cli->sb);
		cli_dispatch(cp->cli, cli_proto, cp->buf);
		sbuf_finish(cp->cli->sb);
		/* XXX: cp->verbose */
		if (cli_writeres(cp->fdo, cp->cli))
			break;
		i = ++p - cp->buf;
		assert(i <= cp->nbuf);
		if (i < cp->nbuf)
			memcpy(cp->buf, p, cp->nbuf - i);
		cp->nbuf -= i;
		return (0);
	}
	sbuf_delete(cp->cli->sb);
	free(cp->buf);
	close(cp->fdi);
	close(cp->fdo);
	free(cp);
	return (1);
}

void
mgt_cli_setup(int fdi, int fdo, int verbose)
{
	struct cli_port *cp;

	cp = calloc(sizeof *cp, 1);
	assert(cp != NULL);

	sprintf(cp->name, "cli %d->%d", fdi, fdo);
	cp->magic = CLI_PORT_MAGIC;

	cp->fdi = fdi;
	cp->fdo = fdo;
	cp->verbose = verbose;

	cp->lbuf = 4096;
	cp->buf = malloc(cp->lbuf);
	assert(cp->buf != NULL);

	cp->cli->sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	assert(cp->cli->sb != NULL);

	cp->ev = calloc(sizeof *cp->ev, 1);
	cp->ev->name = cp->name;
	cp->ev->fd = fdi;
	cp->ev->fd_flags = EV_RD;
	cp->ev->callback = mgt_cli_callback;
	cp->ev->priv = cp;
	ev_add(mgt_evb, cp->ev);
}
