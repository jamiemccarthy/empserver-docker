/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2008, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  laun.c: Launch missiles from land or sea
 * 
 *  Known contributors to this file:
 *     Dave Pare, 1986
 *     Ken Stevens, 1995
 *     Steve McClure, 1998-2000
 */

#include <config.h>

#include "commands.h"
#include "damage.h"
#include "mission.h"
#include "news.h"
#include "optlist.h"
#include "path.h"
#include "plane.h"
#include "ship.h"

static int launch_as(struct plnstr *pp);
static int launch_missile(struct plnstr *pp, int sublaunch);
static int launch_sat(struct plnstr *pp, int sublaunch);

/*
 * laun <PLANES>
 */
int
laun(void)
{
    struct nstr_item nstr;
    struct plnstr plane;
    struct shpstr ship;
    struct sctstr sect;
    int sublaunch;
    struct plchrstr *pcp;
    int rel, retval, gone;
    struct natstr *natp;

    if (!snxtitem(&nstr, EF_PLANE, player->argp[1]))
	return RET_SYN;
    while (nxtitem(&nstr, &plane)) {
	if (plane.pln_own != player->cnum)
	    continue;
	pcp = &plchr[(int)plane.pln_type];
	if ((pcp->pl_flags & (P_M | P_O)) == 0) {
	    pr("%s isn't a missile!\n", prplane(&plane));
	    continue;
	}
	if (pcp->pl_flags & P_F) {
	    pr("%s is a surface-to-air missile!\n", prplane(&plane));
	    continue;
	}
	if (pcp->pl_flags & P_N) {
	    pr("%s is an anti-ballistic-missile missile!\n",
	       prplane(&plane));
	    continue;
	}
	if (pln_is_in_orbit(&plane)) {
	    pr("%s already in orbit!\n", prplane(&plane));
	    continue;
	}
	if (opt_MARKET) {
	    if (ontradingblock(EF_PLANE, &plane)) {
		pr("plane #%d inelligible - it's for sale.\n",
		   plane.pln_uid);
		continue;
	    }
	}

	sublaunch = 0;
	if (plane.pln_ship >= 0) {
	    getship(plane.pln_ship, &ship);
	    if (!ship.shp_own) {
		pr("%s: ship #%d was sunk!\n",
		   prplane(&plane), ship.shp_uid);
		plane.pln_effic = 0;
		putplane(plane.pln_uid, &plane);
		continue;
	    }
	    natp = getnatp(ship.shp_own);
	    rel = getrel(natp, player->cnum);
	    if (ship.shp_own != player->cnum && rel != ALLIED) {
		pr("%s: you or an ally do not own ship #%d\n",
		   prplane(&plane), ship.shp_uid);
		continue;
	    }
	    if (mchr[(int)ship.shp_type].m_flags & M_SUB)
		sublaunch = 1;
	} else {
	    sublaunch = 0;
	    getsect(plane.pln_x, plane.pln_y, &sect);
	    natp = getnatp(sect.sct_own);
	    rel = getrel(natp, player->cnum);
	    if (sect.sct_own && sect.sct_own != player->cnum
		&& rel != ALLIED) {
		pr("%s: you or an ally do not own sector %s!\n",
		   prplane(&plane), xyas(plane.pln_x, plane.pln_y,
					 player->cnum));
		continue;
	    }
	}
	if (plane.pln_effic < 60) {
	    pr("%s is damaged (%d%%)\n", prplane(&plane), plane.pln_effic);
	    continue;
	}
	pr("%s at %s; range %d, eff %d%%\n", prplane(&plane),
	   xyas(plane.pln_x, plane.pln_y, player->cnum),
	   plane.pln_range, plane.pln_effic);
	if (!(pcp->pl_flags & P_O)) {
	    retval = launch_missile(&plane, sublaunch);
	    gone = 1;
	} else if ((pcp->pl_flags & (P_M | P_O)) == (P_M | P_O)) {
	    retval = launch_as(&plane);
	    gone = 1;
	} else {		/* satellites */
	    retval = launch_sat(&plane, sublaunch);
	    gone = !(plane.pln_flags & PLN_LAUNCHED);
	}
	if (retval != RET_OK)
	    return retval;
	if (gone) {
	    plane.pln_effic = 0;
	    putplane(plane.pln_uid, &plane);
	}
    }
    return RET_OK;
}

/*
 * Launch anti-sat weapon PP.
 * Return RET_OK if launched (even when missile explodes),
 * else RET_SYN or RET_FAIL.
 */
