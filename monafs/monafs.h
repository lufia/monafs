#include <u.h>
#include <libc.h>
#include <thread.h>

typedef struct Aux Aux;
typedef struct Node Node;

#pragma varargck type "Z" Node*
#pragma varargck type "U" char*

enum {
	Qroot,
	Qcategory,
	Qboard,
	Qthread,
	Qarticle,
	Qtext,
};

struct Aux {
	Node *node;
	char *s;
	long ns;
};
struct Node {
	Ref;
	Dir;		/* name,qid,atime,mtime */
	int type;
	Node *parent;
	Node **sibs;
	int nsib;
	int n;		/* mem size for sibs */
	int hot;
	char *host;
	char *s;		/* for Qtext */

	char *(*build)(Node*);
	char *(*write)(Node*, char*);
	void (*free)(Node*);

	/* optional data; see 2ch.c:localbuild */
	struct {
		char *attr;
		char *(*post)(Node*, char*);
	};
	struct {
		char *status;
		char *(*ctl)(Node*, char*);
	};
	struct {
		char *subject;
	};
	struct {
		char *from;
		char *mail;
		char *id;
		char *be;
		long clock;
		char *msg;
	};
};

extern char *localbuild(Node *p);
extern void categoryinit(char *file);
extern void init(void);

extern void httpinit(void);
extern void httpend(void);
extern void httpfatal(char *fmt, ...);
extern int httpopen(char *uri);
extern int httppostopen(char *uri, char *data, char *ref);
extern void httpclose(int body);

extern void *emalloc(ulong n);
extern void *erealloc(void *p, ulong n);
extern char *estrdup(char *s);
extern char *esmprint(char *fmt, ...);
extern char *strcut(char *s, char *sep, char **p);
extern char *mestrdup(char *s);
extern void mestrfree(char *s);
extern char *meStrdup(Rune *s);
extern char *sjistostring(char *s);
extern char *readfile(int fd);
extern char *unhtml(char *s);
extern char *decode(char *s);
extern int Ufmt(Fmt *fmt);
extern int split(char *s, char *a[], int n, char *sep);

extern void nodefree(Node *p);
extern void noderefresh(Node *p);
extern Node *nodeopen(Node *p, char *name, char **e);
extern Node *nodecreate(Node *p, char *name, int perm);
extern Node *nodecreateroot(char *name, int perm);
extern int nodefmt(Fmt *fmt);

extern void cacheinit(void);
extern Node *cache(Node *p);

extern int debug;
extern int verbose;
extern char useragent[];
extern Node *mountroot;
extern void (*fatal)(char *fmt, ...);
