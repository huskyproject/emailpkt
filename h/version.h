/* Emailpkt: Husky e-mailer
 *
 * (C) Copyleft Stas Degteff 2:5080/102@FIDOnet, g@grumbler.org
 *
 * $Id$
 * Version related declarations
 * Based on HPT versions composing idea
 *
 */


#ifndef VERSION_H
#define VERSION_H

#include "cvsdate.h"

#define VER_MAJOR 0
#define VER_MINOR 9
#define VER_PATCH 0

/* branch is "" for CVS current, "-stable" for the release candiate branch
   and "-release" for released version.
 */
#define VER_BRANCH ""

extern char *versionStr;

#endif
