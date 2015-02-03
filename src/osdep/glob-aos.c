/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * This code is MKS code ported to Solaris originally with minimum
 * modifications so that upgrades from MKS would readily integrate.
 * The MKS basis for this modification was:
 *
 *	$Id: glob.c 1.31 1994/04/07 22:50:43 mark
 *
 * Additional modifications have been made to this code to make it
 * 64-bit clean.
 */

/*
 * glob, globfree -- POSIX.2 compatible file name expansion routines.
 *
 * Copyright 1985, 1991 by Mortice Kern Systems Inc.  All rights reserved.
 *
 * Written by Eric Gisin.
 */

#include <stdio.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "glob-aos.h"
#include <errno.h>

#define	GLOB__CHECK	0x80	/* stat generated paths */

#define	INITIAL	8		/* initial pathv allocation */
#define	NULLCPP	((char **)0)	/* Null char ** */

static int	globit(size_t, const char *, glob_t *, int,
	int (*)(const char *, int), char **);
static int	pstrcmp(const void *, const void *);
static int	append(glob_t *, const char *);

/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/*
 * Function fnmatch() as proposed in Posix 1003.2 B.6 (rev. 9).
 * Compares a filename or pathname to a pattern.
 */

#include <string.h>
#include <ctype.h>

/* fnmatch function */

#define FNM_NOESCAPE    0x01    /* Disable backslash escaping. */
#define FNM_PATHNAME    0x02    /* Slash must be matched by slash. */
#define FNM_PERIOD      0x04    /* Period must be matched by period. */
#define FNM_LEADING_DIR 0x08    /* Ignore /<tail> after Imatch. */
#define FNM_CASEFOLD    0x10    /* Case insensitive search. */
#define FNM_IGNORECASE  FNM_CASEFOLD
#define FNM_FILE_NAME   FNM_PATHNAME
#define	FNM_NOCASE		FNM_IGNORECASE		/* case insensitive match */
#define	FNM_QUOTE		FNM_PATHNAME		/* escape special chars with \ */
#define FNM_NOMATCH 	1		/* Match failed. */

#define	EOS	'\0'

static char *
rangematch(register char *pattern, register char test)
{
	register char c,
	  c2;
	int negate,
	  ok;

	if ( (negate = (*pattern == '!')) )
		++pattern;

	/* TO DO: quoting */

	for (ok = 0; (c = *pattern++) != ']';) {
		if (c == EOS)
			return (NULL);		/* illegal pattern */
		if (*pattern == '-' && (c2 = pattern[1]) != EOS && c2 != ']') {
			if (c <= test && test <= c2)
				ok = 1;
			pattern += 2;
		} else if (c == test)
			ok = 1;
	}
	return (ok == negate ? NULL : pattern);
}

