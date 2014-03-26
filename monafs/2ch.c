/*
 * build:
 *	p->hot is 1, also p->parent->hot is 1.
 *	p->parent->hot is 0, also p->hot is 0.
 *	1.build all nodes if p->hot is 0(otherwise build don't do).
 *	2.build the parent if p->parent->hot is 0.
 * create:
 *	1.create a node and set optional datas.
 *	2.don't build.
 */
#include "monafs.h"
#include <bio.h>
#include <draw.h>
#include <html.h>
#include <ctype.h>
#include <regexp.h>

static char Menu[] = "http://menu.2ch.net/bbsmenu.html";
static char Broken[] = "[broken]<>[broken]<><> [broken]<>";

static Rune *c_nomk[] = {	/* not make categories */
	L"特別企画",
	L"チャット",
	L"運営案内",
	L"ツール類",
	L"まちＢＢＳ",
	L"他のサイト",
	nil,
};

/* not make boards */
static Rune *b_nomktab[] = {
	L"2chプロジェクト",		/* wiki */
	nil,
};
static Reprog *b_nomkprog;
static char b_nomkexp[] = "^http://[a-zA-Z0-9]+\.(2ch\.net|bbspink\.com)/[a-zA-Z0-9]+/$";

static char Econnect[] = "cannot connect";
static char Esubmit[] = "submit error";
static char Ebusy[] = "server busy";
static char Ecookie[] = "cookie error";
static char Ectlcmd[] = "illegal command";

#define ISCATEGORY(t) (((t)->fnt/NumSize) == FntB)

static char *
buildtext(Node *p)
{
	Node *q;
	char *e;

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildtext %Z\n", p);
	q = p->parent;
	if(e = q->build(q))
		return e;
	if(e = localbuild(q))
		return e;
	p->hot = 1;
	return nil;
}

static Node *
createtext(Node *p, char *name, char *s)
{
	if(verbose)
		fprint(2, "createtext %s %Z\n", name, p);
	p = nodecreate(p, name, 0444);
	p->s = mestrdup(s);
	p->length = strlen(p->s);
	p->type = Qtext;
	p->build = buildtext;
	p->free = nodefree;
	p->hot = 1;
	return p;
}

static char *
buildpp(Node *p)	/* for ctl, post and subject */
{
	Node *q;
	char *e;

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildtext %Z\n", p);
	q = p->parent;
	if(!q->hot){
		/*
		 * for example, a case of thread/ctl was deleted,
		 * set 0 to thread->hot by nodefree.
		 * but thread/ctl depends to cratethread,
		 * and it will be called in buildboard.
		 */
		q->parent->hot = 0;
		if(e = q->parent->build(q->parent))
			return e;
	}
	if(e = localbuild(q))
		return e;
	p->hot = 1;
	return nil;
}

static Node *
createport(Node *p, char *name, char *s, char *(*w)(Node*,char*))
{
	if(verbose)
		fprint(2, "createport %s %Z\n", name, p);
	p = nodecreate(p, name, 0666);
	p->s = mestrdup(s);
	p->length = strlen(p->s);
	p->type = Qtext;
	p->build = buildpp;
	p->write = w;
	p->free = nodefree;
	p->hot = 1;
	return p;
}

char *
localbuild(Node *p)
{
	Node *q;
	Tm tm;
	char date[17];

	if(p->subject){
		q = createtext(p, "subject", p->subject);
		q->build = buildpp;
	}
	if(p->post)
		createport(p, "post", p->attr, p->post);
	if(p->ctl)
		createport(p, "ctl", p->status, p->ctl);

	if(p->from){
		createtext(p, "from", p->from);
		createtext(p, "mail", p->mail);
		createtext(p, "id", p->id);
		createtext(p, "be", p->be);
		tm = *localtime(p->clock);
		sprint(date, "%04d/%02d/%02d %02d:%02d:%02d",
			tm.year+1900, tm.mon+1, tm.mday, tm.hour, tm.min, tm.sec);
		createtext(p, "date", date);
		createtext(p, "message", p->msg);
	}
	return nil;
}

static long
lookup(Rune *a[], Rune *s)
{
	Rune **p;

	for(p = a; *p; p++)
		if(runestrcmp(*p, s) == 0)
			return p - a;
	return -1;
}

