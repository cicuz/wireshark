/* packet-imf.c
 * Routines for Internet Message Format (IMF) packet disassembly
 *
 * $Id$
 *
 * Copyright (c) 2007 by Graeme Lunt
 *
 * Wireshark - Network traffic analyzer
 * By Gerald Combs <gerald@wireshark.org>
 * Copyright 1999 Gerald Combs
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <glib.h>
#include <string.h>
#include <epan/packet.h>
#include <epan/conversation.h>
#include <epan/addr_resolv.h>
#include <epan/prefs.h>
#include <epan/strutil.h>
#include <epan/emem.h>

#define PNAME  "Internet Message Format"
#define PSNAME "IMF"
#define PFNAME "imf"


static int proto_imf = -1;

static int hf_imf_date = -1;
static int hf_imf_from = -1;
static int hf_imf_sender = -1;
static int hf_imf_reply_to = -1;
static int hf_imf_to = -1;
static int hf_imf_cc = -1;
static int hf_imf_bcc = -1;
static int hf_imf_message_id = -1;
static int hf_imf_in_reply_to = -1;
static int hf_imf_references = -1;
static int hf_imf_subject = -1;
static int hf_imf_comments = -1;
static int hf_imf_keywords = -1;
static int hf_imf_resent_date = -1;
static int hf_imf_resent_from = -1;
static int hf_imf_resent_sender = -1;
static int hf_imf_resent_to = -1;
static int hf_imf_resent_cc = -1;
static int hf_imf_resent_bcc = -1;
static int hf_imf_resent_message_id = -1;
static int hf_imf_return_path = -1;
static int hf_imf_received = -1;
static int hf_imf_content_type = -1;
static int hf_imf_content_type_type = -1;
static int hf_imf_content_type_parameters = -1;
static int hf_imf_content_id = -1;
static int hf_imf_content_transfer_encoding = -1;
static int hf_imf_content_description = -1;
static int hf_imf_mime_version = -1;
static int hf_imf_ext_mailer = -1;
static int hf_imf_ext_mimeole = -1;
static int hf_imf_thread_index = -1;
static int hf_imf_extension = -1;
static int hf_imf_extension_type = -1;
static int hf_imf_extension_value = -1;

/* RFC 2156 */
static int hf_imf_autoforwarded = -1;
static int hf_imf_autosubmitted = -1;
static int hf_imf_x400_content_identifier = -1;
static int hf_imf_content_language = -1;
static int hf_imf_conversion = -1;
static int hf_imf_conversion_with_loss = -1;
static int hf_imf_delivery_date = -1;
static int hf_imf_discarded_x400_ipms_extensions = -1;
static int hf_imf_discarded_x400_mts_extensions = -1;
static int hf_imf_dl_expansion_history = -1;
static int hf_imf_deferred_delivery = -1;
static int hf_imf_expires = -1;
static int hf_imf_importance = -1;
static int hf_imf_incomplete_copy = -1;
static int hf_imf_latest_delivery_time = -1;
static int hf_imf_message_type = -1;
static int hf_imf_original_encoded_information_types = -1;
static int hf_imf_originator_return_address = -1;
static int hf_imf_priority = -1;
static int hf_imf_reply_by = -1;
static int hf_imf_sensitivity = -1;
static int hf_imf_supersedes = -1;
static int hf_imf_x400_content_type = -1;
static int hf_imf_x400_mts_identifier = -1;
static int hf_imf_x400_originator = -1;
static int hf_imf_x400_received = -1;
static int hf_imf_x400_recipients = -1;

static int hf_imf_display_name = -1;
static int hf_imf_address = -1;
static int hf_imf_mailbox_list = -1;
static int hf_imf_mailbox_list_item = -1;
static int hf_imf_address_list = -1;
static int hf_imf_address_list_item = -1;

static int ett_imf = -1;
static int ett_imf_content_type = -1;
static int ett_imf_mailbox = -1;
static int ett_imf_group = -1;
static int ett_imf_mailbox_list = -1;
static int ett_imf_address_list = -1;
static int ett_imf_extension = -1;


struct imf_field {
  char         *name;     /* field name */
  int          *hf_id;       /* wireshark field */
  void         (*subdissector)(tvbuff_t *tvb, int offset, int length, proto_item *item);
  gboolean     add_to_col_info; /* add field to column info */    
};

