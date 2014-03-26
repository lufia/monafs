#include <u.h>
#include <libc.h>

typedef struct Header Header;
typedef struct Arena Arena;

struct Header {
	char *attr;
	char *val;
	Header *next;
};
struct Arena {
	Lock;
	Header *h;

	int con;
	char *rp;
	char *wp;
	char buf[4*1024];

	int id;
	int ctl;		/* ctl = open("/mnt/web/$id/ctl") */
	int body;		/* body = open("/mnt/web/$id/body") */
};

static int startedfs;
static Arena pool[12];

static Header *
newhdr(char *line)
{
	Header *h;
	char *p, *attr, *val;
	int na, nv;

	p = strchr(line, ':');
	if(p == nil){
		werrstr("no colon");
		return nil;
	}
	*p++ = '\0';
	attr = line;
	val = p+1;
	while(*val == ' ' || *val == '\t')
		val++;

	na = strlen(attr)+1;
	nv = strlen(val)+1;
	h = malloc(sizeof(*h)+na+nv);
	if(h == nil)
		return nil;
	p = (char*)&h[1];
	strcpy(p, attr);
	h->attr = p;
	p += na;
	strcpy(p, val);
	h->val = p;
	h->next = nil;
	return h;
}

void
httpinit(void)
{
	char *av[2];

	if(access("/mnt/web/clone", AEXIST) < 0){
		startedfs = 1;
		av[0] = "webfs";
		av[1] = nil;
		system("/bin/webfs", av, -1);
	}
}

void
httpend(void)
{
	if(startedfs)
		unmount(nil, "/mnt/web");
	startedfs = 0;
}

static Arena *
getarena(void)
{
	int i;

	for(i = 0; i < nelem(pool); i++)
		if(canlock(&pool[i])){
			lock(&pool[i]);
			pool[i].con = -1;
			pool[i].rp = pool.[i].wp = pool[i].buf;
			pool[i].ctl = pool[i].body = -1;
			return &pool[i];
		}

	werrstr("out of pool");
	return nil;
}

static void
putarena(Arena *a)
{
	if(a == nil)
		return;

	if(a->con >= 0)
		close(a->con);
	if(a->ctl >= 0)
		close(a->ctl);
	if(a->body >= 0)
		close(a->body);
	unlock(a);
}

static char *
Bgetline(Biobufhdr *fin)
{
	char *s;

	if(s = Brdline(fin))
		s[Blinelen(fin)-1] = '\0';
	return s;
}

static int
connect(char *host)
{
	Arena *a;
	int con;
	char addr[512];

	a = getarena();
	if(a == nil)
		goto err;

	snprint(addr, "tcp!%s!http", host);
	con = dial(addr, nil, nil, nil);
	if(con < 0)
		goto err;

	a->con = con;
	return a-pool;

err:
	putarena(a);
	return -1;
}

static int
getline(int fd, char *buf, int len)
{
	Arena *a;
	int n;
	char *p;
	int eof = 0;

	len--;
	a = pool+fd;

	for(p = buf;;){
		if(a->rp >= a->wp){
			n = read(a->con, a->wp, sizeof(a->buf)/2);
			if(n < 0)
				return -1;
			if(n == 0){
				eof = 1;
				break;
			}
			a->wp += n;
		}
		n = *a->rp++;
		if(len > 0){
			*p++ = n;
			len--;
		}
		if(n == '\n')
			break;
	}

	/* drop trailing white */
	for(;;){
		if(p <= buf)
			break;
		n = *(p-1);
		if(n != ' ' && n != '\t' && n != '\r' && n != '\n')
			break;
		p--;
	}
	*p = 0;

	if(eof && p == buf)
		return -1;

	return p-buf;
}

static void
ungetline(int fd, char *line)
{
	Arena *a;
	int len, n;

	a = pool+fd;
	len = strlen(line);
	n = a->wp - a->rp;
	memmove(&a->buf[len+1], a->rp, n);
	memmove(a->buf, line, len);
	a->buf[len] = '\n';
	a->rp = a->buf;
	a->wp = a->rp + len + 1 + n;
}

