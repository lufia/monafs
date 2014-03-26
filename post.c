#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include <String.h>
#include <kanji.h>

typedef struct Arg Arg;
struct Arg {
	char *name;
	char *val;
	Arg *next;
};

enum {
	Get,
	Post,

	UTF_8,
	EUC_JP,
	ISO_2022_JP,
	Shift_JIS,
};

int mode = Post;
int chset = UTF_8;
char *posturi;
Arg *arglist;

void post(int fd);
Rune *getline(Rune *s, long n, Biobufhdr *fin);
int moreinput(Biobufhdr *fin);
Arg *parse(Rune *s);
char *convert(Rune *s);
void escape(String *s, char c);
Rune *Strchr(Rune *s, Rune c);
void *emalloc(long n);
char *estrdup(char *s);

void
usage(void)
{
	fprint(2, "%s [-gejs] uri [file ...]\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, fd;
	Arg *p;

	quotefmtinstall();
	ARGBEGIN {
	case 'e':
		chset = EUC_JP;
		break;
	case 'g':
		mode = Get;
		break;
	case 'j':
		chset = ISO_2022_JP;
		break;
	case 's':
		chset = Shift_JIS;
		break;
	default:
		usage();
	} ARGEND

	if (argc == 0)
		usage();
	posturi = argv[0];
	argc--;
	argv++;

	if (argc == 0)
		post(0);
	else
		for (i = 0; i < argc; i++) {
			fd = open(argv[i], OREAD);
			if (fd < 0)
				sysfatal("open: %r");
			post(fd);
			close(fd);
		}

	switch (mode) {
	case Post:
		print("hget");
		for (p = arglist; p; p = p->next)
			print(" -p '%s=%s'", p->name, p->val);
		print(" %q\n", posturi);
		break;
	case Get:
		print("hget %q", posturi);
		for (p = arglist; p; p = p->next)
			if (p == arglist)
				print("^'?%s=%s'", p->name, p->val);
			else
				print("^'&%s=%s'", p->name, p->val);
		print("\n");
		break;
	default:
		sysfatal("%d unknown mode", mode);
	}
	exits(nil);
}

void
post(int fd)
{
	Biobuf fin;
	Rune buf[4*1024], *s;
	Arg *a;

	Binit(&fin, fd, OREAD);
	while (s = getline(buf, nelem(buf), &fin)) {
		a = parse(s);
		a->next = arglist;
		arglist = a;
	}
}

Rune *
getline(Rune *buf, long n, Biobufhdr *fin)
{
	int i, c;

	i = 0;
	do {
		while (i < n-1 && (c=Bgetrune(fin)) != Beof) {
			buf[i++] = c;
			if (c == '\n')
				break;
		}
		if (c != Beof && c != '\n')
			while ((c=Bgetrune(fin)) != Beof && c != '\n')
				;
	} while (c != Beof && moreinput(fin));

	if (i == 0)
		return nil;
	else {
		buf[i-1] = '\0';
		return buf;
	}
}

int
moreinput(Biobufhdr *fin)
{
	int c;

	if ((c=Bgetrune(fin)) != Beof) {
		if (c == ' ' || c == '\t')
			return 1;		/* skip a space */
		Bungetrune(fin);
	}
	return 0;
}

Arg *
parse(Rune *s)
{
	Arg *a;
	Rune *p;

	p = Strchr(s, ':');
	if (p == nil)
		sysfatal("`%S' no colon", s);
	*p = '\0';
	a = emalloc(sizeof(*a));
	a->name = convert(s);
	a->val = convert(p+1);
	a->next = nil;
	return a;
}

char *
convert(Rune *s)
{
	static String *sbuf;
	int i, n, state;
	char buf[JISmax];

	if (sbuf == nil) {
		sbuf = s_new();
		if (sbuf == nil)
			sysfatal("s_new: %r");
	}

	state = 0;
	s_reset(sbuf);
	for ( ; *s; s++) {
		switch (chset) {
		case EUC_JP:
			n = runetoujis(buf, s);
			break;
		case ISO_2022_JP:
			n = runetojis(buf, s, &state);
			break;
		case Shift_JIS:
			n = runetosjis(buf, s);
			break;
		default:
			n = runetochar(buf, s);
			break;
		}

		for (i = 0; i < n; i++)
			escape(sbuf, buf[i]);
	}
	s_putc(sbuf, '\0');
	return estrdup(s_to_c(sbuf));
}

void
escape(String *s, char c)
{
	/*
	 * a-zA-Z0-9: pass
	 * space(' '): '+'
	 * other: %XX
	 */
	char buf[4];

	if (c == ' ')
		s_putc(s, '+');
	else if (isalnum(c))
		s_putc(s, c);
	else {
		sprint(buf, "%%%02x", (uchar) c);
		s_append(s, buf);
	}
}

Rune *
Strchr(Rune *s, Rune c)
{
	for ( ; *s; s++)
		if (*s == c)
			return s;
	return nil;
}

void *
emalloc(long n)
{
	void *p;

	p = malloc(n);
	if (p == nil)
		sysfatal("malloc: %r");
	return p;
}

char *
estrdup(char *s)
{
	char *p;

	p = emalloc(strlen(s)+1);
	strcpy(p, s);
	return p;
}
