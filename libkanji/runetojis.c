#include <u.h>
#include <libc.h>
#include "impl.h"

enum {
	Ascii,
	Japan646,
	Jp2022,
};

static char *
jisout(char *s, int *state)
{
	if(*state == Jp2022){
		*s++ = Esc;
		*s++ = '(';
		*s++ = 'B';
		*state = Ascii;
	}
	return s;
}

static char *
jisin(char *s, int *state)
{
	if(*state != Jp2022){
		*s++ = Esc;
		*s++ = '$';
		*s++ = 'B';
		*state = Jp2022;
	}
	return s;
}

int
runetojis(char *str, Rune *rune, int *state)
{
	long c;
	int i;
	char *p;

	/*
	 * one character sequence
	 */
	c = *rune;
	if(c < 128){
		p = jisout(str, state);
		*p++ = c;
		return p - str;
	}

	/*
	 * two character sequence
	 */
	i = k208index(c);
	if(i != -1){
		p = jisin(str, state);
		*p++ = i/100 + ' ';
		*p++ = i%100 + ' ';
		return p - str;
	}

	str[0] = Bad;
	return 1;
}
