#include "monafs.h"

enum {
	Nodebufsz = 256,
};

static uvlong idgen;

static Node *
newnode(Node *parent, char *name)
{
	Node *p;

	p = emalloc(sizeof(*p));
	p->qid.path = idgen++;
	incref(parent);
	p->parent = parent;
	p->name = mestrdup(name);
	p->atime = time(nil);
	p->mtime = p->atime;
	return p;
}

static Node *
newnode1(char *name)
{
	/* for used by nodecreateroot only */
	Node *p;

	p = emalloc(sizeof(*p));
	p->qid.path = idgen++;
	incref(p);
	p->parent = p;
	p->name = mestrdup(name);
	p->atime = time(nil);
	p->mtime = p->atime;
	return p;
}

static Node *
findnode(Node *p, char *name, int mk)
{
	Node *np;
	int lo, hi, m, n;

	lo = 0;
	hi = p->nsib;
	while(hi-lo > 1){
		m = (lo+hi) / 2;
		if(strcmp(name, p->sibs[m]->name) < 0)
			hi = m;
		else
			lo = m;
	}
	assert(lo==hi || lo==hi-1);
	if(lo==hi || strcmp(name, p->sibs[lo]->name)!=0){
		if(!mk)
			return nil;
		if(p->nsib >= p->n){
			p->n += Nodebufsz;
			n = p->n * sizeof(p->sibs[0]);
			p->sibs = erealloc(p->sibs, n);
		}
		/* 
		 * if we're down to a single place 'twixt lo and hi, the insertion might need
		 * to go at lo or at hi.  strcmp to find out.  the list needs to stay sorted.
		 */
		if(lo==hi-1 && strcmp(name, p->sibs[lo]->name) < 0)
			hi = lo;

		np = newnode(p, name);
		if(hi < p->nsib){
			n = (p->nsib-hi) * sizeof(p->sibs[0]);
			memmove(p->sibs+hi+1, p->sibs+hi, n);
		}
		incref(np);
		p->sibs[hi] = np;
		p->nsib++;
		p = p->sibs[hi];
	}else
		p = p->sibs[lo];

	p->atime = time(nil);
	return cache(p);
}

static Node *
removenode(Node *p, char *name)
{
	Node *np;
	int lo, hi, m, n;

	lo = 0;
	hi = p->nsib;
	while(hi-lo > 1){
		m = (lo+hi) / 2;
		if(strcmp(name, p->sibs[m]->name) < 0)
			hi = m;
		else
			lo = m;
	}
	assert(lo==hi || lo==hi-1);
	if(lo==hi || strcmp(name, p->sibs[lo]->name)!=0)
		fatal("cannot delete node");
	np = p->sibs[lo];
	n = (p->nsib-lo-1) * sizeof(p->sibs[0]);
	memmove(p->sibs+lo, p->sibs+lo+1, n);
	p->nsib--;
	p->hot = 0;
	decref(np);
	return np;
}

static void
reset(Node *p)
{
	mestrfree(p->host);
	mestrfree(p->s);
	p->host = p->s = nil;
	p->length = 0;
	p->hot = 0;

	/* p->attr don't free */
	mestrfree(p->status);
	mestrfree(p->subject);
	mestrfree(p->from);
	mestrfree(p->mail);
	mestrfree(p->id);
	mestrfree(p->msg);
	p->attr = nil;
	p->status = p->subject = nil;
	p->from = p->mail = nil;
	p->id = p->msg = nil;
}

void
nodefree(Node *p)
{
	if(p == nil)
		return;

	if(verbose)
		fprint(2, "free %Z\n", p);
	assert(p->ref == 1);
	removenode(p->parent, p->name);
	decref(p->parent);

	assert(p->nsib == 0);
	free(p->sibs);

	mestrfree(p->name);
	reset(p);
	free(p);
}

void
noderefresh(Node *p)
{
	int i;

	if(verbose)
		fprint(2, "refresh %Z\n", p);
	p->hot = 0;
	for(i = 0; i < p->nsib; i++)
		noderefresh(p->sibs[i]);
}

Node *
nodeopen(Node *p, char *name, char **e)
{
	Node *q;

	*e = nil;
	if(strcmp(name, "..") == 0)
		return p->parent;

	if(q = findnode(p, name, 0))
		return q;
	if(*e = localbuild(p))
		return nil;
	if(q = findnode(p, name, 0))
		return q;
	if(*e = p->build(p))
		return nil;
	return findnode(p, name, 0);
}

Node *
nodecreate(Node *p, char *name, int perm)
{
	Node *np;

	np = findnode(p, name, 1);
	reset(np);
	np->mode = perm;
	if(perm & DMDIR)
		np->qid.type = QTDIR;
	return np;
}

Node *
nodecreateroot(char *name, int perm)
{
	Node *p;

	p = newnode1(name);
	p->mode = perm;
	if(perm & DMDIR)
		p->qid.type = QTDIR;
	return p;
}

static int
nodeprint(Fmt *fmt, Node *p)
{
	if(p->type == Qroot)
		return fmtprint(fmt, p->name);
	else{
		if(nodeprint(fmt, p->parent) < 0)
			return -1;
		return fmtprint(fmt, "/%s", p->name);
	}
}

int
nodefmt(Fmt *fmt)
{
	Node *p;

	p = va_arg(fmt->args, Node*);
	if(p == nil)
		return fmtprint(fmt, "<nil>");
	else
		return nodeprint(fmt, p);
}
