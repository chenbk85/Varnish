/*
 * $Id$
 *
 * XXX: We need to pass sessions back into the event engine when they are
 * reused.  Not sure what the most efficient way is for that.  For now
 * write the session pointer to a pipe which the event engine monitors.
 */

#define ACCEPTOR_USE_KQUEUE
#undef ACCEPTOR_USE_LIBEVENT

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/uio.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "config.h"
#include "libvarnish.h"
#include "heritage.h"
#include "shmlog.h"
#include "cache.h"

static pthread_t vca_thread;
static unsigned		xids;

static struct sess *
vca_accept_sess(int fd)
{
	socklen_t l;
	struct sockaddr addr[2];	/* XXX: IPv6 hack */
	struct sess *sp;
	int i;
	struct linger linger;

	VSL_stats->client_conn++;

	l = sizeof addr;
	i = accept(fd, addr, &l);
	if (i < 0) {
		VSL(SLT_Debug, fd, "Accept failed errno=%d", errno);
		/* XXX: stats ? */
		return (NULL);
	}
	sp = SES_New(addr, l);
	assert(sp != NULL);	/* XXX handle */

	sp->fd = i;
	sp->id = i;

#ifdef SO_NOSIGPIPE /* XXX Linux */
	i = 1;
	AZ(setsockopt(sp->fd, SOL_SOCKET, SO_NOSIGPIPE, &i, sizeof i));
#endif
#ifdef SO_LINGER /* XXX Linux*/
	linger.l_onoff = 0;
	linger.l_linger = 0;
	AZ(setsockopt(sp->fd, SOL_SOCKET, SO_LINGER, &linger, sizeof linger));
#endif

	TCP_name(addr, l, sp->addr, sizeof sp->addr, sp->port, sizeof sp->port);
	VSL(SLT_SessionOpen, sp->fd, "%s %s", sp->addr, sp->port);
	return (sp);
}

static void
vca_handover(struct sess *sp, int bad)
{

	if (bad) {
		vca_close_session(sp,
		    bad == 1 ? "overflow" : "no request");
		vca_return_session(sp);
		return;
	}
	sp->step = STP_RECV;
	VSL_stats->client_req++;
	sp->xid = xids++;
	VSL(SLT_XID, sp->fd, "%u", sp->xid);
	WRK_QueueSession(sp);
}

#ifdef ACCEPTOR_USE_LIBEVENT

static struct event_base *evb;
static struct event pipe_e;
static int pipes[2];

static struct event tick_e;
static struct timeval tick_rate;

static struct event accept_e[2 * HERITAGE_NSOCKS];
static TAILQ_HEAD(,sess) sesshead = TAILQ_HEAD_INITIALIZER(sesshead);

/*--------------------------------------------------------------------*/

static void
vca_tick(int a, short b, void *c)
{
	struct sess *sp, *sp2;
	struct timespec t;

	(void)a;
	(void)b;
	(void)c;
	AZ(evtimer_add(&tick_e, &tick_rate));
	clock_gettime(CLOCK_MONOTONIC, &t);
	TAILQ_FOREACH_SAFE(sp, &sesshead, list, sp2) {
		CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
		if (sp->t_idle.tv_sec + 30 < t.tv_sec) {
			TAILQ_REMOVE(&sesshead, sp, list);
			vca_close_session(sp, "timeout");
			vca_return_session(sp);
		}
	}
}

static void
vca_rcvhd_f(int fd, short event, void *arg)
{
	struct sess *sp;
	int i;

	(void)event;

	CAST_OBJ_NOTNULL(sp, arg, SESS_MAGIC);
	i = http_RecvSome(fd, sp->http);
	if (i < 0)
		return;

	event_del(&sp->ev);
	TAILQ_REMOVE(&sesshead, sp, list);
	vca_handover(sp, i);
}

static void
vca_rcvhdev(struct sess *sp)
{

	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	clock_gettime(CLOCK_MONOTONIC, &sp->t_idle);
	TAILQ_INSERT_TAIL(&sesshead, sp, list);
	event_set(&sp->ev, sp->fd, EV_READ | EV_PERSIST, vca_rcvhd_f, sp);
	AZ(event_base_set(evb, &sp->ev));
	AZ(event_add(&sp->ev, NULL));      /* XXX: timeout */
}

static void
pipe_f(int fd, short event, void *arg)
{
	struct sess *sp;
	int i;

	(void)event;
	(void)arg;
	i = read(fd, &sp, sizeof sp);
	assert(i == sizeof sp);
	if (http_RecvPrepAgain(sp->http)) {
		vca_handover(sp, 0);
		return;
	}
	vca_rcvhdev(sp);
}

static void
accept_f(int fd, short event, void *arg)
{
	struct sess *sp;

	(void)event;
	(void)arg;

	sp = vca_accept_sess(fd);
	if (sp == NULL)
		return;

	http_RecvPrep(sp->http);
	vca_rcvhdev(sp);
}

