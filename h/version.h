/* Emailpkt: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 * (c) Copyleft Husky developers team http://husky.sorceforge.net/team.html
 *
 * $Id$
 * Version related declarations
 * Based on HPT versions composing idea
 *
 * For latest version see http://husky.sorceforge.net/emailpkt
 *
 * This file is part of EMAILPKT, module of The HUSKY Fidonet Software.
 *
 * EMAILPKT is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * EMAILPKT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; see file COPYING. If not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * See also http://www.gnu.org, license may be found here.
 *****************************************************************************
 */


#ifndef VERSION_H
#define VERSION_H

#include "cvsdate.h"

#include <fidoconf/version.h>

#define VER_MAJOR 0
#define VER_MINOR 9
#define VER_PATCH 0
#define VER_BRANCH BRANCH_CURRENT

extern char *versionStr;

#endif
