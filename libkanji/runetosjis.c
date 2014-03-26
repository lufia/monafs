#include <u.h>
#include <libc.h>
#include "impl.h"

static void
jistoms(uchar *h, uchar *l)
{
	uchar high, low;

	high = *h;
	low = *l;
	if(high-- % 2)
		low += 0x1F;
	else
		low += 0x7D;
	if(low > 0x7E)
		low++;

	high = high/2 + 0x71;
	if(high > 0x9F)
		high += 0x40;
	*h = high;
	*l = low;
}

int
runetosjis(char *str, Rune *rune)
{
	int i;
	long c;

	/*
	 * one character sequence
	 */
	c = *rune;
	if(c < 128){
		str[0] = c;
		return 1;
	}

	/*
	 * two character sequence
	 */
	i = k208index(c);
	if(i != -1){
		str[0] = i/100 + ' ';
		str[1] = i%100 + ' ';
		jistoms((uchar *) &str[0], (uchar *) &str[1]);
		return 2;
	}

	str[0] = Badbyte;
	return 1;
}
