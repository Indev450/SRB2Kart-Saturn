// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1995-2025 by the Free Software Foundation, Inc.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  md5.h
/// \brief Functions to compute MD5 message digest of files or memory blocks
///        according to the definition of MD5 in RFC 1321 from April 1992

/* NOTE: The canonical source of this file is maintained with the GNU C
   Library.  Bugs can be reported to bug-glibc@prep.ai.mit.edu. */

#ifndef _MD5_H
#define _MD5_H 1

#include <stdio.h>

#if defined (HAVE_LIMITS_H) || (defined (_LIBC) && _LIBC) || defined (_WIN32)
# include <limits.h>
#endif

#define MD5_LEN 16

/*	The following contortions are an attempt to use the C preprocessor
	to determine an unsigned integral type that is 32 bits wide.  An
	alternative approach is to use autoconf's AC_CHECK_SIZEOF macro, but
	doing that would require that the configure script compile and *run*
	the resulting executable.  Locally running cross-compiled executables
	is usually not possible.  */

#include <stdint.h>
typedef uint32_t md5_uint32;
typedef uintptr_t md5_uintptr;

/*	Compute MD5 message digest for bytes read from STREAM.  The
	resulting message digest number will be written into the 16 bytes
	beginning at RESBLOCK.  */
int md5_stream(FILE *stream, void *resblock) __THROW;

/*	Compute MD5 message digest for LEN bytes beginning at BUFFER.  The
	result is always in little endian byte order, so that a byte-wise
	output yields to the wanted ASCII representation of the message
	digest.  */
extern void *md5_buffer(const char *buffer, size_t len, void *resblock) __THROW;

#endif /* md5.h */
