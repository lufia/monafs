#include <u.h>
#include <libc.h>
#include "impl.h"

#define kuten208(high, low) ((high)*100 + (low) - 3232)

enum {
	K201head = 0xA1,
	K201tail = 0xDF,

	SHhead = 0x81,
	SHtail = 0xEA,		/* tcs=0xEF */
	SHinvalid = 0xA0,

	SLhead = 0x40,
	SLtail = 0xFC,
	SLinvalid = 0x7F,
};

static Rune k201tab[] = {
	[0xa1] = L'｡', /* ff61 */
	[0xa2] = L'｢', /* ff62 */
	[0xa3] = L'｣', /* ff63 */
	[0xa4] = L'､', /* ff64 */
	[0xa5] = L'･', /* ff65 */
	[0xa6] = L'ｦ', /* ff66 */
	[0xa7] = L'ｧ', /* ff67 */
	[0xa8] = L'ｨ', /* ff68 */
	[0xa9] = L'ｩ', /* ff69 */
	[0xaa] = L'ｪ', /* ff6a */
	[0xab] = L'ｫ', /* ff6b */
	[0xac] = L'ｬ', /* ff6c */
	[0xad] = L'ｭ', /* ff6d */
	[0xae] = L'ｮ', /* ff6e */
	[0xaf] = L'ｯ', /* ff6f */
	[0xb0] = L'ｰ', /* ff70 */
	[0xb1] = L'ｱ', /* ff71 */
	[0xb2] = L'ｲ', /* ff72 */
	[0xb3] = L'ｳ', /* ff73 */
	[0xb4] = L'ｴ', /* ff74 */
	[0xb5] = L'ｵ', /* ff75 */
	[0xb6] = L'ｶ', /* ff76 */
	[0xb7] = L'ｷ', /* ff77 */
	[0xb8] = L'ｸ', /* ff78 */
	[0xb9] = L'ｹ', /* ff79 */
	[0xba] = L'ｺ', /* ff7a */
	[0xbb] = L'ｻ', /* ff7b */
	[0xbc] = L'ｼ', /* ff7c */
	[0xbd] = L'ｽ', /* ff7d */
	[0xbe] = L'ｾ', /* ff7e */
	[0xbf] = L'ｿ', /* ff7f */
	[0xc0] = L'ﾀ', /* ff80 */
	[0xc1] = L'ﾁ', /* ff81 */
	[0xc2] = L'ﾂ', /* ff82 */
	[0xc3] = L'ﾃ', /* ff83 */
	[0xc4] = L'ﾄ', /* ff84 */
	[0xc5] = L'ﾅ', /* ff85 */
	[0xc6] = L'ﾆ', /* ff86 */
	[0xc7] = L'ﾇ', /* ff87 */
	[0xc8] = L'ﾈ', /* ff88 */
	[0xc9] = L'ﾉ', /* ff89 */
	[0xca] = L'ﾊ', /* ff8a */
	[0xcb] = L'ﾋ', /* ff8b */
	[0xcc] = L'ﾌ', /* ff8c */
	[0xcd] = L'ﾍ', /* ff8d */
	[0xce] = L'ﾎ', /* ff8e */
	[0xcf] = L'ﾏ', /* ff8f */
	[0xd0] = L'ﾐ', /* ff90 */
	[0xd1] = L'ﾑ', /* ff91 */
	[0xd2] = L'ﾒ', /* ff92 */
	[0xd3] = L'ﾓ', /* ff93 */
	[0xd4] = L'ﾔ', /* ff94 */
	[0xd5] = L'ﾕ', /* ff95 */
	[0xd6] = L'ﾖ', /* ff96 */
	[0xd7] = L'ﾗ', /* ff97 */
	[0xd8] = L'ﾘ', /* ff98 */
	[0xd9] = L'ﾙ', /* ff99 */
	[0xda] = L'ﾚ', /* ff9a */
	[0xdb] = L'ﾛ', /* ff9b */
	[0xdc] = L'ﾜ', /* ff9c */
	[0xdd] = L'ﾝ', /* ff9d */
	[0xde] = L'ﾞ', /* ff9e */
	[0xdf] = L'ﾟ', /* ff9f */
};

/*
 * tests given bytes from a MS kanji code point which can be
 * transformed to a valid JIS X 208 code point.
 */
static int
canmstojis(uchar high, uchar low)
{
	if(high >= SHhead && high <= SHtail)
	if(high != SHinvalid)
	if(high < K201head || high > K201tail)	/* not hankaku */
	if(low >= SLhead && low <= SLtail)
	if(low != SLinvalid)
		return 1;
	return 0;
}

static void
mstojis(uchar *h, uchar *l)
{
	uchar high, low;

	/*
	 * SLhead <= low < SLinvalid
	 * SLinvalid < low <= SLtail
	 */
	low = *l;
	low -= 0x1F;
	if(low > SLinvalid-0x1F)
		low--;

	/*
	 * SHhead <= high < SHinvalid
	 * (SHinvalid, K201head .. K201tail)
	 * K201tail < high <= SHtail
	 */
	high = *h;
	high -= SHhead;
	if(high > K201tail-SHhead)
		high -= 0x40;
	high = high*2 + 0x21;

	if(low > 0x7E){
		high++;
		low -= 0x5E;
	}
	*h = high;
	*l = low;
}

int
sjistorune(Rune *rune, char *str)
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
	if(c >= K201head && c <= K201tail){
		*rune = k201tab[c];
		return 1;
	}

	/*
	 * two character sequence
	 */
	c1 = *(uchar*)(str+1);
	if(c1 < 0)
		c1 = 0x21 | (c&0x80);
	if(!canmstojis(c, c1))
		goto bad;
	mstojis((uchar*)&c, (uchar*)&c1);
	n = kuten208(c, c1);
	if((l=getk208(n)) != -1){
		*rune = l;
		return 2;
	}

bad:
	*rune = Bad;
	return 1;
}
