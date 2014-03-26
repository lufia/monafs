#include "monafs.h"
#include <draw.h>
#include <html.h>
#include <kanji.h>
#include <ctype.h>

void *
emalloc(ulong n)
{
	void *p;

	p = malloc(n);
	if(p == nil)
		fatal("malloc: %r");
	memset(p, 0, n);
	setmalloctag(p, getcallerpc(&n));
	return p;
}

void *
erealloc(void *p, ulong n)
{
	p = realloc(p, n);
	if(p == nil)
		fatal("realloc: %r");
	setmalloctag(p, getcallerpc(&p));
	return p;
}

char *
estrdup(char *s)
{
	char *t;

	t = emalloc(strlen(s)+1);
	strcpy(t, s);
	setmalloctag(t, getcallerpc(&s));
	return t;
}

char *
esmprint(char *fmt, ...)
{
	va_list arg;
	char *s;

	va_start(arg, fmt);
	s = vsmprint(fmt, arg);
	va_end(arg);
	if(s == nil)
		fatal("vsmprint: %r");
	return s;
}

char *
strcut(char *s, char *sep, char **p)
{
	int n;
	char *t;

	n = strlen(sep);
	if(t = strstr(s, sep)){
		*t = '\0';
		if(p)
			*p = t;
		return t+n;
	}else{
		if(p)
			*p = s;
		return nil;
	}
}

typedef struct Str Str;
struct Str {
	Ref;
	char *s;
	Str *next;
};
static Str strtab[8*1024];

static uint
hash(char *s)
{
	uint h;
	uchar *p;

	h = 0;
	for(p = (uchar*)s; *p; p++)
		h = h*37 + *p;
	return h % nelem(strtab);
}

char *
mestrdup(char *s)
{
	Str *m;
	uint h;

	h = hash(s);
	for(m = strtab[h].next; m; m = m->next)
		if(strcmp(s, m->s) == 0) {
			incref(m);
			return m->s;
		}

	m = emalloc(sizeof(*m));
	m->s = estrdup(s);
	incref(m);
	m->next = strtab[h].next;
	strtab[h].next = m;
	return m->s;
}

void
mestrfree(char *s)
{
	Str *m, *prev;
	uint h;

	if(s == nil)
		return;

	h = hash(s);
	prev = &strtab[h];
	for(m = strtab[h].next; m; m = m->next){
		if(m->s != s){		/* cmp pointers; not strcmp */
			prev = m;
			continue;
		}

		/* match found */
		if(decref(m) == 0){
			prev->next = m->next;
			free(m->s);
			free(m);
		}
		return;
	}

	assert(0);
}

char *
meStrdup(Rune *s)
{
	char *t, *p;

	t = (char*)fromStr(s, runestrlen(s), UTF_8);
	if(t == nil)
		fatal("fromStr: %r");
	p = mestrdup(t);
	free(t);
	return p;
}

typedef struct Bytes Bytes;
struct Bytes {
	uchar *b;
	long n;
	long nalloc;
};

static void
growbytes(Bytes *b, char *s, long ns)
{
	if(b->nalloc < b->n + ns + 1){
		b->nalloc = b->n + ns + 1024;
		/* use realloc to avoid memset */
		b->b = realloc(b->b, b->nalloc);
		if(b->b == nil)
			fatal("growbytes: %r");
	}
	memmove(b->b+b->n, s, ns);
	b->n += ns;
	b->b[b->n] = '\0';
}

static char *
readbytes(int fd)
{
	Bytes *b;
	int n;
	char *p;
	char buf[1024];

	b = emalloc(sizeof(*b));
	while((n=read(fd, buf, sizeof(buf))) > 0)
		growbytes(b, buf, n);
	if(b->n == 0)
		growbytes(b, "", 0);

	p = erealloc(b->b, b->n+1);	/* shrink memory */
	//free(b->b);
	free(b);

	return p;
}

char *
sjistostring(char *s)
{
	Rune r;
	char *t, *p;

	t = p = emalloc(UTFmax*strlen(s)+1);
	while(*s){
		s += sjistorune(&r, s);
		p += runetochar(p, &r);
	}
	*p = '\0';
	return t;
}