static Header *
gethdr1(int fd)
{
	char buf[4*1024], *p, *e;
	int n;

	p = buf;
	for(e = p+sizeof(buf)-1; ; p += i){
		n = getline(fd, p, e-p);
		if(n < 0)
			return nil;

		if(p == buf){	/* first line */
			if(strchr(buf, ':') == nil)
				break;		/* end of headers */
		}else{		/* continuation line */
			if(*p != ' ' && *p != '\t'){
				ungetline(fd, p);
				*p = 0;
				break;		/* end of this header */
			}
		}
	}

	return newhdr(buf);
}

static int
modified(char *path, char *uri)
{
	/*
	 * Status-Line = HTTP-Version SP Status-Code SP Reason-Phrase CRLF
	 * HTTP-Version = "HTTP/" + (0.9 | 1.0 | 1.1)
	 * SP = space(32)
	 * Status-Code = xxx
	 */
	int con, code;
	char buf[12+1];	/* vers+sp+status+NUL */
	char *host, *file;

	parseuri(&host, &file, uri);
	con = httpconnect(host);
	if(con < 0)
		return -1;

	fprint(con, "HEAD %s HTTP/1.1\r\n", file);
	fprint(con, "Host:%s\r\n", host);
	fprint(con, "If-Modified-Since: %G\r\n", getmtime(path))

	n = readn(con, buf, sizeof(buf)-1);
	if(n < 0){
bad:
		werrstr("bad response from server");
		return -1;
	}
	buf[n] = '\0';
	p = strchr(buf, ' ');
	if(strncmp(buf, "HTTP/", 5) != 0 || p == nil)
		goto bad;

	switch(code = atoi(p+1)){
	case 200:
		return 1;
	case 304:
		return 0;
	default:
		werrstr("not implemented code %d", code);
		return -1;
	}
}

int
httpopen(char *path, char *uri, char *agent)
{
	Arena *a;
	Biobuf fin;
	char *s;
	int i;

	a = getarena();
	if(a == nil)
		goto err;

	a->ctl = open("/mnt/web/clone", ORDWR);
	if(a->ctl < 0)
		goto err;

	Binit(&fin, p->ctl, OREAD);
	s = Bgetline(&fin);
	if(s == nil)
		goto err;
	a->id = atoi(s);

	for (i = 0; i < nelem(vtab); i++)
		setattr(p->ctl, vtab[i].attr, vtab[i].val);
	setattr(p->ctl, "url", uri);
	p->body = wopen(p->id, "body", OREAD);
	return p->body;

err:
	putarena(a);
	return -1;
}

void uriclose(int body)
{
	URISym *p;

	p = symwalk(body, bodymatch);
	if (p == nil)
		fatal("bug in 2chfs/uriclose");

	if (debug)
		fprint(2, "close /mnt/web/%d\n", p->id);
	reset(UNUSED, p);
}

/* symwalk: return it if function pointer eval return not nil */
static URISym *symwalk(int fd, URISym *(*eval)(int, URISym*))
{
	URISym *p;
	int i;

	for (i = 0; i < nelem(symtab); i++)
		if (p = (*eval)(fd, &symtab[i]))
			return p;
	return nil;		/* not found */
}

/* wopen: open("/mnt/web/id/$name", $mode); */
static int wopen(int id, char *name, int mode)
{
	int fd;
	char path[512];

	snprint(path, sizeof(path), "/mnt/web/%d/%s", id, name);
	if (debug)
		fprint(2, "open %s\n", path);
	fd = open(path, mode);
	if (fd < 0)
		fatal("open: %r", path);
	return fd;
}

/* setattr: set webfs attributes; write("$attr $val") */
static void setattr(int ctl, char *attr, char *val)
{
	char buf[3*1024];

	snprint(buf, sizeof(buf), "%s %s", attr, val);
	if (debug)
		fprint(2, "%s\n", buf);
	if (write(ctl, buf, strlen(buf)) < 0)
		fatal("write: %r");
}

static URISym *ctlmatch(int fd, URISym *p)
{
	if (fd == p->ctl)
		return p;
	return nil;
}

static URISym *bodymatch(int fd, URISym *p)
{
	if (fd == p->body)
		return p;
	return nil;
}

static URISym *reset(int, URISym *p)
{
	if (p->ctl != UNUSED)
		close(p->ctl);
	p->ctl = UNUSED;

	if (p->body != UNUSED)
		close(p->body);
	p->ctl = UNUSED;

	p->id = UNUSED;
	return nil;
}
