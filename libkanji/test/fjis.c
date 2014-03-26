#include <u.h>
#include <libc.h>
#include <bio.h>
#include <kanji.h>

void test(Biobuf *fin);

void main(int argc, char *argv[])
{
	Biobuf fin;

	ARGBEGIN {
	default:
		break;
	} ARGEND

	Binit(&fin, 0, OREAD);
	test(&fin);
	exits(nil);
}

void test(Biobuf *fin)
{
	int n, state;
	Rune r;
	char *p, *line;

	state = 0;
	while (line = Brdline(fin, '\n')) {
		line[Blinelen(fin)-1] = '\0';
		for (p = line; *p != '\0'; p += n) {
			n = jistorune(&r, p, &state);
			print("%C", r);
		}
		print("\n");
	}
}
