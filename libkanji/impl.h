enum {
	NRune = 65536,
	K208max = 8407,
	Bad = Runeerror,
	Badbyte = '?',

	Esc = 033,
	Yen = 0xA5,
	Macron = 0xAF,
};

extern long getk208(int n);
extern int k208index(Rune c);
