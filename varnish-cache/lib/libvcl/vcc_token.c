/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006 Linpro AS
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
 * Initial implementation by Poul-Henning Kamp <phk@phk.freebsd.dk>
 *
 * $Id$
 */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vsb.h"
#include "queue.h"

#include "libvarnish.h"
#include "vcc_priv.h"
#include "vcl_returns.h"
#include "vcc_compile.h"

#include "libvcl.h"

/*--------------------------------------------------------------------*/

void
vcc_ErrToken(struct tokenlist *tl, struct token *t)
{

	if (t->tok == EOI)
		vsb_printf(tl->sb, "end of input");
	else
		vsb_printf(tl->sb, "'%.*s'", PF(t));
}

void
vcc__ErrInternal(struct tokenlist *tl, const char *func, unsigned line)
{

	vsb_printf(tl->sb, "VCL compiler internal error at %s():%u\n",
	    func, line);
	tl->err = 1;
}

void
vcc_ErrWhere(struct tokenlist *tl, struct token *t)
{
	unsigned lin, pos, x, y;
	const char *p, *l, *f, *b, *e;
	
	lin = 1;
	pos = 0;
	if (t->tok == METHOD)
		return;
	if (t->b >= vcc_default_vcl_b && t->b < vcc_default_vcl_e) {
		f = "Default VCL code (compiled in)";
		b = vcc_default_vcl_b;
		e = vcc_default_vcl_e;
	} else {
		f = "VCL code";
		b = tl->b;
		e = tl->e;
	}
	for (l = p = b; p < t->b; p++) {
		if (*p == '\n') {
			lin++;
			pos = 0;
			l = p + 1;
		} else if (*p == '\t') {
			pos &= ~7;
			pos += 8;
		} else
			pos++;
	}
	vsb_printf(tl->sb, "In %s Line %d Pos %d\n", f, lin, pos);
	x = y = 0;
	for (p = l; p < e && *p != '\n'; p++) {
		if (*p == '\t') {
			y &= ~7;
			y += 8;
			while (x < y) {
				vsb_bcat(tl->sb, " ", 1);
				x++;
			}
		} else {
			x++;
			y++;
			vsb_bcat(tl->sb, p, 1);
		}
	}
	vsb_cat(tl->sb, "\n");
	x = y = 0;
	for (p = l; p < e && *p != '\n'; p++) {
		if (p >= t->b && p < t->e) {
			vsb_bcat(tl->sb, "#", 1);
			x++;
			y++;
			continue;
		}
		if (*p == '\t') {
			y &= ~7;
			y += 8;
		} else
			y++;
		while (x < y) {
			vsb_bcat(tl->sb, "-", 1);
			x++;
		}
	}
	vsb_cat(tl->sb, "\n");
	tl->err = 1;
}

/*--------------------------------------------------------------------*/

void
vcc_NextToken(struct tokenlist *tl)
{
	tl->t = TAILQ_NEXT(tl->t, list);
	if (tl->t == NULL) {
		vsb_printf(tl->sb,
		    "Ran out of input, something is missing or"
		    " maybe unbalanced (...) or {...}\n");
		tl->err = 1;
		return;
	}
}

void
vcc__Expect(struct tokenlist *tl, unsigned tok, int line)
{
	if (tl->t->tok == tok)
		return;
	vsb_printf(tl->sb, "Expected %s got ", vcl_tnames[tok]);
	vcc_ErrToken(tl, tl->t);
	vsb_printf(tl->sb, "\n(program line %u), at\n", line);
	vcc_ErrWhere(tl, tl->t);
}

/*--------------------------------------------------------------------
 * Compare token to token
 */

int
vcc_Teq(struct token *t1, struct token *t2)
{
	if (t1->e - t1->b != t2->e - t2->b)
		return (0);
	return (!memcmp(t1->b, t2->b, t1->e - t1->b));
}

/*--------------------------------------------------------------------
 * Compare ID token to string, return true of match
 */

int
vcc_IdIs(struct token *t, const char *p)
{
	const char *q;

	assert(t->tok == ID);
	for (q = t->b; q < t->e && *p != '\0'; p++, q++)
		if (*q != *p)
			return (0);
	if (q != t->e || *p != '\0')
		return (0);
	return (1);
}

/*--------------------------------------------------------------------
 * Decode %xx in a string
 */

static int
vcc_xdig(const char c)
{
	static const char *xdigit =
	    "0123456789abcdef"
	    "0123456789ABCDEF";
	const char *p;

	p = strchr(xdigit, c);
	assert(p != NULL);
	return ((p - xdigit) % 16);
}