int fnmatch(const char *pattern, char *string, int flags)
{
	register const char *p = pattern, *n = string;
	register unsigned char c;

#define FOLD(c)	((flags & FNM_CASEFOLD) ? tolower(c) : (c))

	while ((c = *p++) != '\0')
	{
		c = FOLD (c);

		switch (c)
		{
			case '?':
				if (*n == '\0')
					return FNM_NOMATCH;
				else if ((flags & FNM_FILE_NAME) && *n == '/')
					return FNM_NOMATCH;
				else if ((flags & FNM_PERIOD) && *n == '.' &&
					(n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
					return FNM_NOMATCH;
			break;
			case '\\':
				if (!(flags & FNM_NOESCAPE))
				{
					c = *p++;
					c = FOLD (c);
				}
				if (FOLD ((unsigned char)*n) != c)
					return FNM_NOMATCH;
			break;

			case '*':
				if ((flags & FNM_PERIOD) && *n == '.' &&
						(n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
					return FNM_NOMATCH;

				for (c = *p++; c == '?' || c == '*'; c = *p++, ++n)
					if (((flags & FNM_FILE_NAME) && *n == '/') ||
						(c == '?' && *n == '\0'))
					return FNM_NOMATCH;

				if (c == '\0')
					return 0;

				{
					unsigned char c1 = (!(flags & FNM_NOESCAPE) && c == '\\') ? *p : c;
					c1 = FOLD (c1);
					for (--p; *n != '\0'; ++n)
						if ((c == '[' || FOLD ((unsigned char)*n) == c1) &&
							fnmatch (p, (char *)n, flags & ~FNM_PERIOD) == 0)
						return 0;
					return FNM_NOMATCH;
				}

			case '[':
			{
				/* Nonzero if the sense of the character class is inverted.  */
				register int _not;

				if (*n == '\0')
					return FNM_NOMATCH;

				if ((flags & FNM_PERIOD) && *n == '.' &&
					(n == string || ((flags & FNM_FILE_NAME) && n[-1] == '/')))
				return FNM_NOMATCH;

				_not = (*p == '!' || *p == '^');
				if (_not)
					++p;

				c = *p++;
				for (;;)
				{
					register unsigned char cstart = c, cend = c;

					if (!(flags & FNM_NOESCAPE) && c == '\\')
						cstart = cend = *p++;

					cstart = cend = FOLD (cstart);

					if (c == '\0')
						/* [ (unterminated) loses.  */
						return FNM_NOMATCH;

					c = *p++;
					c = FOLD (c);

					if ((flags & FNM_FILE_NAME) && c == '/')
						/* [/] can never match.  */
						return FNM_NOMATCH;

					if (c == '-' && *p != ']')
					{
						cend = *p++;
						if (!(flags & FNM_NOESCAPE) && cend == '\\')
							cend = *p++;
						if (cend == '\0')
							return FNM_NOMATCH;
						cend = FOLD (cend);

						c = *p++;
					}

					if (FOLD ((unsigned char)*n) >= cstart
						&& FOLD ((unsigned char)*n) <= cend)
					goto matched;

					if (c == ']')
						break;
				}
				if (!_not)
					return FNM_NOMATCH;
			break;

matched:;
				/* Skip the rest of the [...] that already matched.  */
			while (c != ']')
			{
				if (c == '\0')
				/* [... (unterminated) loses.  */
					return FNM_NOMATCH;

				c = *p++;
				if (!(flags & FNM_NOESCAPE) && c == '\\')
				/* XXX 1003.2d11 is unclear if this is right.  */
					++p;
			}
			if (_not)
				return FNM_NOMATCH;
		}
		break;

		default:
		if (c != FOLD ((unsigned char)*n))
			return FNM_NOMATCH;
	}

	++n;
	}

	if (*n == '\0')
	return 0;

	if ((flags & FNM_LEADING_DIR) && *n == '/')
	/* The FNM_LEADING_DIR flag says that "foo*" matches "foobar/frobozz".  */
	return 0;

	return FNM_NOMATCH;
}

/*
 * Free all space consumed by glob.
 */
void
globfree(glob_t *gp)
{
	size_t i;

	if (gp->gl_pathv == 0)
		return;

	for (i = gp->gl_offs; i < gp->gl_offs + gp->gl_pathc; ++i)
		free(gp->gl_pathv[i]);
	free((void *)gp->gl_pathv);

	gp->gl_pathc = 0;
	gp->gl_pathv = NULLCPP;
}

/*
 * Do filename expansion.
 */
int
glob(const char *pattern, int flags,
	int (*errfn)(const char *, int), glob_t *gp)
{
	int rv;
	size_t i;
	size_t ipathc;
	char	*path;

	if ((flags & GLOB_DOOFFS) == 0)
		gp->gl_offs = 0;

	if (!(flags & GLOB_APPEND)) {
		gp->gl_pathc = 0;
		gp->gl_pathn = gp->gl_offs + INITIAL;
		gp->gl_pathv = (char **)malloc(sizeof (char *) * gp->gl_pathn);

		if (gp->gl_pathv == NULLCPP)
			return (GLOB_NOSPACE);
		gp->gl_pathp = gp->gl_pathv + gp->gl_offs;

		for (i = 0; i < gp->gl_offs; ++i)
			gp->gl_pathv[i] = NULL;
	}

	if ((path = (char *)malloc(strlen(pattern)+1)) == NULL)
		return (GLOB_NOSPACE);

	ipathc = gp->gl_pathc;
	rv = globit(0, pattern, gp, flags, errfn, &path);

	if (rv == GLOB_ABORTED) {
		/*
		 * User's error function returned non-zero, or GLOB_ERR was
		 * set, and we encountered a directory we couldn't search.
		 */
		free(path);
		return (GLOB_ABORTED);
	}

	i = gp->gl_pathc - ipathc;
	if (i >= 1 && !(flags & GLOB_NOSORT)) {
		qsort((char *)(gp->gl_pathp+ipathc), i, sizeof (char *),
			pstrcmp);
	}
	if (i == 0) {
		if (flags & GLOB_NOCHECK)
			(void) append(gp, pattern);
		else
			rv = GLOB_NOMATCH;
	}
	gp->gl_pathp[gp->gl_pathc] = NULL;
	free(path);

	return (rv);
}


/*
 * Recursive routine to match glob pattern, and walk directories.
 */
int
globit(size_t dend, const char *sp, glob_t *gp, int flags,
	int (*errfn)(const char *, int), char **path)
{
	size_t n;
	size_t m;
	ssize_t end = 0;	/* end of expanded directory */
	char *pat = (char *)sp;	/* pattern component */
	char *dp = (*path) + dend;
	int expand = 0;		/* path has pattern */
	char *cp;
	struct stat sb;
	DIR *dirp;
	struct dirent *d;
	struct dirent *entry = NULL;
	int namemax;
	int err;

#define	FREE_ENTRY	if (entry) free(entry)

	for (;;)
	{
		memset(&sb,0x0,sizeof(struct stat));

		switch (*dp++ = *(unsigned char *)sp++) {
		case '\0':	/* end of source path */
			if (expand)
				goto Expand;
			else {
					if (stat(*path, &sb) < 0) {
						FREE_ENTRY;
						return (0);
					}
				if (S_ISDIR(sb.st_mode)) {
					*dp = '\0';
					*--dp = '/';
				}
				if (append(gp, *path) < 0) {
					FREE_ENTRY;
					return (GLOB_NOSPACE);
				}
				return (0);
			}
			/*NOTREACHED*/

		case '*':
		case '?':
		case '[':
		case '\\':
			++expand;
			break;

		case '/':
			if (expand)
				goto Expand;
			end = dp - *path;
			pat = (char *)sp;
			break;

		Expand:
			/* determine directory and open it */
			(*path)[end] = '\0';
#ifdef NAME_MAX
			namemax = NAME_MAX;
#else
			namemax = 1024;  /* something large */
#endif
			if ((entry = (struct dirent *)malloc(
				sizeof (struct dirent) + namemax + 1))
				== NULL) {
				return (GLOB_NOSPACE);
			}

			dirp = opendir(**path == '\0' ? "." : *path);
			if (dirp == (DIR *)0 || namemax == -1) {
				if (errfn != 0 && errfn(*path, errno) != 0 ||
				    flags&GLOB_ERR) {
					FREE_ENTRY;
					return (GLOB_ABORTED);
				}
				FREE_ENTRY;
				return (0);
			}

			/* extract pattern component */
			n = sp - pat;
			if ((cp = (char *)malloc(n)) == NULL) {
				(void) closedir(dirp);
				FREE_ENTRY;
				return (GLOB_NOSPACE);
			}
			pat = (char *)memcpy(cp, pat, n);
			pat[n-1] = '\0';
			if (*--sp != '\0')
				flags |= GLOB__CHECK;

			/* expand path to max. expansion */
			n = dp - *path;
			*path = (char *)realloc(*path,
				strlen(*path)+namemax+strlen(sp)+1);
			if (*path == NULL) {
				(void) closedir(dirp);
				free(pat);
				FREE_ENTRY;
				return (GLOB_NOSPACE);
			}
			dp = (*path) + n;

			/* read directory and match entries */
			err = 0;
			while ((d=readdir(dirp)) != NULL) {
				cp = d->d_name;
				if ((flags&GLOB_NOESCAPE)
				    ? fnmatch(pat, cp, FNM_PERIOD|FNM_NOESCAPE)
				    : fnmatch(pat, cp, FNM_PERIOD))
					continue;

				n = strlen(cp);
				(void) memcpy((*path) + end, cp, n);
				m = dp - *path;
				err = globit(end+n, sp, gp, flags, errfn, path);
				dp = (*path) + m;   /* globit can move path */
				if (err != 0)
					break;
			}

			(void) closedir(dirp);
			free(pat);
			FREE_ENTRY;
			return (err);
		}
		/* NOTREACHED */
	}
}

/*
 * Comparison routine for two name arguments, called by qsort.
 */
int
pstrcmp(const void *npp1, const void *npp2)
{
	return (strcoll(*(char **)npp1, *(char **)npp2));
}

/*
 * Add a new matched filename to the glob_t structure, increasing the
 * size of that array, as required.
 */
int
append(glob_t *gp, const char *str)
{
	char *cp;

	if ((cp = (char *)malloc(strlen(str)+1)) == NULL)
		return (GLOB_NOSPACE);
	gp->gl_pathp[gp->gl_pathc++] = strcpy(cp, str);

	if ((gp->gl_pathc + gp->gl_offs) >= gp->gl_pathn) {
		gp->gl_pathn *= 2;
		gp->gl_pathv = (char **)realloc((void *)gp->gl_pathv,
						gp->gl_pathn * sizeof (char *));
		if (gp->gl_pathv == NULLCPP)
			return (GLOB_NOSPACE);
		gp->gl_pathp = gp->gl_pathv + gp->gl_offs;
	}
	return (0);
}