char *
readfile(int fd)
{
	char *s, *t;

	s = readbytes(fd);
	t = sjistostring(s);
	free(s);
	return t;
}

/*
 * Character entity to unicode character number map.
 * Keep sorted by name. (copy and paste from libhtml)
 */
static struct {
	char *name;
	Rune val;
} chartab[] = {
	{ "AElig", 198 },
	{ "Aacute", 193 },
	{ "Acirc", 194 },
	{ "Agrave", 192 },
	{ "Aring", 197 },
	{ "Atilde", 195 },
	{ "Auml", 196 },
	{ "Ccedil", 199 },
	{ "ETH", 208 },
	{ "Eacute", 201 },
	{ "Ecirc", 202 },
	{ "Egrave", 200 },
	{ "Euml", 203 },
	{ "Iacute", 205 },
	{ "Icirc", 206 },
	{ "Igrave", 204 },
	{ "Iuml", 207 },
	{ "Ntilde", 209 },
	{ "Oacute", 211 },
	{ "Ocirc", 212 },
	{ "Ograve", 210 },
	{ "Oslash", 216 },
	{ "Otilde", 213 },
	{ "Ouml", 214 },
	{ "THORN", 222 },
	{ "Uacute", 218 },
	{ "Ucirc", 219 },
	{ "Ugrave", 217 },
	{ "Uuml", 220 },
	{ "Yacute", 221 },
	{ "aacute", 225 },
	{ "acirc", 226 },
	{ "acute", 180 },
	{ "aelig", 230 },
	{ "agrave", 224 },
	{ "alpha", 945 },
	{ "amp", 38 },
	{ "aring", 229 },
	{ "atilde", 227 },
	{ "auml", 228 },
	{ "beta", 946 },
	{ "brvbar", 166 },
	{ "ccedil", 231 },
	{ "cdots", 8943 },
	{ "cedil", 184 },
	{ "cent", 162 },
	{ "chi", 967 },
	{ "copy", 169 },
	{ "curren", 164 },
	{ "ddots", 8945 },
	{ "deg", 176 },
	{ "delta", 948 },
	{ "divide", 247 },
	{ "eacute", 233 },
	{ "ecirc", 234 },
	{ "egrave", 232 },
	{ "emdash", 8212 },
	{ "emsp", 8195 },
	{ "endash", 8211 },
	{ "ensp", 8194 },
	{ "epsilon", 949 },
	{ "eta", 951 },
	{ "eth", 240 },
	{ "euml", 235 },
	{ "frac12", 189 },
	{ "frac14", 188 },
	{ "frac34", 190 },
	{ "gamma", 947 },
	{ "gt", 62 },
	{ "iacute", 237 },
	{ "icirc", 238 },
	{ "iexcl", 161 },
	{ "igrave", 236 },
	{ "iota", 953 },
	{ "iquest", 191 },
	{ "iuml", 239 },
	{ "kappa", 954 },
	{ "lambda", 955 },
	{ "laquo", 171 },
	{ "ldots", 8230 },
	{ "lt", 60 },
	{ "macr", 175 },
	{ "micro", 181 },
	{ "middot", 183 },
	{ "mu", 956 },
	{ "nbsp", 160 },
	{ "not", 172 },
	{ "ntilde", 241 },
	{ "nu", 957 },
	{ "oacute", 243 },
	{ "ocirc", 244 },
	{ "ograve", 242 },
	{ "omega", 969 },
	{ "omicron", 959 },
	{ "ordf", 170 },
	{ "ordm", 186 },
	{ "oslash", 248 },
	{ "otilde", 245 },
	{ "ouml", 246 },
	{ "para", 182 },
	{ "phi", 966 },
	{ "pi", 960 },
	{ "plusmn", 177 },
	{ "pound", 163 },
	{ "psi", 968 },
	{ "quad", 8193 },
	{ "quot", 34 },
	{ "raquo", 187 },
	{ "reg", 174 },
	{ "rho", 961 },
	{ "sect", 167 },
	{ "shy", 173 },
	{ "sigma", 963 },
	{ "sp", 8194 },
	{ "sup1", 185 },
	{ "sup2", 178 },
	{ "sup3", 179 },
	{ "szlig", 223 },
	{ "tau", 964 },
	{ "theta", 952 },
	{ "thinsp", 8201 },
	{ "thorn", 254 },
	{ "times", 215 },
	{ "trade", 8482 },
	{ "uacute", 250 },
	{ "ucirc", 251 },
	{ "ugrave", 249 },
	{ "uml", 168 },
	{ "upsilon", 965 },
	{ "uuml", 252 },
	{ "varepsilon", 8712 },
	{ "varphi", 981 },
	{ "varpi", 982 },
	{ "varrho", 1009 },
	{ "vdots", 8942 },
	{ "vsigma", 962 },
	{ "vtheta", 977 },
	{ "xi", 958 },
	{ "yacute", 253 },
	{ "yen", 165 },
	{ "yuml", 255 },
	{ "zeta", 950 }
};

