/* dfilter.c
 * Routines for display filters
 *
 * $Id: dfilter.c,v 1.6 1999/08/03 15:04:25 gram Exp $
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifndef _STDIO_H
#include <stdio.h>
#endif

#ifndef _STRING_H
#include <string.h>
#endif

#ifndef __G_LIB_H__
#include <glib.h>
#endif

#ifndef __PROTO_H__
#include "proto.h"
#endif

#ifndef __DFILTER_H__
#include "dfilter.h"
#endif
#include "dfilter-grammar.h"

int yyparse(void); /* yacc entry-point */

#define DFILTER_LEX_ABBREV_OFFSET	2000

/* Balanced tree of abbreviations and IDs */
GTree *dfilter_tokens = NULL;

/* Comparision function for tree insertion. A wrapper around strcmp() */
static int g_strcmp(gconstpointer a, gconstpointer b);

/* Silly global variables used to pass parameter to check_relation_bytes() */
int bytes_offset = 0;
int bytes_length = 0;

YYSTYPE yylval;

/* in dfilter-grammar.y */
extern GMemChunk *gmc_dfilter_nodes; 
extern GNode *dfilter_tree;
extern GSList *dfilter_list_byte_arrays;

/* in dfilter-scanner.l */
void dfilter_scanner_text(char*);
void dfilter_scanner_cleanup(void);

static gboolean dfilter_apply_node(GNode *gnode, proto_tree *ptree, const guint8 *pd);
static gboolean check_relation(gint operand, GNode *a, GNode *b, proto_tree *ptree, const guint8 *pd);
static gboolean check_logical(gint operand, GNode *a, GNode *b, proto_tree *ptree, const guint8 *pd);
static GArray* get_values_from_ptree(dfilter_node *dnode, proto_tree *ptree, const guint8 *pd);
static GArray* get_values_from_dfilter(dfilter_node *dnode, GNode *gnode);
static gboolean check_existence_in_ptree(dfilter_node *dnode, proto_tree *ptree);
static void clear_byte_array(gpointer data, gpointer user_data);

/* this is not so pretty. I need my own g_array "function" (macro) to
 * retreive the pointer to the data stored in an array cell. I need this
 * for type ether.. GArray makes it easy for me to store 6 bytes inside an array
 * cell, but hard to retrieve it.
 */
#define g_array_index_ptr(a,s,i)      (((guint8*) (a)->data) + (i*s))

void
dfilter_init(void)
{
	int i, num_symbols, symbol;
	char *s;

	dfilter_tokens = g_tree_new(g_strcmp);

	/* Add the header field and protocol abbrevs to the symbol table */
	num_symbols = proto_registrar_n();
	for (i=0; i < num_symbols; i++) {
		s = proto_registrar_get_abbrev(i);
		if (s) {
			symbol = DFILTER_LEX_ABBREV_OFFSET + i;
			g_tree_insert(dfilter_tokens, s, GINT_TO_POINTER(symbol));
		}
	}
}
/* I should eventually g_tree_destroy(dfilter_tokens), when ethereal shuts down */

/* Compiles the textual representation of the display filter into a tree
 * of operations to perform.
 */
int
dfilter_compile(char *dfilter_text, GNode **p_dfcode)
{
	int retval;

	g_assert(dfilter_text != NULL);
	dfilter_scanner_text(dfilter_text);

	if (dfilter_tree) {
		/* clear tree */
		dfilter_tree = NULL;
	}
	/* clear the memory that the tree was using for nodes */
	g_mem_chunk_reset(gmc_dfilter_nodes);

	/* clear the memory that the tree was using for byte arrays */
	if (dfilter_list_byte_arrays) {
		g_slist_foreach(dfilter_list_byte_arrays, clear_byte_array, NULL);
		g_slist_free(dfilter_list_byte_arrays);
		dfilter_list_byte_arrays = NULL;
	}

	if (*p_dfcode != NULL)
		g_node_destroy(*p_dfcode);

	retval = yyparse();
	dfilter_scanner_cleanup();
	*p_dfcode = dfilter_tree;

	return retval;
}

static void
clear_byte_array(gpointer data, gpointer user_data)
{
	GByteArray *barray = data;

	g_byte_array_free(barray, TRUE);
}