static void *
vca_main(void *arg)
{
	unsigned u;
	struct event *ep;

	(void)arg;

	tick_rate.tv_sec = 1;
	tick_rate.tv_usec = 0;
	AZ(pipe(pipes));
	evb = event_init();
	assert(evb != NULL);

	event_set(&pipe_e, pipes[0], EV_READ | EV_PERSIST, pipe_f, NULL);
	AZ(event_base_set(evb, &pipe_e));
	AZ(event_add(&pipe_e, NULL));

	evtimer_set(&tick_e, vca_tick, NULL);
	AZ(event_base_set(evb, &tick_e));
	
	AZ(evtimer_add(&tick_e, &tick_rate));

	ep = accept_e;
	for (u = 0; u < HERITAGE_NSOCKS; u++) {
		if (heritage.sock_local[u] >= 0) {
			event_set(ep, heritage.sock_local[u],
			    EV_READ | EV_PERSIST,
			    accept_f, NULL);
			AZ(event_base_set(evb, ep));
			AZ(event_add(ep, NULL));
			ep++;
		}
		if (heritage.sock_remote[u] >= 0) {
			event_set(ep, heritage.sock_remote[u],
			    EV_READ | EV_PERSIST,
			    accept_f, NULL);
			AZ(event_base_set(evb, ep));
			AZ(event_add(ep, NULL));
			ep++;
		}
	}

	AZ(event_base_loop(evb, 0));
	INCOMPL();
}

/*--------------------------------------------------------------------*/

void
vca_return_session(struct sess *sp)
{

	if (sp->fd < 0) {
		SES_Delete(sp);
		return;
	}
	VSL(SLT_SessionReuse, sp->fd, "%s %s", sp->addr, sp->port);
	assert(sizeof sp == write(pipes[1], &sp, sizeof sp));
}

#endif /* ACCEPTOR_USE_LIBEVENT */

#ifdef ACCEPTOR_USE_KQUEUE
#include <sys/event.h>

static int kq = -1;

static void
vca_kq_sess(struct sess *sp, int arm)
{
	struct kevent ke[2];

	assert(arm == EV_ADD || arm == EV_DELETE);
	CHECK_OBJ_NOTNULL(sp, SESS_MAGIC);
	memset(ke, 0, sizeof ke);
	EV_SET(&ke[0], sp->fd, EVFILT_READ, arm, 0, 0, sp);
	EV_SET(&ke[1], sp->fd, EVFILT_TIMER, arm , 0, 5000, sp);
	AZ(kevent(kq, ke, 2, NULL, 0, NULL));
}

static void
accept_f(int fd)
{
	struct sess *sp;

	sp = vca_accept_sess(fd);
	if (sp == NULL)
		return;
	clock_gettime(CLOCK_MONOTONIC, &sp->t_idle);
	http_RecvPrep(sp->http);
	vca_kq_sess(sp, EV_ADD);
}

static void *
vca_main(void *arg)
{
	unsigned u;
	struct kevent ke;
	int i;
	struct sess *sp;

	(void)arg;

	kq = kqueue();
	assert(kq >= 0);


	for (u = 0; u < HERITAGE_NSOCKS; u++) {
		if (heritage.sock_local[u] >= 0) {
			memset(&ke, 0, sizeof ke);
			EV_SET(&ke, heritage.sock_local[u],
			    EVFILT_READ, EV_ADD, 0, 0, accept_f);
			AZ(kevent(kq, &ke, 1, NULL, 0, NULL));
		}
		if (heritage.sock_remote[u] >= 0) {
			memset(&ke, 0, sizeof ke);
			EV_SET(&ke, heritage.sock_remote[u],
			    EVFILT_READ, EV_ADD, 0, 0, accept_f);
			AZ(kevent(kq, &ke, 1, NULL, 0, NULL));
		}
	}

	while (1) {
		i = kevent(kq, NULL, 0, &ke, 1, NULL);
		assert(i == 1);
#if 0
		printf("i = %d\n", i);
		printf("ke.ident = %ju\n", (uintmax_t)ke.ident);
		printf("ke.filter = %u\n", ke.filter);
		printf("ke.flags = %u\n", ke.flags);
		printf("ke.fflags = %u\n", ke.fflags);
		printf("ke.data = %jd\n", (intmax_t)ke.data);
		printf("ke.udata = %p\n", ke.udata);
#endif
		if (ke.udata == accept_f) {
			accept_f(ke.ident);
			continue;
		}
		CAST_OBJ_NOTNULL(sp, ke.udata, SESS_MAGIC);
		if (ke.filter == EVFILT_READ) {
			i = http_RecvSome(sp->fd, sp->http);
			if (i == -1)
				continue;
			vca_kq_sess(sp, EV_DELETE);
			vca_handover(sp, i);
			continue;
		}
		if (ke.filter == EVFILT_TIMER) {
			vca_kq_sess(sp, EV_DELETE);
			vca_close_session(sp, "timeout");
			vca_return_session(sp);
			continue;
		} 
		INCOMPL();
	}

	INCOMPL();
}

/*--------------------------------------------------------------------*/

void
vca_return_session(struct sess *sp)
{

	if (sp->fd < 0) {
		SES_Delete(sp);
		return;
	}
	VSL(SLT_SessionReuse, sp->fd, "%s %s", sp->addr, sp->port);
	if (http_RecvPrepAgain(sp->http))
		vca_handover(sp, 0);
	else
		vca_kq_sess(sp, EV_ADD);
}

#endif /* ACCEPTOR_USE_KQUEUE */

/*--------------------------------------------------------------------*/

void
vca_close_session(struct sess *sp, const char *why)
{

	VSL(SLT_SessionClose, sp->fd, why);
	if (sp->fd >= 0)
		AZ(close(sp->fd));
	sp->fd = -1;
}

/*--------------------------------------------------------------------*/

void
VCA_Init(void)
{

	AZ(pthread_create(&vca_thread, NULL, vca_main, NULL));
	srandomdev();
	xids = random();
}
