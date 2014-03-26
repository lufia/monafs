#include "monafs.h"
#include <bio.h>

typedef struct Arena Arena;

struct Arena {
	Lock;
	int id;
	int ctl;
	int body;
};

static int startedfs;
static Arena pool[12];

static void
system(char *cmd, char *av[], int in)
{
	int pid;

	switch (pid = fork()) {
	case -1:
		return;
	case 0:
		if (in >= 0) {
			dup(in, 0);
			close(in);
		}
		exec(cmd, av);
		perror("system");
		exits("system");
	default:
		while (waitpid() < 0) {
			//if (!interrupted)
			//	break;
			postnote(PNPROC, pid, "die");
			continue;
		}
	}
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

void
httpfatal(char *fmt, ...)
{
	va_list arg;
	char buf[ERRMAX];

	va_start(arg, fmt);
	vsnprint(buf, sizeof(buf), fmt, arg);
	va_end(arg);

	if(argv0)
		fprint(2, "%s: ", argv0);
	fprint(2, "%s\n", buf);
	httpend();
	exits(buf);
}

static Arena *
getarena(void)
{
	int i;

	for(i = 0; i < nelem(pool); i++)
		if(canlock(&pool[i])){
			pool[i].id = -1;
			pool[i].ctl = -1;
			pool[i].body = -1;
			return &pool[i];
		}

	werrstr("out of pool");
	return nil;
}

static Arena *
findarena(int body)
{
	int i;

	for(i = 0; i < nelem(pool); i++)
		if(!canlock(&pool[i]))
		if(pool[i].body == body)
			return &pool[i];

	werrstr("cannot find");
	return nil;
}

static void
putarena(Arena *a)
{
	if(a == nil)
		return;

	if(a->ctl >= 0)
		close(a->ctl);
	a->ctl = -1;
	if(a->body >= 0)
		close(a->body);
	a->body = -1;
	a->id = -1;
	unlock(a);
}

static char *
Bgetline(Biobufhdr *fin)
{
	char *s;

	if(s = Brdline(fin, '\n'))
		s[Blinelen(fin)-1] = '\0';
	return s;
}

static void
setattr(int ctl, char *attr, char *val)
{
	char buf[3*1024];

	snprint(buf, sizeof(buf), "%s %s", attr, val);
	if(write(ctl, buf, strlen(buf)) < 0)
		httpfatal("write: %r");
}

int
httppostopen(char *uri, char *data, char *ref)
{
	Arena *a;
	Biobuf fin;
	int fd;
	char *s, buf[512];

	a = getarena();
	if(a == nil)
		goto err;

	a->ctl = open("/mnt/web/clone", ORDWR);
	if(a->ctl < 0)
		goto err;

	Binit(&fin, a->ctl, OREAD);
	s = Bgetline(&fin);
	if(s == nil)
		goto err;
	a->id = atoi(s);

	if(ref)
		setattr(a->ctl, "referer", ref);
	setattr(a->ctl, "useragent", useragent);
	setattr(a->ctl, "url", uri);
	if(data){
		snprint(buf, sizeof(buf), "/mnt/web/%d/postbody", a->id);
		fd = open(buf, OWRITE);
		if(fd < 0)
			goto err;
		if(write(fd, data, strlen(data)) < 0)
			goto err;
		close(fd);
	}
	snprint(buf, sizeof(buf), "/mnt/web/%d/body", a->id);
	a->body = open(buf, OREAD);
	if(a->body < 0)
		goto err;
	return a->body;

err:
	putarena(a);
	return -1;
}

int
httpopen(char *uri)
{
	return httppostopen(uri, nil, nil);
}

void
httpclose(int body)
{
	Arena *a;

	a = findarena(body);
	if(a == nil)
		httpfatal("cannot find http-fd");
	putarena(a);
}