void
yyerror(char *s)
{
/*	fprintf(stderr, "%s\n", s);
	Do not report the error, just let yyparse() return 1 */
}

void
dfilter_yyerror(char *fmt, ...)
{
	dfilter_tree = NULL;
	yyerror(fmt);
}

/* lookup an abbreviation in our token tree, returing the ID #
 * If the abbreviation doesn't exit, returns 0 */
int dfilter_lookup_token(char *abbrev)
{
	int value;

	g_assert(abbrev != NULL);
	value =  GPOINTER_TO_INT(g_tree_lookup(dfilter_tokens, abbrev));

	if (value < DFILTER_LEX_ABBREV_OFFSET) {
		return 0;
	}
	return value - DFILTER_LEX_ABBREV_OFFSET;
}

static int
g_strcmp(gconstpointer a, gconstpointer b)
{
	return strcmp((const char*)a, (const char*)b);
}


gboolean
dfilter_apply(GNode *dfcode, proto_tree *ptree, const guint8* pd)
{
	gboolean retval;
	retval = dfilter_apply_node(dfcode, ptree, pd);
	return retval;
}

static gboolean
dfilter_apply_node(GNode *gnode, proto_tree *ptree, const guint8* pd)
{
	GNode		*gnode_a, *gnode_b;
	dfilter_node	*dnode = (dfilter_node*) (gnode->data);

	/* We'll get 2 NULLs if we don't have children */
	gnode_a = g_node_nth_child(gnode, 0);
	gnode_b = g_node_nth_child(gnode, 1);

	switch(dnode->ntype) {
	case variable:
		/* We'll never see this case because if the parser finds the name of
		 * a variable, it will cause it to be an 'existence' operation.
		 */
		g_assert_not_reached();

	case logical:
		return check_logical(dnode->value.logical, gnode_a, gnode_b, ptree, pd);

	case relation:
		g_assert(gnode_a && gnode_b);
		return check_relation(dnode->value.relation, gnode_a, gnode_b, ptree, pd);

	case alternation:
		g_assert_not_reached();
		/* not coded yet */
	
	case numeric:
	case ipv4:
	case boolean:
	case ether:
	case string:
	case abs_time:
	case bytes:
	case ipxnet:
		/* the only time we'll see these at this point is if the display filter
		 * is really wacky. (like simply "192.168.1.1"). The parser as it stands
		 * now let these by. Just return TRUE */
		g_assert(!gnode_a && !gnode_b);
		return TRUE;

	case existence:	/* checking the existence of a protocol or hf*/
		g_assert(!gnode_a && !gnode_b);
		return check_existence_in_ptree(dnode, ptree);
	}

	g_assert_not_reached();
	return FALSE;
}

static gboolean
check_logical(gint operand, GNode *a, GNode *b, proto_tree *ptree, const guint8 *pd)
{
	gboolean val_a = dfilter_apply_node(a, ptree, pd);
	gboolean val_b;

	switch(operand) {
	case TOK_AND:
		return (val_a && dfilter_apply_node(b, ptree, pd));
	case TOK_OR:
		return (val_a || dfilter_apply_node(b, ptree, pd));
	case TOK_XOR:
		val_b = dfilter_apply_node(b, ptree, pd);
		return ( ( val_a || val_b ) && ! ( val_a && val_b ) );
	case TOK_NOT:
		return (!val_a);
	default:
		g_assert_not_reached();
	}	
	g_assert_not_reached();
	return FALSE;
}

/* this is inefficient. I get arrays for both a and b that represent all the values present. That is,
 * if a is bootp.option, e.g., i'll get an array showing all the bootp.option values in the protocol
 * tree. Then I'll get an array for b, which more than likely is a single int, and then I'll compare
 * them all. It makes my coding easier in the beginning, but I should change this to make it run
 * faster.
 */
