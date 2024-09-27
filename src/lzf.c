// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 2000-2005 by Marc Alexander Lehmann <schmorp@schmorp.de>
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  lzf.c
/// \brief LZF de/compression routines

/* LZF decompression routines copied from lzf_d.c from liblzf 1.7 */
/* LZF compression routines copied from lzf_c.c from liblzf 1.7 */

/*
 * lzfP.h included here by Graue.
 */

#ifndef LZFP_h
#define LZFP_h

#include "lzf.h"
#include "doomdef.h"

/*
 * Size of hashtable is (1 << HLOG) * sizeof (char *)
 * decompression is independent of the hash table size
 * the difference between 15 and 14 is very small
 * for small blocks (and 14 is usually a bit faster).
 * For a low-memory/faster configuration, use HLOG == 13;
 * For best compression, use 15 or 16 (or more).
 */
#ifndef HLOG
# define HLOG 15
#endif

/*
 * Sacrifice very little compression quality in favour of compression speed.
 * This gives almost the same compression as the default code, and is
 * (very roughly) 15% faster. This is the preferred mode of operation.
 */
#ifndef VERY_FAST
# define VERY_FAST 1
#endif

/*
 * Sacrifice some more compression quality in favour of compression speed.
 * (roughly 1-2% worse compression for large blocks and
 * 9-10% for small, redundant, blocks and >>20% better speed in both cases)
 * In short: when in need for speed, enable this for binary data,
 * possibly disable this for text data.
 */
#ifndef ULTRA_FAST
# define ULTRA_FAST 0
#endif

/*
 * Unconditionally aligning does not cost very much, so do it if unsure
 */
#ifndef STRICT_ALIGN
#if !(defined(__i386) || defined (__amd64)) || defined (__clang__)
#define STRICT_ALIGN 1
#else
#define STRICT_ALIGN 0
#endif
#endif

/*
 * You may choose to pre-set the hash table (might be faster on some
 * modern cpus and large (>>64k) blocks, and also makes compression
 * deterministic/repeatable when the configuration otherwise is the same).
 */
#ifndef INIT_HTAB
# define INIT_HTAB 1
#endif

/*
 * Avoid assigning values to errno variable? for some embedding purposes
 * (linux kernel for example), this is necessary. NOTE: this breaks
 * the documentation in lzf.h. Avoiding errno has no speed impact.
 */
#ifndef AVOID_ERRNO
# define AVOID_ERRNO 0
#endif

/*
 * Whether to pass the LZF_STATE variable as argument, or allocate it
 * on the stack. For small-stack environments, define this to 1.
 * NOTE: this breaks the prototype in lzf.h.
 */
#ifndef LZF_STATE_ARG
# define LZF_STATE_ARG 0
#endif

/*
 * Whether to add extra checks for input validity in lzf_decompress
 * and return EINVAL if the input stream has been corrupted. This
 * only shields against overflowing the input buffer and will not
 * detect most corrupted streams.
 * This check is not normally noticeable on modern hardware
 * (<1% slowdown), but might slow down older cpus considerably.
 */
#ifndef CHECK_INPUT
# define CHECK_INPUT 1
#endif

/*
 * Whether the target CPU has a slow multiplication. This affects
 * the default hash function for the compressor, and enables a slightly
 * worse hash function that needs only shifts.
 */
#ifndef MULTIPLICATION_IS_SLOW
# define MULTIPLICATION_IS_SLOW 0
#endif

/*
 * If defined, then this data type will be used for storing offsets.
 * This can be useful if you want to use a huge hashtable, want to
 * conserve memory, or both, and your data fits into e.g. 64kb.
 * If instead you want to compress data > 4GB, then it's better to
 * to "#define LZF_USE_OFFSETS 0" instead.
 */
/*#define LZF_HSLOT unsigned short*/

/*
 * Whether to store pointers or offsets inside the hash table. On
 * 64 bit architetcures, pointers take up twice as much space,
 * and might also be slower. Default is to autodetect.
 */
/*#define LZF_USE_OFFSETS autodetect */

/*
 * Whether to optimise code for size, at the expense of speed. Use
 * this when you are extremely tight on memory, perhaps in combination
 * with AVOID_ERRNO 1 and CHECK_INPUT 0.
 */
#ifndef OPTIMISE_SIZE
# ifdef __OPTIMIZE_SIZE__
#  define OPTIMISE_SIZE 1
# else
#  define OPTIMISE_SIZE 0
# endif
#endif

/*****************************************************************************/
/* nothing should be changed below */

