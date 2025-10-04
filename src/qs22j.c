//  License: 0BSD
//
//  Copyright 2022 Raymond Gardner
//
//  Permission to use, copy, modify, and/or distribute this software for any
//  purpose with or without fee is hereby granted.
//
//  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
//  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
//  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
//  SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
//  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
//  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
//  IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
//

#include <stddef.h>
#include <stdint.h>

#include "qs22j.h"

#define INSORTTHRESH	5			// if n < this use insertion sort
									// MUST be >= 2
#define MIDTHRESH		20			// < this use middle as pivot
#define MEDOF3THRESH	50			// < this use median-of-3 as pivot
									// larger subfiles use med-of-3-medians

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

typedef int32_t WORD;
typedef int64_t DWORD;
typedef void *pref_typ;

// if no uintptr_t, use Bentley-McIlroy trick (undefined behavior)
//#define ptr_to_int(p) (p-(char*)0)
#define ptr_to_int(p) ((uintptr_t)(void *)p)

#define ASWAP(a, b, t) ((void)(t = a, a = b, b = t))

#define SWAP(a, b) if (swap_type) swapf(a, b, size);\
	else do {pref_typ t; ASWAP(*(pref_typ*)(a), *(pref_typ*)(b), t);} while (0)

#define COMP(a, b)  ((*compar)((void *)(a), (void *)(b)))

static inline void swapbytes(void *a0, void *b0, size_t n)
{
	char *a = a0, *b = b0, t;
	do {ASWAP(*a, *b, t); a++; b++;} while (--n);
}

static inline void swapdword(void *a0, void *b0, size_t n)
{
	(void)n;
	DWORD *a = a0, *b = b0, t;
	ASWAP(*a, *b, t);
}

static inline void swapdwords(void *a0, void *b0, size_t n)
{
	DWORD *a = a0, *b = b0, t;
	do {ASWAP(*a, *b, t); a++; b++;} while (n -= sizeof(DWORD));
}

static inline void swapword(void *a0, void *b0, size_t n)
{
	(void)n;
	WORD *a = a0, *b = b0, t;
	ASWAP(*a, *b, t);
}

static inline void swapwords(void *a0, void *b0, size_t n)
{
	WORD *a = a0, *b = b0, t;
	do {ASWAP(*a, *b, t); a++; b++;} while (n -= sizeof(WORD));
}

typedef void (*swapf_typ)(void *, void *, size_t);

static inline char *med3(char *a, char *b, char *c, int (*compar)(const void *, const void *))
{
	return COMP(a, b) < 0 ?
		(COMP(b, c) < 0 ? b : COMP(a, c) < 0 ? c : a) :
		(COMP(b, c) > 0 ? b : COMP(a, c) > 0 ? c : a);
}

void qs22j(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))
{
	// we have nothing to sort
	if (__builtin_expect(!!(nmemb <= 1), 0) ) //UNLIKELY - thx windoze
	{
		return;
	}

	char *stack[2*8*sizeof(size_t)], **sp = stack;	// stack and stack pointer
	char *left = base;								// set up char * base pointer
	char *limit = left + nmemb * size;				// pointer past end of array
	char *i, *ii, *j, *jj;							// scan pointers
	int ki = 0, kj = 0;
	int swap_type = 1;
	swapf_typ swapf, vecswapf;

	vecswapf = swapf = swapbytes;
	if ((ptr_to_int(left) | size) % sizeof(WORD))
	{
		;											// unaligned or not multple of WORD size; swap bytes
	}
	else if (size == sizeof(DWORD))
	{
		swapf = swapdword;
		vecswapf = swapdwords;
		if (size == sizeof(pref_typ))
		swap_type = 0;
	}
	else if (size == sizeof(WORD))
	{
		swapf = swapword;
		vecswapf = swapwords;
		if (size == sizeof(pref_typ))
			swap_type = 0;
    }
	else if ((size % sizeof(DWORD)) == 0)
	{
		swapf = vecswapf = swapdwords;
	}
	else if ((size % sizeof(WORD)) == 0)
	{
		swapf = vecswapf = swapwords;
	}

	for (;;)
	{
		nmemb = (limit - left) / size;
		for (i = left + size; i < limit && COMP(i - size, i) <= 0; i += size)
			;

		if (i == limit)								// if already in order
			goto pop;

		if (nmemb >= INSORTTHRESH)					// otherwise use insertion sort
		{
			char *right = limit - size;
			// best so far? fewer compares, a few more swaps
			char *p = left + (nmemb / 2) * size;
			if (nmemb >= MIDTHRESH)
			{
				char *pleft = left + size;
				char *pright = right - size;

				if (nmemb >= MEDOF3THRESH)
				{
					size_t k = (nmemb / 8) * size;
					pleft = med3(pleft, left + k, left + k * 2, compar);
					p = med3(p - k, p, p + k, compar);
					pright = med3(right - k * 2, right - k, pright, compar);
				}

				p = med3(pleft, p, pright, compar);
			}

			i = ii = left;							// i scans left to right
			j = jj = right;							// j scans right to left
            for (;;)
			{
                while (i <= j)
				{
                    if (i != p && ((ki = COMP(i, p)) >= 0))
					{
						if (ki)
							break;

                        if (ii == p)
						{
							p = i;
						}
                        else if (i != ii)
						{
							SWAP(i, ii);
						}

						ii += size;
					}
					i += size;
				}

				while (i < j)
				{
					if (j != p && ((kj = COMP(j, p)) <= 0))
					{
						if (kj)
							break;

						if (jj == p)
						{
							p = j;
						}
						else if (j != jj)
						{
							SWAP(j, jj);
						}

						jj -= size;
					}
					j -= size;
				}

				if (i >= j)
					break;

				SWAP(i, j);
				i += size;
				j -= size;
			}

			if (p < i)
				i -= size;

            if (p != i)
			{
                SWAP(p, i);
			}

			ptrdiff_t lessthan = i - ii;
			size_t k = min(lessthan, ii - left);

			if (k)
				vecswapf(left, i - k, k);

			ptrdiff_t morethan = jj - i;
			k = min(morethan, right - jj);

			if (k)
				vecswapf(i + size, limit - k, k);

			if (lessthan > morethan)
			{
				if (lessthan > 1)
				{
					sp[0] = left;
					sp[1] = left + lessthan;
					sp += 2;						// increment stack pointer
				}

				if (morethan <= 1)
					goto pop;

				left = limit - morethan;
            }
			else
			{
				if (morethan > 1)
				{
					sp[0] = limit - morethan;
					sp[1] = limit;
					sp += 2;						// increment stack pointer
				}

				if (lessthan <= 1)
					goto pop;

				limit = left + lessthan;
			}
		}
		else			// else subfile is small, use insertion sort
		{
			for (i = left + size; i < limit; i += size)
			{
				for (j = i; j != left && COMP(j - size, j) > 0; j -= size)
				{
					SWAP(j - size, j);
				}
			}
pop:
			if (sp != stack)						// if any entries on stack
			{
				sp -= 2;							// pop the left and limit
				left = sp[0];
				limit = sp[1];
			}
			else									// else stack empty, done
				break;
        }
    }
}