static long
cracktime(char *date)
{
	Tm tm;
	char *p;

	/*
	 * oldstyle: YYYY/MM/DD(W) HH:MM
	 * newstyle: YY/MM/DD HH:MM
	 * newstyle2: YY/MM/DD HH:MM:SS
	 */
	if(!isdigit(date[0]))
		return 0;

	tm.year = strtol(date, &p, 10);
	if(tm.year >= 1900)
		tm.year -= 1900;
	else
		tm.year += 100;

	/* 0 <= tm.mon <= 11 */
	tm.mon = strtol(p+1, &p, 10) - 1;

	tm.mday = strtol(p+1, &p, 10);
	if(*p == '(')
		p = strchr(p, ')') + 1;

	tm.hour = strtol(p+1, &p, 10);
	tm.min = strtol(p+1, &p, 10);
	if(*p == ':')
		tm.sec = strtol(p+1, &p, 10);
	else
		tm.sec = 0;

	strcpy(tm.zone, "JST");
	return tm2sec(&tm);
}

static char *
buildarticle(Node *p)
{
	Node *q;
	char *e;

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildarticle %Z\n", p);
	q = p->parent;
	if(e = q->build(q))
		return e;
	if(e = localbuild(p))
		return e;
	p->hot = 1;
	return nil;
}

static char *
trim1(char *s)
{
	int n;

	if(*s == ' '){
		n = strlen(s+1);
		if(n > 0 && (s+1)[n-1] == ' '){
			s++;
			s[n-1] = '\0';
			return s;
		}
	}
	return nil;
}

static int
canpress(char *s)
{
	char *p;

	for(p = s; p = strchr(p, '\n'); p++)
		if(p <= s || strncmp(p-1, " \n ", 3) != 0)
			return 0;
	return 1;
}

static char *
press(char *s)
{
	/*
	 * 1: s = " xxxx \n yyyy \n zzz " (default)
	 * 2: s = " xxxx\nyyyy\nzzzz " (/n/2ch/pc/unix/1000022300/143)
	 * 3: s = more?
	 */
	int n;
	char *p, *t, *u;

	u = t = s;
	p = trim1(s);
	if(p == nil)
		return s;
	s = p;
	if(!canpress(s))
		return s;
	for( ; p = strchr(s, '\n'); s = p+3){
		*--p = '\n';
		n = p-s+1;		/* including \n */
		memmove(t, s, n+1);
		t += n;
	}
	n = strlen(s);
	memmove(t, s, n+1);	/* including \0 */
	return u;
}

static Node *
createarticle(Node *p, char *a[4], int n)
{
	/*
	 * a[0] = "from"
	 * a[1] = "mail"
	 * a[2] = "date[ ID:xxxx][ BE:xxxxx-xxxx]"
	 * a[3] = "message"
	 */
	char *s, buf[5];

	snprint(buf, sizeof(buf), "%d", n);
	if(verbose)
		fprint(2, "createarticle %s %Z\n", buf, p);
	p = nodecreate(p, buf, DMDIR|0555);
	p->type = Qarticle;
	p->build = buildarticle;
	p->free = nodefree;

	p->from = mestrdup(unhtml(a[0]));
	p->mail = mestrdup(decode(a[1]));
	p->id = strcut(a[2], " ID:", &s);
	p->be = strcut(s+1, " BE:", nil);
	p->clock = cracktime(a[2]);
	p->id = mestrdup(p->id ? p->id : "");
	p->be = mestrdup(p->be ? p->be : "");
	s = unhtml(a[3]);
	s = press(s);
	p->msg = mestrdup(s);

	return p;
}

enum {
	Xfatal,
	Xtrue,
	//Xfalse,
	Xerror,
	Xcheck,
	Xcookie,
	Xbusy,
};

static char *xtags[] = {
	[Xtrue]	"<!-- 2ch_X:true -->",
	[Xerror]	"<!-- 2ch_X:error -->",
	[Xcookie]	"<!-- 2ch_X:cookie -->",
};

static Rune *xtitles[] = {
	[Xtrue]	L"書きこみました",
	[Xerror]	L"ＥＲＲＯＲ",
	[Xcheck]	L"書き込み確認",
	[Xcookie]	L"クッキーがないか期限切れです",
	[Xbusy]	L"お茶でも飲みましょう",
};

