/*
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pthread.h>
#include <unistd.h>
#include <queue.h>
#include <sys/time.h>
#include <sbuf.h>
#include <event.h>
#include <md5.h>

#include "libvarnish.h"
#include "shmlog.h"
#include "vcl_lang.h"
#include "cache.h"

static TAILQ_HEAD(, sess) shd = TAILQ_HEAD_INITIALIZER(shd);

static pthread_cond_t	shdcnd;

static int
LookupSession(struct worker *w, struct sess *sp)
{
	struct object *o;
	unsigned char key[16];
	MD5_CTX ctx;

	if (w->nobj == NULL) {
		w->nobj = calloc(sizeof *w->nobj, 1);	
		assert(w->nobj != NULL);
		w->nobj->busy = 1;
		TAILQ_INIT(&w->nobj->store);
	}

	MD5Init(&ctx);
	MD5Update(&ctx, sp->http.url, strlen(sp->http.url));
	MD5Final(key, &ctx);
	o = hash->lookup(key, w->nobj);
	if (o == w->nobj) {
		VSL(SLT_Debug, 0, "Lookup new %p %s", o, sp->http.url);
		w->nobj = NULL;
	} else {
		VSL(SLT_Debug, 0, "Lookup found %p %s", o, sp->http.url);
	}
	/*
	 * XXX: if obj is busy, park session on it
	 */

	sp->obj = o;
	sp->handling = HND_Unclass;
	sp->vcl->lookup_func(sp);
	if (sp->handling == HND_Unclass) {
		if (o->valid && o->cacheable)
			sp->handling = HND_Deliver;
		else
			sp->handling = HND_Pass;
	}
	return (0);
}

static int
DeliverSession(struct worker *w, struct sess *sp)
{
	char buf[BUFSIZ];
	int i, j;
	struct storage *st;

	sprintf(buf,
	    "HTTP/1.1 200 OK\r\n"
	    "Server: Varnish\r\n"
	    "Content-Length: %u\r\n"
	    "\r\n", sp->obj->len);

	j = strlen(buf);
	i = write(sp->fd, buf, j);
	assert(i == j);
	TAILQ_FOREACH(st, &sp->obj->store, list) {
		i = write(sp->fd, st->ptr, st->len);
		assert(i == st->len);
	}
	return (1);
}

static void *
CacheWorker(void *priv)
{
	struct sess *sp;
	struct worker w;
	int done;

	memset(&w, 0, sizeof w);
	w.eb = event_init();
	assert(w.eb != NULL);
	w.sb = sbuf_new(NULL, NULL, 0, SBUF_AUTOEXTEND);
	assert(w.sb != NULL);
	
	(void)priv;
	AZ(pthread_mutex_lock(&sessmtx));
	while (1) {
		while (1) {
			sp = TAILQ_FIRST(&shd);
			if (sp != NULL)
				break;
			AZ(pthread_cond_wait(&shdcnd, &sessmtx));
		}
		TAILQ_REMOVE(&shd, sp, list);
		sp->vcl = GetVCL();
		AZ(pthread_mutex_unlock(&sessmtx));

		HttpdAnalyze(sp, 1);

		sp->backend = sp->vcl->default_backend;

		/*
		 * Call the VCL recv function.
		 * Default action is to lookup
		 */
		sp->handling = HND_Lookup;
		
		sp->vcl->recv_func(sp);

		for (done = 0; !done; ) {
			printf("Handling: %d\n", sp->handling);
			switch(sp->handling) {
			case HND_Lookup:
				done = LookupSession(&w, sp);
				break;
			case HND_Fetch:
				done = FetchSession(&w, sp);
				break;
			case HND_Deliver:
				done = DeliverSession(&w, sp);
				break;
			case HND_Pipe:
				PipeSession(&w, sp);
				done = 1;
				break;
			case HND_Pass:
				PassSession(&w, sp);
				done = 1;
				break;
			case HND_Unclass:
				assert(sp->handling == HND_Unclass);
			}
		}
		if (sp->http.H_Connection != NULL &&
		    !strcmp(sp->http.H_Connection, "close")) {
			close(sp->fd);
			sp->fd = -1;
		}

		AZ(pthread_mutex_lock(&sessmtx));
		RelVCL(sp->vcl);
		sp->vcl = NULL;
		if (sp->fd < 0) 
			vca_retire_session(sp);
		else
			vca_recycle_session(sp);
	}
}

void
DealWithSession(struct sess *sp)
{
	AZ(pthread_mutex_lock(&sessmtx));
	TAILQ_INSERT_TAIL(&shd, sp, list);
	AZ(pthread_mutex_unlock(&sessmtx));
	AZ(pthread_cond_signal(&shdcnd));
}

void
CacheInitPool(void)
{
	pthread_t tp;

	AZ(pthread_cond_init(&shdcnd, NULL));

	AZ(pthread_create(&tp, NULL, CacheWorker, NULL));
	AZ(pthread_detach(tp));
}
