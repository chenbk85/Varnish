/*
 * $Id$
 */

#include <assert.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sbuf.h>
#include <event.h>

#include "libvarnish.h"
#include "shmlog.h"
#include "vcl_lang.h"
#include "cache.h"

static void
PassReturn(struct sess *sp)
{

	HttpdAnalyze(sp, 2);
}

/*--------------------------------------------------------------------*/
void
PassSession(struct worker *w, struct sess *sp)
{
	int fd, i, j;
	void *fd_token;
	struct sess sp2;
	char buf[BUFSIZ];
	off_t	cl;

	fd = VBE_GetFd(sp->backend, &fd_token);
	assert(fd != -1);

	HttpdBuildSbuf(0, 1, w->sb, sp);

	i = write(fd, sbuf_data(w->sb), sbuf_len(w->sb));
	assert(i == sbuf_len(w->sb));

	/* XXX: copy any contents */


	memset(&sp2, 0, sizeof sp2);
	sp2.rd_e = &w->e1;
	sp2.fd = fd;
	HttpdGetHead(&sp2, w->eb, PassReturn);
	event_base_loop(w->eb, 0);

	HttpdBuildSbuf(1, 1, w->sb, &sp2);
	i = write(sp->fd, sbuf_data(w->sb), sbuf_len(w->sb));
	assert(i == sbuf_len(w->sb));

	if (sp2.http.H_Content_Length != NULL) {
		cl = strtoumax(sp2.http.H_Content_Length, NULL, 0);
		i = fcntl(sp2.fd, F_GETFL);
		i &= ~O_NONBLOCK;
		i = fcntl(sp2.fd, F_SETFL, i);
		assert(i != -1);
		i = sp2.rcv_len - sp2.hdr_end;
		if (i > 0) {
			j = write(sp->fd, sp2.rcv + sp2.hdr_end, i);
			assert(j == i);
			cl -= i;
		}
		while (cl > 0) {
			j = sizeof buf;
			if (j > cl)
				j = cl;
			i = recv(sp2.fd, buf, j, 0);
			assert(i >= 0);
			if (i > 0) {
				cl -= i;
				j = write(sp->fd, buf, i);
				assert(j == i);
			} else if (i == 0) {
				break;
			}
		}
	}

	/* XXX: move remaining input in sp->rcv */
	/* XXX: return session to acceptor */
}