# include <string.h>
# include <limits.h>

#if ULTRA_FAST
# undef VERY_FAST
#endif

#ifndef LZF_USE_OFFSETS
# ifdef _WIN32
#  define LZF_USE_OFFSETS defined(_M_X64)
# else
#   include <stdint.h>
#  define LZF_USE_OFFSETS (UINTPTR_MAX > 0xffffffffU)
# endif
#endif

typedef unsigned char u8;

#ifdef LZF_HSLOT
# define LZF_HSLOT_BIAS ((const u8 *)in_data)
#else
# if LZF_USE_OFFSETS
#  define LZF_HSLOT_BIAS ((const u8 *)in_data)
   typedef unsigned int LZF_HSLOT;
# else
#  define LZF_HSLOT_BIAS 0
   typedef const u8 *LZF_HSLOT;
# endif
#endif

#if USHRT_MAX == 65535
   typedef unsigned short u16;
#elif UINT_MAX == 65535
   typedef unsigned int u16;
#else
# undef STRICT_ALIGN
# define STRICT_ALIGN 1
#endif

#define LZF_MAX_LIT (1 <<  5)
#define LZF_MAX_OFF (1 << 13)
#define LZF_MAX_REF ((1 << 8) + (1 << 3))

typedef LZF_HSLOT LZF_STATE[1 << (HLOG)];

typedef struct
{
	const u8 *first [1 << (6+8)]; /* most recent occurance of a match */
	u16 prev [LZF_MAX_OFF]; /* how many bytes to go backwards for the next match */
} LZF_STATE_BEST[1];

#endif


/*
 * lzfP.h ends here. lzf_d.c follows.
 */

#if AVOID_ERRNO
# define SET_ERRNO(n)
#else
# include <errno.h>
# define SET_ERRNO(n) errno = (n)
#endif

#ifndef USE_REP_MOVSB
#if (defined (__i386) || defined (__amd64)) && __GNUC__ >= 3
#define USE_REP_MOVSB 1
#else
#define USE_REP_MOVSB 0  // Default to 0 if not defined elsewhere
#endif
#endif

#if USE_REP_MOVSB /* small win on amd, big loss on intel */
#if (defined (__i386) || defined (__amd64)) && __GNUC__ >= 3
# define lzf_movsb(dst, src, len)                \
   asm ("rep movsb"                              \
        : "=D" (dst), "=S" (src), "=c" (len)     \
        :  "0" (dst),  "1" (src),  "2" (len));
#endif
#endif