#define NO_SUBDISSECTION NULL

static void dissect_imf_mailbox(tvbuff_t *tvb, int offset, int length, proto_item *item);
static void dissect_imf_address(tvbuff_t *tvb, int offset, int length, proto_item *item);
static void dissect_imf_address_list(tvbuff_t *tvb, int offset, int length, proto_item *item);
static void dissect_imf_mailbox_list(tvbuff_t *tvb, int offset, int length, proto_item *item);

struct imf_field imf_fields[] = {
  {"Unknown-Extension", &hf_imf_extension_type, NO_SUBDISSECTION, FALSE}, /* unknown extension */
  {"Date", &hf_imf_date, NO_SUBDISSECTION, FALSE}, /* date-time */
  {"From", &hf_imf_from, dissect_imf_mailbox_list , TRUE}, /* mailbox_list */
  {"Sender", &hf_imf_sender, dissect_imf_mailbox, FALSE}, /* mailbox */
  {"Reply-To", &hf_imf_reply_to, dissect_imf_address_list , FALSE}, /* address_list */
  {"To", &hf_imf_to, dissect_imf_address_list , FALSE}, /* address_list */
  {"Cc", &hf_imf_cc, dissect_imf_address_list , FALSE}, /* address_list */
  {"Bcc", &hf_imf_bcc, dissect_imf_address_list , FALSE}, /* address_list */
  {"Message-ID", &hf_imf_message_id, NO_SUBDISSECTION, FALSE}, /* msg-id */
  {"In-Reply-To", &hf_imf_in_reply_to, NO_SUBDISSECTION, FALSE}, /* msg-id */
  {"References", &hf_imf_references, NO_SUBDISSECTION, FALSE}, /* msg-id */
  {"Subject", &hf_imf_subject, NO_SUBDISSECTION, TRUE}, /* unstructured */
  {"Comments", &hf_imf_comments, NO_SUBDISSECTION, FALSE}, /* unstructured */
  {"Keywords", &hf_imf_keywords, NULL, FALSE}, /* phrase_list */
  {"Resent-Date", &hf_imf_resent_date, NO_SUBDISSECTION, FALSE},
  {"Resent-From", &hf_imf_resent_from, dissect_imf_mailbox_list, FALSE},
  {"Resent-Sender", &hf_imf_resent_sender, dissect_imf_mailbox, FALSE},
  {"Resent-To", &hf_imf_resent_to, dissect_imf_address_list, FALSE},
  {"Resent-Cc", &hf_imf_resent_cc, dissect_imf_address_list, FALSE},
  {"Resent-Bcc", &hf_imf_resent_bcc, dissect_imf_address_list, FALSE},
  {"Resent-Message-ID", &hf_imf_resent_message_id, NO_SUBDISSECTION, FALSE},
  {"Return-Path", &hf_imf_return_path, NULL, FALSE},
  {"Received", &hf_imf_received, NO_SUBDISSECTION, FALSE},
  /* these are really multi-part - but we parse them anyway */
  {"Content-Type", &hf_imf_content_type, NULL, FALSE}, /* handled separately as a special case */
  {"Content-ID", &hf_imf_content_id, NULL, FALSE},
  {"Content-Description", &hf_imf_content_description, NULL, FALSE},
  {"Content-Transfer-Encoding", &hf_imf_content_transfer_encoding, NULL, FALSE}, 
  {"MIME-Version", &hf_imf_mime_version, NO_SUBDISSECTION, FALSE},
  /* MIXER - RFC 2156 */
  {"Autoforwarded", &hf_imf_autoforwarded, NULL, FALSE},
  {"Autosubmitted", &hf_imf_autosubmitted, NULL, FALSE},
  {"X400-Content-Identifier", &hf_imf_x400_content_identifier, NULL, FALSE},
  {"Content-Language", &hf_imf_content_language, NULL, FALSE},
  {"Conversion", &hf_imf_conversion, NULL, FALSE},
  {"Conversion-With-Loss", &hf_imf_conversion_with_loss, NULL, FALSE},
  {"Delivery-Date", &hf_imf_delivery_date, NULL, FALSE},
  {"Discarded-X400-IPMS-Extensions", &hf_imf_discarded_x400_ipms_extensions, NULL, FALSE},
  {"Discarded-X400-MTS-Extensions", &hf_imf_discarded_x400_mts_extensions, NULL, FALSE},
  {"DL-Expansion-History", &hf_imf_dl_expansion_history, NULL, FALSE},
  {"Deferred-Delivery", &hf_imf_deferred_delivery, NULL, FALSE},
  {"Expires", &hf_imf_expires, NULL, FALSE},
  {"Importance", &hf_imf_importance, NULL, FALSE},
  {"Incomplete-Copy", &hf_imf_incomplete_copy, NULL, FALSE},
  {"Latest-Delivery-Time", &hf_imf_latest_delivery_time, NULL, FALSE},
  {"Message-Type", &hf_imf_message_type, NULL, FALSE},
  {"Original-Encoded-Information-Types", &hf_imf_original_encoded_information_types, NULL, FALSE},
  {"Originator-Return-Address", &hf_imf_originator_return_address, NULL, FALSE},
  {"Priority", &hf_imf_priority, NULL, FALSE},
  {"Reply-By", &hf_imf_reply_by, NULL, FALSE},
  {"Sensitivity", &hf_imf_sensitivity, NULL, FALSE},
  {"Supersedes", &hf_imf_supersedes, NULL, FALSE},
  {"X400-Content-Type", &hf_imf_x400_content_type, NULL, FALSE},
  {"X400-MTS-Identifier", &hf_imf_x400_mts_identifier, NULL, FALSE},
  {"X400-Originator", &hf_imf_x400_originator, NULL, FALSE},
  {"X400-Received", &hf_imf_x400_received, NULL, FALSE},
  {"X400-Recipients", &hf_imf_x400_recipients, NULL, FALSE},
  /* some others */
  {"X-Mailer", &hf_imf_ext_mailer, NO_SUBDISSECTION, FALSE}, /* unstructured */
  {"Thread-Index", &hf_imf_thread_index, NO_SUBDISSECTION, FALSE}, /* unstructured */
  {"X-MimeOLE", &hf_imf_ext_mimeole, NO_SUBDISSECTION, FALSE}, /* unstructured */
  {NULL, NULL, NULL, FALSE},
};