static gboolean
check_relation(gint operand, GNode *a, GNode *b, proto_tree *ptree, const guint8* pd)
{
	dfilter_node	*node_a = (dfilter_node*) (a->data);
	dfilter_node	*node_b = (dfilter_node*) (b->data);
	GArray		*vals_a, *vals_b;
	gboolean	retval;


	bytes_length = MIN(node_a->length, node_b->length);
	bytes_offset = MIN(node_a->offset, node_b->offset);
	if (node_a->ntype == variable)
		vals_a = get_values_from_ptree(node_a, ptree, pd);
	else
		vals_a = get_values_from_dfilter(node_a, a);

	if (node_b->ntype == variable)
		vals_b = get_values_from_ptree(node_b, ptree, pd);
	else
		vals_b = get_values_from_dfilter(node_b, b);

	retval =  node_a->check_relation_func(operand, vals_a, vals_b);

	g_array_free(vals_a, FALSE);
	g_array_free(vals_b, FALSE);

	return retval;
}

static gboolean
check_existence_in_ptree(dfilter_node *dnode, proto_tree *ptree)
{
	int		target_field;
	proto_tree	*subtree;

	target_field = dnode->value.variable;
	subtree = proto_find_field(ptree, target_field);

	if (subtree)
		return TRUE;
	else
		return FALSE;
}

static GArray*
get_values_from_ptree(dfilter_node *dnode, proto_tree *ptree, const guint8 *pd)
{
	GArray		*array;
	int		parent_protocol;
	int		target_field;
	proto_tree	*subtree = NULL; /* where the parent protocol's sub-tree starts */
	proto_tree_search_info sinfo;

	g_assert(dnode->elem_size > 0);
	array = g_array_new(FALSE, FALSE, dnode->elem_size);

	target_field = dnode->value.variable;

	/* Find the proto_tree subtree where we should start searching.*/
	if (proto_registrar_is_protocol(target_field)) {
		subtree = proto_find_protocol(ptree, target_field);
	}
	else {
		parent_protocol = proto_registrar_get_parent(target_field);
		if (parent_protocol >= 0) {
			subtree = proto_find_protocol(ptree, parent_protocol);
		}
	}

	if (subtree) {
		sinfo.target_field = target_field;
		sinfo.result_array = array;
		sinfo.packet_data = pd;
		proto_get_field_values(subtree, dnode->fill_array_func, &sinfo);
	}

	return array;
}

static GArray*
get_values_from_dfilter(dfilter_node *dnode, GNode *gnode)
{
	GArray		*array;

	g_assert(dnode->elem_size > 0);
	array = g_array_new(FALSE, FALSE, dnode->elem_size);

	g_node_traverse(gnode, G_IN_ORDER, G_TRAVERSE_ALL, -1, dnode->fill_array_func, array);
/*	dnode->fill_array_func(gnode, array);*/
	return array;
}

