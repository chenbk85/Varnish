/*
 * $Id$
 */

#include <assert.h>
#include <pthread.h>
#include <sys/uio.h>
#include <sys/time.h>

#include "queue.h"
#include "event.h"
#include "sbuf.h"

#include "vcl_returns.h"
#include "common.h"
#include "miniobj.h"

#define MAX_IOVS		10

struct event_base;
struct cli;
struct sbuf;
struct sess;
struct object;
struct objhead;

/*--------------------------------------------------------------------*/

enum step {
#define STEP(l, u)	STP_##u,
#include "steps.h"
#undef STEP
};

/*--------------------------------------------------------------------
 * HTTP Request/Response/Header handling structure.
 * RSN: struct worker and struct session will have one of these embedded.
 */

typedef void http_callback_f(void *, int bad);

struct http {
	unsigned		magic;
#define HTTP_MAGIC		0x6428b5c9
	struct event		ev;
	http_callback_f		*callback;
	void			*arg;

	char			*s;		/* start of buffer */
	char			*t;		/* start of trailing data */
	char			*v;		/* end of valid bytes */
	char			*e;		/* end of buffer */

	char			*req;
	char			*url;
	char			*proto;
	char			*status;
	char			*response;
	
	unsigned		nhdr;
	char			**hdr;
};

/*--------------------------------------------------------------------*/

struct worker {
	unsigned		magic;
#define WORKER_MAGIC		0x6391adcf
	struct event_base	*eb;
	struct sbuf		*sb;
	struct objhead		*nobjhead;
	struct object		*nobj;

	unsigned		nbr;
	pthread_cond_t		cv;
	TAILQ_ENTRY(worker)	list;

	struct iovec		iov[MAX_IOVS];
	unsigned		niov;
	size_t			liov;
};

struct workreq {
	unsigned		magic;
#define WORKREQ_MAGIC		0x5ccb4eb2
	TAILQ_ENTRY(workreq)	list;
	struct sess		*sess;
};

#include "hash_slinger.h"

/* Backend Connection ------------------------------------------------*/

struct vbe_conn {
	unsigned		magic;
#define VBE_CONN_MAGIC		0x0c5e6592
	TAILQ_ENTRY(vbe_conn)	list;
	struct vbc_mem		*vbcm;
	struct vbe		*vbe;
	int			fd;
	struct event		ev;
	int			inuse;
	struct http		*http;
};

/* Storage -----------------------------------------------------------*/

struct storage {
	unsigned		magic;
#define STORAGE_MAGIC		0x1a4e51c0
	TAILQ_ENTRY(storage)	list;
	unsigned char		*ptr;
	unsigned		len;
	unsigned		space;
	void			*priv;
	struct stevedore	*stevedore;
};

#include "stevedore.h"

/*
 * XXX: in the longer term, we want to support multiple stevedores,
 * XXX: selected by some kind of heuristics based on size, lifetime
 * XXX: etc etc.  For now we support only one.
 */
extern struct stevedore *stevedore;

/* -------------------------------------------------------------------*/

struct object {	
	unsigned		magic;
#define OBJECT_MAGIC		0x32851d42
	unsigned 		refcnt;
	unsigned		xid;
	struct objhead		*objhead;

	unsigned		heap_idx;
	unsigned		ban_seq;

	unsigned		pass;

	unsigned		response;

	unsigned		valid;
	unsigned		cacheable;

	unsigned		busy;
	unsigned		len;

	time_t			age;
	time_t			entered;
	time_t			ttl;

	char			*header;
	TAILQ_ENTRY(object)	list;

	TAILQ_ENTRY(object)	deathrow;

	TAILQ_HEAD(, storage)	store;

	TAILQ_HEAD(, sess)	waitinglist;
};

struct objhead {
	unsigned		magic;
#define OBJHEAD_MAGIC		0x1b96615d
	void			*hashpriv;

	pthread_mutex_t		mtx;
	TAILQ_HEAD(,object)	objects;
};

/* -------------------------------------------------------------------*/

struct srcaddr {
	unsigned		magic;
#define SRCADDR_MAGIC		0x375111db
	TAILQ_ENTRY(srcaddr)	list;
	unsigned		nsess;
	char			addr[TCP_ADDRBUFSIZE];
	unsigned		sum;
	time_t			first;
	time_t			ttl;
	uint64_t		bytes;
	struct srcaddrhead	*sah;
};

struct sess {
	unsigned		magic;
#define SESS_MAGIC		0x2c2f9c5a
	int			fd;
	unsigned		xid;

	struct worker		*wrk;

	/* formatted ascii client address */
	char			addr[TCP_ADDRBUFSIZE];
	char			port[TCP_PORTBUFSIZE];
	struct srcaddr		*srcaddr;

	/* HTTP request */
	struct http		*http;

	time_t			t_req;
	time_t			t_resp;

	enum step		step;
	unsigned 		handling;

	TAILQ_ENTRY(sess)	list;

	struct vbe_conn		*vbc;
	struct http		*bkd_http;
	struct backend		*backend;
	struct object		*obj;
	struct VCL_conf		*vcl;

	/* Various internal stuff */
	struct sessmem		*mem;
	time_t			t0;