static int
submit(char *url, char *data, char *ref)
{
	int i, fd, code;
	char *s;
	Rune *r;
	Item *m;
	Docinfo *pdi;

	fd = httppostopen(url, data, ref);
	if(fd < 0)
		return -1;
	s = readfile(fd);
	close(fd);

	if(debug){
		fprint(2, "======= DEBUG =======\n");
		fprint(2, "POST:%s\n", url);
		fprint(2, "Data:%s\n", data);
		fprint(2, "Referer:%s\n", ref);
		fprint(2, "%s\n", s);
		fprint(2, "=======  END  =======\n");
	}

	code = Xfatal;
	for(i = 0; i < nelem(xtags); i++)
		if(xtags[i])
		if(strstr(s, xtags[i])){
			code = i;
			goto end;
		}

	r = toStr((uchar*)url, strlen(url), ISO_8859_1);
	m = parsehtml((uchar*)s, strlen(s), r, TextHtml, UTF_8, &pdi);
	freeitems(m);
	free(r);

	for(i = 0; i < nelem(xtitles); i++)
		if(xtitles[i])
		if(runestrstr(pdi->doctitle, xtitles[i])){
			code = i;
			break;
		}
	freedocinfo(pdi);

end:
	free(s);
	return code;
}

typedef struct AForm AForm;
struct AForm {
	char *from;
	char *mail;
	char *s;
};

static char *a_submit[] = {	/* for new article */
	"書き込む",
	"上記全てを承諾して書き込む",
};

static char Articleattr[] = "from:\nmail:\n";

static char *
submitarticle(Node *p, AForm *fm)
{
	Node *q;
	char *e;
	char *url, *data, *ref;
	int trial;

	trial = 0;
	e = nil;
	q = p->parent;
	url = esmprint("http://%s/test/bbs.cgi", q->host);
	ref = esmprint("http://%s/%s/%s",
		q->host, q->name, p->name);
	data = nil;

retry:
	free(data);
	data = esmprint("bbs=%s&key=%s&time=1&submit=%U"
		"&FROM=%U&mail=%U&MESSAGE=%U",
		q->name, p->name, a_submit[trial],
		fm->from, fm->mail, fm->s);

	switch(submit(url, data, ref)){
	case -1:
		e = Econnect;
		break;
	case Xtrue:
		e = nil;
		/* require to subject.txt */
		noderefresh(q);
		break;
	case Xerror:
		e = Esubmit;
		break;
	case Xbusy:
		e = Ebusy;
		break;
	case Xcheck:
	case Xcookie:
		if(++trial < nelem(a_submit))
			goto retry;
		e = Ecookie;
		break;
	case Xfatal:
	default:
		assert(0);
	}
	free(url);
	free(data);
	free(ref);
	return e;
}

static char *
posttothread(Node *p, char *s)
{
	AForm fm;
	char *t;

	fm.from = fm.mail = "";
	for( ; *s; s = t+1){
		/* see monafs.c:fsdestroyfid */
		t = strchr(s, '\n');
		assert(t != nil);
		*t = '\0';

		if(strncmp(s, "from:", 5) == 0)
			fm.from = s + 5;
		else if(strncmp(s, "mail:", 5) == 0)
			fm.mail = s + 5;
		else{
			*t = '\n';
			break;
		}
	}
	fm.s = s;
	return submitarticle(p->parent, &fm);
}

static char *
ctlthread(Node *p, char *s)
{
	if(strncmp(s, "refresh", 7) == 0){
		/* require a subject.txt */
		noderefresh(p->parent->parent);
		return nil;
	}

	return Ectlcmd;
}

static char *
buildthread(Node *p)
{
	/* line:from<>mail<>date,ID<>message<>subject(1only) */
	Node *q;
	Biobuf fin;
	int i, fd;
	char *s, *e, *line, *buf, *a[4];

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildthread %Z\n", p);
	q = p->parent;
	if(e = q->build(q))
		return e;
	s = esmprint("http://%s/%s/dat/%s.dat",
		q->host, q->name, p->name);
	fd = httpopen(s);
	free(s);
	if(fd < 0)
		return Econnect;

	Binit(&fin, fd, OREAD);
	for(i = 1; line = Brdline(&fin, '\n'); i++){
		line[Blinelen(&fin)-1] = '\0';
		buf = sjistostring(line);
		if(split(buf, a, nelem(a), "<>") < 4){
			free(buf);
			buf = estrdup(Broken);
			split(buf, a, nelem(a), "<>");
		}
		createarticle(p, a, i);
		free(buf);
	}
	httpclose(fd);
	localbuild(p);
	p->hot = 1;
	return nil;
}

