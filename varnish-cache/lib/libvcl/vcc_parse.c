/*-
 * Copyright (c) 2006 Verdens Gang AS
 * Copyright (c) 2006-2007 Linpro AS
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
 * $Id$
 */

#include <stdio.h>
#include <string.h>

#include "vsb.h"

#include "vcc_priv.h"
#include "vcc_compile.h"
#include "libvarnish.h"

#include "vrt.h"

/*--------------------------------------------------------------------*/

static void Compound(struct tokenlist *tl);
static void Cond_0(struct tokenlist *tl);

/*--------------------------------------------------------------------*/

#define L(tl, foo)	do {	\
	tl->indent += INDENT;	\
	foo;			\
	tl->indent -= INDENT;	\
} while (0)

#define C(tl, sep)	do {					\
	Fb(tl, 1, "VRT_count(sp, %u)%s\n", ++tl->cnt, sep);	\
	tl->t->cnt = tl->cnt; 					\
} while (0)

/*--------------------------------------------------------------------
 * Recognize and convert units of time, return seconds.
 */

static double
TimeUnit(struct tokenlist *tl)
{
	double sc = 1.0;

	assert(tl->t->tok == ID);
	if (vcc_IdIs(tl->t, "ms"))
		sc = 1e-3;
	else if (vcc_IdIs(tl->t, "s"))
		sc = 1.0;
	else if (vcc_IdIs(tl->t, "m"))
		sc = 60.0;
	else if (vcc_IdIs(tl->t, "h"))
		sc = 60.0 * 60.0;
	else if (vcc_IdIs(tl->t, "d"))
		sc = 60.0 * 60.0 * 24.0;
	else {
		vsb_printf(tl->sb, "Unknown time unit ");
		vcc_ErrToken(tl, tl->t);
		vsb_printf(tl->sb, ".  Legal are 's', 'm', 'h' and 'd'\n");
		vcc_ErrWhere(tl, tl->t);
		return (1.0);
	}
	vcc_NextToken(tl);
	return (sc);
}

/*--------------------------------------------------------------------
 * Recognize and convert units of size, return bytes.
 */

static double
SizeUnit(struct tokenlist *tl)
{
	double sc = 1.0;

	assert(tl->t->tok == ID);
	if (vcc_IdIs(tl->t, "b"))
		sc = 1.0;
	else if (vcc_IdIs(tl->t, "kb"))
		sc = 1024.0;
	else if (vcc_IdIs(tl->t, "mb") || vcc_IdIs(tl->t, "Mb"))
		sc = 1024.0 * 1024.0;
	else if (vcc_IdIs(tl->t, "gb") || vcc_IdIs(tl->t, "Gb"))
		sc = 1024.0 * 1024.0 * 1024.0;
	else {
		vsb_printf(tl->sb, "Unknown size unit ");
		vcc_ErrToken(tl, tl->t);
		vsb_printf(tl->sb, ".  Legal are 'kb', 'mb' and 'gb'\n");
		vcc_ErrWhere(tl, tl->t);
		return (1.0);
	}
	vcc_NextToken(tl);
	return (sc);
}

/*--------------------------------------------------------------------
 * Recognize and convert units of rate as { space '/' time }
 */

static double
RateUnit(struct tokenlist *tl)
{
	double sc = 1.0;

	assert(tl->t->tok == ID);
	sc = SizeUnit(tl);
	Expect(tl, '/');
	vcc_NextToken(tl);
	sc /= TimeUnit(tl);
	return (sc);
}

/*--------------------------------------------------------------------
 * Recognize and convert { CNUM } to unsigned value
 */

unsigned
vcc_UintVal(struct tokenlist *tl)
{
	unsigned d = 0;
	const char *p;

	Expect(tl, CNUM);
	for (p = tl->t->b; p < tl->t->e; p++) {
		d *= 10;
		d += *p - '0';
	}
	vcc_NextToken(tl);
	return (d);
}

/*--------------------------------------------------------------------
 * Recognize and convert { CNUM [ '.' [ CNUM ] ] } to double value
 */

double
vcc_DoubleVal(struct tokenlist *tl)
{
	double d = 0.0, e = 0.1;
	const char *p;

	Expect(tl, CNUM);
	for (p = tl->t->b; p < tl->t->e; p++) {
		d *= 10;
		d += *p - '0';
	}
	vcc_NextToken(tl);
	if (tl->t->tok != '.')
		return (d);
	vcc_NextToken(tl);
	if (tl->t->tok != CNUM)
		return (d);
	for (p = tl->t->b; p < tl->t->e; p++) {
		d += (*p - '0') * e;
		e *= 0.1;
	}
	vcc_NextToken(tl);
	return (d);
}

/*--------------------------------------------------------------------*/

void
vcc_TimeVal(struct tokenlist *tl)
{
	double v, sc;

	v = vcc_DoubleVal(tl);
	ExpectErr(tl, ID);
	sc = TimeUnit(tl);
	Fb(tl, 0, "(%g * %g)", v, sc);
}

