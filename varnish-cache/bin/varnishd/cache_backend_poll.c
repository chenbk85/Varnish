/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2008 Linpro AS
 * All rights reserved.
 *
 * Author: Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: cache_backend_cfg.c 2905 2008-07-08 10:09:03Z phk $
 *
 * Poll backends for collection of health statistics
 *
 * We co-opt threads from the worker pool for probing the backends,
 * but we want to avoid a potentially messy cleanup operation when we
 * retire the backend, so the thread owns the health information, which
 * the backend references, rather than the other way around.
 *
 */

#include "config.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>

#include <sys/socket.h>

#include "shmlog.h"
#include "cli_priv.h"
#include "cache.h"
#include "vrt.h"
#include "cache_backend.h"

static MTX	vbp_mtx;

struct vbp_target {
	unsigned			magic;
#define VBP_TARGET_MAGIC		0x6b7cb656

	struct backend			*backend;
	struct vrt_backend_probe 	probe;
	struct workreq			wrq;
	int				stop;
	int				req_len;
	
	/* Collected statistics */
#define BITMAP(n, c, t, b)	uint64_t	n;
#include "cache_backend_poll.h"
#undef BITMAP

	VTAILQ_ENTRY(vbp_target)	list;
};

static VTAILQ_HEAD(, vbp_target)	vbp_list =
    VTAILQ_HEAD_INITIALIZER(vbp_list);

static char default_request[] = 
    "GET / HTTP/1.1\r\n"
    "Connection: close\r\n"
    "\r\n";

static void
dsleep(double t)
{
	if (t > 10.0)
		(void)sleep((int)round(t));
	else
		(void)usleep((int)round(t * 1e6));
}

/*--------------------------------------------------------------------
 * Poke one backend, once, but possibly at both IPv4 and IPv6 addresses.
 *
 * We do deliberately not use the stuff in cache_backend.c, because we
 * want to measure the backends response without local distractions.
 */

static int
vbp_connect(int pf, const struct sockaddr *sa, socklen_t salen, int tmo)
{
	int s, i;

	s = socket(pf, SOCK_STREAM, 0);
	if (s < 0)
		return (s);

	i = TCP_connect(s, sa, salen, tmo);
	if (i == 0)
		return (s);
	TCP_close(&s);
	return (-1);
}

static int
vbp_poke(struct vbp_target *vt)
{
	int s, tmo, i;
	double t_start, t_now, t_end, rlen;
	struct backend *bp;
	char buf[8192];
	struct pollfd pfda[1], *pfd = pfda;

	bp = vt->backend;
	CHECK_OBJ_NOTNULL(bp, BACKEND_MAGIC);

	t_start = t_now = TIM_real();
	t_end = t_start + vt->probe.timeout;
	tmo = (int)round((t_end - t_now) * 1e3);

	s = -1;
	if (params->prefer_ipv6 && bp->ipv6 != NULL) {
		s = vbp_connect(PF_INET6, bp->ipv6, bp->ipv6len, tmo);
		t_now = TIM_real();
		tmo = (int)round((t_end - t_now) * 1e3);
		if (s >= 0)
			vt->good_ipv6 |= 1;
	}
	if (tmo > 0 && s < 0 && bp->ipv4 != NULL) {
		s = vbp_connect(PF_INET, bp->ipv4, bp->ipv4len, tmo);
		t_now = TIM_real();
		tmo = (int)round((t_end - t_now) * 1e3);
		if (s >= 0)
			vt->good_ipv4 |= 1;
	}
	if (tmo > 0 && s < 0 && bp->ipv6 != NULL) {
		s = vbp_connect(PF_INET6, bp->ipv6, bp->ipv6len, tmo);
		t_now = TIM_real();
		tmo = (int)round((t_end - t_now) * 1e3);
		if (s >= 0)
			vt->good_ipv6 |= 1;
	}
	if (s < 0) {
		/* Got no connection: failed */
		return (0);
	}

	i = write(s, vt->probe.request, vt->req_len);
	if (i != vt->req_len) {
		if (i < 0)
			vt->err_xmit |= 1;
		TCP_close(&s);
		return (0);
	}
	vt->good_xmit |= 1;
	i = shutdown(s, SHUT_WR);
	if (i != 0) {
		vt->err_shut |= 1;
		TCP_close(&s);
		return (0);
	}
	vt->good_shut |= 1;

	t_now = TIM_real();
	tmo = (int)round((t_end - t_now) * 1e3);
	if (tmo < 0) {
		TCP_close(&s);
		return (0);
	}

	pfd->fd = s;
	rlen = 0;
	do {
		pfd->events = POLLIN;
		pfd->revents = 0;
		tmo = (int)round((t_end - t_now) * 1e3);
		if (tmo > 0)
			i = poll(pfd, 1, tmo);
		if (i == 0 || tmo <= 0) {
			TCP_close(&s);
			return (0);
		}
		i = read(s, buf, sizeof buf);
		rlen += i;
	} while (i > 0);

	if (i < 0) {
		vt->err_recv |= 1;
		TCP_close(&s);
		return (0);
	}

	TCP_close(&s);
	t_now = TIM_real();
	vt->good_recv |= 1;
	return (1);
}

