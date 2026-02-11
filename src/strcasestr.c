/*
strcasestr -- case insensitive substring searching function.
*/
/*
Copyright 2019 James R.
All rights reserved.

Redistribution and use in source forms, with or without modification, is
permitted provided that the following condition is met:

1. Redistributions of source code must retain the above copyright notice, this
   condition and the following disclaimer.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string.h>
#include <ctype.h>
#include <stddef.h>

static inline int
trycmp (const char **pp, const char *cp,
		const char *q, size_t qn)
{
	const char *p;
	p = (*pp);
	if (strncasecmp(p, q, qn) == 0)
		return 0;
	(*pp) = strchr(&p[1], (*cp));
	return 1;
}

static inline void
swapp (const char ***ppap, const char ***ppbp, const char **cpap, const char **cpbp)
{
	const char **pp;
	const char  *p;

	pp    = *ppap;
	*ppap = *ppbp;
	*ppbp =  pp;

	p     = *cpap;
	*cpap = *cpbp;
	*cpbp =   p;
}

const char *nongnu_strcasestr(const char *in, const char *what)
{
	size_t  qn;

	char    uc;
	char    lc;

	const char   *up;
	const char   *lp;

	const char **ppa;
	const char **ppb;

	const char  *cpa;
	const char  *cpb;

	uc = toupper(*what);
	lc = tolower(*what);

	up = strchr(in, uc);
	lp = strchr(in, lc);

	if (up == NULL && lp == NULL)
		return NULL;

	if (!lp || ( up && up < lp ))
	{
		ppa = &up;
		ppb = &lp;

		cpa = &uc;
		cpb = &lc;
	}
	else
	{
		ppa = &lp;
		ppb = &up;

		cpa = &lc;
		cpb = &uc;
	}

	qn = strlen(what);

	for (;;)
	{
		if (trycmp(ppa, cpa, what, qn) == 0)
			return (*ppa);

		if (up == NULL && lp == NULL)
			break;

		if (!(*ppa) || ( (*ppb) && (*ppb) < (*ppa) ))
			swapp(&ppa, &ppb, &cpa, &cpb);
	}

	return NULL;
}