static int
isdigits(char *s)
{
	for( ; isdigit(*s); s++)
		;
	return *s == '\0';
}

static int
isxdigits(char *s)
{
	for( ; isxdigit(*s); s++)
		;
	return *s == '\0';
}

static long
nametorune(char *name)
{
	int n, (*iscode)(char *s);
	int lo, hi, m, cmp;

	if(*name == '#'){
		name++;
		iscode = isdigits;
		n = 10;
		if(*name == 'x' || *name == 'X'){
			name++;
			iscode = isxdigits;
			n = 16;
		}
		if(*name && iscode(name))
			return strtol(name, nil, n);
		else
			return -1;
	}

	lo = 0;
	hi = nelem(chartab) - 1;
	while(lo <= hi){
		m = (lo+hi) / 2;
		cmp = cistrcmp(name, chartab[m].name);
		if(cmp > 0)
			lo = m + 1;
		else if(cmp < 0)
			hi = m - 1;
		else
			return chartab[m].val;
	}
	return -1;
}

static Rune
getentity(char *s, char **r)
{
	long c;
	int n;
	char *p, name[256];

	p = strchr(++s, ';');
	if(p == nil)
		goto unmatched;

	n = p - s;
	if(n >= sizeof(name))
		goto unmatched;

	memmove(name, s, n);
	name[n] = '\0';
	if((c=nametorune(name)) >= 0){
		*r = s + n;
		return c;
	}

unmatched:
	*r = s - 1;		/* recovery: lt; -> &lt; */
	return **r;
}

char *
unhtml(char *s)
{
	Rune c;
	char *t, *p;

	t = s;
	for(p = s; *p; p++)
		if(*p == '<'){
			if (strncmp(p, "<br>", 4) == 0)
				*t++ = '\n';
			p = strchr(p, '>');
		}else if(*p == '&'){
			/*
			 * UTFmax=3, and the smallest code "&c;"=3.
			 * ("&;" isn't included)
			 * so, buffer overflow will not be happen.
			 */
			c = getentity(p, &p);
			t += runetochar(t, &c);
		}else
			*t++ = *p;
	*t = '\0';
	return s;
}

char *
decode(char *s)
{
	char *t, *p, buf[3];

	t = p = s;
	for( ; *s; s++)
		if(*s == '%'){
			memmove(buf, s+1, 2);
			buf[2] = '\0';
			*p++ = atoi(buf);
			s += 2;
		}else
			*p++ = *s;
	*p = '\0';
	return t;
}

int
Ufmt(Fmt *fmt)		/* convert to SJIS and URI encode */
{
	char *s;
	int i, n;
	Rune r;
	char buf[JISmax];

	s = va_arg(fmt->args, char*);
	while(*s)
		if(isalnum(*s))
			fmtprint(fmt, "%c", *s++);
		else if(*s == ' '){
			fmtprint(fmt, "+");
			s++;
		}else{
			s += chartorune(&r, s);
			n = runetosjis(buf, &r);
			for(i = 0; i < n; i++)
				fmtprint(fmt, "%%%02x", (uchar)buf[i]);
		}
	return 0;
}

int
split(char *s, char *a[], int n, char *sep)
{
	int i, len;
	char *p;

	len = strlen(sep);
	for(i = 0; i < n; i++){
		p = strstr(s, sep);
		if(p == nil){
			a[i++] = s;
			break;
		}
		*p = '\0';
		a[i] = s;
		s = p+len;
	}
	return i;
}