static GHashTable *imf_field_table=NULL;

/* Define media_type/Content type table */
static dissector_table_t media_type_dissector_table;

static void dissect_imf_address(tvbuff_t *tvb, int offset, int length, proto_item *item)
{
  proto_tree *group_tree;
  proto_item *group_item;
  int addr_pos;

  /* if there is a colon present it is a group */
  if((addr_pos = tvb_find_guint8(tvb, offset, length, ':')) == -1) {
    
    /* there isn't - so it must be a mailbox */
    dissect_imf_mailbox(tvb, offset, length, item);
    
  } else {

    /* it is a group */
    group_tree = proto_item_add_subtree(item, ett_imf_group);

    /* the display-name is mandatory */
    group_item = proto_tree_add_item(group_tree, hf_imf_display_name, tvb, offset, addr_pos - offset - 1, FALSE);

    /* consume any whitespace */
    for(addr_pos++ ;addr_pos < (offset + length); addr_pos++) {
      if(!isspace(tvb_get_guint8(tvb, addr_pos))) {
	break;
      }
    }

    if(tvb_get_guint8(tvb, addr_pos) != ';') {
      
      dissect_imf_mailbox_list(tvb, addr_pos, length - (addr_pos - offset), group_item);

      /* XXX: need to check for final ';' */

    }
    
  }
  

  return;

}

