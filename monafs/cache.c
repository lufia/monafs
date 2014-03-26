#include "monafs.h"

typedef struct Page Page;

enum {
	Pagemax = 1024*8,
	//Pagemax = 700,
	Lifespan = 4,
	Lifecycle = Pagemax/Lifespan,
};

struct Page {
	Node *node;
	Page *prev;	/* for doubly-linked list */
	Page *next;	/* for doubly-linked list */
	Page *chain;	/* for chaining */
};

Page tab[Pagemax];

static uint
hash(Node *p)
{
	ulong n;

	n = (ulong)p / sizeof(p);
	return n % nelem(tab);
}

static Page *
lookup(Node *node)
{
	Page *p;
	uint h;

	h = hash(node);
	for(p = tab[h].chain; p; p = p->chain)
		if(p->node == node)
			return p;
	return nil;
}

static void
install(Page *p)
{
	Page *q;
	uint h;

	h = hash(p->node);
	for(q = tab[h].chain; q; q = q->chain)
		if(q->node == p->node)
			fatal("doubly installation");
	p->chain = tab[h].chain;
	tab[h].chain = p;
}

static void
delete(Page *page)
{
	Page *p, *q;
	uint h;

	h = hash(page->node);
	q = &tab[h];
	for(p = tab[h].chain; p; p = p->chain){
		if(p->node == page->node){
			q->chain = p->chain;
			p->chain = nil;
			return;
		}
		q = p;
	}

	fatal("cannot delete cache page: no hit");
}

static void
insert(Page *p, Page *newp)		/* insert on head->next */
{
	newp->prev = p;
	newp->next = p->next;
	p->next->prev = newp;
	p->next = newp;
}

static Page *
clunk(Page *p)
{
	p->prev->next = p->next;
	p->next->prev = p->prev;
	p->next = p->prev = nil;
	return p;
}

/*
 * head[(now+1) % nelem(head)] is the oldest box
 * head[now] is the latest box
 */
static Page head[Lifespan];
static Page pool;
static ulong npage;
static int now;

void
cacheinit(void)
{
	int i;

	for(i = 0; i < nelem(head); i++)
		head[i].prev = head[i].next = &head[i];
	pool.prev = pool.next = &pool;
}

static Page *
rotate(void)
{
	now = (now+1) % nelem(head);
	if(debug)
		fprint(2, "now block %d\n", now);
	return &head[now];
}

static void
pageout(void)
{
	Page *hd, *p, *q;

	hd = rotate();
	for(p = hd->prev; p != hd; p = q){
		q = p->prev;
		if(debug)
			fprint(2, "pageout ref %ld\n", p->node->ref);
		if(p->node->ref == 1){	/* pointed from parent only */
			clunk(p);
			insert(&pool, p);
			npage--;
		}
	}
}

static Page *
newpage(void)
{
	Page *p;

	if(debug)
		fprint(2, "newpage %uld of %d\n", npage, Pagemax);
	if(npage%Lifecycle == 0)
		pageout();

	if(pool.prev != &pool){
		p = clunk(pool.prev);
		delete(p);
		p->node->free(p->node);
		p->node = nil;
	}else
		p = emalloc(sizeof(*p));
	npage++;
	return p;
}

Node *
cache(Node *node)
{
	Page *p;

	if(p = lookup(node)){
		clunk(p);
		insert(&head[now], p);
	}else{
		p = newpage();
		p->node = node;
		insert(&head[now], p);
		install(p);
	}
	return p->node;
}
