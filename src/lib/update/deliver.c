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
 *  deliver.c: Deliver commodities to neighboring sector
 * 
 *  Known contributors to this file:
 *  
 */

#include "misc.h"
#include "var.h"
#include "sect.h"
#include "item.h"
#include "path.h"
#include "file.h"
#include "xy.h"
#include "update.h"
#include "subs.h"
#include "common.h"

int
deliver(register struct sctstr *from, struct ichrstr *ip, int dir,
	int thresh, int amt_src, int plague)
{
    register struct sctstr *to;
    int vtype;			/* item vartype */
    int pack_src;
    int amt_moved;
    int amt_dst;
    int mobility;
    float mcost;
    struct dchrstr *dp;
    int n;

    if (dir <= 0 || dir > DIR_UL)
	return 0;
    if (amt_src <= 0)
	return 0;
    if ((amt_moved = amt_src - thresh) <= 0)
	return 0;
    /*
     * make sure delivery looks ok.  Check where its going,
     * where its coming from, and see if there is more than
     * the threshold amount
     */
    if (!military_control(from))
	return 0;
    to = getsectp(from->sct_x + diroff[dir][0],
		  from->sct_y + diroff[dir][1]);
    if (to->sct_own != from->sct_own) {
	wu(0, from->sct_own, "%s delivery walkout at %s\n",
	   ip->i_name, ownxy(from));
	return 0;
    }
    dp = &dchr[from->sct_type];
    vtype = ip->i_vtype;
    pack_src = ip->i_pkg[from->sct_effic >= 60 ? dp->d_pkg : NPKG];
    mobility = from->sct_mobil / 2;
    if (vtype == I_CIVIL && from->sct_own != from->sct_oldown) {
	wu(0, from->sct_own,
	   "The conquered populace in %s refuses to relocate!\n",
	   ownxy(from));
	return 0;
    }
    /*
     * disallow delivery into prohibited sectors.
     * calculate unit movement cost; decrease amount if
     * there isn't enough mobility.
     */
    mcost = sector_mcost(to, MOB_ROAD) * ip->i_lbs / pack_src;
    mcost /= 4.0;

    if (mobility < mcost * amt_moved) {
	/* XXX can mcost be == 0? */
	amt_moved = (int)(mobility / mcost);
	if (amt_moved <= 0)
	    return 0;
    }
    amt_dst = to->sct_item[vtype];
    if (amt_moved > ITEM_MAX - amt_dst) {
	/* delivery backlog */
	amt_moved = ITEM_MAX - amt_dst;
    }
    to->sct_item[vtype] = amt_moved + amt_dst;
    /* deliver the plague too! */
    if (plague == PLG_INFECT && to->sct_pstage == PLG_HEALTHY)
	to->sct_pstage = PLG_EXPOSED;
    n = from->sct_mobil - (int)(mcost * amt_moved);
    if (n < 0)
	n = 0;
    from->sct_mobil = n;
    return amt_moved;
}