static void dissect_imf_mailbox(tvbuff_t *tvb, int offset, int length, proto_item *item)
{
  proto_tree *mbox_tree;
  int        addr_pos, end_pos;

  mbox_tree = proto_item_add_subtree(item, ett_imf_mailbox);

  /* Here is the plan:
     If we can't find and angle brackets, then the whole field is an address.
     If we find angle brackets, then the address is between them and the display name is 
     anything before the opening angle bracket
  */

  if((addr_pos = tvb_find_guint8(tvb, offset, length, '<')) == -1) {
    /* we can't find an angle bracket - the whole field is therefore the address */
 
    (void) proto_tree_add_item(mbox_tree, hf_imf_address, tvb, offset, length, FALSE);

  } else {
    /* we can find an angle bracket - let's see if we can find a display name */
    /* XXX: the '<' could be in the display name */

    for(; offset < addr_pos; offset++) {
      if(!isspace(tvb_get_guint8(tvb, offset))) {
	break;
      }
    }

    if(offset != addr_pos) /* there is a display name */
      (void) proto_tree_add_item(mbox_tree, hf_imf_display_name, tvb, offset, addr_pos - offset - 1, FALSE);

    end_pos = tvb_find_guint8(tvb, addr_pos + 1, length - (addr_pos + 1 - offset), '>');

    if(end_pos != -1) 
      (void) proto_tree_add_item(mbox_tree, hf_imf_address, tvb, addr_pos + 1, end_pos - addr_pos - 1, FALSE);

  }
}

static void dissect_imf_address_list(tvbuff_t *tvb, int offset, int length, proto_item *item)
{
  proto_item *addr_item = NULL;
  proto_tree *tree = NULL;
  int         count = 0;
  int         item_offset;
  int         end_offset;
  int         item_length;

  /* a comma separated list of addresses */
  tree = proto_item_add_subtree(item, ett_imf_address_list);

  item_offset = offset;

  do {

    end_offset = tvb_find_guint8(tvb, item_offset, length - (item_offset - offset), ',');

    count++; /* increase the number of items */

    if(end_offset == -1)
      /* length is to the end of the buffer */
      item_length = length - (item_offset - offset);
    else
      item_length = end_offset - item_offset;

    addr_item = proto_tree_add_item(tree, hf_imf_address_list_item, tvb, item_offset, item_length, FALSE);
    dissect_imf_address(tvb, item_offset, item_length, addr_item);

    if(end_offset != -1)
      item_offset = end_offset + 1;

  } while(end_offset != -1);

  /* now indicate the number of items found */
  proto_item_append_text(item, ", %d item%s", count, plurality(count, "", "s"));
  
  return;
}

static void dissect_imf_mailbox_list(tvbuff_t *tvb, int offset, int length, proto_item *item)
{
  proto_item *mbox_item = NULL;
  proto_tree *tree = NULL;
  int         count = 0;
  int         item_offset;
  int         end_offset;
  int         item_length;

  /* a comma separated list of mailboxes */
  tree = proto_item_add_subtree(item, ett_imf_mailbox_list);

  item_offset = offset;

  do {

    end_offset = tvb_find_guint8(tvb, item_offset, length - (item_offset - offset), ',');

    count++; /* increase the number of items */

    if(end_offset == -1)
      /* length is to the end of the buffer */
      item_length = length - (item_offset - offset);
    else
      item_length = end_offset - item_offset;

    mbox_item = proto_tree_add_item(tree, hf_imf_mailbox_list_item, tvb, item_offset, item_length, FALSE);
    dissect_imf_mailbox(tvb, item_offset, item_length, mbox_item);

    if(end_offset != -1)
      item_offset = end_offset + 1;

  } while(end_offset != -1);

  /* now indicate the number of items found */
  proto_item_append_text(item, ", %d item%s", count, plurality(count, "", "s"));
  
  return;
}


static void dissect_imf_content_type(tvbuff_t *tvb, int offset, int length, proto_item *item, 
				     char **type, char **parameters)
{
  int first_colon;
  int len;
  int i; 
  proto_tree *ct_tree;

  /* first strip any whitespace */
  for(i = 0; i < length; i++) {
    if(!isspace(tvb_get_guint8(tvb, offset + i))) {
      offset += i;
      break;
    }
  }

  /* find the first colon - there has to be a colon as there will have to be a boundary */
  first_colon = tvb_find_guint8(tvb, offset, length, ';');

  if(first_colon != -1) {
    ct_tree = proto_item_add_subtree(item, ett_imf_content_type);
    
    len = first_colon - offset;
    proto_tree_add_item(ct_tree, hf_imf_content_type_type, tvb, offset, len, FALSE);
    if(type)
      /* this string must be freed */
      (*type) = tvb_get_string(tvb, offset, len);
    
    len = length - (first_colon + 1 - offset);
    proto_tree_add_item(ct_tree, hf_imf_content_type_parameters, tvb, first_colon + 1, len, FALSE);
    if(parameters) 
      /* this string must be freed */
      (*parameters) = tvb_get_string(tvb, first_colon + 1, len);
  }
}


