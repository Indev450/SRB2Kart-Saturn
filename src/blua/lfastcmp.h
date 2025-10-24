#ifndef __FASTCMP_H__
#define __FASTCMP_H__

#include <ctype.h>
#include <stdbool.h>

// same as fastcmp but we cant use our fancy macros

// returns false if s != c
// returns true if s == c
__attribute__((always_inline)) static inline bool fasticmp(const char *s, const char *c)
{
	for (; *s && toupper(*s) == toupper(*c); s++, c++) ;
	return (*s == *c); // make sure both strings ended
}

// case-sensitive of the above
__attribute__((always_inline)) static inline bool fastcmp(const char *s, const char *c)
{
	for (; *s && *s == *c; s++, c++) ;
	return (*s == *c); // make sure both strings ended
}

// length-limited of the above
// only true if both strings are at least l characters long AND match, case-sensitively!
__attribute__((always_inline)) static inline bool fastncmp(const char *s, const char *c, uint16_t l)
{
	for (; *s && *s == *c && --l; s++, c++) ;
	return !l; // make sure you reached the end
}

#endif
