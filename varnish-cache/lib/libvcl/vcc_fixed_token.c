/*
 * $Id$
 *
 * NB:  This file is machine generated, DO NOT EDIT!
 *
 * Edit and run vcc_gen_fixed_token.tcl instead
 */

#include "config.h"
#include <stdio.h>
#include <ctype.h>
#include "config.h"
#include "vcc_priv.h"
#include "vsb.h"

#define M1()     do {*q = p + 1; return (p[0]); } while (0)
#define M2(c, t) do {if (p[1] == (c)) { *q = p + 2; return (t); }} while (0)

unsigned
vcl_fixed_token(const char *p, const char **q)
{

	switch (p[0]) {
	case '!':
		M2('~', T_NOMATCH);
		M2('=', T_NEQ);
		M1();
	case '%':
		M1();
	case '&':
		M2('&', T_CAND);
		M1();
	case '(':
		M1();
	case ')':
		M1();
	case '*':
		M2('=', T_MUL);
		M1();
	case '+':
		M2('=', T_INCR);
		M2('+', T_INC);
		M1();
	case ',':
		M1();
	case '-':
		M2('=', T_DECR);
		M2('-', T_DEC);
		M1();
	case '.':
		M1();
	case '/':
		M2('=', T_DIV);
		M1();
	case ';':
		M1();
	case '<':
		M2('=', T_LEQ);
		M2('<', T_SHL);
		M1();
	case '=':
		M2('=', T_EQ);
		M1();
	case '>':
		M2('>', T_SHR);
		M2('=', T_GEQ);
		M1();
	case 'e':
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'i' &&
		    p[4] == 'f' && !isvar(p[5])) {
			*q = p + 5;
			return (T_ELSIF);
		}
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e' &&
		    p[4] == 'i' && p[5] == 'f' && !isvar(p[6])) {
			*q = p + 6;
			return (T_ELSEIF);
		}
		if (p[1] == 'l' && p[2] == 's' && p[3] == 'e'
		     && !isvar(p[4])) {
			*q = p + 4;
			return (T_ELSE);
		}
		return (0);
	case 'i':
		if (p[1] == 'n' && p[2] == 'c' && p[3] == 'l' &&
		    p[4] == 'u' && p[5] == 'd' && p[6] == 'e'
		     && !isvar(p[7])) {
			*q = p + 7;
			return (T_INCLUDE);
		}
		M2('f', T_IF);
		return (0);
	case '{':
		M1();
	case '|':
		M2('|', T_COR);
		M1();
	case '}':
		M1();
	case '~':
		M1();
	default:
		return (0);
	}
}

const char * const vcl_tnames[256] = {
	['!'] = "'!'",
	['%'] = "'%'",
	['&'] = "'&'",
	['('] = "'('",
	[')'] = "')'",
	['*'] = "'*'",
	['+'] = "'+'",
	[','] = "','",
	['-'] = "'-'",
	['.'] = "'.'",
	['/'] = "'/'",
	['<'] = "'<'",
	['='] = "'='",
	['>'] = "'>'",
	['{'] = "'{'",
	['}'] = "'}'",
	['|'] = "'|'",
	['~'] = "'~'",
	[';'] = "';'",
	[CNUM] = "CNUM",
	[CSRC] = "CSRC",
	[CSTR] = "CSTR",
	[EOI] = "EOI",
	[ID] = "ID",
	[T_CAND] = "&&",
	[T_COR] = "||",
	[T_DEC] = "--",
	[T_DECR] = "-=",
	[T_DIV] = "/=",
	[T_ELSE] = "else",
	[T_ELSEIF] = "elseif",
	[T_ELSIF] = "elsif",
	[T_EQ] = "==",
	[T_GEQ] = ">=",
	[T_IF] = "if",
	[T_INC] = "++",
	[T_INCLUDE] = "include",
	[T_INCR] = "+=",
	[T_LEQ] = "<=",
	[T_MUL] = "*=",
	[T_NEQ] = "!=",
	[T_NOMATCH] = "!~",
	[T_SHL] = "<<",
	[T_SHR] = ">>",
	[VAR] = "VAR",
};

