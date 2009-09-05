/* packet_list_store.h
 *
 * $Id$
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1998 Gerald Combs
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#ifndef __NEW_PACKET_LIST_H__
#define __NEW_PACKET_LIST_H__

#ifdef NEW_PACKET_LIST

#include "epan/column_info.h"
#include "epan/frame_data.h"

#define PACKETLIST_TYPE_LIST (packet_list_get_type())
#define PACKET_LIST(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), PACKETLIST_TYPE_LIST, PacketList))
#define PACKETLIST_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_CART((klass), PACKETLIST_TYPE_LIST))
#define PACKETLIST_IS_LIST(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), PACKETLIST_TYPE_LIST))
#define PACKETLIST_IS_LIST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass), PACKETLIST_TYPE_LIST)
#define PACKETLIST_LIST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), PACKETLIST_TYPE_LIST, PacketListClass))

typedef struct _PacketListRecord PacketListRecord;
typedef struct _PacketList PacketList;
typedef struct _PacketListClass PacketListClass;

#define PACKET_LIST_RECORD_GET(rows, pos)	((PacketListRecord*) g_ptr_array_index((rows), (pos)))
#define PACKET_LIST_RECORD_SET(rows, pos, item) PACKET_LIST_RECORD_GET((rows), (pos)) = (item)
#define PACKET_LIST_RECORD_APPEND(rows, item) g_ptr_array_add((rows), (item))
#define PACKET_LIST_RECORD_COUNT(rows) ((rows) ? (rows)->len : 0)
#define PACKET_LIST_RECORD_INDEX_VALID(rows, idx) ((rows) ? (((guint) (idx)) < (rows)->len) : FALSE)

/* PacketListRecord: represents a row */
struct _PacketListRecord
{
	gboolean dissected;
	frame_data *fdata;

	/* admin stuff used by the custom list model */
	/* position within the physical array */
	guint physical_pos; 
	/* position within the visible array */
	gint visible_pos; 
};

/* PacketListRecord: Everything for our model implementation. */
struct _PacketList
{
	GObject parent; /* MUST be first */

	GPtrArray *visible_rows;
	/* Array of pointers to the PacketListRecord structure for each row. */
	GPtrArray *physical_rows; 

	gint n_columns;
	/* Note: We need one extra column to store the entire PacketListRecord */
	GType column_types[NUM_COL_FMTS+1];
	GtkWidget *view; /* XXX - Does this really belong here?? */

	gint sort_id;
	GtkSortType sort_order;

	/* Random integer to check whether an iter belongs to our model. */
	gint stamp;
};

/* PacketListClass: more boilerplate GObject stuff */
struct _PacketListClass
{
	GObjectClass parent_class;
};

GType packet_list_list_get_type(void);
PacketList *new_packet_list_new(void);
void new_packet_list_store_clear(PacketList *packet_list);
guint packet_list_recreate_visible_rows(PacketList *packet_list);
gboolean packet_list_visible_record(PacketList *packet_list, GtkTreeIter *iter);
gint packet_list_append_record(PacketList *packet_list, frame_data *fdata);
void packet_list_change_record(PacketList *packet_list, guint row, gint col, column_info *cinfo);
void packet_list_reset_dissected(PacketList *packet_list);
#endif /* NEW_PACKET_LIST */

#endif /* __NEW_PACKET_LIST_H__ */
