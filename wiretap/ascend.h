/* ascend.h
 *
 * $Id: ascend.h,v 1.6 2000/05/18 09:09:24 guy Exp $
 *
 * Wiretap Library
 * Copyright (c) 1998 by Gilbert Ramirez <gram@xiexie.org>
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#define ASCEND_MAX_DATA_ROWS 8
#define ASCEND_MAX_DATA_COLS 16
#define ASCEND_MAX_PKT_LEN (ASCEND_MAX_DATA_ROWS * ASCEND_MAX_DATA_COLS)

int ascend_open(wtap *wth, int *err);
