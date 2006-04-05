/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2006, Dave Pare, Jeff Bailey, Thomas Ruschak,
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
 *  bestpath.c: Find the best path between sectors
 * 
 *  Known contributors to this file:
 *     Steve McClure, 1998-2000
 */

/* 
 * IMPORTANT: These routines are very selectively used in the server.
 *
 * "bestownedpath" is only used to determine paths for ships and planes.
 * 
 * Callers should not be calling these directly anymore. They should use
 * the "BestShipPath", "BestAirPath", "BestLandPath" and "BestDistPath"
 * functions.  Note that those last two use the A* algorithms to find
 * information.
 */

#include <config.h>

#include "misc.h"
#include "xy.h"
#include "sect.h"
#include "file.h"
#include "nat.h"
#include "common.h"
#include "optlist.h"

static int owned_and_navigable(char *, int, int, char *, int);

#define MAXROUTE	100	/* return '?' if path longer than this */
#define valid(x,y)	(((x^y)&1)==0)

/* ________________________________________________________________
**
**  bestpath(x1,y1,x2,y2,(s_char *)terrain);
**
**  Calculate routing string to get from sector [x1,y1] to sector [x2,y2]
**  via a specified type of terrain.
**
**  Specify:
**
**	x1,y1	     starting coordinates
**
**	x2,y2	     destination coordinates
**
**	terrain	     ptr to string showing the types of sectors that
**		     we're allowed to pass through:
**
**		     A null string enables routing through any kind of
**		     sector (useful for airplanes).
**
**		     A string that begins with an 'R' ensures that
**		     the source and destination sectors also match
**		     the specified type of terrain.
**
**		     A string that begins with a '~' (after the 'R',
**		     if necessary) specifies that we can pass through
**		     any kind of sector EXCEPT those in the remainder
**                   of the string.
**
**		     Examples:
**
**			"R~.^"	all sectors along route must be
**				non-ocean, non-mountain
**
**			"+"	all sectors between start and end
**				must be highway
**
**			"h. "	all sectors along route must be
**				harbor, water, or unmapped
**
**  'bestpath' returns a pointer to a route string containing either:
**
**     yugjbn - string of normal routing characters if route possible
**     ?      - if route is longer than MAXROUTE characters
**     \0     - (null string) if no route possible
**     h      - if start and end points are the same sector
** ________________________________________________________________
*/

static char *dirchar = "juygbn";
int dx[6] = { 2, 1, -1, -2, -1, 1 };
int dy[6] = { 0, -1, -1, 0, 1, 1 };

/*
 * Ok, note that here we malloc some buffers.  BUT, we never
 * free them.  Why, you may ask?  Because we want to allocate
 * them based on world size which is now (or soon to be) dynamic,
 * but we don't want to allocate each and every time, since that
 * would be slow.  And, since world size only changes at init
 * time, we can do this safely.
 */
static unsigned int *mapbuf;
static unsigned int **mapindex;

