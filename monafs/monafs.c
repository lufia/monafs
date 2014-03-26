#include "monafs.h"
#include <fcall.h>
#include <9p.h>

int debug;
int verbose;
Node *mountroot;
void (*fatal)(char *fmt, ...);

static void
usage(void)
{
	fprint(2, "usage: monafs [-dv] [-m mtpt] [-s srv]\n");
	exits("usage");
}

static void
fsattach(Req *r)
{
	Aux *a;
	char *spec;

	spec = r->ifcall.aname;
	if(spec && spec[0] != '\0'){
		respond(r, "invalid attach specifier");
		return;
	}

	a = emalloc(sizeof(*a));
	incref(mountroot);
	a->node = mountroot;
	r->fid->aux = a;
	r->ofcall.qid = a->node->qid;
	r->fid->qid = r->ofcall.qid;
	respond(r, nil);
}

static char *
fsclone(Fid *fid, Fid *newfid)
{
	Aux *a, *na;

	a = fid->aux;
	na = emalloc(sizeof(*na));
	incref(a->node);
	na->node = a->node;
	newfid->aux = na;
	return nil;
}

static void
fsdestroyfid(Fid *fid)
{
	Aux *a;
	Node *p;
	char *e;

	a = fid->aux;
	if(a == nil)
		return;

	p = a->node;
	if(a->s){
		if(a->s[a->ns-1] != '\n'){
			a->s = erealloc(a->s, a->ns+2);
			a->s[a->ns++] = '\n';
			a->s[a->ns] = '\0';
		}
		if(e = p->write(p, a->s))
			fprint(2, "%s\n", e);
	}
	free(a->s);
	a->ns = 0;
	a->s = nil;

	decref(p);
	free(a);
}

static char *
fswalk1(Fid *fid, char *name, Qid *qid)
{
	Aux *a;
	Node *p, *np;
	char *e;

	a = fid->aux;
	p = a->node;
	if(p->type == Qtext)
		return "protocol botch";

	e = nil;
	if(np = nodeopen(p, name, &e)){
		*qid = np->qid;
		fid->qid = *qid;
		decref(p);
		incref(np);
		a->node = np;
		return nil;
	}
	if(e)
		return e;

	return "file does not exist";
}

static void
fsopen(Req *r)
{
	Node *p;

	p = ((Aux*)r->fid->aux)->node;
	switch(r->ifcall.mode&~(OCEXEC|ORCLOSE|OTRUNC)){
	case OREAD:
		if(p->mode & 0444){
			respond(r, nil);
			return;
		}
		break;
	case OWRITE:
		if(p->mode & 0222){
			respond(r, nil);
			return;
		}
		break;
	}
	respond(r, "permission denied");
}

static void
fillstat(Dir *d, Node *p)
{
	*d = *p;

	/* will be freed by lib9p */
	d->name = estrdup(p->name);
	d->uid = estrdup("2ch");
	d->gid = estrdup("2ch");
}

static int
dirgen(int i, Dir *d, void *aux)
{
	Node *p;

	p = aux;
	if(i >= p->nsib)
		return -1;
	fillstat(d, p->sibs[i]);
	return 0;
}

static void
fsstat(Req *r)
{
	Node *p;
	char *e;

	p = ((Aux*)r->fid->aux)->node;
	if(e = p->build(p)){
		respond(r, e);
		return;
	}
	fillstat(&r->d, p);
	respond(r, nil);
}

static void
fsread(Req *r)
{
	Node *p;
	char *e;

	p = ((Aux*)r->fid->aux)->node;
	if(e = p->build(p)){
		respond(r, e);
		return;
	}

	if(p->type == Qtext)
		readstr(r, p->s);
	else
		dirread9p(r, dirgen, p);
	respond(r, nil);
}

static void
fswrite(Req *r)
{
	Aux *a;
	long n, off;

	a = r->fid->aux;
	n = r->ifcall.count;
	off = r->ifcall.offset;
	if(a->ns < n+off+1){
		a->s = erealloc(a->s, n+off+1);
		a->ns = n+off;
		a->s[a->ns] = '\0';
	}
	memmove(a->s+off, r->ifcall.data, n);
	r->ofcall.count = n;
	respond(r, nil);
}

Srv fs = {
	.attach = fsattach,
	.clone = fsclone,
	.destroyfid = fsdestroyfid,
	.walk1 = fswalk1,
	.open = fsopen,
	.stat = fsstat,
	.read = fsread,
	.write = fswrite,
};

void
main(int argc, char *argv[])
{
	char *mtpt, *srv;

	mtpt = "/n/2ch";
	srv = nil;
	ARGBEGIN {
	case 'd':
		debug = 1;
		break;
	case 'm':
		mtpt = EARGF(usage());
		break;
	case 's':
		srv = EARGF(usage());
		break;
	case 'v':
		verbose = 1;
		break;
	default:
		usage();
	} ARGEND

	if(argc != 0)
		usage();
	fmtinstall('U', Ufmt);
	fmtinstall('Z', nodefmt);
	cacheinit();
	init();

	fatal = httpfatal;
	httpinit();
	postmountsrv(&fs, srv, mtpt, MREPL);
	httpend();
	exits(nil);
}