static int find_field_end(tvbuff_t *tvb, int offset, gint max_length, gboolean *last_field)
{

  while(offset < max_length) {

    /* look for CR */
    offset = tvb_find_guint8(tvb, offset, max_length - offset, '\r');

    if(offset != -1) {
      if(tvb_get_guint8(tvb, ++offset) == '\n') {

	/* OK - so we have found CRLF */
	/* peek the next character */

	switch(tvb_get_guint8(tvb, ++offset)) {
	case '\r':
	  /* probably end of the fields */
	  if(last_field)
	    *last_field = TRUE;
	  return offset;
	  break;
	case  ' ':
	case '\t':
	  /* continuation line */
	  break;
	default:
	  /* this is a new field */
	  return offset;
	  break;
	}
      }
    } else {

      /* couldn't find a CR - strange */
      return offset;
    }
      
  }
      
  return offset;

}

static void dissect_imf(tvbuff_t *tvb, packet_info *pinfo, proto_tree *tree)
{
  proto_item  *item;
  proto_tree  *unknown_tree;
  char  *content_type_str = NULL;
  char  *parameters = NULL;
  int   hf_id;
  gint  start_offset = 0;
  gint  unknown_offset = 0;
  gint  end_offset = 0;
  gint   max_length;
  guint8 *key;
  gboolean last_field = FALSE;
  gboolean dissected = FALSE;
  tvbuff_t *next_tvb;
  struct imf_field *f_info;
  
  if (check_col(pinfo->cinfo, COL_PROTOCOL))
    col_set_str(pinfo->cinfo, COL_PROTOCOL, PSNAME);
  if (check_col(pinfo->cinfo, COL_INFO))
    col_clear(pinfo->cinfo, COL_INFO);

  if(tree){
    item = proto_tree_add_item(tree, proto_imf, tvb, 0, -1, FALSE);
    tree = proto_item_add_subtree(item, ett_imf);
  }

  max_length = tvb_length_remaining(tvb, 0);
  /* first go through the tvb until we find a blank line and extract the content type if
     we find one */

  while(!last_field) {

    /* look for a colon first */
    end_offset = tvb_find_guint8(tvb, start_offset, max_length - start_offset, ':');

    if(end_offset == -1) {
      /* we couldn't find another colon - strange - we should have broken out of here by now */
      /* XXX: flag an error */
      break;
    } else {
      key = tvb_get_ephemeral_string(tvb, start_offset, end_offset - start_offset);

      /* look up the key */
      f_info = (struct imf_field *)g_hash_table_lookup(imf_field_table, key);
      
      if(f_info == (struct imf_field *)NULL) {
	/* set as an unknown extension */
	f_info = imf_fields;
	unknown_offset = start_offset;
      }
      
      hf_id = *(f_info->hf_id);

      /* value starts immediately after the colon */
      start_offset = end_offset+1;

      end_offset = find_field_end(tvb, start_offset, max_length, &last_field);

      if(end_offset != -1) {

	/* remove any leading whitespace */

	for(; start_offset < end_offset; start_offset++)
	  if(!isspace(tvb_get_guint8(tvb, start_offset))) {
	    break;
	  }

	if(hf_id == hf_imf_extension_type) {

	  /* remove 2 bytes to take off the final CRLF to make things a little prettier */
	  item = proto_tree_add_item(tree, hf_imf_extension, tvb, unknown_offset, end_offset - unknown_offset - 2, FALSE);

	  proto_item_append_text(item, " (Contact Wireshark developers if you want this supported.)");

	  unknown_tree = proto_item_add_subtree(item, ett_imf_extension);
	  
	  item = proto_tree_add_item(unknown_tree, hf_imf_extension_type, tvb, unknown_offset, start_offset - 1 - unknown_offset, FALSE);

	  /* remove 2 bytes to take off the final CRLF to make things a little prettier */
	  item = proto_tree_add_item(unknown_tree, hf_id, tvb, start_offset, end_offset - start_offset - 2, FALSE);

	} else
	
	  /* remove 2 bytes to take off the final CRLF to make things a little prettier */
	  item = proto_tree_add_item(tree, hf_id, tvb, start_offset, end_offset - start_offset - 2, FALSE);

	if(f_info->add_to_col_info && check_col(pinfo->cinfo, COL_INFO)) {
	  
	  col_append_fstr(pinfo->cinfo, COL_INFO, "%s: %s, ", f_info->name, 
			  tvb_format_text(tvb, start_offset, end_offset - start_offset - 2));
	}

	if(hf_id == hf_imf_content_type) {
	  /* we need some additional processing to extract the content type and parameters */

	  dissect_imf_content_type(tvb, start_offset, end_offset - start_offset, item,
				   &content_type_str, &parameters);

	} else if(f_info && f_info->subdissector) {

	  /* we have a subdissector */
	  f_info->subdissector(tvb, start_offset, end_offset - start_offset, item);

	}
      }
    }
    start_offset = end_offset;
  }

  /* specify a content type until we can work it out for ourselves */
  /* content_type_str = "multipart/mixed"; */

  /* now dissect the MIME based upon the content type */

  if(content_type_str && media_type_dissector_table) {

    pinfo->private_data = parameters; /* "boundary=\"----=_NextPart_000_0012_01C7B64E.426C8120\""; */

    next_tvb = tvb_new_subset(tvb, end_offset, -1, -1);

    dissected = dissector_try_string(media_type_dissector_table, content_type_str, next_tvb, pinfo, tree);
    
    g_free(content_type_str); 
    content_type_str = NULL;

    if(parameters) {
      g_free(parameters);
      parameters = NULL;
    }
  } else {

    /* just show the lines or highlight the rest of the buffer as message text */

  }
}