s_char *
bestownedpath(s_char *bpath,
	      s_char *bigmap,
	      int x, int y, int ex, int ey, s_char *terrain, int own)
{
    int i, j, tx, ty, markedsectors, restr2;
    int minx, maxx, miny, maxy, scanx, scany;
    unsigned int routelen;

    if (!mapbuf)
	mapbuf = malloc((WORLD_X * WORLD_Y) *
					sizeof(unsigned int));
    if (!mapbuf)
	return NULL;
    if (!mapindex) {
	mapindex = malloc(WORLD_X * sizeof(unsigned int *));
	if (mapindex) {
	    /* Setup the map pointers */
	    for (i = 0; i < WORLD_X; i++)
		mapindex[i] = &mapbuf[WORLD_Y * i];
	}
    }
    if (!mapindex)
	return NULL;

    if (0 != (restr2 = (*terrain == 'R')))
	terrain++;

    x = XNORM(x);
    y = YNORM(y);
    ex = XNORM(ex);
    ey = YNORM(ey);

    if (x == ex && y == ey)
	return "h";

    if (!valid(x, y) || !valid(ex, ey))
	return NULL;

    if (restr2 && (!owned_and_navigable(bigmap, x, y, terrain, own) ||
		   !owned_and_navigable(bigmap, ex, ey, terrain, own)))
	return NULL;

    for (i = 0; i < WORLD_X; i++)
	for (j = 0; j < WORLD_Y; j++)
	    mapindex[i][j] = 0xFFFF;	/* clear the workspace  */

    routelen = 0;		/* path length is now 0 */
    mapindex[x][y] = 0;		/* mark starting spot   */
    markedsectors = 1;		/* source sector marked */
    minx = x - 2;		/* set X scan bounds    */
    maxx = x + 2;
    miny = y - 1;		/* set Y scan bounds    */
    maxy = y + 1;

    do {
	if (++routelen == MAXROUTE)
	    return "?";
	markedsectors = 0;
	for (scanx = minx; scanx <= maxx; scanx++) {
	    x = XNORM(scanx);
	    for (scany = miny; scany <= maxy; scany++) {
		y = YNORM(scany);
		if (valid(x, y)) {
		    if ((((mapindex[x][y]) & 0x1FFF) == (routelen - 1))) {
			for (i = 0; i < 6; i++) {
			    tx = x + dx[i];
			    ty = y + dy[i];
			    tx = XNORM(tx);
			    ty = YNORM(ty);
			    if (mapindex[tx][ty] == 0xFFFF) {
				if (owned_and_navigable(bigmap, tx, ty,
							terrain, own)
				    || (tx == ex && ty == ey && !restr2)) {
				    mapindex[tx][ty] =
					((i + 1) << 13) + routelen;
				    markedsectors++;
				}
			    }
			    if (tx == ex && ty == ey) {
				bpath[routelen] = 0;
				while (routelen--) {
				    i = ((mapindex[tx][ty]) >> 13) - 1;
				    bpath[routelen] = dirchar[i];
				    tx = tx - dx[i];
				    ty = ty - dy[i];
				    tx = XNORM(tx);
				    ty = YNORM(ty);
				}
				return bpath;
			    }
			}
		    }
		}
	    }
	}
	miny--;
	maxy++;
	minx -= 2;
	maxx += 2;
    } while (markedsectors);

    return NULL;		/* no route possible    */
}

/* return TRUE if sector is passable */
static int
owned_and_navigable(char *bigmap, int x, int y, char *terrain, int own)
{
    char *t;
    char mapspot;		/* What this spot on the bmap is */
    struct sctstr sect;
    int negate;

    /* No terrain to check?  Everything is navigable! (this
       probably means we are flying) */
    if (!*terrain)
	return 1;

    /* Owned or allied sector?  Check the real sector.  */
    getsect(x, y, &sect);
    if (sect.sct_own == own
	|| (sect.sct_own && getrel(getnatp(sect.sct_own), own) == ALLIED)) {
	/* FIXME duplicates shp_check_nav() logic */
	switch (dchr[sect.sct_type].d_nav) {
	case NAVOK:
	    return 1;
	case NAV_CANAL:
	    /* FIXME return 1 when all ships have M_CANAL */
	    return 0;
	case NAV_02:
	    return sect.sct_effic >= 2;
	case NAV_60:
	    return sect.sct_effic >= 60;
	default:
	    return 0;
	}
    }

    /* Can only check bigmap */
    if (bigmap) {
	/* Do we know what this sector is?  If not, we assume it's ok,
	   since otherwise we'll never venture anywhere */
	mapspot = bigmap[sctoff(x, y)];
	if (mapspot == ' ' || mapspot == 0)
	    return 1;

	/* Now, is it marked with a 'x' or 'X'? If so, avoid it! */
	if (mapspot == 'x' || mapspot == 'X')
	    return 0;
    } else {
	/* We don't know what it is since we have no map, so return ok! */
	return 1;
    }

    /* Now, check this bmap entry to see if it is one of the
       terrain types. */
    t = terrain;
    if (*t == '~') {
	negate = 1;
	t++;
    } else
	negate = 0;

    while (*t) {
	if (*t == mapspot)
	    break;
	t++;
    }
    if (negate && *t) {
	/* We found it, so we say it's bad since we are negating */
	return 0;
    } else if (!negate && !*t) {
	/* We didn't find it, so we say it's bad since we aren't negating */
	return 0;
    }

    /* According to our bmap, this sector is ok. */
    return 1;
}
