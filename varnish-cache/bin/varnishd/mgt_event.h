
#include <poll.h>
#include "queue.h"

struct ev;
struct evbase;

typedef int ev_cb_f(struct ev *, int what);

struct ev {
	unsigned	magic;
#define EV_MAGIC	0x15c8134b

	/* pub */
	const char	*name;
	int		fd;
	unsigned	fd_flags;
#define		EV_RD	POLLIN
#define		EV_WR	POLLOUT
#define		EV_ERR	POLLERR
#define		EV_HUP	POLLHUP
#define		EV_SIG	-1
	int		sig;
	unsigned	sig_flags;
	double		timeout;
	ev_cb_f		*callback;
	void		*priv;

	/* priv */
	double		__when;
	TAILQ_ENTRY(ev)	__list;
	unsigned	__binheap_idx;
	unsigned	__privflags;
	struct evbase	*__evb;
	int		__poll_idx;
};

struct evbase;

struct evbase *ev_new_base(void);
void ev_destroy_base(struct evbase *evb);

struct ev *ev_new(void);

int ev_add(struct evbase *evb, struct ev *e);
void ev_del(struct evbase *evb, struct ev *e);

int ev_schedule_one(struct evbase *evb);
int ev_schedule(struct evbase *evb);