size_t
lzf_decompress (const void *const in_data,  size_t in_len,
				void			*out_data, size_t out_len)
{
	u8 const *ip = (const u8 *)in_data;
	u8		 *op = (u8 *)out_data;
	u8 const *const in_end  = ip + in_len;
	u8		 *const out_end = op + out_len;

	do
	{
		unsigned int ctrl = *ip++;

		if (ctrl < (1 << 5)) /* literal run */
		{
			ctrl++;

			if (op + ctrl > out_end)
			{
				SET_ERRNO (E2BIG);
				return 0;
			}

#if CHECK_INPUT
			if (ip + ctrl > in_end)
			{
				SET_ERRNO (EINVAL);
				return 0;
			}
#endif

#ifdef lzf_movsb
			lzf_movsb (op, ip, ctrl);
#elif OPTIMISE_SIZE
			while (ctrl--)
				*op++ = *ip++;
#else
			switch (ctrl)
			{
				case 32: *op++ = *ip++; case 31: *op++ = *ip++; case 30: *op++ = *ip++; case 29: *op++ = *ip++;
				case 28: *op++ = *ip++; case 27: *op++ = *ip++; case 26: *op++ = *ip++; case 25: *op++ = *ip++;
				case 24: *op++ = *ip++; case 23: *op++ = *ip++; case 22: *op++ = *ip++; case 21: *op++ = *ip++;
				case 20: *op++ = *ip++; case 19: *op++ = *ip++; case 18: *op++ = *ip++; case 17: *op++ = *ip++;
				case 16: *op++ = *ip++; case 15: *op++ = *ip++; case 14: *op++ = *ip++; case 13: *op++ = *ip++;
				case 12: *op++ = *ip++; case 11: *op++ = *ip++; case 10: *op++ = *ip++; case  9: *op++ = *ip++;
				case  8: *op++ = *ip++; case  7: *op++ = *ip++; case  6: *op++ = *ip++; case  5: *op++ = *ip++;
				case  4: *op++ = *ip++; case  3: *op++ = *ip++; case  2: *op++ = *ip++; case  1: *op++ = *ip++;
			}
#endif
		}
		else /* back reference */
		{
			unsigned int len = ctrl >> 5;

			u8 *ref = op - ((ctrl & 0x1f) << 8) - 1;

#if CHECK_INPUT
			if (ip >= in_end)
			{
				SET_ERRNO (EINVAL);
				return 0;
			}
#endif
			if (len == 7)
			{
				len += *ip++;
#if CHECK_INPUT
				if (ip >= in_end)
				{
					SET_ERRNO (EINVAL);
					return 0;
				}
#endif
			}

			ref -= *ip++;

			if (op + len + 2 > out_end)
			{
				SET_ERRNO (E2BIG);
				return 0;
			}

			if (ref < (u8 *)out_data)
			{
				SET_ERRNO (EINVAL);
				return 0;
			}

#ifdef lzf_movsb
			len += 2;
			lzf_movsb (op, ref, len);
#elif OPTIMISE_SIZE
			len += 2;

			do
				*op++ = *ref++;
			while (--len);
#else
			switch (len)
			{
				default:
					len += 2;

					if (op >= ref + len)
					{
						/* disjunct areas */
						memcpy (op, ref, len);
						op += len;
					}
					else
					{
						/* overlapping, use octet by octet copying */
						do
							*op++ = *ref++;
						while (--len);
					}
				break;

				case 9: *op++ = *ref++;
				case 8: *op++ = *ref++;
				case 7: *op++ = *ref++;
				case 6: *op++ = *ref++;
				case 5: *op++ = *ref++;
				case 4: *op++ = *ref++;
				case 3: *op++ = *ref++;
				case 2: *op++ = *ref++;
				case 1: *op++ = *ref++;
				case 0: *op++ = *ref++; /* two octets more */
						*op++ = *ref++;
			}
#endif
		}
	}
	while (ip < in_end);

	return op - (u8 *)out_data;
}


 /*
 * lzf_d.c ends here. lzf_c.c follows.
 */

#define HSIZE (1 << (HLOG))

/*
 * don't play with this unless you benchmark!
 * the data format is not dependent on the hash function.
 * the hash function might seem strange, just believe me,
 * it works ;)
 */
#ifndef FRST
# define FRST(p) (((p[0]) << 8) | p[1])
# define NEXT(v,p) (((v) << 8) | p[2])
# if MULTIPLICATION_IS_SLOW
#  if ULTRA_FAST
#   define IDX(h) ((( h             >> (3*8 - HLOG)) - h  ) & (HSIZE - 1))
#  elif VERY_FAST
#   define IDX(h) ((( h             >> (3*8 - HLOG)) - h*5) & (HSIZE - 1))
#  else
#   define IDX(h) ((((h ^ (h << 5)) >> (3*8 - HLOG)) - h*5) & (HSIZE - 1))
#  endif
# else
/* this one was developed with sesse,
 * and is very similar to the one in snappy.
 * it does need a modern enough cpu with a fast multiplication.
 */
#  define IDX(h) (((h * 0x1e35a7bdU) >> (32 - HLOG - 8)) & (HSIZE - 1))
# endif
#endif

#if 0
/* original lzv-like hash function, much worse and thus slower */
# define FRST(p) (p[0] << 5) ^ p[1]
# define NEXT(v,p) ((v) << 5) ^ p[2]
# define IDX(h) ((h) & (HSIZE - 1))
#endif

#if __GNUC__ >= 3
# define expect(expr,value)         __builtin_expect ((expr),(value))
# define inline                     inline
#else
# define expect(expr,value)         (expr)
# define inline                     static
#endif

#define expect_false(expr) expect ((expr) != 0, 0)
#define expect_true(expr)  expect ((expr) != 0, 1)

/*
 * compressed format
 *
 * 000LLLLL <L+1>    ; literal, L+1=1..33 octets
 * LLLooooo oooooooo ; backref L+1=1..7 octets, o+1=1..4096 offset
 * 111ooooo LLLLLLLL oooooooo ; backref L+8 octets, o+1=1..4096 offset
 *
 */

