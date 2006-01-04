/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2005, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  nat.h: Definitions for things having to do with nations
 * 
 *  Known contributors to this file:
 *     Thomas Ruschak
 *     Ken Stevens, 1995
 *     Steve McClure, 1998-2000
 */

#ifndef NAT_H
#define NAT_H

#include "sect.h"

#define	MAXNOR		50	/* max # realms */

enum {				/* Priorities */
    /* sector types are also priorities */
    PRI_SMAINT = SCT_MAXDEF+1,	/* ship maintenance */
    PRI_PMAINT,			/* plane maintenance */
    PRI_LMAINT,			/* land unit maintenance */
    PRI_SBUILD,			/* ship building */
    PRI_PBUILD,			/* plane building */
    PRI_LBUILD,			/* land building */
    PRI_MAX = PRI_LBUILD
};

struct boundstr {
    short b_xl, b_xh;		/* horizontal bounds */
    short b_yl, b_yh;		/* vertical bounds */
};

struct natstr {
    /* initial part must match struct genitem */
    short ef_type;
    natid nat_cnum;		/* our country number */
    /* end of part matching struct genitem */
    s_char nat_stat;		/* inuse, norm, god, abs */
    char nat_cnam[20];		/* country name */
    char nat_pnam[20];		/* representative */
    char nat_hostaddr[32];	/* host addr of last user */
    char nat_hostname[512];	/* hostname of last user, may be empty */
    char nat_userid[32];	/* userid of last user, may be empty */
    coord nat_xcap, nat_ycap;	/* cap location in abs coords */
    coord nat_xorg, nat_yorg;	/* origin location in abs coords */
    s_char nat_dayno;		/* day of the year mod 128 */
    s_char nat_update;		/* Want an update or not. */
    u_char nat_missed;		/* How many updates missed */
    u_short nat_tgms;		/* # of telegrams to be announced */
    u_short nat_ann;		/* # of annos pending */
    u_short nat_minused;	/* number of minutes used today */
    short nat_btu;		/* bureaucratic time units */
    long nat_reserve;		/* military reserves */
    long nat_money;		/* moola */
    time_t nat_last_login;	/* time of last login, 0 menas never */
    time_t nat_last_logout;	/* time of last logout, 0 means never */
    time_t nat_newstim;		/* date news last read */
    time_t nat_annotim;		/* date annos last read */
    float nat_level[4];		/* technology, etc */
    struct boundstr nat_b[MAXNOR];	/* realm bounds */
    short nat_relate[MAXNOC];
    unsigned char nat_contact[MAXNOC];
    short nat_rejects[(MAXNOC + 3) / 4]; /* four bits for each country */
    s_char nat_priorities[PRI_MAX+1]; /* budget priority */
    long nat_flags;		/* nation flags */
    char nat_spare[15];
};

	/* nation status types */
#define STAT_INUSE	bit(0)	/* cnum in use */
#define STAT_SANCT	bit(1)	/* country in sanctuary */
#define STAT_NORM	bit(2)	/* normal country */
#define STAT_GOD	bit(3)	/* deity powers */
#define STAT_NEW	bit(5)	/* just initialized */

	/* Update fields. */
#define	WUPD_WANT	bit(0)

	/* nstat values */
#define VIS		STAT_INUSE
#define	NORM		(STAT_INUSE|STAT_NORM)
#define	GOD		(STAT_INUSE|STAT_GOD)
#define	SANCT		(STAT_INUSE|STAT_SANCT)
#define	CAP		bit(6)
#define	MONEY		bit(7)

	/* nation relation codes */
#define	AT_WAR		0
#define SITZKRIEG       1
#define MOBILIZATION    2
#define	HOSTILE		3
#define	NEUTRAL		4
#define FRIENDLY        5
#define	ALLIED		6

	/* nation reject codes */
#define	REJ_TELE	bit(0)	/* dont allow telegrams to be sent */
#define	REJ_TREA	bit(1)	/* dont allow treaties to be offered */
#define	REJ_ANNO	bit(2)	/* don't receive announcements */
#define	REJ_LOAN	bit(3)	/* don't allow loans to be offered */

#define	NAT_TLEV	0
#define	NAT_RLEV	1
#define	NAT_ELEV	2
#define	NAT_HLEV	3

/*
 * Number of updates contact lasts for various ways of making contact.
 * These are only useful with option LOSE_CONTACT option, which
 * implies option HIDDEN.
 */
/* Planes spotting and being spotted */
#define FOUND_FLY	3
/* Lookout */
#define FOUND_LOOK	5
/* Spies and ground combat */
#define FOUND_SPY	6
/* Communication */
#define FOUND_TELE	3
/* Coastwatch and skywatch */
#define FOUND_COAST	3

extern s_char *relates[];

/* procedures relating to nation stuff */

#define putnat(n) \
	ef_write(EF_NATION, n->nat_cnum, n)
#define getnatp(n) \
	(struct natstr *) ef_ptr(EF_NATION, (int)n)

extern double tfact(natid cn, double mult);
extern double tfactfire(natid cn, double mult);
extern double techfact(int level, double mult);

extern s_char *cname(natid n);
extern s_char *relatename(struct natstr *np, natid other);
extern s_char *rejectname(struct natstr *np, natid other);
extern s_char *natstate(struct natstr *np);
extern int getrel(struct natstr *np, natid them);
extern int getrejects(natid them, struct natstr *np);
extern int getcontact(struct natstr *np, natid them);
extern void putrel(struct natstr *np, natid them, int relate);
extern void putreject(struct natstr *np, natid them, int how, int what);
extern void putcontact(struct natstr *np, natid them, int contact);
extern void agecontact(struct natstr *np);
extern int influx(struct natstr *np);

/* nation flags */
#define NF_INFORM    	bit(0)	/* Inform me of telegrams right away */
#define NF_FLASH     	bit(1)	/* Allow other players to flash me (sicko :) */
#define NF_BEEP      	bit(2)	/* Make beeping sounds when appropriate */
#define NF_COASTWATCH	bit(3)	/* Turn auto-coastwatch on */
#define NF_SONAR	bit(4)	/* Turn auto-sonar on */
#define NF_TECHLISTS    bit(5)	/* Sort lists by tech not type */
#define NF_SACKED       bit(6)	/* Capital was sacked, and hasn't been reset yet */

#endif
