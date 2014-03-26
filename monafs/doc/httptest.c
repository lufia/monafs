#include "monafs.h"
#include <draw.h>
#include <html.h>

#define STYLE(fnt) ((fnt) / NumSize)

void
parse(char *data, char *uri)
{
	Rune *s;
	Item *items, *il;
	Itext *text;
	Docinfo *pdi;

	s = toStr((uchar*)uri, strlen(uri), ISO_8859_1);
	items = parsehtml((uchar*)data, strlen(data), s, TextHtml, UTF_8, &pdi);
	free(s);
	if(pdi->doctitle)
		print("title: %S\n", pdi->doctitle);
	for(il = items; il; il = il->next)
		if(il->tag == Itexttag){
			text = (Itext*)il;
			if(STYLE(text->fnt) == FntB)
				print("category: %S\n", text->s);
			else
				print("board: %S\n", text->s);
		}
	freeitems(items);
	freedocinfo(pdi);
}

void
main(int argc, char *argv[])
{
	int i, fd;
	long n, clock;
	char *data;

	clock = time(nil);
	httpinit();
	for(i = 1; i < argc; i++){
		if(httpmodified(argv[i], clock) > 0)
			print("modified now");
		if(httpmodified(argv[i], clock-10000) > 0)
			print("modified before 10000sec");
		fd = httpopen(argv[i]);
		if(fd < 0)
			httpfatal("httpopen: %r");
		data = readfile(fd);
		parse(data, argv[i]);
		free(data);
		httpclose(fd);
	}
	httpend();
	exits(nil);
}