static int
launch_as(struct plnstr *pp)
{
    coord sx, sy;
    char *cp, buf[1024];
    struct plnstr plane;
    struct nstr_item ni;
    int goodtarget;
    int dam, nukedam;
    natid oldown;

    cp = getstarg(player->argp[2], "Target sector? ", buf);
    if (!check_plane_ok(pp))
	return RET_FAIL;
    if (!cp || !*cp)
	return RET_SYN;
    if (!sarg_xy(cp, &sx, &sy)) {
	pr("Bad sector designation!\n");
	return RET_SYN;
    }
    if (mapdist(pp->pln_x, pp->pln_y, sx, sy) > pp->pln_range) {
	pr("Range too great!\n");
	return RET_FAIL;
    }
    goodtarget = 0;
    snxtitem_dist(&ni, EF_PLANE, sx, sy, 0);
    while (!goodtarget && nxtitem(&ni, &plane)) {
	if (!plane.pln_own)
	    continue;
	if (!pln_is_in_orbit(&plane))
	    continue;
	goodtarget = 1;

    }
    if (!goodtarget) {
	pr("No satellites there!\n");
	return RET_FAIL;
    }
    if (msl_equip(pp, 'p') < 0) {
	pr("%s not enough petrol or shells!\n", prplane(pp));
	return RET_FAIL;
    }
    if (msl_hit(pp, pln_def(&plane), EF_PLANE, N_SAT_KILL, N_SAT_KILL,
		prplane(&plane), sx, sy, plane.pln_own)) {
	dam = pln_damage(pp, sx, sy, 'p', &nukedam, 1);
	oldown = plane.pln_own;
	planedamage(&plane, dam);
	pr("Hit satellite for %d%% damage!\n", dam);
	mpr(oldown, "%s anti-sat did %d%% damage to %s over %s\n",
	    cname(player->cnum), dam, prplane(&plane),
	    xyas(plane.pln_x, plane.pln_y, plane.pln_own));
	putplane(plane.pln_uid, &plane);
	if (!plane.pln_own)
	    mpr(oldown, "Satellite shot down\n");
    }
    return RET_OK;
}

/*
 * Launch missile PP.
 * If SUBLAUNCH, it's sub-launched.
 * Return RET_OK if launched (even when missile explodes),
 * else RET_SYN or RET_FAIL.
 */
static int
launch_missile(struct plnstr *pp, int sublaunch)
{
    struct plchrstr *pcp = plchr + pp->pln_type;
    coord sx, sy;
    int n, dam;
    char *cp;
    struct mchrstr *mcp;
    struct shpstr target_ship;
    struct sctstr sect;
    int nukedam;
    int rel;
    struct natstr *natp;
    char buf[1024];

    if (pcp->pl_flags & P_MAR)
	cp = getstarg(player->argp[2], "Target ship? ", buf);
    else
	cp = getstarg(player->argp[2], "Target sector? ", buf);
    if (!cp || !*cp)
	return RET_SYN;
    if (!check_plane_ok(pp))
	return RET_FAIL;
    if (opt_PINPOINTMISSILE && sarg_type(cp) == NS_LIST) {
	if (!(pcp->pl_flags & P_MAR)) {
	    pr("Missile not designed to attack ships!\n");
	    return RET_FAIL;
	}
	n = atoi(cp);
	if ((n < 0) || !getship(n, &target_ship) ||
	    !target_ship.shp_own) {
	    pr("Bad ship number!\n");
	    return RET_FAIL;
	}
	sx = target_ship.shp_x;
	sy = target_ship.shp_y;
	mcp = &mchr[(int)target_ship.shp_type];
	if (mcp->m_flags & M_SUB) {
	    pr("Bad ship number!\n");
	    return RET_FAIL;
	}
    } /* not PINPOINTMISSILE for ships */
    else if (!sarg_xy(cp, &sx, &sy)) {
	pr("Not a sector!\n");
	return RET_FAIL;
    } else if (opt_PINPOINTMISSILE) {
	if (pcp->pl_flags & P_MAR) {
	    pr("Missile designed to attack ships!\n");
	    return RET_FAIL;
	}
    }
    /* end PINPOINTMISSILE */
    if (mapdist(pp->pln_x, pp->pln_y, sx, sy) > pp->pln_range) {
	pr("Range too great; try again!\n");
	return RET_FAIL;
    }
    if (msl_equip(pp, 'p') < 0) {
	pr("%s not enough shells!\n", prplane(pp));
	return RET_FAIL;
    }
    if (opt_PINPOINTMISSILE == 0 || !(pcp->pl_flags & P_MAR)) {
	getsect(sx, sy, &sect);
	if (opt_SLOW_WAR) {
	    natp = getnatp(player->cnum);
	    rel = getrel(natp, sect.sct_own);
	    if ((rel != AT_WAR) && (sect.sct_own != player->cnum) &&
		(sect.sct_own) && (sect.sct_oldown != player->cnum)) {
		pr("You are not at war with the player->owner of the target sector!\n");
		pr_beep();
		pr("Kaboom!!!\n");
		pr("Missile monitoring officer destroys RV before detonation.\n");
		return RET_OK;
	    }
	}
	if (!msl_hit(pp, SECT_HARDTARGET, EF_SECTOR, N_SCT_MISS,
		     N_SCT_SMISS, "sector", sx, sy, sect.sct_own)) {
	    /*
	       dam = pln_damage(pp, sect.sct_x, sect.sct_y, 's', &nukedam, 0);
	       collateral_damage(sect.sct_x, sect.sct_y, dam, 0);
	     */
	    return RET_OK;
	}
	dam = pln_damage(pp, sect.sct_x, sect.sct_y, 's', &nukedam, 1);
	if (!nukedam) {
	    pr("did %d damage in %s\n", PERCENT_DAMAGE(dam),
	       xyas(sx, sy, player->cnum));
	    if (sect.sct_own != 0) {
		if (sublaunch)
		    wu(0, sect.sct_own,
		       "Sub missile attack did %d damage in %s\n",
		       dam, xyas(sx, sy, sect.sct_own));
		else
		    wu(0, sect.sct_own,
		       "%s missile attack did %d damage in %s\n",
		       cname(player->cnum), dam,
		       xyas(sx, sy, sect.sct_own));
	    }
	    sectdamage(&sect, dam);
	    putsect(&sect);
	}
    } /* end PINPOINTMISSILE conditional */
    else if (opt_PINPOINTMISSILE) {	/* else */
	if (!msl_hit(pp, shp_hardtarget(&target_ship), EF_SHIP,
		     N_SHP_MISS, N_SHP_SMISS, prship(&target_ship),
		     target_ship.shp_x, target_ship.shp_y,
		     target_ship.shp_own)) {
	    pr("splash\n");
	    /*
	       dam = pln_damage(pp,target_ship.shp_x,target_ship.shp_y,'p',&nukedam, 0);
	       collateral_damage(target_ship.shp_x, target_ship.shp_y, dam, 0);
	     */
	    return RET_OK;
	}
	dam =
	    pln_damage(pp, target_ship.shp_x, target_ship.shp_y, 'p',
		       &nukedam, 1);
	if (!nukedam) {
	    check_retreat_and_do_shipdamage(&target_ship, dam);
	    if (target_ship.shp_effic < SHIP_MINEFF)
		pr("\t%s sunk!\n", prship(&target_ship));
	    putship(target_ship.shp_uid, &target_ship);
	}
	getship(target_ship.shp_uid, &target_ship);
	if (!target_ship.shp_own)
	    pr("%s sunk!\n", prship(&target_ship));
    }
    /* end PINPOINTMISSILE */
    return RET_OK;
}