size_t
lzf_compress (const void *const in_data, size_t in_len,
			  void 			 *out_data, size_t out_len
#if LZF_STATE_ARG
			  , LZF_STATE htab
#endif
			  )
{
#if !LZF_STATE_ARG
	LZF_STATE htab;
#endif
	const u8 *ip = (const u8 *)in_data;
		  u8 *op = (u8 *)out_data;
	const u8 *in_end  = ip + in_len;
		  u8 *out_end = op + out_len;
	const u8 *ref;

	/*
	* off requires a type wide enough to hold a general pointer difference.
	* ISO C doesn't have that (size_t might not be enough and ptrdiff_t only
	* works for differences within a single object). We also assume that
	* no bit pattern traps. Since the only platform that is both non-POSIX
	* and fails to support both assumptions is windows 64 bit, we make a
	* special workaround for it.
	*/
#if defined (_WIN32) && defined (_M_X64)
	/* workaround for missing POSIX compliance */
#if __GNUC__
	unsigned long long off;
#else
	unsigned __int64 off;
#endif
#else
	unsigned long off;
#endif
	unsigned int hval;
	int lit;

	if (!in_len || !out_len)
		return 0;

#if INIT_HTAB
	memset (htab, 0, sizeof (htab));
#endif

	lit = 0; op++; /* start run */

	hval = FRST (ip);
	while (ip < in_end - 2)
	{
		LZF_HSLOT *hslot;

		hval = NEXT (hval, ip);
		hslot = htab + IDX (hval);
		ref = *hslot + LZF_HSLOT_BIAS; *hslot = ip - LZF_HSLOT_BIAS;

		if (1
#if INIT_HTAB
			&& ref < ip /* the next test will actually take care of this, but this is faster */
#endif
			&& (off = ip - ref - 1) < LZF_MAX_OFF
			&& ref > (const u8 *)in_data
			&& ref[2] == ip[2]
#if STRICT_ALIGN
			&& ((ref[1] << 8) | ref[0]) == ((ip[1] << 8) | ip[0])
#else
			&& *(const u16 *)ref == *(const u16 *)ip
#endif
		)
		{
			/* match found at *ref++ */
			unsigned int len = 2;
			unsigned int maxlen = in_end - ip - len;
			maxlen = maxlen > LZF_MAX_REF ? LZF_MAX_REF : maxlen;

			if (expect_false (op + 3 + 1 >= out_end)) /* first a faster conservative test */
				if (op - !lit + 3 + 1 >= out_end) /* second the exact but rare test */
					return 0;

			op [- lit - 1] = lit - 1; /* stop run */
			op -= !lit; /* undo run if length is zero */

			for (;;)
			{
				if (expect_true (maxlen > 16))
				{
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;

					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;

					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;

					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
					len++; if (ref [len] != ip [len]) break;
				}

				do
					len++;
				while (len < maxlen && ref[len] == ip[len]);

				break;
			}

			len -= 2; /* len is now #octets - 1 */
			ip++;

			if (len < 7)
			{
				*op++ = (off >> 8) + (len << 5);
			}
			else
			{
				*op++ = (off >> 8) + (  7 << 5);
				*op++ = len - 7;
			}

			*op++ = off;

			lit = 0; op++; /* start run */

			ip += len + 1;

			if (expect_false (ip >= in_end - 2))
				break;

#if ULTRA_FAST || VERY_FAST
			--ip;
# if VERY_FAST && !ULTRA_FAST
			--ip;
# endif
			hval = FRST (ip);

			hval = NEXT (hval, ip);
			htab[IDX (hval)] = ip - LZF_HSLOT_BIAS;
			ip++;

# if VERY_FAST && !ULTRA_FAST
			hval = NEXT (hval, ip);
			htab[IDX (hval)] = ip - LZF_HSLOT_BIAS;
			ip++;
# endif
#else
			ip -= len + 1;

			do
			{
				hval = NEXT (hval, ip);
				htab[IDX (hval)] = ip - LZF_HSLOT_BIAS;
				ip++;
			}
			while (len--);
#endif
		}
		else
		{
			/* one more literal byte we must copy */
			if (expect_false (op >= out_end))
				return 0;

			lit++; *op++ = *ip++;

			if (expect_false (lit == LZF_MAX_LIT))
			{
				op [- lit - 1] = lit - 1; /* stop run */
				lit = 0; op++; /* start run */
			}
		}
	}

	if (op + 3 > out_end) /* at most 3 bytes can be missing here */
		return 0;

	while (ip < in_end)
	{
		lit++; *op++ = *ip++;

		if (expect_false (lit == LZF_MAX_LIT))
		{
			op [- lit - 1] = lit - 1; /* stop run */
			lit = 0; op++; /* start run */
		}
	}

	op [- lit - 1] = lit - 1; /* end run */
	op -= !lit; /* undo run if length is zero */

	return op - (u8 *)out_data;
}