/*--------------------------------------------------------------------
 * One thread per backend to be poked.
 */

static void
vbp_wrk_poll_backend(struct worker *w, void *priv)
{
	struct vbp_target *vt;

	(void)w;
	THR_SetName("backend poll");

	CAST_OBJ_NOTNULL(vt, priv, VBP_TARGET_MAGIC);

	LOCK(&vbp_mtx);
	VTAILQ_INSERT_TAIL(&vbp_list, vt, list);
	UNLOCK(&vbp_mtx);

	/* Establish defaults (XXX: Should they go in VCC instead ?) */
	if (vt->probe.request == NULL)
		vt->probe.request = default_request;
	if (vt->probe.timeout == 0.0)
		vt->probe.timeout = 2.0;
	if (vt->probe.interval == 0.0)
		vt->probe.timeout = 5.0;

	printf("Probe(\"%s\", %g, %g)\n",
	    vt->probe.request,
	    vt->probe.timeout,
	    vt->probe.interval);

	vt->req_len = strlen(vt->probe.request);

	/*lint -e{525} indent */
	while (!vt->stop) {
#define BITMAP(n, c, t, b)	vt->n <<= 1;
#include "cache_backend_poll.h"
#undef BITMAP
		vbp_poke(vt);
		dsleep(vt->probe.interval);
	}
	LOCK(&vbp_mtx);
	VTAILQ_REMOVE(&vbp_list, vt, list);
	UNLOCK(&vbp_mtx);
	vt->backend->probe = NULL;
	FREE_OBJ(vt);
	THR_SetName("cache-worker");
}

/*--------------------------------------------------------------------
 * Cli functions
 */

static void
vbp_bitmap(struct cli *cli, const char *s, uint64_t map, const char *lbl)
{
	int i;
	uint64_t u = (1ULL << 63);

	for (i = 0; i < 64; i++) {
		if (map & u)
			cli_out(cli, s);
		else
			cli_out(cli, "-");
		map <<= 1;
	}
	cli_out(cli, " %s\n", lbl);
}

/*lint -e{506} constant value boolean */
/*lint -e{774} constant value boolean */
static void
vbp_health_one(struct cli *cli, struct vbp_target *vt)
{

	cli_out(cli, "Health stats for backend %s\n",
	    vt->backend->vcl_name);
	cli_out(cli, 
	    "Oldest ______________________"
	    "____________________________ Newest\n");

#define BITMAP(n, c, t, b)					\
		if ((vt->n != 0) || (b)) 				\
			vbp_bitmap(cli, (c), vt->n, (t));
#include "cache_backend_poll.h"
#undef BITMAP
}

static void
vbp_health(struct cli *cli, const char * const *av, void *priv)
{
	struct vbp_target *vt;

	(void)av;
	(void)priv;

	VTAILQ_FOREACH(vt, &vbp_list, list)
		vbp_health_one(cli, vt);
}

static struct cli_proto debug_cmds[] = {
        { "debug.health", "debug.health",
                "\tDump backend health stuff\n",
                0, 0, vbp_health },
        { NULL }
};

/*--------------------------------------------------------------------
 * Start/Stop called from cache_backend_cfg.c
 */

void
VBP_Start(struct backend *b, struct vrt_backend_probe const *p)
{
	struct vbp_target *vt;

	ASSERT_CLI();

	ALLOC_OBJ(vt, VBP_TARGET_MAGIC);
	AN(vt);
	if (!memcmp(&vt->probe, p, sizeof *p)) {
		FREE_OBJ(vt);
		return;
	}
	vt->backend = b;
	vt->probe = *p;
	b->probe = vt;

	vt->wrq.func = vbp_wrk_poll_backend;
	vt->wrq.priv = vt;
	if (WRK_Queue(&vt->wrq) == 0)
		return;
	assert(0 == __LINE__);
	b->probe = NULL;
	FREE_OBJ(vt);
}

void
VBP_Stop(struct backend *b)
{
	if (b->probe == NULL)
		return;
	b->probe->stop = 1;
}

/*--------------------------------------------------------------------
 * Initialize the backend probe subsystem
 */

void
VBP_Init(void)
{

	MTX_INIT(&vbp_mtx);

	CLI_AddFuncs(DEBUG_CLI, debug_cmds);
}
