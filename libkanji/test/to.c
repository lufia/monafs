#include <u.h>
#include <libc.h>
#include <bio.h>
#include <kanji.h>

void test(Biobuf *fin);

int (*conv)(char*, Rune*);

void main(int argc, char *argv[])
{
	Biobuf fin;

	conv = runetochar;
	ARGBEGIN {
	case 'e':
		conv = runetoujis;
		break;
	case 's':
		conv = runetosjis;
		break;
	default:
		break;
	} ARGEND

	Binit(&fin, 0, OREAD);
	test(&fin);
	exits(nil);
}

void test(Biobuf *fin)
{
	int i, n;
	Rune r;
	char *p, buf[5], *line;

	while (line = Brdline(fin, '\n')) {
		line[Blinelen(fin)-1] = '\0';
		for (p = line; *p != '\0'; p += n) {
			n = chartorune(&r, p);
			i = conv(buf, &r);
			write(1, buf, i);
		}
		print("\n");
	}
}
