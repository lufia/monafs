#include <u.h>
#include <libc.h>
#include "impl.h"

static int first = 1;
static int tab[NRune];

int
k208index(Rune c)
{
	int i;
	long l;

	if(first){
		for(i = 0; i < NRune; i++)
			tab[i] = -1;
		for(i = 0; i < K208max; i++)
			if((l=getk208(i)) != -1)
				tab[l] = i;
		first = 0;
	}

	return tab[c];
}