void
vcc_SizeVal(struct tokenlist *tl)
{
	double v, sc;

	v = vcc_DoubleVal(tl);
	ExpectErr(tl, ID);
	sc = SizeUnit(tl);
	Fb(tl, 0, "(%g * %g)", v, sc);
}

void
vcc_RateVal(struct tokenlist *tl)
{
	double v, sc;

	v = vcc_DoubleVal(tl);
	ExpectErr(tl, ID);
	sc = RateUnit(tl);
	Fb(tl, 0, "(%g * %g)", v, sc);
}

/*--------------------------------------------------------------------*/

static void
Cond_String(const struct var *vp, struct tokenlist *tl)
{
	char *p;

	switch (tl->t->tok) {
	case '~':
		vcc_NextToken(tl);
		p = vcc_regexp(tl, 0);
		vcc_NextToken(tl);
		Fb(tl, 1, "VRT_re_match(%s, %s)\n", vp->rname, p);
		break;
	case T_EQ:
	case T_NEQ:
		Fb(tl, 1, "%sstrcmp(%s, ",
		    tl->t->tok == T_EQ ? "!" : "", vp->rname);
		vcc_NextToken(tl);
		ExpectErr(tl, CSTR);
		EncToken(tl->fb, tl->t);
		Fb(tl, 0, ")\n");
		vcc_NextToken(tl);
		break;
	default:
		Fb(tl, 1, "%s != (void*)0\n", vp->rname);
		break;
	}
}

static void
Cond_Int(const struct var *vp, struct tokenlist *tl)
{

	Fb(tl, 1, "%s ", vp->rname);
	switch (tl->t->tok) {
	case T_EQ:
	case T_NEQ:
	case T_LEQ:
	case T_GEQ:
	case '>':
	case '<':
		Fb(tl, 0, "%.*s ", PF(tl->t));
		vcc_NextToken(tl);
		switch(vp->fmt) {
		case TIME:
			vcc_TimeVal(tl);
			break;
		case INT:
			ExpectErr(tl, CNUM);
			Fb(tl, 0, "%.*s ", PF(tl->t));
			vcc_NextToken(tl);
			break;
		case SIZE:
			vcc_SizeVal(tl);
			break;
		default:
			vsb_printf(tl->sb,
			    "No conditions available for variable '%s'\n",
			    vp->name);
			vcc_ErrWhere(tl, tl->t);
			return;
		}
		Fb(tl, 0, "\n");
		break;
	default:
		vsb_printf(tl->sb, "Invalid condition ");
		vcc_ErrToken(tl, tl->t);
		vsb_printf(tl->sb, " on integer variable\n");
		vsb_printf(tl->sb,
		    "  only '==', '!=', '<', '>', '<=' and '>=' are legal\n");
		vcc_ErrWhere(tl, tl->t);
		break;
	}
}

static void
Cond_Bool(const struct var *vp, const struct tokenlist *tl)
{

	Fb(tl, 1, "%s\n", vp->rname);
}

static void
Cond_Backend(const struct var *vp, const struct tokenlist *tl)
{

	Fb(tl, 1, "%s\n", vp->rname);
}

static void
Cond_2(struct tokenlist *tl)
{
	struct var *vp;

	C(tl, ",");
	if (tl->t->tok == '!') {
		Fb(tl, 1, "!(\n");
		vcc_NextToken(tl);
	} else {
		Fb(tl, 1, "(\n");
	}
	if (tl->t->tok == '(') {
		vcc_NextToken(tl);
		Cond_0(tl);
		ExpectErr(tl, ')');
		vcc_NextToken(tl);
	} else if (tl->t->tok == VAR) {
		vp = vcc_FindVar(tl, tl->t, vcc_vars);
		ERRCHK(tl);
		assert(vp != NULL);
		vcc_NextToken(tl);
		switch (vp->fmt) {
		case INT:	L(tl, Cond_Int(vp, tl)); break;
		case SIZE:	L(tl, Cond_Int(vp, tl)); break;
		case BOOL:	L(tl, Cond_Bool(vp, tl)); break;
		case IP:	L(tl, vcc_Cond_Ip(vp, tl)); break;
		case STRING:	L(tl, Cond_String(vp, tl)); break;
		case TIME:	L(tl, Cond_Int(vp, tl)); break;
		case BACKEND:	L(tl, Cond_Backend(vp, tl)); break;
		default:
			vsb_printf(tl->sb,
			    "Variable '%s'"
			    " has no conditions that can be checked\n",
			    vp->name);
			vcc_ErrWhere(tl, tl->t);
			return;
		}
	} else {
		vsb_printf(tl->sb,
		    "Syntax error in condition, expected '(', '!' or"
		    " variable name, found ");
		vcc_ErrToken(tl, tl->t);
		vsb_printf(tl->sb, "\n");
		vcc_ErrWhere(tl, tl->t);
		return;
	}
	Fb(tl, 1, ")\n");
}

static void
Cond_1(struct tokenlist *tl)
{

	Fb(tl, 1, "(\n");
	L(tl, Cond_2(tl));
	while (tl->t->tok == T_CAND) {
		vcc_NextToken(tl);
		Fb(tl, 1, ") && (\n");
		L(tl, Cond_2(tl));
	}
	Fb(tl, 1, ")\n");
}

