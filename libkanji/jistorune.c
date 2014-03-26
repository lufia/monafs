#include <u.h>
#include <libc.h>
#include "impl.h"

enum {
	/* modes; fig-2 of int */
	Ascii = 0*10,
	Set8 = 1*10,
	Japan646 = 2*10,

	/* states; fig-1 of int */
	Tidle = 0,		/* idle state */
	Tesc,		/* seen an escape */
	Tin,			/* may be shifting into JIS */
	Tout,		/* may be shifting out of JIS */
	Tchar,		/* two part char */
};
#define getmode(m) ((m)/10 * 10)
#define getstate(m) ((m)%10)
#define setmode(m, mode) (*(m) = getstate(*(m))+(mode))
#define setstate(m, state) (*(m) = (state)+getmode(*(m)))

#define kuten208(high, low) (((high)&0x7F)*100 + ((low)&0x7f) - 3232)

static int
convert(Rune *rune, char *str, int *state)
{
	int n, mode;
	long c, lastc, l;
	char *p;

	lastc = -1;
	p = str;
again:
	c = *(uchar*)p++;
	switch (getstate(*state)) {
	case Tidle:
		if(c == Esc){
			setstate(state, Tesc);
			goto again;
		}

		mode = getmode(*state);
		if(mode != Set8 && c < 128){
			if(mode == Japan646) {
				if(c == '\\')
					*rune = Yen;
				else if(c == '~')
					*rune = Macron;
				else
					*rune = c;
			}else
				*rune = c;
			return p - str;
		}

		lastc = c;
		setstate(state, Tchar);
		goto again;

	case Tesc:
		if(c == '$'){
			setstate(state, Tin);
			goto again;
		}else if(c == '('){
			setstate(state, Tout);
			goto again;
		}

		*rune = Esc;
		setstate(state, Tidle);
		return p - str;

	case Tin:
		if(c == '@' || c == 'B'){
			setmode(state, Set8);
			setstate(state, Tidle);
			goto again;
		}

		*rune = Esc;
		setstate(state, Tidle);
		return p - str-2;		/* -2: next, will read at '$' */

	case Tout:
		if(c == 'J' || c == 'H' || c == 'B'){
			if(c == 'J')
				setmode(state, Japan646);
			else
				setmode(state, Ascii);
			setstate(state, Tidle);
			return p - str;
		}

		*rune = Esc;
		setstate(state, Tidle);
		return p - str-2;		/* -2: next, will read at '(' */

	case Tchar:
		if(c < 0)
			c = 0x21 | (lastc&0x80);
		if((lastc&0x80) != (c&0x80)){	/* guard against latin1 in jis */
			*rune = lastc;
			setstate(state, Tidle);
			return p - str-1;
		}

		n = kuten208(lastc, c);
		if((l=getk208(n)) != -1)
			*rune = l;
		else
			*rune = Bad;
		setstate(state, Tidle);
		return p - str;

	default:
		abort();
		return -1;
	}
}

int
jistorune(Rune *rune, char *str, int *state)
{
	int n;
	Rune r;	/* dummy */

	n = convert(rune, str, state);
	if(str[n] == Esc && str[n+1] == '(' && strchr("JHB", str[n+2]))
		n += convert(&r, str+n, state);
	return n;
}
