/* packet-flexnet.c
 *
 * Routines for Amateur Packet Radio protocol dissection
 * Copyright 2005,2006,2007,2008,2009,2010,2012 R.W. Stearn <richard@rns-stearn.demon.co.uk>
 *
 * $Id$
 *
 * Ethereal - Network traffic analyzer
 * By Gerald Combs <gerald@ethereal.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Information on the protocol drawn from:
 *
 * Inspiration on how to build the dissector drawn from
 *   packet-sdlc.c
 *   packet-x25.c
 *   packet-lapb.c
 *   paket-gprs-llc.c
 *   xdlc.c
 * with the base file built from README.developers.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include <epan/strutil.h>
#include <epan/packet.h>
#include <epan/emem.h>
#include <epan/xdlc.h>
#include <packet-ip.h>

#define FLEXNET_ADRLEN  15
#define FLEXNET_CTLLEN  15
#define FLEXNET_HDRLEN  (FLEXNET_ADRLEN + FLEXNET_ADRLEN + FLEXNET_CTLLEN)

/* Forward declaration we need below */
void proto_reg_handoff_flexnet(void);

/* Dissector handles - all the possibles are listed */
static dissector_handle_t default_handle;

/* Initialize the protocol and registered fields */
static int proto_flexnet	= -1;
static int hf_flexnet_dst	= -1;
static int hf_flexnet_src	= -1;
static int hf_flexnet_ctl	= -1;

/* Initialize the subtree pointers */
static gint ett_flexnet = -1;
static gint ett_flexnet_ctl = -1;

/* Code to actually dissect the packets */
static void
dissect_flexnet(tvbuff_t *tvb, packet_info *pinfo, proto_tree *parent_tree)
{
	proto_item *ti;
	proto_tree *flexnet_tree;
	int offset;
	void *saved_private_data;
	tvbuff_t *next_tvb = NULL;

	col_set_str( pinfo->cinfo, COL_PROTOCOL, "Flexnet");

	col_clear( pinfo->cinfo, COL_INFO );

	if ( parent_tree )
		{
		/* create display subtree for the protocol */

		ti = proto_tree_add_protocol_format( parent_tree, proto_flexnet, tvb, 0, FLEXNET_HDRLEN, "FLEXNET" );

		flexnet_tree = proto_item_add_subtree( ti, ett_flexnet );

		offset = 0;

		proto_tree_add_item( flexnet_tree, hf_flexnet_dst, tvb, offset, FLEXNET_ADRLEN, FALSE );
		offset +=FLEXNET_ADRLEN;

		proto_tree_add_item( flexnet_tree, hf_flexnet_src, tvb, offset, FLEXNET_ADRLEN, FALSE );
		offset +=FLEXNET_ADRLEN;

		proto_tree_add_item( flexnet_tree, hf_flexnet_ctl, tvb, offset, FLEXNET_CTLLEN, FALSE );
		offset +=FLEXNET_CTLLEN;
		}

	/* Call sub-dissectors here */

	saved_private_data = pinfo->private_data;
	next_tvb = tvb_new_subset(tvb, FLEXNET_HDRLEN, -1, -1);

	call_dissector( default_handle , next_tvb, pinfo, parent_tree );

	pinfo->private_data = saved_private_data;
}

void
proto_register_flexnet(void)
{
	/* Setup list of header fields */
	static hf_register_info hf[] = {
		{ &hf_flexnet_dst,
			{ "Destination",		"flexnet.dst",
			FT_BYTES, BASE_NONE, NULL, 0x0,
			"Destination address", HFILL }
		},
		{ &hf_flexnet_src,
			{ "Source",			"flexnet.src",
			FT_BYTES, BASE_NONE, NULL, 0x0,
			"Source address", HFILL }
		},
		{ &hf_flexnet_ctl,
			{ "Control",			"flexnet.ctl",
			FT_BYTES, BASE_NONE, NULL, 0x0,
			NULL, HFILL }
		},
	};

	/* Setup protocol subtree array */
	static gint *ett[] = {
		&ett_flexnet,
		&ett_flexnet_ctl,
	};

	/* Register the protocol name and description */
	proto_flexnet = proto_register_protocol("FlexNet", "FLEXNET", "flexnet");

	/* Register the dissector */
	register_dissector( "flexnet", dissect_flexnet, proto_flexnet );

	/* Required function calls to register the header fields and subtrees used */
	proto_register_field_array( proto_flexnet, hf, array_length( hf ) );
	proto_register_subtree_array( ett, array_length( ett ) );
}

void
proto_reg_handoff_flexnet(void)
{
	static gboolean inited = FALSE;

	if( !inited ) {

		/*
		*/
		default_handle  = find_dissector( "data" );

		inited = TRUE;
	}
}