gboolean fill_array_numeric_variable(GNode *gnode, gpointer data)
{
	proto_tree_search_info	*sinfo = (proto_tree_search_info*)data;
	field_info		*fi = (field_info*) (gnode->data);

	if (fi->hfinfo->id == sinfo->target_field) {
		g_array_append_val(sinfo->result_array, fi->value.numeric);
	}

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_ether_variable(GNode *gnode, gpointer data)
{
	proto_tree_search_info	*sinfo = (proto_tree_search_info*)data;
	field_info		*fi = (field_info*) (gnode->data);

	if (fi->hfinfo->id == sinfo->target_field) {
		g_array_append_val(sinfo->result_array, fi->value.ether);
	}

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_bytes_variable(GNode *gnode, gpointer data)
{
	proto_tree_search_info	*sinfo = (proto_tree_search_info*)data;
	field_info		*fi = (field_info*) (gnode->data);
	GByteArray		*barray;

	if (fi->hfinfo->id == sinfo->target_field) {
		barray = g_byte_array_new();
		/*dfilter_list_byte_arrays = g_slist_append(dfilter_list_byte_arrays, barray);*/
		g_byte_array_append(barray, sinfo->packet_data + fi->start + bytes_offset, bytes_length);
		g_array_append_val(sinfo->result_array, barray);
	}

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_boolean_variable(GNode *gnode, gpointer data)
{
	proto_tree_search_info	*sinfo = (proto_tree_search_info*)data;
	field_info		*fi = (field_info*) (gnode->data);

	if (fi->hfinfo->id == sinfo->target_field) {
		g_array_append_val(sinfo->result_array, fi->value.boolean);
	}

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_numeric_value(GNode *gnode, gpointer data)
{
	GArray		*array = (GArray*)data;
	dfilter_node	*dnode = (dfilter_node*) (gnode->data);

	g_array_append_val(array, dnode->value.numeric);

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_ether_value(GNode *gnode, gpointer data)
{
	GArray		*array = (GArray*)data;
	dfilter_node	*dnode = (dfilter_node*) (gnode->data);

	g_array_append_val(array, dnode->value.ether);

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_bytes_value(GNode *gnode, gpointer data)
{
	GArray		*array = (GArray*)data;
	dfilter_node	*dnode = (dfilter_node*) (gnode->data);
	GByteArray	*barray = dnode->value.bytes;

	g_array_append_val(array, barray);

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}

gboolean fill_array_boolean_value(GNode *gnode, gpointer data)
{
	GArray		*array = (GArray*)data;
	dfilter_node	*dnode = (dfilter_node*) (gnode->data);

	g_array_append_val(array, dnode->value.boolean);

	return FALSE; /* FALSE = do not end traversal of GNode tree */
}


gboolean check_relation_numeric(gint operand, GArray *a, GArray *b)
{
	int	i, j, len_a, len_b;
	guint32	val_a;

	len_a = a->len;
	len_b = b->len;


	switch(operand) {
	case TOK_EQ:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a == g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_NE:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a != g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_GT:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a > g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_GE:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a >= g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_LT:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a < g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_LE:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a <= g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	default:
		g_assert_not_reached();
	}

	g_assert_not_reached();
	return FALSE;
}


gboolean check_relation_ether(gint operand, GArray *a, GArray *b)
{
	int	i, j, len_a, len_b;
	guint8	*ptr_a, *ptr_b;

	len_a = a->len;
	len_b = b->len;


	switch(operand) {
	case TOK_EQ:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index_ptr(a, 6, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index_ptr(b, 6, j);
				if (memcmp(ptr_a, ptr_b, 6) == 0)
					return TRUE;
			}
		}
		return FALSE;

	case TOK_NE:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index_ptr(a, 6, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index_ptr(b, 6, j);
				if (memcmp(ptr_a, ptr_b, 6) != 0)
					return TRUE;
			}
		}
		return FALSE;
	}

	g_assert_not_reached();
	return FALSE;
}

gboolean check_relation_bytes(gint operand, GArray *a, GArray *b)
{
	int	i, j, len_a, len_b;
	GByteArray	*ptr_a,*ptr_b;

	len_a = a->len;
	len_b = b->len;


	switch(operand) {
	case TOK_EQ:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index(a, GByteArray*, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index(b, GByteArray*, j);
				if (memcmp(ptr_a->data, ptr_b->data, bytes_length) == 0)
					return TRUE;
			}
		}
		return FALSE;

	case TOK_NE:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index(a, GByteArray*, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index(b, GByteArray*, j);
				if (memcmp(ptr_a->data, ptr_b->data, bytes_length) != 0)
					return TRUE;
			}
		}
		return FALSE;

	case TOK_GT:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index(a, GByteArray*, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index(b, GByteArray*, j);
				if (memcmp(ptr_a->data, ptr_b->data, bytes_length) > 0)
					return TRUE;
			}
		}
		return FALSE;

	case TOK_LT:
		for(i = 0; i < len_a; i++) {
			ptr_a = g_array_index(a, GByteArray*, i);
			for (j = 0; j < len_b; j++) {
				ptr_b = g_array_index(b, GByteArray*, j);
				if (memcmp(ptr_a->data, ptr_b->data, bytes_length) < 0)
					return TRUE;
			}
		}
		return FALSE;
	}

	g_assert_not_reached();
	return FALSE;
}

gboolean check_relation_boolean(gint operand, GArray *a, GArray *b)
{
	int	i, j, len_a, len_b;
	guint32	val_a;

	len_a = a->len;
	len_b = b->len;


	switch(operand) {
	case TOK_EQ:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a == g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	case TOK_NE:
		for(i = 0; i < len_a; i++) {
			val_a = g_array_index(a, guint32, i);
			for (j = 0; j < len_b; j++) {
				if (val_a != g_array_index(b, guint32, j))
					return TRUE;
			}
		}
		return FALSE;

	default:
		g_assert_not_reached();
	}

	g_assert_not_reached();
	return FALSE;
}

