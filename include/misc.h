/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2000, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                           Ken Stevens, Steve McClure
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  ---
 *
 *  See the "LEGAL", "LICENSE", "CREDITS" and "README" files for all the
 *  related information and legal notices. It is expected that any future
 *  projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  misc.h: Misc. definitions which don't really belong anywhere,
 *          but have to go somewhere!
 * 
 *  Known contributors to this file:
 *       Doug Hay, 1998
 */

#ifndef _MISC_H_
#define _MISC_H_

#include <ctype.h>
#include <string.h>
#include <sys/types.h>

#if defined(_WIN32)
typedef unsigned char u_char;
typedef unsigned short u_short;
typedef char *caddr_t;
typedef unsigned int u_int;

/* integral mismatch, due to misuse of sector short */
#pragma warning (disable : 4761 )

#include <minmax.h>
#include <io.h>

#define random rand
#define srandom srand

/*#define _POSIX_ */
#endif

#include "options.h"

/* This is the structure we use to keep track of the global mobility
   things, such as when the server is supposed to start/stop updating
   mobility and the timestamp of when the game was last up.  These
   times are in seconds. */
struct mob_acc_globals {
    time_t timestamp;		/* Last timestamp of when the game was up */
    time_t starttime;		/* When we should start updating mobility again. */
};

#define ANNO_KEEP_DAYS 7	/* Keep announcements around for this many days */
/* This uses a lot of thread stack with some versions of GNU libc,
   which can lead to nasty heap smashes (observed with 2.2.93).
   Disabled for now, until we readjust thread stack sizes.  */
#if 0
#define RESOLVE_IPADDRESS	/* resolve ip addresses into hostnames */
#endif

typedef u_char natid;		/* NSC_NATID must match this */

#if defined(__linux__ ) || defined(__osf__)
#define POSIXSIGNALS 1
#endif

#if !defined (_EMPTH_LWP) && !defined (_EMPTH_POSIX)
#define _EMPTH_LWP     1
#endif

#if defined(_EMPTH_POSIX) && !defined(POSIXSIGNALS)
#define POSIXSIGNALS
#endif


#if !defined(aix) && !defined(sgi)
#ifndef ultrix			/* already defined in ultrix */
typedef char s_char;		/* change to signed char for aix */
#endif /* ultrix */
#else
typedef signed char s_char;
#endif /* !aix && !sgi */
typedef short coord;
/*
 * watch it; your compiler might not like this.
 * If you think this is bogus, look at /usr/include/struct.h
 */
#define	OFFSET(stype, oset) ((int)(&(((struct stype *)0)->oset)))
#define	SETOFF(sinst, oset) ((s_char *) (sinst) + (oset))

#ifndef bit
#define	bit(x)		(1<<(x))
#endif

#define minutes(x)	(60*(x))
#define hours(x)	(60*60*(x))
#define days(x)		(60*60*24*(x))

typedef int (*qsort_func_t) (const void *, const void *);

	/* return codes from command routines */
#define	RET_OK		0	/* command completed sucessfully */
#define	RET_FAIL	1	/* command completed unsucessfully [?] */
#define	RET_SYN		2	/* syntax error in command */
#define	RET_SYS		3	/* system error (missing file, etc) */

double dmax(double n1, double n2);
double dmin(double n1, double n2);

extern s_char *numstr(s_char buf[], int n);
extern s_char *esplur(int n);
extern s_char *splur(int n);
extern s_char *iesplur(int n);
extern s_char *plur(int n, s_char *no, s_char *yes);
extern s_char *getstarg(s_char *input, s_char *prompt, s_char buf[]);
extern s_char *getstring(s_char *prompt, s_char buf[]);
extern s_char *prbuf(s_char *format, ...);

#define MAXCHRNV 12

#include "prototypes.h"		/* must come at end, after defines and typedefs */

#endif /* _MISC_H_ */