/*
 * Launch a satellite.
 * Return RET_OK if launched (even when satellite fails),
 * else RET_SYN or RET_FAIL.
 */
static int
launch_sat(struct plnstr *pp, int sublaunch)
{
    struct plchrstr *pcp = plchr + pp->pln_type;
    coord sx, sy;
    int i;
    int dist;
    int dir;
    char *cp;
    char *p;
    char buf[1024];

    pr("\n");
    cp = getstarg(player->argp[2], "Target sector? ", buf);
    if (!check_plane_ok(pp))
	return RET_FAIL;
    if (!cp || !*cp)
	return RET_SYN;
    if (!sarg_xy(cp, &sx, &sy)) {
	pr("Bad sector designation!\n");
	return RET_SYN;
    }
    if ((dist = mapdist(pp->pln_x, pp->pln_y, sx, sy)) > pp->pln_range) {
	pr("Range too great; try again!\n");
	return RET_FAIL;
    }
    p = getstring("Geostationary orbit? ", buf);
    if (p == 0)
	return RET_SYN;
    if (!check_plane_ok(pp))
	return RET_FAIL;
    if (msl_equip(pp, 'r') < 0) {
	pr("%s not enough petrol!\n", prplane(pp));
	return RET_FAIL;
    }
    pp->pln_theta = 0;
    pp->pln_flags |= PLN_SYNCHRONOUS;
    if (*p == 0 || *p == 'n')
	pp->pln_flags &= ~(PLN_SYNCHRONOUS);
    pr("3... 2... 1... Blastoff!!!\n");
    if (chance(0.07 + (100 - pp->pln_effic) / 100.0)) {
	pr("KABOOOOM!  Range safety officer detonates booster!\n");
	pp->pln_effic = 0;
	return RET_OK;
    }
    i = pp->pln_tech + pp->pln_effic;
    if (chance(1.0 - (i / (i + 50.0)))) {
	dir = (random() % 6) + 1;
	sx += diroff[dir][0];
	sy += diroff[dir][1];
	pr("Your trajectory was a little off.\n");
    }
    nreport(player->cnum, N_LAUNCH, 0, 1);
    pr("%s positioned over %s", prplane(pp), xyas(sx, sy, player->cnum));
    if (msl_intercept(sx, sy, pp->pln_own, pcp->pl_def, sublaunch, P_O, 0)) {
	return RET_OK;
    }
    pp->pln_x = sx;
    pp->pln_y = sy;
    CANT_HAPPEN(pp->pln_flags & PLN_LAUNCHED);
    pp->pln_flags |= PLN_LAUNCHED;
    pp->pln_mobil = pp->pln_mobil > dist ? pp->pln_mobil - dist : 0;
    putplane(pp->pln_uid, pp);
    pr(", will be ready for use in %d time units\n",
       plane_mob_max - pp->pln_mobil);
    return RET_OK;
}