static Node *
createthread(Node *p, char *a[2], int i)
{
	/*
	 * a[0] = "1000000000.dat";
	 * a[1] = "subject (n)"
	 *
	 * rank XXXX\n (10)
	 * article XXXX\n (13)
	 */
	char *s, status[10+13+1];

	*strchr(a[0], '.') = '\0';
	if(verbose)
		fprint(2, "createthread %s %Z\n", a[0], p);
	p = nodecreate(p, a[0], DMDIR|0555);
	p->type = Qthread;
	p->build = buildthread;
	p->free = nodefree;

	s = strrchr(a[1], ' ');
	*s = '\0';
	s += 2;
	*strchr(s, ')') = '\0';
	sprint(status, "rank %d\narticle %d\n", i, atoi(s));
	p->status = mestrdup(status);
	p->ctl = ctlthread;
	p->subject = mestrdup(unhtml(a[1]));
	p->attr = Articleattr;
	p->post = posttothread;

	return p;
}

typedef struct TForm TForm;
struct TForm {
	char *subject;
	AForm;
};

static char *t_cgi[] = {	/* for new thread */
	"bbs.cgi",
	"subbbs.cgi",
};

static char *t_submit[] = {	/* for new thread */
	"新規スレッド作成",
	"全責任を負うことを承諾して書き込む",
};

static char Threadattr[] = "subject:\nfrom:\nmail:\n";

static char *
submitthread(Node *p, TForm *fm)
{
	char *e, *url, *data, *ref;
	int ncookie, ncheck;

	e = nil;
	ncookie = ncheck = 0;
	ref = esmprint("http://%s/%s/", p->host, p->name);
	url = data = nil;

retry:
	free(url);
	free(data);
	url = esmprint("http://%s/test/%s", p->host, t_cgi[ncheck]);
	data = esmprint("bbs=%s&time=1&submit=%U"
		"&subject=%U&FROM=%U&mail=%U&MESSAGE=%U",
		p->parent->name, t_submit[ncookie],
		fm->subject, fm->from, fm->mail, fm->s);

	switch(submit(url, data, ref)){
	case -1:
		e = Econnect;
		break;
	case Xtrue:
		e = nil;
		noderefresh(p);
		break;
	case Xerror:
		e = Esubmit;
		break;
	case Xbusy:
		e = Ebusy;
		break;
	case Xcheck:
		if(++ncheck < nelem(t_submit))
			goto retry;
		e = Esubmit;
		break;
	case Xcookie:
		if(++ncookie < nelem(t_submit))
			goto retry;
		e = Ecookie;
		break;
	case Xfatal:
	default:
		assert(0);
	}
	free(url);
	free(data);
	free(ref);
	return e;
}

static char *
posttoboard(Node *p, char *s)
{
	TForm fm;
	char *t;

	fm.subject = fm.from = fm.mail = "";
	for( ; *s; s = t+1){
		/* see monafs.c:fsdestroyfid */
		t = strchr(s, '\n');
		assert(t != nil);
		*t = '\0';

		if(strncmp(s, "subject:", 8) == 0)
			fm.subject = s + 8;
		else if(strncmp(s, "from:", 5) == 0)
			fm.from = s + 5;
		else if(strncmp(s, "mail:", 5) == 0)
			fm.mail = s + 5;
		else{
			*t = '\n';
			break;
		}
	}

	fm.s = s;
	return submitthread(p->parent, &fm);
}

static char *
ctlboard(Node *p, char *s)
{
	if(strncmp(s, "refresh", 7) == 0){
		noderefresh(p->parent);
		return nil;
	}

	return Ectlcmd;
}

static char *
buildboard(Node *p)
{
	/* line:"00000000.dat<>subject (n)" */
	Node *q;
	Biobuf fin;
	int i, fd;
	char *s, *e, *line, *buf, *a[2];

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildboard %Z\n", p);
	q = p->parent;
	if(e = q->build(q))
		return e;
	s = esmprint("http://%s/%s/subject.txt", p->host, p->name);
	fd = httpopen(s);
	free(s);
	if(fd < 0)
		return Econnect;

	Binit(&fin, fd, OREAD);
	for(i = 1; line = Brdline(&fin, '\n'); i++){
		line[Blinelen(&fin)-1] = '\0';
		buf = sjistostring(line);
		/* jump to http://news6.2ch.net/live.html rarely. why? */
		if(split(buf, a, 2, "<>") != 2){
			httpclose(fd);
			free(buf);
			return Ebusy;
		}
		createthread(p, a, i);
		free(buf);
	}
	httpclose(fd);
	localbuild(p);
	p->hot = 1;
	return nil;
}

static Rune *
lookuphref(Anchor *a, Item *t)
{
	if(t->anchorid == 0)
		return nil;

	for( ; a; a = a->next)
		if(a->index == t->anchorid)
			return a->href;
	return nil;
}

static void
gethostandboard(char *s, char **h, char **b)
{
	/* s = "http://srv.domain.dom/board/" */
	char *p;

	s[strlen(s)-1] = '\0';
	*h = s + 7;		/* skip "http://" */
	p = strchr(*h, '/');
	*p = '\0';
	*b = p+1;
}

