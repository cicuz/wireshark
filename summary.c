/* summary.c
 * Routines for capture file summary info
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#include <epan/packet.h>
#include "cfile.h"
#include "summary.h"
#ifdef HAVE_LIBPCAP
#include "capture_ui_utils.h"
#endif


static void
tally_frame_data(frame_data *cur_frame, summary_tally *sum_tally)
{
  double cur_time;

  cur_time = nstime_to_sec(&cur_frame->abs_ts);

  if (cur_time < sum_tally->start_time) {
    sum_tally->start_time = cur_time;
  }
  if (cur_time > sum_tally->stop_time){
    sum_tally->stop_time = cur_time;
  }
  sum_tally->bytes += cur_frame->pkt_len;
  if (cur_frame->flags.passed_dfilter){
    if (sum_tally->filtered_count==0){
	    sum_tally->filtered_start= cur_time;
	    sum_tally->filtered_stop = cur_time;
    } else {
	    if (cur_time < sum_tally->filtered_start) {
		    sum_tally->filtered_start = cur_time;
	    }
	    if (cur_time > sum_tally->filtered_stop) {
		    sum_tally->filtered_stop = cur_time;
	    }
    }
    sum_tally->filtered_count++;
    sum_tally->filtered_bytes += cur_frame->pkt_len ;
  }
  if (cur_frame->flags.marked){
    if (sum_tally->marked_count==0){
	    sum_tally->marked_start= cur_time;
	    sum_tally->marked_stop = cur_time;
    } else {
	    if (cur_time < sum_tally->marked_start) {
		    sum_tally->marked_start = cur_time;
	    }
	    if (cur_time > sum_tally->marked_stop) {
		    sum_tally->marked_stop = cur_time;
	    }
    }
    sum_tally->marked_count++;
    sum_tally->marked_bytes += cur_frame->pkt_len ;
  }
  if (cur_frame->flags.ignored){
    sum_tally->ignored_count++;
  }
}

void
summary_fill_in(capture_file *cf, summary_tally *st)
{

  frame_data    *first_frame, *cur_frame;
  guint32        framenum;

  st->start_time = 0;
  st->stop_time = 0;
  st->bytes = 0;
  st->filtered_count = 0;
  st->filtered_start = 0;
  st->filtered_stop = 0;
  st->filtered_bytes = 0;
  st->marked_count = 0;
  st->marked_start = 0;
  st->marked_stop = 0;
  st->marked_bytes = 0;
  st->ignored_count = 0;

  /* initialize the tally */
  if (cf->count != 0) {
    first_frame = frame_data_sequence_find(cf->frames, 1);
    st->start_time = nstime_to_sec(&first_frame->abs_ts);
    st->stop_time = nstime_to_sec(&first_frame->abs_ts);

    for (framenum = 1; framenum <= cf->count; framenum++) {
      cur_frame = frame_data_sequence_find(cf->frames, framenum);
      tally_frame_data(cur_frame, st);
    }
  }

  st->filename = cf->filename;
  st->file_length = cf->f_datalen;
  st->file_type = cf->cd_t;
  st->encap_type = cf->lnk_t;
  st->has_snap = cf->has_snap;
  st->snap = cf->snap;
  st->elapsed_time = nstime_to_sec(&cf->elapsed_time);
  st->packet_count = cf->count;
  st->drops_known = cf->drops_known;
  st->drops = cf->drops;
  st->dfilter = cf->dfilter;

  st->ifaces  = g_array_new(FALSE, FALSE, sizeof(iface_options));
}


#ifdef HAVE_LIBPCAP
void
summary_fill_in_capture(capture_options *capture_opts, summary_tally *st)
{
  iface_options iface;
  interface_options interface_opts;
  guint i;

  if (capture_opts->ifaces->len > 0) {
    while (st->ifaces->len > 0) {
      iface = g_array_index(st->ifaces, iface_options, 0);
      st->ifaces = g_array_remove_index(st->ifaces, 0);
      g_free(iface.name);
      g_free(iface.descr);
      g_free(iface.cfilter);
    }
    for (i = 0; i < capture_opts->ifaces->len; i++) {
      interface_opts = g_array_index(capture_opts->ifaces, interface_options, i);
      iface.cfilter = g_strdup(interface_opts.cfilter);
      iface.name = g_strdup(interface_opts.name);
      iface.descr = g_strdup(interface_opts.descr);
      iface.drops_known = FALSE;
      iface.drops = 0;
      iface.has_snap = interface_opts.has_snaplen;
      iface.snap = interface_opts.snaplen;
      iface.linktype = interface_opts.linktype;
      g_array_append_val(st->ifaces, iface);
    }
  } 
}
#endif