static void
Cond_0(struct tokenlist *tl)
{

	Fb(tl, 1, "(\n");
	L(tl, Cond_1(tl));
	while (tl->t->tok == T_COR) {
		vcc_NextToken(tl);
		Fb(tl, 1, ") || (\n");
		L(tl, Cond_1(tl));
	}
	Fb(tl, 1, ")\n");
}

static void
Conditional(struct tokenlist *tl)
{

	ExpectErr(tl, '(');
	vcc_NextToken(tl);
	Fb(tl, 1, "(\n");
	L(tl, Cond_0(tl));
	ERRCHK(tl);
	Fb(tl, 1, ")\n");
	ExpectErr(tl, ')');
	vcc_NextToken(tl);
}

/*--------------------------------------------------------------------*/

static void
IfStmt(struct tokenlist *tl)
{

	ExpectErr(tl, T_IF);
	Fb(tl, 1, "if \n");
	vcc_NextToken(tl);
	L(tl, Conditional(tl));
	ERRCHK(tl);
	L(tl, Compound(tl));
	ERRCHK(tl);
	while (1) {
		switch (tl->t->tok) {
		case T_ELSE:
			vcc_NextToken(tl);
			if (tl->t->tok != T_IF) {
				Fb(tl, 1, "else \n");
				L(tl, Compound(tl));
				ERRCHK(tl);
				return;
			}
			/* FALLTHROUGH */
		case T_ELSEIF:
		case T_ELSIF:
			Fb(tl, 1, "else if \n");
			vcc_NextToken(tl);
			L(tl, Conditional(tl));
			ERRCHK(tl);
			L(tl, Compound(tl));
			ERRCHK(tl);
			break;
		default:
			C(tl, ";");
			return;
		}
	}
}

/*--------------------------------------------------------------------*/

static void
Compound(struct tokenlist *tl)
{

	ExpectErr(tl, '{');
	Fb(tl, 1, "{\n");
	tl->indent += INDENT;
	C(tl, ";");
	vcc_NextToken(tl);
	while (1) {
		ERRCHK(tl);
		switch (tl->t->tok) {
		case '{':
			Compound(tl);
			break;
		case T_IF:
			IfStmt(tl);
			break;
		case '}':
			vcc_NextToken(tl);
			tl->indent -= INDENT;
			Fb(tl, 1, "}\n");
			return;
		case CSRC:
			Fb(tl, 1, "%.*s\n",
			    tl->t->e - (tl->t->b + 2),
			    tl->t->b + 1);
			vcc_NextToken(tl);
			break;
		case EOI:
			vsb_printf(tl->sb,
			    "End of input while in compound statement\n");
			tl->err = 1;
			return;
		default:
			vcc_ParseAction(tl);
			ERRCHK(tl);
			ExpectErr(tl, ';');
			vcc_NextToken(tl);
			break;
		}
	}
}

/*--------------------------------------------------------------------*/

static void
Function(struct tokenlist *tl)
{
	int m;

	vcc_NextToken(tl);
	ExpectErr(tl, ID);

	m = IsMethod(tl->t);
	if (m != -1) {
		assert(m < N_METHODS);
		tl->fb = tl->fm[m];
		if (tl->mprocs[m] == NULL) {
			tl->mprocs[m] = vcc_AddProc(tl, tl->t);
			vcc_AddDef(tl, tl->t, R_FUNC);
			vcc_AddRef(tl, tl->t, R_FUNC);
		}
		tl->curproc = tl->mprocs[m];
	} else {
		tl->fb = tl->fc;
		tl->curproc = vcc_AddProc(tl, tl->t);
		vcc_AddDef(tl, tl->t, R_FUNC);
		Fh(tl, 0, "static int VGC_function_%.*s (struct sess *sp);\n",
		    PF(tl->t));
		Fc(tl, 1, "static int\n");
		Fc(tl, 1, "VGC_function_%.*s (struct sess *sp)\n", PF(tl->t));
	}
	vcc_NextToken(tl);
	tl->indent += INDENT;
	Fb(tl, 1, "{\n");
	L(tl, Compound(tl));
	Fb(tl, 1, "}\n");
	tl->indent -= INDENT;
	Fb(tl, 0, "\n");
	tl->fb = NULL;
	tl->curproc = NULL;
}

/*--------------------------------------------------------------------
 * Top level of parser, recognize:
 *	Function definitions
 *	Backend definitions
 *	End of input
 */

void
vcc_Parse(struct tokenlist *tl)
{

	while (tl->t->tok != EOI) {
		ERRCHK(tl);
		switch (tl->t->tok) {
		case T_ACL:
			vcc_Acl(tl);
			break;
		case T_SUB:
			Function(tl);
			break;
		case T_BACKEND:
			vcc_ParseBackend(tl);
			break;
		case EOI:
			break;
		default:
			vsb_printf(tl->sb,
			    "Expected 'acl', 'sub' or 'backend', found ");
			vcc_ErrToken(tl, tl->t);
			vsb_printf(tl->sb, " at\n");
			vcc_ErrWhere(tl, tl->t);
			return;
		}
	}
}