static Node *
createboard(Node *p, Itext *t, Anchor *a)
{
	Rune *r;
	char *s, *h, *b;

	if(lookup(b_nomktab, t->s) >= 0)
		return nil;

	r = lookuphref(a, t);
	if(r == nil || !rregexec(b_nomkprog, r, nil, 0))
		return nil;

	s = (char*)fromStr(r, runestrlen(r), UTF_8);
	gethostandboard(s, &h, &b);
	if(verbose)
		fprint(2, "createboard %s %Z\n", b, p);
	p = nodecreate(p, b, DMDIR|0555);
	p->type = Qboard;
	p->build = buildboard;
	p->free = nodefree;
	p->host = mestrdup(h);
	free(s);

	p->subject = meStrdup(t->s);
	p->post = posttoboard;
	p->attr = Threadattr;
	p->ctl = ctlboard;
	s = esmprint("host %s\n", p->host);
	p->status = mestrdup(s);
	free(s);

	return p;
}

static char *
buildcategory(Node *p)
{
	Node *q;
	char *e;

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildcategory %Z\n", p);
	q = p->parent;
	/*
	 * nodefree will set 0 to p->hot.
	 * but really depend to buildroot.
	 * buildroot was set to p->parent->build.
	 * see init and buildroot.
	 */
	if(!p->hot)
		q->hot = 0;
	if(e = q->build(q))
		return e;
	if(e = localbuild(p))
		return e;
	p->hot = 1;
	return nil;
}

static int
hash(int lim, Rune *s)
{
	char buf[UTFmax];
	uint h;
	int i, n;

	h = 0;
	for( ; *s; s++){
		n = runetochar(buf, s);
		for(i = 0; i < n; i++)
			h = h*37 + (uchar)(buf[i]);
	}
	return h % lim;
}

typedef struct Nametab Nametab;
struct Nametab {
	char *name;
	Rune *subj;
	Nametab *next;
};

static Nametab *nametab[100];
static int ncategory;

static char *
getnamebysubj(Rune *s)
{
	uint h;
	char buf[100];
	Nametab *p;

	h = hash(nelem(nametab), s);
	for(p = nametab[h]; p; p = p->next)
		if(runestrcmp(p->subj, s) == 0)
			return p->name;

	sprint(buf, "%d", ++ncategory);
	p = emalloc(sizeof(*p));
	p->name = mestrdup(buf);
	p->subj = runestrdup(s);
	if(p->subj == nil)
		sysfatal("runestrdup: %r");
	p->next = nametab[h];
	nametab[h] = p;
	return p->name;
}

static Node *
createcategory(Node *p, Itext *t)
{
	char *s;

	if(lookup(c_nomk, t->s) >= 0)
		return nil;

	s = getnamebysubj(t->s);
	if(verbose)
		fprint(2, "createcategory %s %Z\n", s, p);
	p = nodecreate(p, s, DMDIR|0555);
	p->type = Qcategory;
	p->build = buildcategory;
	p->free = nodefree;

	p->subject = meStrdup(t->s);

	return p;
}

static char *
buildroot(Node *p)
{
	Item *items, *il;
	Itext *text;
	Rune *url;
	Docinfo *pdi;
	Node *c;	/* category; a case of nil is passing boards */
	int fd;
	char *s;

	if(p->hot)
		return nil;

	if(verbose)
		fprint(2, "buildroot %Z\n", p);
	fd = httpopen(Menu);
	if(fd < 0)
		return Econnect;
	s = readfile(fd);
	url = toStr((uchar*)Menu, strlen(Menu), ISO_8859_1);
	items = parsehtml((uchar*)s, strlen(s), url, TextHtml, UTF_8, &pdi);

	c = nil;
	for(il = items; il; il = il->next){
		if(il->tag != Itexttag)
			continue;

		text = (Itext*)il;
		if(ISCATEGORY(text))
			c = createcategory(p, text);
		else if(c)
			createboard(c, text, pdi->anchors);
	}

	freeitems(items);
	freedocinfo(pdi);
	free(url);
	free(s);
	httpclose(fd);

	p->hot = 1;
	return nil;
}

void
init(void)
{
	b_nomkprog = regcomp(b_nomkexp);
	if(b_nomkprog == nil)
		fatal("regcomp: %r");

	mountroot = nodecreateroot("", DMDIR|0555);
	mountroot->type = Qroot;
	mountroot->build = buildroot;
	mountroot->free = nodefree;
}
