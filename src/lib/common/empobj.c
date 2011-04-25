/*
 *  Empire - A multi-player, client/server Internet based war game.
 *  Copyright (C) 1986-2011, Dave Pare, Jeff Bailey, Thomas Ruschak,
 *                Ken Stevens, Steve McClure, Markus Armbruster
 *
 *  Empire is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  ---
 *
 *  See files README, COPYING and CREDITS in the root of the source
 *  tree for related information and legal notices.  It is expected
 *  that future projects/authors will amend these files as needed.
 *
 *  ---
 *
 *  empobj.c: Common functions on struct empobj and
 *            union empobj_storage
 *
 *  Known contributors to this file:
 *     Ron Koenderink, 2006
 *     Markus Armbruster, 2006-2008
 */

#include <config.h>

#include "empobj.h"
#include "file.h"
#include "optlist.h"

char *
empobj_chr_name(struct empobj *gp)
{
    switch (gp->ef_type) {
    case EF_LAND:
	return lchr[(int)gp->type].l_name;
    case EF_SHIP:
	return mchr[(int)gp->type].m_name;
    case EF_PLANE:
	return plchr[(int)gp->type].pl_name;
    case EF_NUKE:
	return nchr[(int)gp->type].n_name;
    case EF_SECTOR:
	return dchr[(int)gp->type].d_name;
    }
    CANT_REACH();
    return "The Beast";
}

int
get_empobj_mob_max(int type)
{
    switch (type) {
    case EF_SHIP:
	return ship_mob_max;
    case EF_LAND:
	return land_mob_max;
    case EF_PLANE:
	return plane_mob_max;
    case EF_SECTOR:
	return sect_mob_max;
    }
    CANT_REACH();
    return -1;
}