/* Register all the bits needed by the filtering engine */

void
proto_register_imf(void)
{
  static hf_register_info hf[] = {
    { &hf_imf_date,
      { "Date", "imf.date", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"imf.DateTime", HFILL }},
    { &hf_imf_from,
      { "From", "imf.from", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"imf.MailboxList", HFILL }},
    { &hf_imf_sender,
      { "Sender", "imf.sender", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_reply_to,
      { "Reply-To", "imf.reply_to", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_to,
      { "To", "imf.to", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_cc,
      { "Cc", "imf.cc", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_bcc,
      { "Bcc", "imf.bcc", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_message_id,
      { "Message-ID", "imf.message_id", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_in_reply_to,
      { "In-Reply-To", "imf.in_reply_to", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_references,
      { "References", "imf.references", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_subject,
      { "Subject", "imf.subject", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_comments,
      { "Comments", "imf.comments", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_keywords,
      { "Keywords", "imf.keywords", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_date,
      { "Resent-Date", "imf.resent.date", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_from,
      { "Resent-From", "imf.resent.from", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_sender,
      { "Resent-Sender", "imf.resent.sender", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_to,
      { "Resent-To", "imf.resent.to", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_cc,
      { "Resent-Cc", "imf.resent.cc", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_bcc,
      { "Resent-Bcc", "imf.resent.bcc", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_resent_message_id,
      { "Resent-Message-ID", "imf.resent.message_id", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_return_path,
      { "Return-Path", "imf.return_path", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_received,
      { "Received", "imf.received", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_type,
      { "Content-Type", "imf.content.type", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_type_type,
      { "Type", "imf.content.type.type", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_type_parameters,
      { "Parameters", "imf.content.type.parameters", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_description,
      { "Content-Description", "imf.content.description", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_id,
      { "Content-ID", "imf.content.id", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_content_transfer_encoding,
      { "Content-Transfer-Encoding", "imf.content.transfer_encoding", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_mime_version,
      { "MIME-Version", "imf.mime_version", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_autoforwarded,
      { "Autoforwarded", "imf.autoforwarded", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_autosubmitted,
      { "Autosubmitted", "imf.autosubmitted", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_x400_content_identifier,
      { "X400-Content-Identifier", "imf.x400_content_identifier", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_content_language,
      { "Content-Language", "imf.content_language", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_conversion,
	{ "Conversion", "imf.conversion", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_conversion_with_loss,
	{ "Conversion-With-Loss", "imf.conversion_with_loss", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_delivery_date,
	{ "Delivery-Date", "imf.delivery_date", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_discarded_x400_ipms_extensions,
	{ "Discarded-X400-IPMS-Extensions", "imf.discarded_x400_ipms_extensions", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_discarded_x400_mts_extensions,
      { "Discarded-X400-MTS-Extensions", "imf.discarded_x400_mts_extensions", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_dl_expansion_history,
	{ "DL-Expansion-History", "imf.dl_expansion_history", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_deferred_delivery,
	{ "Deferred-Delivery", "imf.deferred_delivery", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_expires,
	{ "Expires", "imf.expires", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_importance,
      { "Importance", "imf.importance", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_incomplete_copy,
	{ "Incomplete-Copy", "imf.incomplete_copy", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_latest_delivery_time,
      { "Latest-Delivery-Time", "imf.latest_delivery_time", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_message_type,
	{ "Message-Type", "imf.message_type", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_original_encoded_information_types,
	{ "Original-Encoded-Information-Types", "imf.original_encoded_information_types", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_originator_return_address,
	{ "Originator-Return-Address", "imf.originator_return_address", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_priority,
	{ "Priority", "imf.priority", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_reply_by,
	{ "Reply-By", "imf.reply_by", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_sensitivity,
	{ "Sensitivity", "imf.sensitivity", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_supersedes,
	{ "Supersedes", "imf.supersedes", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_x400_content_type,
	{ "X400-Content-Type", "imf.x400_content_type", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_x400_mts_identifier,
	{ "X400-MTS-Identifier", "imf.x400_mts_identifier", FT_STRING, BASE_NONE, NULL, 0x0,
	  "", HFILL }},
    { &hf_imf_x400_originator,
	{ "X400-Originator", "imf.x400_originator", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_x400_received,
	{ "X400-Received", "imf.x400_received", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_x400_recipients,
	{ "X400-Recipients", "imf.x400_recipients", FT_STRING, BASE_NONE, NULL, 0x0,
	"", HFILL }},
    { &hf_imf_ext_mailer,
      { "X-Mailer", "imf.ext.mailer", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_ext_mimeole,
      { "X-MimeOLE", "imf.ext.mimeole", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_thread_index,
      { "Thread-Index", "imf.thread-index", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_extension,
      { "Unknown-Extension", "imf.extension", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_extension_type,
      { "Type", "imf.extension.type", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_extension_value,
      { "Value", "imf.extension.value", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_display_name,
      { "Display-Name", "imf.display_name", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_address,
      { "Address", "imf.address", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_address_list,
      { "Address List", "imf.address_list", FT_UINT32,  BASE_DEC, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_address_list_item,
      { "Item", "imf.address_list.item", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_mailbox_list,
      { "Mailbox List", "imf.mailbox_list", FT_UINT32,  BASE_DEC, NULL, 0x0,
      	"", HFILL }},
    { &hf_imf_mailbox_list_item,
      { "Item", "imf.mailbox_list.item", FT_STRING,  BASE_NONE, NULL, 0x0,
      	"", HFILL }},
  };
  static gint *ett[] = {
    &ett_imf,
    &ett_imf_content_type,
    &ett_imf_group,
    &ett_imf_mailbox,
    &ett_imf_mailbox_list,
    &ett_imf_address_list,
    &ett_imf_extension,
  };

  struct imf_field *f;

  proto_imf = proto_register_protocol(PNAME, PSNAME, PFNAME);

  proto_register_field_array(proto_imf, hf, array_length(hf));
  proto_register_subtree_array(ett, array_length(ett));

  /* Allow dissector to find be found by name. */
  register_dissector(PFNAME, dissect_imf, proto_imf);

  imf_field_table=g_hash_table_new(g_str_hash, g_str_equal); /* oid to syntax */

  /* register the fields for lookup */
  for(f = imf_fields; f->name; f++)
    g_hash_table_insert(imf_field_table, (const gpointer)f->name, (const gpointer)f);

}

/* The registration hand-off routine */
void
proto_reg_handoff_imf(void)
{
  dissector_handle_t imf_handle;

  imf_handle = create_dissector_handle(dissect_imf, proto_imf);

  dissector_add_string("media_type",
		       "message/rfc822", imf_handle);

  /*
   * Get the content type and Internet media type table
   */
  media_type_dissector_table = find_dissector_table("media_type");

}