void
vcl_output_lang_h(struct vsb *sb)
{

	/* ../../include/vcl.h */

	vsb_cat(sb, "/*\n * $Id$\n *\n * NB:  This file is machine generate");
	vsb_cat(sb, "d, DO NOT EDIT!\n *\n * Edit and run vcc_gen_fixed_tok");
	vsb_cat(sb, "en.tcl instead\n */\n\nstruct sess;\n");
	vsb_cat(sb, "struct cli;\n\ntypedef void vcl_init_f(struct cli *);\n");
	vsb_cat(sb, "typedef void vcl_fini_f(struct cli *);\n");
	vsb_cat(sb, "typedef int vcl_func_f(struct sess *sp);\n");
	vsb_cat(sb, "\n/* VCL Methods */\n#define VCL_MET_RECV\t\t(1U << 0)");
	vsb_cat(sb, "\n#define VCL_MET_PIPE\t\t(1U << 1)\n");
	vsb_cat(sb, "#define VCL_MET_PASS\t\t(1U << 2)\n");
	vsb_cat(sb, "#define VCL_MET_HASH\t\t(1U << 3)\n");
	vsb_cat(sb, "#define VCL_MET_MISS\t\t(1U << 4)\n");
	vsb_cat(sb, "#define VCL_MET_HIT\t\t(1U << 5)\n");
	vsb_cat(sb, "#define VCL_MET_FETCH\t\t(1U << 6)\n");
	vsb_cat(sb, "#define VCL_MET_DELIVER\t\t(1U << 7)\n");
	vsb_cat(sb, "#define VCL_MET_ERROR\t\t(1U << 8)\n");
	vsb_cat(sb, "\n#define VCL_MET_MAX\t\t9\n\n/* VCL Returns */\n");
	vsb_cat(sb, "#define VCL_RET_DELIVER\t\t0\n#define VCL_RET_ERROR\t\t");
	vsb_cat(sb, "1\n#define VCL_RET_FETCH\t\t2\n");
	vsb_cat(sb, "#define VCL_RET_HASH\t\t3\n#define VCL_RET_LOOKUP\t\t4");
	vsb_cat(sb, "\n#define VCL_RET_PASS\t\t5\n#define VCL_RET_PIPE\t\t6");
	vsb_cat(sb, "\n#define VCL_RET_RESTART\t\t7\n");
	vsb_cat(sb, "\n#define VCL_RET_MAX\t\t8\n\nstruct VCL_conf {\n");
	vsb_cat(sb, "\tunsigned\tmagic;\n#define VCL_CONF_MAGIC\t0x7406c509");
	vsb_cat(sb, "\t/* from /dev/random */\n\n\tstruct director\t**direc");
	vsb_cat(sb, "tor;\n\tunsigned\tndirector;\n\tstruct vrt_ref\t*ref;\n");
	vsb_cat(sb, "\tunsigned\tnref;\n\tunsigned\tbusy;\n");
	vsb_cat(sb, "\tunsigned\tdiscard;\n\n\tunsigned\tnsrc;\n");
	vsb_cat(sb, "\tconst char\t**srcname;\n\tconst char\t**srcbody;\n");
	vsb_cat(sb, "\n\tvcl_init_f\t*init_func;\n\tvcl_fini_f\t*fini_func;");
	vsb_cat(sb, "\n\n\tvcl_func_f\t*recv_func;\n");
	vsb_cat(sb, "\tvcl_func_f\t*pipe_func;\n\tvcl_func_f\t*pass_func;\n");
	vsb_cat(sb, "\tvcl_func_f\t*hash_func;\n\tvcl_func_f\t*miss_func;\n");
	vsb_cat(sb, "\tvcl_func_f\t*hit_func;\n\tvcl_func_f\t*fetch_func;\n");
	vsb_cat(sb, "\tvcl_func_f\t*deliver_func;\n\tvcl_func_f\t*error_fun");
	vsb_cat(sb, "c;\n};\n");

	/* ../../include/vrt.h */

	vsb_cat(sb, "/*-\n * Copyright (c) 2006 Verdens Gang AS\n");
	vsb_cat(sb, " * Copyright (c) 2006-2009 Linpro AS\n");
	vsb_cat(sb, " * All rights reserved.\n *\n * Author: Poul-Henning K");
	vsb_cat(sb, "amp <phk@phk.freebsd.dk>\n *\n * Redistribution and us");
	vsb_cat(sb, "e in source and binary forms, with or without\n");
	vsb_cat(sb, " * modification, are permitted provided that the follo");
	vsb_cat(sb, "wing conditions\n * are met:\n * 1. Redistributions of");
	vsb_cat(sb, " source code must retain the above copyright\n");
	vsb_cat(sb, " *    notice, this list of conditions and the followin");
	vsb_cat(sb, "g disclaimer.\n * 2. Redistributions in binary form mu");
	vsb_cat(sb, "st reproduce the above copyright\n");
	vsb_cat(sb, " *    notice, this list of conditions and the followin");
	vsb_cat(sb, "g disclaimer in the\n *    documentation and/or other ");
	vsb_cat(sb, "materials provided with the distribution.\n");
	vsb_cat(sb, " *\n * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CON");
	vsb_cat(sb, "TRIBUTORS ``AS IS'' AND\n * ANY EXPRESS OR IMPLIED WAR");
	vsb_cat(sb, "RANTIES, INCLUDING, BUT NOT LIMITED TO, THE\n");
	vsb_cat(sb, " * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS F");
	vsb_cat(sb, "OR A PARTICULAR PURPOSE\n * ARE DISCLAIMED.  IN NO EVE");
	vsb_cat(sb, "NT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE\n");
	vsb_cat(sb, " * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEM");
	vsb_cat(sb, "PLARY, OR CONSEQUENTIAL\n * DAMAGES (INCLUDING, BUT NO");
	vsb_cat(sb, "T LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS\n");
	vsb_cat(sb, " * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSI");
	vsb_cat(sb, "NESS INTERRUPTION)\n * HOWEVER CAUSED AND ON ANY THEOR");
	vsb_cat(sb, "Y OF LIABILITY, WHETHER IN CONTRACT, STRICT\n");
	vsb_cat(sb, " * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWI");
	vsb_cat(sb, "SE) ARISING IN ANY WAY\n * OUT OF THE USE OF THIS SOFT");
	vsb_cat(sb, "WARE, EVEN IF ADVISED OF THE POSSIBILITY OF\n");
	vsb_cat(sb, " * SUCH DAMAGE.\n *\n * $Id$\n *\n");
	vsb_cat(sb, " * Runtime support for compiled VCL programs.\n");
	vsb_cat(sb, " *\n * XXX: When this file is changed, lib/libvcl/vcc_");
	vsb_cat(sb, "gen_fixed_token.tcl\n * XXX: *MUST* be rerun.\n");
	vsb_cat(sb, " */\n\nstruct sess;\nstruct vsb;\n");
	vsb_cat(sb, "struct cli;\nstruct director;\n");
	vsb_cat(sb, "struct VCL_conf;\nstruct sockaddr;\n");
	vsb_cat(sb, "\n/*\n * A backend probe specification\n");
	vsb_cat(sb, " */\n\nextern const void * const vrt_magic_string_end;");
	vsb_cat(sb, "\n\nstruct vrt_backend_probe {\n");
	vsb_cat(sb, "\tconst char\t*url;\n\tconst char\t*request;\n");
	vsb_cat(sb, "\tdouble\t\ttimeout;\n\tdouble\t\tinterval;\n");
	vsb_cat(sb, "\tunsigned\texp_status;\n\tunsigned\twindow;\n");
	vsb_cat(sb, "\tunsigned\tthreshold;\n\tunsigned\tinitial;\n");
	vsb_cat(sb, "};\n\n/*\n * A backend is a host+port somewhere on the");
	vsb_cat(sb, " network\n */\nstruct vrt_backend {\n");
	vsb_cat(sb, "\tconst char\t\t\t*vcl_name;\n\tconst char\t\t\t*ident");
	vsb_cat(sb, ";\n\n\tconst char\t\t\t*hosthdr;\n");
	vsb_cat(sb, "\n\tconst unsigned char\t\t*ipv4_sockaddr;\n");
	vsb_cat(sb, "\tconst unsigned char\t\t*ipv6_sockaddr;\n");
	vsb_cat(sb, "\n\tdouble\t\t\t\tconnect_timeout;\n");
	vsb_cat(sb, "\tdouble\t\t\t\tfirst_byte_timeout;\n");
	vsb_cat(sb, "\tdouble\t\t\t\tbetween_bytes_timeout;\n");
	vsb_cat(sb, "\tunsigned\t\t\tmax_connections;\n");
	vsb_cat(sb, "\tunsigned\t\t\tsaintmode_threshold;\n");
	vsb_cat(sb, "\tstruct vrt_backend_probe\tprobe;\n");
	vsb_cat(sb, "};\n\n/*\n * A director with an unpredictable reply\n");
	vsb_cat(sb, " */\n\nstruct vrt_dir_random_entry {\n");
	vsb_cat(sb, "\tint\t\t\t\t\thost;\n\tdouble\t\t\t\t\tweight;\n");
	vsb_cat(sb, "};\n\nstruct vrt_dir_random {\n");
	vsb_cat(sb, "\tconst char\t\t\t\t*name;\n\tunsigned\t\t\t\tretries;");
	vsb_cat(sb, "\n\tunsigned\t\t\t\tnmember;\n\tconst struct vrt_dir_r");
	vsb_cat(sb, "andom_entry\t*members;\n};\n\n/*\n");
	vsb_cat(sb, " * A director with round robin selection\n");
	vsb_cat(sb, " */\n\nstruct vrt_dir_round_robin_entry {\n");
	vsb_cat(sb, "\tint\t\t\t\t\thost;\n};\n\nstruct vrt_dir_round_robin");
	vsb_cat(sb, " {\n\tconst char\t\t\t\t*name;\n");
	vsb_cat(sb, "\tunsigned\t\t\t\tnmember;\n\tconst struct vrt_dir_rou");
	vsb_cat(sb, "nd_robin_entry\t*members;\n};\n");
	vsb_cat(sb, "\n\n/*\n * other stuff.\n * XXX: document when bored\n");
	vsb_cat(sb, " */\n\nstruct vrt_ref {\n\tunsigned\tsource;\n");
	vsb_cat(sb, "\tunsigned\toffset;\n\tunsigned\tline;\n");
	vsb_cat(sb, "\tunsigned\tpos;\n\tunsigned\tcount;\n");
	vsb_cat(sb, "\tconst char\t*token;\n};\n\n/* ACL related */\n");
	vsb_cat(sb, "#define VRT_ACL_MAXADDR\t\t16\t/* max(IPv4, IPv6) */\n");
	vsb_cat(sb, "\nvoid VRT_acl_log(const struct sess *, const char *ms");
	vsb_cat(sb, "g);\n\n/* Regexp related */\nvoid VRT_re_init(void **,");
	vsb_cat(sb, " const char *);\nvoid VRT_re_fini(void *);\n");
	vsb_cat(sb, "int VRT_re_match(const char *, void *re);\n");
	vsb_cat(sb, "const char *VRT_regsub(const struct sess *sp, int all,");
	vsb_cat(sb, " const char *,\n    void *, const char *);\n");
	vsb_cat(sb, "\nvoid VRT_panic(struct sess *sp, const char *, ...);\n");
	vsb_cat(sb, "void VRT_purge(struct sess *sp, char *, ...);\n");
	vsb_cat(sb, "void VRT_purge_string(struct sess *sp, const char *, .");
	vsb_cat(sb, "..);\n\nvoid VRT_count(const struct sess *, unsigned);");
	vsb_cat(sb, "\nint VRT_rewrite(const char *, const char *);\n");
	vsb_cat(sb, "void VRT_error(struct sess *, unsigned, const char *);");
	vsb_cat(sb, "\nint VRT_switch_config(const char *);\n");
	vsb_cat(sb, "\nenum gethdr_e { HDR_REQ, HDR_RESP, HDR_OBJ, HDR_BERE");
	vsb_cat(sb, "Q, HDR_BERESP };\nchar *VRT_GetHdr(const struct sess *");
	vsb_cat(sb, ", enum gethdr_e where, const char *);\n");
	vsb_cat(sb, "void VRT_SetHdr(const struct sess *, enum gethdr_e whe");
	vsb_cat(sb, "re, const char *,\n    const char *, ...);\n");
	vsb_cat(sb, "void VRT_handling(struct sess *sp, unsigned hand);\n");
	vsb_cat(sb, "\n/* Simple stuff */\nint VRT_strcmp(const char *s1, c");
	vsb_cat(sb, "onst char *s2);\nvoid VRT_memmove(void *dst, const voi");
	vsb_cat(sb, "d *src, unsigned len);\n\nvoid VRT_ESI(struct sess *sp");
	vsb_cat(sb, ");\nvoid VRT_Rollback(struct sess *sp);\n");
	vsb_cat(sb, "\n/* Synthetic pages */\nvoid VRT_synth_page(struct se");
	vsb_cat(sb, "ss *sp, unsigned flags, const char *, ...);\n");
	vsb_cat(sb, "\n/* Backend related */\nvoid VRT_init_dir(struct cli ");
	vsb_cat(sb, "*, struct director **, const char *name,\n");
	vsb_cat(sb, "    int idx, const void *priv);\n");
	vsb_cat(sb, "void VRT_fini_dir(struct cli *, struct director *);\n");
	vsb_cat(sb, "\nchar *VRT_IP_string(const struct sess *sp, const str");
	vsb_cat(sb, "uct sockaddr *sa);\nchar *VRT_int_string(const struct ");
	vsb_cat(sb, "sess *sp, int);\nchar *VRT_double_string(const struct ");
	vsb_cat(sb, "sess *sp, double);\nchar *VRT_time_string(const struct");
	vsb_cat(sb, " sess *sp, double);\nconst char *VRT_backend_string(st");
	vsb_cat(sb, "ruct sess *sp);\n\n#define VRT_done(sp, hand)\t\t\t\\\n");
	vsb_cat(sb, "\tdo {\t\t\t\t\t\\\n\t\tVRT_handling(sp, hand);\t\t\\\n");
	vsb_cat(sb, "\t\treturn (1);\t\t\t\\\n\t} while (0)\n");

	/* ../../include/vrt_obj.h */

	vsb_cat(sb, "/*\n * $Id$\n *\n * NB:  This file is machine generate");
	vsb_cat(sb, "d, DO NOT EDIT!\n *\n * Edit and run vcc_gen_fixed_tok");
	vsb_cat(sb, "en.tcl instead\n */\n\nstruct sockaddr * VRT_r_client_");
	vsb_cat(sb, "ip(const struct sess *);\nstruct sockaddr * VRT_r_serv");
	vsb_cat(sb, "er_ip(struct sess *);\nconst char * VRT_r_server_hostn");
	vsb_cat(sb, "ame(struct sess *);\nconst char * VRT_r_server_identit");
	vsb_cat(sb, "y(struct sess *);\nint VRT_r_server_port(struct sess *");
	vsb_cat(sb, ");\nconst char * VRT_r_req_request(const struct sess *");
	vsb_cat(sb, ");\nvoid VRT_l_req_request(const struct sess *, const ");
	vsb_cat(sb, "char *, ...);\nconst char * VRT_r_req_url(const struct");
	vsb_cat(sb, " sess *);\nvoid VRT_l_req_url(const struct sess *, con");
	vsb_cat(sb, "st char *, ...);\nconst char * VRT_r_req_proto(const s");
	vsb_cat(sb, "truct sess *);\nvoid VRT_l_req_proto(const struct sess");
	vsb_cat(sb, " *, const char *, ...);\nvoid VRT_l_req_hash(struct se");
	vsb_cat(sb, "ss *, const char *);\nstruct director * VRT_r_req_back");
	vsb_cat(sb, "end(struct sess *);\nvoid VRT_l_req_backend(struct ses");
	vsb_cat(sb, "s *, struct director *);\nint VRT_r_req_restarts(const");
	vsb_cat(sb, " struct sess *);\ndouble VRT_r_req_grace(struct sess *");
	vsb_cat(sb, ");\nvoid VRT_l_req_grace(struct sess *, double);\n");
	vsb_cat(sb, "const char * VRT_r_req_xid(struct sess *);\n");
	vsb_cat(sb, "unsigned VRT_r_req_esi(struct sess *);\n");
	vsb_cat(sb, "void VRT_l_req_esi(struct sess *, unsigned);\n");
	vsb_cat(sb, "unsigned VRT_r_req_backend_healthy(const struct sess *");
	vsb_cat(sb, ");\nconst char * VRT_r_bereq_request(const struct sess");
	vsb_cat(sb, " *);\nvoid VRT_l_bereq_request(const struct sess *, co");
	vsb_cat(sb, "nst char *, ...);\nconst char * VRT_r_bereq_url(const ");
	vsb_cat(sb, "struct sess *);\nvoid VRT_l_bereq_url(const struct ses");
	vsb_cat(sb, "s *, const char *, ...);\nconst char * VRT_r_bereq_pro");
	vsb_cat(sb, "to(const struct sess *);\nvoid VRT_l_bereq_proto(const");
	vsb_cat(sb, " struct sess *, const char *, ...);\n");
	vsb_cat(sb, "double VRT_r_bereq_connect_timeout(struct sess *);\n");
	vsb_cat(sb, "void VRT_l_bereq_connect_timeout(struct sess *, double");
	vsb_cat(sb, ");\ndouble VRT_r_bereq_first_byte_timeout(struct sess ");
	vsb_cat(sb, "*);\nvoid VRT_l_bereq_first_byte_timeout(struct sess *");
	vsb_cat(sb, ", double);\ndouble VRT_r_bereq_between_bytes_timeout(s");
	vsb_cat(sb, "truct sess *);\nvoid VRT_l_bereq_between_bytes_timeout");
	vsb_cat(sb, "(struct sess *, double);\nconst char * VRT_r_beresp_pr");
	vsb_cat(sb, "oto(const struct sess *);\nvoid VRT_l_beresp_proto(con");
	vsb_cat(sb, "st struct sess *, const char *, ...);\n");
	vsb_cat(sb, "void VRT_l_beresp_saintmode(const struct sess *, doubl");
	vsb_cat(sb, "e);\nint VRT_r_beresp_status(const struct sess *);\n");
	vsb_cat(sb, "void VRT_l_beresp_status(const struct sess *, int);\n");
	vsb_cat(sb, "const char * VRT_r_beresp_response(const struct sess *");
	vsb_cat(sb, ");\nvoid VRT_l_beresp_response(const struct sess *, co");
	vsb_cat(sb, "nst char *, ...);\nunsigned VRT_r_beresp_cacheable(con");
	vsb_cat(sb, "st struct sess *);\nvoid VRT_l_beresp_cacheable(const ");
	vsb_cat(sb, "struct sess *, unsigned);\ndouble VRT_r_beresp_ttl(con");
	vsb_cat(sb, "st struct sess *);\nvoid VRT_l_beresp_ttl(const struct");
	vsb_cat(sb, " sess *, double);\ndouble VRT_r_beresp_grace(const str");
	vsb_cat(sb, "uct sess *);\nvoid VRT_l_beresp_grace(const struct ses");
	vsb_cat(sb, "s *, double);\nconst char * VRT_r_obj_proto(const stru");
	vsb_cat(sb, "ct sess *);\nvoid VRT_l_obj_proto(const struct sess *,");
	vsb_cat(sb, " const char *, ...);\nint VRT_r_obj_status(const struc");
	vsb_cat(sb, "t sess *);\nvoid VRT_l_obj_status(const struct sess *,");
	vsb_cat(sb, " int);\nconst char * VRT_r_obj_response(const struct s");
	vsb_cat(sb, "ess *);\nvoid VRT_l_obj_response(const struct sess *, ");
	vsb_cat(sb, "const char *, ...);\nint VRT_r_obj_hits(const struct s");
	vsb_cat(sb, "ess *);\nunsigned VRT_r_obj_cacheable(const struct ses");
	vsb_cat(sb, "s *);\nvoid VRT_l_obj_cacheable(const struct sess *, u");
	vsb_cat(sb, "nsigned);\ndouble VRT_r_obj_ttl(const struct sess *);\n");
	vsb_cat(sb, "void VRT_l_obj_ttl(const struct sess *, double);\n");
	vsb_cat(sb, "double VRT_r_obj_grace(const struct sess *);\n");
	vsb_cat(sb, "void VRT_l_obj_grace(const struct sess *, double);\n");
	vsb_cat(sb, "double VRT_r_obj_lastuse(const struct sess *);\n");
	vsb_cat(sb, "const char * VRT_r_resp_proto(const struct sess *);\n");
	vsb_cat(sb, "void VRT_l_resp_proto(const struct sess *, const char ");
	vsb_cat(sb, "*, ...);\nint VRT_r_resp_status(const struct sess *);\n");
	vsb_cat(sb, "void VRT_l_resp_status(const struct sess *, int);\n");
	vsb_cat(sb, "const char * VRT_r_resp_response(const struct sess *);");
	vsb_cat(sb, "\nvoid VRT_l_resp_response(const struct sess *, const ");
	vsb_cat(sb, "char *, ...);\ndouble VRT_r_now(const struct sess *);\n");
}
