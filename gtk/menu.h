/* menu.h
 * Menu definitions
 *
 * $Id: menu.h,v 1.9 2003/04/23 05:37:22 guy Exp $
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@zing.org>
 * Copyright 1998 Gerald Combs
 *
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
 */

#ifndef __GTKGUIMENU_H__
#define __GTKGUIMENU_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void get_main_menu (GtkWidget **, GtkAccelGroup **);
void set_menu_object_data (gchar *path, gchar *key, gpointer data);
gint popup_menu_handler(GtkWidget *widget, GdkEvent *event, gpointer data);

/*
 * Add a new menu item for a statistical tap.
 * This must be called after we've created the main menu, so it can't
 * be called from the routine that registers taps - we have to introduce
 * another per-tap registration routine.
 */
extern void register_tap_menu_item(char *name, GtkItemFactoryCallback callback);

extern GtkWidget           *popup_menu_object;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __GTKGUIMENU_H__ */
