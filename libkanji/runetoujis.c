#include <u.h>
#include <libc.h>
#include "impl.h"

int
runetoujis(char *str, Rune *rune)
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
		str[0] = 0x80 | (i/100 + ' ');
		str[1] = 0x80 | (i%100 + ' ');
		return 2;
	}

	str[0] = Badbyte;
	return 1;
}
