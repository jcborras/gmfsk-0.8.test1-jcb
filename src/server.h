/*
 *    server.h  --  gMFSK remote server
 *
 *    Copyright (C) 2006
 *      Tomi Manninen (oh2bns@sral.fi)
 *
 *    This file is part of gMFSK.
 *
 *    gMFSK is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    gMFSK is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with gMFSK; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef _SERVER_H
#define _SERVER_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gnome.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void server_init(void);
extern void server_write(gchar *str);

#ifdef __cplusplus
}
#endif

#endif
