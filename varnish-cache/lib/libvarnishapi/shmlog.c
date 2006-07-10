/*
 * $Id: varnishlog.c 153 2006-04-25 08:17:43Z phk $
 */

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "shmlog.h"
#include "varnishapi.h"

struct VSL_data {
	struct shmloghead	*head;
	unsigned char		*logstart;
	unsigned char		*logend;
	unsigned char		*ptr;

	FILE			*fi;
	unsigned char		rbuf[4 + 255 + 1];

	int			ix_opt;
	unsigned char 		supr[256];
};

#ifndef MAP_HASSEMAPHORE
#define MAP_HASSEMAPHORE 0 /* XXX Linux */
#endif

static int vsl_fd;
static struct shmloghead *vsl_lh;

/*--------------------------------------------------------------------*/

const char *VSL_tags[256] = {
#define SLTM(foo)       [SLT_##foo] = #foo,
#include "shmlog_tags.h"
#undef SLTM
};

/*--------------------------------------------------------------------*/

static int
vsl_shmem_map(void)
{
	int i;
	struct shmloghead slh;

	if (vsl_lh != NULL)
		return (0);

	vsl_fd = open(SHMLOG_FILENAME, O_RDONLY);
	if (vsl_fd < 0) {
		fprintf(stderr, "Cannot open %s: %s\n",
		    SHMLOG_FILENAME, strerror(errno));
		return (1);
	}
	i = read(vsl_fd, &slh, sizeof slh);
	if (i != sizeof slh) {
		fprintf(stderr, "Cannot read %s: %s\n",
		    SHMLOG_FILENAME, strerror(errno));
		return (1);
	}
	if (slh.magic != SHMLOGHEAD_MAGIC) {
		fprintf(stderr, "Wrong magic number in file %s\n",
		    SHMLOG_FILENAME);
		return (1);
	}

	vsl_lh = mmap(NULL, slh.size + sizeof slh,
	    PROT_READ, MAP_HASSEMAPHORE, vsl_fd, 0);
	if (vsl_lh == MAP_FAILED) {
		fprintf(stderr, "Cannot mmap %s: %s\n",
		    SHMLOG_FILENAME, strerror(errno));
		return (1);
	}
	return (0);
}

/*--------------------------------------------------------------------*/

struct VSL_data *
VSL_New(void)
{
	struct VSL_data *vd;

	vd = calloc(sizeof *vd, 1);
	return (vd);
}

int
VSL_OpenLog(struct VSL_data *vd)
{

	if (vd->fi != NULL)
		return (0);

	if (vsl_shmem_map())
		return (1);

	vd->head = vsl_lh;
	vd->logstart = (unsigned char *)vsl_lh + vsl_lh->start;
	vd->logend = vd->logstart + vsl_lh->size;
	vd->ptr = vd->logstart;
	return (0);
}

unsigned char *
VSL_NextLog(struct VSL_data *vd)
{
	unsigned char *p;
	int i;

	if (vd->fi != NULL) {
		while (1) {
			i = fread(vd->rbuf, 4, 1, vd->fi);
			if (i != 1)
				return (NULL);
			if (vd->rbuf[1] > 0) {
				i = fread(vd->rbuf + 4, vd->rbuf[1], 1, vd->fi);
				if (i != 1)
					return (NULL);
			}
			if (!vd->supr[vd->rbuf[0]])
				return (vd->rbuf);
		}
	}

	p = vd->ptr;
	for (p = vd->ptr; ; p = vd->ptr) {
		if (*p == SLT_WRAPMARKER) {
			p = vd->logstart;
			continue;
		}
		if (*p == SLT_ENDMARKER) {
			vd->ptr = p;
			return (NULL);
		}
		vd->ptr = p + p[1] + 4;
		if (!vd->supr[p[0]]) 
			return (p);
	}
}

/*--------------------------------------------------------------------*/

static int
vsl_r_arg(struct VSL_data *vd, const char *opt)
{

	if (!strcmp(opt, "-"))
		vd->fi = stdin;
	else
		vd->fi = fopen(opt, "r");
	if (vd->fi != NULL)
		return (1);
	perror(opt);
	return (-1);
}

/*--------------------------------------------------------------------*/

static int
vsl_ix_arg(struct VSL_data *vd, const char *opt, int arg)
{
	int i, j, l;
	const char *b, *e, *p, *q;

	/* If first option is 'i', set all bits for supression */
	if (arg == 'i' && vd->ix_opt == 0)
		for (i = 0; i < 256; i++)
			vd->supr[i] = 1;
	vd->ix_opt = 1;

	for (b = opt; *b; b = e) {
		while (isspace(*b))
			b++;
		e = strchr(b, ',');
		if (e == NULL)
			e = strchr(b, '\0');
		l = e - b;
		if (*e == ',')
			e++;
		while (isspace(b[l - 1]))
			l--;
		for (i = 0; i < 256; i++) {
			if (VSL_tags[i] == NULL)
				continue;
			p = VSL_tags[i];
			q = b;
			for (j = 0; j < l; j++)
				if (tolower(*q++) != tolower(*p++))
					break;
			if (j != l)
				continue;

			if (arg == 'x')
				vd->supr[i] = 1;
			else
				vd->supr[i] = 0;
			break;
		}
		if (i == 256) {
			fprintf(stderr,
			    "Could not match \"%*.*s\" to any tag\n", l, l, b);
			return (-1);
		}
	}
	return (1);
}

/*--------------------------------------------------------------------*/

int
VSL_Arg(struct VSL_data *vd, int arg, const char *opt)
{
	switch (arg) {
	case 'i':
	case 'x':
		return (vsl_ix_arg(vd, opt, arg));
	case 'r':
		return (vsl_r_arg(vd, opt));
	default:
		return (0);
	}
}

struct varnish_stats *
VSL_OpenStats(void)
{

	if (vsl_shmem_map())
		return (NULL);
	return (&vsl_lh->stats);
}

