#include <u.h>
#include <libc.h>
#include <bio.h>
#include <kanji.h>

void test(Biobuf *fin);

int (*conv)(Rune*, char*);

void main(int argc, char *argv[])
{
	Biobuf fin;

	conv = chartorune;
	ARGBEGIN {
	case 'e':
		conv = ujistorune;
		break;
	case 's':
		conv = sjistorune;
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
	int n;
	Rune r;
	char *p, *line;

	while (line = Brdline(fin, '\n')) {
		line[Blinelen(fin)-1] = '\0';
		for (p = line; *p != '\0'; p += n) {
			n = conv(&r, p);
			print("%C", r);
		}
		print("\n");
	}
}
