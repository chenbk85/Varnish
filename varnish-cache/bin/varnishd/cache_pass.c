/*
 * $Id$
 *
 * XXX: charge bytes to srcaddr
 */

#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "shmlog.h"
#include "cache.h"


/*--------------------------------------------------------------------*/

static int
pass_straight(struct sess *sp, int fd, struct http *hp, char *bi)
{
	int i;
	char *b, *e;
	off_t	cl;
	unsigned c;
	char buf[BUFSIZ];

	if (bi != NULL)
		cl = strtoumax(bi, NULL, 0);
	else
		cl = (1<<30);		/* XXX */

	i = fcntl(fd, F_GETFL);		/* XXX ? */
	i &= ~O_NONBLOCK;
	i = fcntl(fd, F_SETFL, i);

	while (cl != 0) {
		c = cl;
		if (c > sizeof buf)
			c = sizeof buf;
		if (http_GetTail(hp, c, &b, &e)) {
			i = e - b;
			vca_write(sp, b, i);
			cl -= i;
			continue;
		}
		i = read(fd, buf, c);
		if (i == 0 && bi == NULL)
			return (1);
		assert(i > 0);
		vca_write(sp, buf, i);
		vca_flush(sp);
		cl -= i;
	}
	return (0);
}


/*--------------------------------------------------------------------*/

static int
pass_chunked(struct sess *sp, int fd, struct http *hp)
{
	int i, j;
	char *b, *q, *e;
	char *p;
	unsigned u;
	char buf[BUFSIZ];
	char *bp, *be;

	i = fcntl(fd, F_GETFL);		/* XXX ? */
	i &= ~O_NONBLOCK;
	i = fcntl(fd, F_SETFL, i);

	bp = buf;
	be = buf + sizeof buf;
	p = buf;
	while (1) {
		if (http_GetTail(hp, be - bp, &b, &e)) {
			memcpy(bp, b, e - b);
			bp += e - b;
		} else {
			/* XXX: must be safe from backend */
			i = read(fd, bp, be - bp);
			assert(i > 0);
			bp += i;
		}
		/* buffer valid from p to bp */

		u = strtoul(p, &q, 16);
		if (q == NULL || (*q != '\n' && *q != '\r')) {
			INCOMPL();
			/* XXX: move bp to buf start, get more */
		}
		if (*q == '\r')
			q++;
		assert(*q == '\n');
		q++;
		if (u == 0)
			break;

		vca_write(sp, p, q - p);

		p = q;

		while (u > 0) {
			j = u;
			if (bp == p) {
				bp = p = buf;
				break;
			}
			if (bp - p < j)
				j = bp - p;
			vca_write(sp, p, j);
			p += j;
			u -= j;
		}
		while (u > 0) {
			if (http_GetTail(hp, u, &b, &e)) {
				j = e - b;
				vca_write(sp, q, j);
				u -= j;
			} else
				break;
		}
		vca_flush(sp);
		while (u > 0) {
			j = u;
			if (j > sizeof buf)
				j = sizeof buf;
			i = read(fd, buf, j);
			assert(i > 0);
			vca_write(sp, buf, i);
			u -= i;
			vca_flush(sp);
		}
	}
	return (0);
}


/*--------------------------------------------------------------------*/

void
PassBody(struct worker *w, struct sess *sp)
{
	struct vbe_conn *vc;
	struct http *hp;
	char *b;
	int cls;

	hp = sp->bkd_http;
	assert(hp != NULL);
	vc = sp->vbc;
	assert(vc != NULL);

	http_BuildSbuf(sp->fd, Build_Reply, w->sb, hp);
	vca_write(sp, sbuf_data(w->sb), sbuf_len(w->sb));

	if (http_GetHdr(hp, "Content-Length", &b))
		cls = pass_straight(sp, vc->fd, hp, b);
	else if (http_HdrIs(hp, "Connection", "close"))
		cls = pass_straight(sp, vc->fd, hp, NULL);
	else if (http_HdrIs(hp, "Transfer-Encoding", "chunked"))
		cls = pass_chunked(sp, vc->fd, hp);
	else {
		cls = pass_straight(sp, vc->fd, hp, NULL);
	}
	vca_flush(sp);

	if (http_GetHdr(hp, "Connection", &b) && !strcasecmp(b, "close"))
		cls = 1;

	if (cls)
		VBE_ClosedFd(vc);
	else
		VBE_RecycleFd(vc);
}

/*--------------------------------------------------------------------*/
void
PassSession(struct worker *w, struct sess *sp)
{
	int i;
	struct vbe_conn *vc;
	struct http *hp;

	vc = VBE_GetFd(sp->backend, sp->xid);
	assert(vc != NULL);
	VSL(SLT_Backend, sp->fd, "%d %s", vc->fd, sp->backend->vcl_name);

	http_BuildSbuf(vc->fd, Build_Pass, w->sb, sp->http);
	i = write(vc->fd, sbuf_data(w->sb), sbuf_len(w->sb));
	assert(i == sbuf_len(w->sb));

	/* XXX: copy any contents */

	/*
	 * XXX: It might be cheaper to avoid the event_engine and simply
	 * XXX: read(2) the header
	 */
	hp = vc->http;
	http_RecvHead(hp, vc->fd, w->eb, NULL, NULL);
	(void)event_base_loop(w->eb, 0);
	http_DissectResponse(hp, vc->fd);

	sp->bkd_http = hp;
	sp->vbc = vc;
}
