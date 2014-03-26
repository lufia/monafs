#include <u.h>
#include <libc.h>
#include "impl.h"

enum {
	Codeset2 = 0x8E,
	Codeset3 = 0x8F,
};

#define kuten208(high, low) (((high)&0x7F)*100 + ((low)&0x7F) - 3232)

int
ujistorune(Rune *rune, char *str)
{
	int n, c, c1;
	long l;

	/*
	 * one character sequence
	 */
	c = *(uchar*)str;
	if(c < 128){
		*rune = c;
		return 1;
	}
	if(c == Codeset2 || c == Codeset3)
		goto bad;

	/*
	 * two character sequence
	 */
	c1 = *(uchar*)(str+1);
	if(c1 < 0)			/* unexpected EOF */
		c1 = 0xA1;
	n = kuten208(c, c1);
	if((l=getk208(n)) != -1){
		*rune = l;
		return 2;
	}

bad:
	*rune = Bad;
	return 1;
}
