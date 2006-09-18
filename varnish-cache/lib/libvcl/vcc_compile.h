/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006 Linpro AS
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
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
 * $Id$
 */

#include "queue.h"
#include "vcl_returns.h"

#define INDENT		2

struct token {
	unsigned		tok;
	const char		*b;
	const char		*e;
	TAILQ_ENTRY(token)	list;
	unsigned		cnt;
	char			*dec;
};

struct tokenlist {
	TAILQ_HEAD(, token)	tokens;
	const char		*b;
	const char		*e;
	struct token		*t;
	int			indent;
	unsigned		cnt;
	struct vsb		*fc, *fh, *fi, *ff;
	TAILQ_HEAD(, ref)	refs;
	struct vsb		*sb;
	int			err;
	int			nbackend;
	TAILQ_HEAD(, proc)	procs;
	struct proc		*curproc;

	unsigned		recnt;
};

enum var_type {
	BACKEND,
	BOOL,
	INT,
	FLOAT,
	SIZE,
	RATE,
	TIME,
	STRING,
	IP,
	HOSTNAME,
	PORTNAME,
	HEADER
};

enum ref_type {
	R_FUNC,
	R_ACL,
	R_BACKEND
};

struct ref {
	enum ref_type		type;
	struct token		*name;
	unsigned		defcnt;
	unsigned		refcnt;
	TAILQ_ENTRY(ref)	list;
};

struct var {
	const char		*name;
	enum var_type		fmt;
	unsigned		len;
	const char		*rname;
	const char		*lname;
};

struct method {
	const char		*name;
	const char		*defname;
	unsigned		returns;
};

struct proccall {
	TAILQ_ENTRY(proccall)	list;
	struct proc		*p;
	struct token		*t;
};

struct proc {
	TAILQ_ENTRY(proc)	list;
	TAILQ_HEAD(,proccall)	calls;
	struct token		*name;
	unsigned		returns;
	unsigned		exists;
	unsigned		called;
	unsigned		active;
	struct token		*returnt[VCL_RET_MAX];
};


/*--------------------------------------------------------------------*/

/* vcc_acl.c */

void vcc_Acl(struct tokenlist *tl);
void vcc_Cond_Ip(struct var *vp, struct tokenlist *tl);

/* vcc_compile.c */
extern const char *vcc_default_vcl_b, *vcc_default_vcl_e;
void Fh(struct tokenlist *tl, int indent, const char *fmt, ...);
void Fc(struct tokenlist *tl, int indent, const char *fmt, ...);
void Fi(struct tokenlist *tl, int indent, const char *fmt, ...);
void Ff(struct tokenlist *tl, int indent, const char *fmt, ...);
unsigned UintVal(struct tokenlist *tl);
void AddDef(struct tokenlist *tl, struct token *t, enum ref_type type);
void AddRef(struct tokenlist *tl, struct token *t, enum ref_type type);
void EncString(struct vsb *sb, struct token *t);


/* vcc_obj.c */
extern struct var vcc_be_vars[];
extern struct var vcc_vars[];
extern const char *vrt_obj_h;


/* vcc_token.c */
void vcc_ErrToken(struct tokenlist *tl, struct token *t);
void vcc_ErrWhere(struct tokenlist *tl, struct token *t);
void vcc__Expect(struct tokenlist *tl, unsigned tok, int line);
int vcc_Teq(struct token *t1, struct token *t2);
int vcc_IdIs(struct token *t, const char *p);
void vcc_Lexer(struct tokenlist *tl, const char *b, const char *e);
void vcc_NextToken(struct tokenlist *tl);
void vcc__ErrInternal(struct tokenlist *tl, const char *func, unsigned line);
void vcc_AddToken(struct tokenlist *tl, unsigned tok, const char *b, const char *e);

#define ERRCHK(tl)      do { if ((tl)->err) return; } while (0)
#define ErrInternal(tl) vcc__ErrInternal(tl, __func__, __LINE__)
#define Expect(a, b) vcc__Expect(a, b, __LINE__)
#define ExpectErr(a, b) do { vcc__Expect(a, b, __LINE__); ERRCHK(a);} while (0)