	struct workreq		workreq;
};

struct backend {
	unsigned		magic;
#define BACKEND_MAGIC		0x64c4c7c6
	const char	*vcl_name;
	const char	*hostname;
	const char	*portname;
	unsigned	ip;

	struct addrinfo	*addr;
	struct addrinfo	*last_addr;
#if 0
	double		responsetime;
	double		timeout;
	double		bandwidth;
	int		down;
#endif

	/* internal stuff */
	struct vbe	*vbe;
};

/* Prototypes etc ----------------------------------------------------*/


/* cache_acceptor.c */
void vca_return_session(struct sess *sp);
void vca_close_session(struct sess *sp, const char *why);
void VCA_Init(void);

/* cache_backend.c */
void VBE_Init(void);
struct vbe_conn *VBE_GetFd(struct backend *bp, unsigned xid);
void VBE_ClosedFd(struct vbe_conn *vc);
void VBE_RecycleFd(struct vbe_conn *vc);

/* cache_ban.c */
void BAN_Init(void);
void cli_func_url_purge(struct cli *cli, char **av, void *priv);
void BAN_NewObj(struct object *o);
int BAN_CheckObject(struct object *o, const char *url);

/* cache_center.c [CNT] */
void CNT_Session(struct sess *sp);

/* cache_expiry.c */
void EXP_Insert(struct object *o);
void EXP_Init(void);
void EXP_TTLchange(struct object *o);

/* cache_fetch.c */
int FetchBody(struct worker *w, struct sess *sp);
int FetchHeaders(struct worker *w, struct sess *sp);

/* cache_hash.c */
struct object *HSH_Lookup(struct sess *sp);
void HSH_Unbusy(struct object *o);
void HSH_Ref(struct object *o);
void HSH_Deref(struct object *o);
void HSH_Init(void);

/* cache_http.c */
void http_Init(struct http *ht, void *space);
int http_GetHdr(struct http *hp, const char *hdr, char **ptr);
int http_GetHdrField(struct http *hp, const char *hdr, const char *field, char **ptr);
int http_GetStatus(struct http *hp);
int http_HdrIs(struct http *hp, const char *hdr, const char *val);
int http_GetTail(struct http *hp, unsigned len, char **b, char **e);
int http_Read(struct http *hp, int fd, void *b, unsigned len);
void http_RecvHead(struct http *hp, int fd, struct event_base *eb, http_callback_f *func, void *arg);
int http_DissectRequest(struct http *sp, int fd);
int http_DissectResponse(struct http *sp, int fd);
enum http_build {
	Build_Pipe,
	Build_Pass,
	Build_Fetch,
	Build_Reply
};
void http_BuildSbuf(int fd, enum http_build mode, struct sbuf *sb, struct http *hp);

/* cache_pass.c */
void PassSession(struct worker *w, struct sess *sp);
void PassBody(struct worker *w, struct sess *sp);

/* cache_pipe.c */
void PipeSession(struct worker *w, struct sess *sp);

/* cache_pool.c */
void WRK_Init(void);
void WRK_QueueSession(struct sess *sp);

/* cache_session.c [SES] */
void SES_Init(void);
struct sess *SES_New(struct sockaddr *addr, unsigned len);
void SES_Delete(struct sess *sp);
void SES_RefSrcAddr(struct sess *sp);
void SES_RelSrcAddr(struct sess *sp);
void SES_ChargeBytes(struct sess *sp, uint64_t bytes);

/* cache_shmlog.c */

void VSL_Init(void);
#ifdef SHMLOGHEAD_MAGIC
void VSLR(enum shmlogtag tag, unsigned id, const char *b, const char *e);
void VSL(enum shmlogtag tag, unsigned id, const char *fmt, ...);
#define HERE() VSL(SLT_Debug, 0, "HERE: %s(%d)", __func__, __LINE__)
#define INCOMPL() do {							\
	VSL(SLT_Debug, 0, "INCOMPLETE AT: %s(%d)", __func__, __LINE__); \
	fprintf(stderr,"INCOMPLETE AT: %s(%d)\n", (const char *)__func__, __LINE__);	\
	abort();							\
	} while (0)
#endif

/* cache_response.c */
void RES_Error(struct worker *w, struct sess *sp, int error, const char *msg);
void RES_Flush(struct sess *sp);
void RES_Write(struct sess *sp, void *ptr, size_t len);
void RES_WriteObj(struct worker *w, struct sess *sp);

/* cache_vcl.c */
void VCL_Init(void);
void VCL_Rel(struct VCL_conf *vc);
struct VCL_conf *VCL_Get(void);
int VCL_Load(const char *fn, const char *name, struct cli *cli);

#define VCL_RET_MAC(l,u,b)
#define VCL_MET_MAC(l,u,b) void VCL_##l##_method(struct sess *);
#include "vcl_returns.h"
#undef VCL_MET_MAC
#undef VCL_RET_MAC

#ifdef CLI_PRIV_H
cli_func_t	cli_func_config_list;
cli_func_t	cli_func_config_load;
cli_func_t	cli_func_config_unload;
cli_func_t	cli_func_config_use;
#endif

/* rfc2616.c */
int RFC2616_cache_policy(struct sess *sp, struct http *hp);
