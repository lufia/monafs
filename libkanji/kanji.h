#pragma src "/usr/glenda/sys/src/libkanji"
#pragma lib "libkanji.a"

extern int runetojis(char *str, Rune *rune, int *state);
extern int runetosjis(char *str, Rune *rune);
extern int runetoujis(char *str, Rune *rune);
extern int jistorune(Rune *rune, char *str, int *state);
extern int sjistorune(Rune *rune, char *str);
extern int ujistorune(Rune *rune, char *str);

enum { JISmax = 5 };