static int
vcc_decstr(struct tokenlist *tl)
{
	const char *p;
	char *q;
	unsigned char u;

	assert(tl->t->tok == CSTR);
	tl->t->dec = malloc((tl->t->e - tl->t->b) - 1);
	assert(tl->t->dec != NULL);
	q = tl->t->dec;
	for (p = tl->t->b + 1; p < tl->t->e - 1; ) {
		if (*p != '%') {
			*q++ = *p++;
			continue;
		}
		if (p + 4 > tl->t->e) {
			vcc_AddToken(tl, CSTR, p, tl->t->e);
			vsb_printf(tl->sb,
			    "Incomplete %%xx escape\n");
			vcc_ErrWhere(tl, tl->t);
			return(1);
		}
		if (!isxdigit(p[1]) || !isxdigit(p[2])) {
			vcc_AddToken(tl, CSTR, p, p + 3);
			vsb_printf(tl->sb,
			    "Illegal hex char in %%xx escape\n");
			vcc_ErrWhere(tl, tl->t);
			return(1);
		}
		u = vcc_xdig(p[1]) * 16 + vcc_xdig(p[2]);
		if (!isgraph(u)) {
			vcc_AddToken(tl, CSTR, p, p + 3);
			vsb_printf(tl->sb,
			    "Control character in %%xx escape\n");
			vcc_ErrWhere(tl, tl->t);
			return(1);
		}
		*q++ = u;
		p += 3;
	}
	*q++ = '\0';
	return (0);
}

/*--------------------------------------------------------------------
 * Add a token to the token list.
 */

void
vcc_AddToken(struct tokenlist *tl, unsigned tok, const char *b, const char *e)
{
	struct token *t;

	t = calloc(sizeof *t, 1);
	assert(t != NULL);
	t->tok = tok;
	t->b = b;
	t->e = e;
	TAILQ_INSERT_TAIL(&tl->tokens, t, list);
	tl->t = t;
	if (0) {
		fprintf(stderr, "[%s %.*s] ",
		    vcl_tnames[tok],(int)(e - b), b);
		if (tok == EOI)
			fprintf(stderr, "\n");
	}
}

/*--------------------------------------------------------------------
 * Lexical analysis and token generation
 */

void
vcc_Lexer(struct tokenlist *tl, const char *b, const char *e)
{
	const char *p, *q;
	unsigned u;

	for (p = b; p < e; ) {

		/* Skip any whitespace */
		if (isspace(*p)) {
			p++;
			continue;
		}

		/* Skip '#.*\n' comments */
		if (*p == '#') {
			while (p < e && *p != '\n')
				p++;
			continue;
		}

		/* Skip C-style comments */
		if (*p == '/' && p[1] == '*') {
			p += 2;
			for (p += 2; p < e; p++) {
				if (*p == '*' && p[1] == '/') {
					p += 2;
					break;
				}
			}
			continue;
		}

		/* Match for the fixed tokens (see token.tcl) */
		u = vcl_fixed_token(p, &q);
		if (u != 0) {
			vcc_AddToken(tl, u, p, q);
			p = q;
			continue;
		}

		/* Match strings, with \\ and \" escapes */
		if (*p == '"') {
			for (q = p + 1; q < e; q++) {
				if (*q == '"') {
					q++;
					break;
				}
				if (*q == '\r' || *q == '\n') {
					vcc_AddToken(tl, EOI, p, q);
					vsb_printf(tl->sb,
					    "Unterminated string at\n");
					vcc_ErrWhere(tl, tl->t);
					return;
				}
			}
			vcc_AddToken(tl, CSTR, p, q);
			if (vcc_decstr(tl))
				return;
			p = q;
			continue;
		}

		/* Match Identifiers */
		if (isident1(*p)) {
			for (q = p; q < e; q++)
				if (!isident(*q))
					break;
			if (isvar(*q)) {
				for (; q < e; q++)
					if (!isvar(*q))
						break;
				vcc_AddToken(tl, VAR, p, q);
			} else {
				vcc_AddToken(tl, ID, p, q);
			}
			p = q;
			continue;
		}

		/* Match numbers { [0-9]+ } */
		if (isdigit(*p)) {
			for (q = p; q < e; q++)
				if (!isdigit(*q))
					break;
			vcc_AddToken(tl, CNUM, p, q);
			p = q;
			continue;
		}
		vcc_AddToken(tl, EOI, p, p + 1);
		vsb_printf(tl->sb, "Syntax error at\n");
		vcc_ErrWhere(tl, tl->t);
		return;
	}
}
