/*
 * lua_proto.c
 *
 * Ethereal's interface to the Lua Programming Language
 *
 * (c) 2006, Luis E. Garcia Ontanon <luis.ontanon@gmail.com>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "packet-lua.h"

LUA_CLASS_DEFINE(Proto,PROTO,if (! *p) luaL_error(L,"null Proto"));
LUA_CLASS_DEFINE(ProtoField,PROTO_FIELD,if (! *p) luaL_error(L,"null ProtoField"));
LUA_CLASS_DEFINE(ProtoFieldArray,PROTO_FIELD_ARRAY,if (! *p) luaL_error(L,"null ProtoFieldArray"));
LUA_CLASS_DEFINE(SubTreeType,SUB_TREE_TYPE,NOP);
LUA_CLASS_DEFINE(SubTreeTypeArray,SUB_TREE_TYPE_ARRAY,if (! *p) luaL_error(L,"null SubTreeTypeArray"));
LUA_CLASS_DEFINE(Dissector,DISSECTOR,NOP);
LUA_CLASS_DEFINE(DissectorTable,DISSECTOR_TABLE,NOP);


/*
 * ProtoField class
 */

static const eth_ft_types_t ftenums[] = {
{"FT_BOOLEAN",FT_BOOLEAN},
{"FT_UINT8",FT_UINT8},
{"FT_UINT16",FT_UINT16},
{"FT_UINT24",FT_UINT24},
{"FT_UINT32",FT_UINT32},
{"FT_UINT64",FT_UINT64},
{"FT_INT8",FT_INT8},
{"FT_INT16",FT_INT16},
{"FT_INT24",FT_INT24},
{"FT_INT32",FT_INT32},
{"FT_INT64",FT_INT64},
{"FT_FLOAT",FT_FLOAT},
{"FT_DOUBLE",FT_DOUBLE},
{"FT_STRING",FT_STRING},
{"FT_STRINGZ",FT_STRINGZ},
{"FT_ETHER",FT_ETHER},
{"FT_BYTES",FT_BYTES},
{"FT_UINT_BYTES",FT_UINT_BYTES},
{"FT_IPv4",FT_IPv4},
{"FT_IPv6",FT_IPv6},
{"FT_IPXNET",FT_IPXNET},
{"FT_FRAMENUM",FT_FRAMENUM},
{"FT_GUID",FT_GUID},
{"FT_OID",FT_OID},
{NULL,FT_NONE}
};

static enum ftenum get_ftenum(const gchar* type) {
    const eth_ft_types_t* ts;
    for (ts = ftenums; ts->str; ts++) {
        if ( g_str_equal(ts->str,type) ) {
            return ts->id;
        }
    }
    
    return FT_NONE;
}

static const gchar* ftenum_to_string(enum ftenum ft) {
    const eth_ft_types_t* ts;
    for (ts = ftenums; ts->str; ts++) {
        if ( ts->id == ft ) {
            return ts->str;
        }
    }
    
    return NULL;
}

struct base_display_string_t {
    const gchar* str;
    base_display_e base;
};

static const struct base_display_string_t base_displays[] = {
	{ "BASE_NONE", BASE_NONE},
	{"BASE_DEC", BASE_DEC},
	{"BASE_HEX", BASE_HEX},
	{"BASE_OCT", BASE_OCT},
	{"BASE_DEC_HEX", BASE_DEC_HEX},
	{"BASE_HEX_DEC", BASE_HEX_DEC},
	{NULL,0}
};

static const gchar* base_to_string(base_display_e base) {
    const struct base_display_string_t* b;
    for (b=base_displays;b->str;b++) {
        if ( base == b->base)
            return b->str;
    }
    return NULL;
}

static base_display_e string_to_base(const gchar* str) {
    const struct base_display_string_t* b;
    for (b=base_displays;b->str;b++) {
        if ( g_str_equal(str,b->str))
            return b->base;
    }
    return BASE_NONE;
}

static value_string* value_string_from_table(lua_State* L, int idx) {
    GArray* vs = g_array_new(TRUE,TRUE,sizeof(value_string));
    value_string* ret;
    
    if (!lua_istable(L,idx)) {
        luaL_argerror(L,idx,"must be a table");
        g_array_free(vs,TRUE);
        return NULL;
    }
    
    lua_pushnil(L);
    
    while (lua_next(L, idx) != 0) {
        value_string v = {0,NULL};
        
        if (! lua_isnumber(L,-2)) {
            luaL_argerror(L,idx,"All keys of a table used as vaalue_string must be integers");
            g_array_free(vs,TRUE);
            return NULL;
        }
        
        if (! lua_isstring(L,-1)) {
            luaL_argerror(L,idx,"All values of a table used as vaalue_string must be strings");
            g_array_free(vs,TRUE);
            return NULL;
        }
        
        v.value = (guint32)lua_tonumber(L,-2);
        v.strptr = g_strdup(lua_tostring(L,-1));
        
        g_array_append_val(vs,v);
        
        lua_pop(L, 1);
    }
    
    lua_pop(L, 1);
    
    ret = (value_string*)vs->data;
    
    g_array_free(vs,FALSE);

    return ret;
    
}    

static int ProtoField_new(lua_State* L) {
    ProtoField f = g_malloc(sizeof(eth_field_t));
    value_string* vs;
    
    /* will be using -2 as far as the field has not been added to an array then it will turn -1 */
    f->hfid = -2;
    f->name = g_strdup(luaL_checkstring(L,1));
    f->abbr = g_strdup(luaL_checkstring(L,2));
    f->type = get_ftenum(luaL_checkstring(L,3));
    
    if (f->type == FT_NONE) {
        luaL_argerror(L, 3, "invalid FT_type");
        return 0;
    }
    
    if (! lua_isnil(L,4) ) {
        vs = value_string_from_table(L,4);
        
        if (vs) {
            f->vs = vs;
        } else {
            g_free(f);
            return 0;
        }
        
        
    } else {
        f->vs = NULL;
    }
    
    /* XXX: need BASE_ERROR */
    f->base = string_to_base(luaL_optstring(L, 5, "BASE_NONE"));
    f->mask = luaL_optint(L, 6, 0x0);
    f->blob = g_strdup(luaL_optstring(L,7,""));
    
    pushProtoField(L,f);
    
    return 1;
}




static int ProtoField_integer(lua_State* L, enum ftenum type) {
    ProtoField f = g_malloc(sizeof(eth_field_t));
    const gchar* abbr = luaL_checkstring(L,1); 
    const gchar* name = luaL_optstring(L,2,abbr);
    const gchar* base = luaL_optstring(L, 3, "BASE_DEC");
    value_string* vs = (!lua_isnil(L,4)) ? value_string_from_table(L,4) : NULL;
    int mask = luaL_optint(L, 5, 0x0);
    const gchar* blob = luaL_optstring(L,6,"");


    f->hfid = -2;
    f->name = g_strdup(name);
    f->abbr = g_strdup(abbr);
    f->type = type;
    f->vs = vs;
    f->base = string_to_base(base);
    f->mask = mask;
    f->blob = g_strdup(blob);
    
    pushProtoField(L,f);
    
    return 1;
}

#define PROTOFIELD_INTEGER(lower,FT) static int ProtoField_##lower(lua_State* L) { return ProtoField_integer(L,FT); }
PROTOFIELD_INTEGER(uint8,FT_UINT8);
PROTOFIELD_INTEGER(uint16,FT_UINT16);
PROTOFIELD_INTEGER(uint24,FT_UINT24);
PROTOFIELD_INTEGER(uint32,FT_UINT32);
PROTOFIELD_INTEGER(uint64,FT_UINT64);
PROTOFIELD_INTEGER(int8,FT_INT8);
PROTOFIELD_INTEGER(int16,FT_INT8);
PROTOFIELD_INTEGER(int24,FT_INT8);
PROTOFIELD_INTEGER(int32,FT_INT8);
PROTOFIELD_INTEGER(int64,FT_INT8);
PROTOFIELD_INTEGER(framenum,FT_FRAMENUM);

static int ProtoField_other(lua_State* L,enum ftenum type) {
    ProtoField f = g_malloc(sizeof(eth_field_t));
    const gchar* abbr = luaL_checkstring(L,1); 
    const gchar* name = luaL_optstring(L,2,abbr);
    const gchar* blob = luaL_optstring(L,3,"");
    
    f->hfid = -2;
    f->name = g_strdup(name);
    f->abbr = g_strdup(abbr);
    f->type = type;
    f->vs = NULL;
    f->base = ( type == FT_FLOAT || type == FT_DOUBLE) ? BASE_DEC : BASE_NONE;
    f->mask = 0;
    f->blob = g_strdup(blob);
    
    pushProtoField(L,f);
    
    return 1;
}

#define PROTOFIELD_OTHER(lower,FT) static int ProtoField_##lower(lua_State* L) { return ProtoField_other(L,FT); }
PROTOFIELD_OTHER(ipv4,FT_IPv4);
PROTOFIELD_OTHER(ipv6,FT_IPv6);
PROTOFIELD_OTHER(ipx,FT_IPXNET);
PROTOFIELD_OTHER(ether,FT_ETHER);
PROTOFIELD_OTHER(bool,FT_BOOLEAN);
PROTOFIELD_OTHER(float,FT_FLOAT);
PROTOFIELD_OTHER(double,FT_DOUBLE);
PROTOFIELD_OTHER(string,FT_STRING);
PROTOFIELD_OTHER(stringz,FT_STRINGZ);
PROTOFIELD_OTHER(bytes,FT_BYTES);
PROTOFIELD_OTHER(ubytes,FT_UINT_BYTES);
PROTOFIELD_OTHER(guid,FT_GUID);
PROTOFIELD_OTHER(oid,FT_OID);


static int ProtoField_tostring(lua_State* L) {
    ProtoField f = checkProtoField(L,1);
    gchar* s = g_strdup_printf("ProtoField(%i): %s %s %s %s %p %.8x %s",f->hfid,f->name,f->abbr,ftenum_to_string(f->type),base_to_string(f->base),f->vs,f->mask,f->blob);
    
    lua_pushstring(L,s);
    g_free(s);
    
    return 1;
}

static int ProtoField_gc(lua_State* L) {
    ProtoField f = checkProtoField(L,1);

    /*
     * A garbage collector for ProtoFields makes little sense.
     * Even if This cannot be used anymore because it has gone out of scope, 
     * we can destroy the ProtoField only if it is not part of a ProtoFieldArray,
     * if it actualy belongs to one we need to preserve it as it is pointed by
     * a field array that may be registered afterwards causing a crash or memory corruption.
     */
    
    if (!f) {
        luaL_argerror(L,1,"BUG: ProtoField_gc called for something not ProtoField");
        /* g_assert() ?? */
    } else if (f->hfid == -2) {
        g_free(f->name);
        g_free(f->abbr);
        g_free(f->blob);
        g_free(f);
    }
    
    return 0;
}


static const luaL_reg ProtoField_methods[] = {
    {"new",   ProtoField_new},
    {"uint8",ProtoField_uint8},
    {"uint16",ProtoField_uint16},
    {"uint24",ProtoField_uint24},
    {"uint32",ProtoField_uint32},
    {"uint64",ProtoField_uint64},
    {"int8",ProtoField_int8},
    {"int16",ProtoField_int16},
    {"int24",ProtoField_int24},
    {"int32",ProtoField_int32},
    {"int64",ProtoField_int64},
    {"framenum",ProtoField_framenum},
    {"ipv4",ProtoField_ipv4},
    {"ipv6",ProtoField_ipv6},
    {"ipx",ProtoField_ipx},
    {"ether",ProtoField_ether},
    {"bool",ProtoField_bool},
    {"float",ProtoField_float},
    {"double",ProtoField_double},
    {"string",ProtoField_string},
    {"stringz",ProtoField_stringz},
    {"bytes",ProtoField_bytes},
    {"ubytes",ProtoField_ubytes},
    {"guid",ProtoField_guid},
    {"oid",ProtoField_oid},
    {0,0}
};

static const luaL_reg ProtoField_meta[] = {
    {"__gc", ProtoField_gc },
    {"__tostring", ProtoField_tostring },
    {0, 0}
};

int ProtoField_register(lua_State* L) {
    const eth_ft_types_t* ts;
    const struct base_display_string_t* b;
    
    luaL_openlib(L, PROTO_FIELD, ProtoField_methods, 0);
    luaL_newmetatable(L, PROTO_FIELD);
    luaL_openlib(L, 0, ProtoField_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    /* add a global FT_* variable for each FT_ type */
    for (ts = ftenums; ts->str; ts++) {
        lua_pushstring(L, ts->str);
        lua_pushstring(L, ts->str);
        lua_settable(L, LUA_GLOBALSINDEX);
    }
    
    /* add a global BASE_* variable for each BASE_ */
    for (b=base_displays;b->str;b++) {
        lua_pushstring(L, b->str);
        lua_pushstring(L, b->str);
        lua_settable(L, LUA_GLOBALSINDEX);
    }
    
    return 1;
}


/*
 * ProtoFieldArray class
 */


static int ProtoFieldArray_new(lua_State* L) {
    ProtoFieldArray fa;
    guint i;
    guint num_args = lua_gettop(L);
    
    fa = g_array_new(TRUE,TRUE,sizeof(hf_register_info));
    
    for ( i = 1; i <= num_args; i++) {
        ProtoField f = checkProtoField(L,i);
        hf_register_info hfri = { &(f->hfid), {f->name,f->abbr,f->type,f->base,VALS(f->vs),f->mask,f->blob,HFILL}};
        
        if (f->hfid != -2) {
            luaL_argerror(L, i, "field has already been added to an array");
            return 0;
        }
        
        f->hfid = -1;
        
        g_array_append_val(fa,hfri);
    }
    
    pushProtoFieldArray(L,fa);
    return 1;
}


static int ProtoFieldArray_add(lua_State* L) {
    ProtoFieldArray fa = checkProtoFieldArray(L,1);
    guint i;
    guint num_args = lua_gettop(L);
    
    for ( i = 2; i <= num_args; i++) {
        ProtoField f = checkProtoField(L,i);
        hf_register_info hfri = { &(f->hfid), {f->name,f->abbr,f->type,f->base,VALS(f->vs),f->mask,f->blob,HFILL}};
        
        if (f->hfid != -2) {
            luaL_argerror(L, i, "field has already been added to an array");
            return 0;
        }
        
        f->hfid = -1;
        
        g_array_append_val(fa,hfri);
    }
    
    return 0;
}

static int ProtoFieldArray_tostring(lua_State* L) {
    GString* s = g_string_new("ProtoFieldArray:\n");
    hf_register_info* f;
    ProtoFieldArray fa = checkProtoFieldArray(L,1);
    unsigned i;
    
    for(i = 0; i< fa->len; i++) {
        f = &(((hf_register_info*)(fa->data))[i]);
        g_string_sprintfa(s,"%i %s %s %s %u %p %.8x %s\n",*(f->p_id),f->hfinfo.name,f->hfinfo.abbrev,ftenum_to_string(f->hfinfo.type),f->hfinfo.display,f->hfinfo.strings,f->hfinfo.bitmask,f->hfinfo.blurb);
    };
    
    lua_pushstring(L,s->str);
    g_string_free(s,TRUE);
    
    return 1;
}

static int ProtoFieldArray_gc(lua_State* L) {
    ProtoFieldArray fa = checkProtoFieldArray(L,1);
    gboolean free_it = FALSE;
    
    /* we'll keep the data if the array was registered to a protocol */
    if (fa->len) {
        hf_register_info* f = (hf_register_info*)fa->data;
        
        if ( *(f->p_id) == -1)
            free_it = TRUE;
    } else {
        free_it = TRUE;        
    }
    
    g_array_free(fa,free_it);
    
    return 0;
}


static const luaL_reg ProtoFieldArray_methods[] = {
    {"new",   ProtoFieldArray_new},
    {"add",   ProtoFieldArray_add},
    {0,0}
};

static const luaL_reg ProtoFieldArray_meta[] = {
    {"__gc",       ProtoFieldArray_gc},
    {"__tostring", ProtoFieldArray_tostring},
    {0, 0}
};

int ProtoFieldArray_register(lua_State* L) {
    luaL_openlib(L, PROTO_FIELD_ARRAY, ProtoFieldArray_methods, 0);
    luaL_newmetatable(L, PROTO_FIELD_ARRAY);
    luaL_openlib(L, 0, ProtoFieldArray_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
}




/*
 * SubTreeType class
 */


static int SubTreeType_new(lua_State* L) {
    SubTreeType e = g_malloc(sizeof(int));
    *e = -2;
    pushSubTreeType(L,e);
    
    return 1;
}

static int SubTreeType_tostring(lua_State* L) {
    SubTreeType e = checkSubTreeType(L,1);
    gchar* s = g_strdup_printf("SubTreeType: %i",*e);
    
    lua_pushstring(L,s);
    g_free(s);
    
    return 1;
}


static const luaL_reg SubTreeType_methods[] = {
    {"new",   SubTreeType_new},
    {0,0}
};

static const luaL_reg SubTreeType_meta[] = {
    {"__tostring", SubTreeType_tostring},
    {0, 0}
};

int SubTreeType_register(lua_State* L) {
    luaL_openlib(L, SUB_TREE_TYPE, SubTreeType_methods, 0);
    luaL_newmetatable(L, SUB_TREE_TYPE);
    luaL_openlib(L, 0, SubTreeType_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
}





/*
 * SubTreeTypeArray class
 */

static int SubTreeTypeArray_new(lua_State* L) {
    SubTreeTypeArray ea = g_array_new(TRUE,TRUE,sizeof(gint*));
    guint i;
    guint num_args = lua_gettop(L);
    
    for (i = 1; i <= num_args; i++) {
        SubTreeType e = checkSubTreeType(L,i);
        
        if(*e != -2) {
            luaL_argerror(L, i, "SubTree has already been added to an array");
            return 0;
        }
        
        *e = -1;
        
        g_array_append_val(ea,e);
    }
    
    pushSubTreeTypeArray(L,ea);
    return 1;
}


static int SubTreeTypeArray_add(lua_State* L) {
    SubTreeTypeArray ea = checkSubTreeTypeArray(L,1);
    guint i;
    guint num_args = lua_gettop(L);
    
    for (i = 2; i <= num_args; i++) {
        SubTreeType e = checkSubTreeType(L,i);
        if(*e != -2) {
            luaL_argerror(L, i, "SubTree has already been added to an array");
            return 0;
        }
        
        *e = -1;
        
        g_array_append_val(ea,e);
    }
    
    return 0;
}

static int SubTreeTypeArray_tostring(lua_State* L) {
    GString* s = g_string_new("SubTreeTypeArray:\n");
    SubTreeTypeArray ea = checkSubTreeTypeArray(L,1);
    unsigned i;
    
    for(i = 0; i< ea->len; i++) {
        gint ett = *(((gint**)(ea->data))[i]);
        g_string_sprintfa(s,"%i\n",ett);
    };
    
    lua_pushstring(L,s->str);
    g_string_free(s,TRUE);
    
    return 1;
}

static int SubTreeTypeArray_register_to_ethereal(lua_State* L) {
    SubTreeTypeArray ea = checkSubTreeTypeArray(L,1);
    
    if (!ea->len) {
        luaL_argerror(L,1,"empty array");
        return 0;
    }
    
    /* is last ett -1? */
    if ( *(((gint *const *)ea->data)[ea->len -1])  != -1) {
        luaL_argerror(L,1,"array has been registered already");
        return 0;
    }
    
    proto_register_subtree_array((gint *const *)ea->data, ea->len);
    return 0;
}

static int SubTreeTypeArray_gc(lua_State* L) {
    SubTreeTypeArray ea = checkSubTreeTypeArray(L,1);
    
    /* XXX - free the array if unregistered */
    g_array_free(ea,FALSE);
    
    return 0;
}


static const luaL_reg SubTreeTypeArray_methods[] = {
    {"new",   SubTreeTypeArray_new},
    {"add",   SubTreeTypeArray_add},
    {"register",   SubTreeTypeArray_register_to_ethereal},
    {0,0}
};

static const luaL_reg SubTreeTypeArray_meta[] = {
    {"__gc",       SubTreeTypeArray_gc},
    {"__tostring", SubTreeTypeArray_tostring},
    {0, 0}
};

int SubTreeTypeArray_register(lua_State* L) {
    luaL_openlib(L, SUB_TREE_TYPE_ARRAY, SubTreeTypeArray_methods, 0);
    luaL_newmetatable(L, SUB_TREE_TYPE_ARRAY);
    luaL_openlib(L, 0, SubTreeTypeArray_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
}




/*
 * Proto class
 */


static int Proto_new(lua_State* L) {
    const gchar* name = luaL_checkstring(L,1);
    const gchar* desc = luaL_optstring(L,2,"");
    
    if ( name ) {
        if ( proto_get_id_by_filter_name(name) > 0 ) { 
            luaL_argerror(L,1,"Protocol exists already");
            return 0;
        } else {
            Proto proto = g_malloc(sizeof(eth_proto_t));
            
            /* XXX - using the same name and filtername to have to deal just with one name */
            proto->name = g_strdup(name);
            proto->filter = proto->name;
            proto->desc = g_strdup(desc);
            proto->hfarray = NULL;
            proto->prefs_module = NULL;
            proto->prefs = NULL;
            proto->is_postdissector = FALSE;
            proto->hfid = proto_register_protocol(proto->desc,proto->name,proto->filter);
            proto->handle = create_dissector_handle(dissect_lua,proto->hfid);
            
            pushProto(L,proto);
            return 1;
        }
     } else {
        luaL_argerror(L,1,"missing name");
        return 0;
     }
}

static int Proto_register_field_array(lua_State* L) {
    Proto proto = checkProto(L,1);
    ProtoFieldArray fa = checkProtoFieldArray(L,2);
    
    if (!proto) {
        luaL_argerror(L,1,"not a good proto");
        return 0;
    }

    if (! fa) {
        luaL_argerror(L,2,"not a good field_array");
        return 0;
    }

    if( ! fa->len ) {
        luaL_argerror(L,2,"empty field_array");
        return 0;
    }

    if (proto->hfarray) {
        luaL_argerror(L,1,"field_array already registered for this protocol");
    }
    
    if ( *(((hf_register_info*)(fa->data))->p_id) != -1 ) {
        luaL_argerror(L,1,"this field_array has been already registered to another protocol");
    }
        
    proto->hfarray = (hf_register_info*)(fa->data);
    proto_register_field_array(proto->hfid,proto->hfarray,fa->len);
    
    return 0;
}

static int Proto_add_uint_pref(lua_State* L) {
    Proto proto = checkProto(L,1);
    gchar* abbr = g_strdup(luaL_checkstring(L,2));
    guint def = (guint)luaL_optint(L,3,0);
    guint base = (guint)luaL_optint(L,4,10);
    gchar* name = g_strdup(luaL_optstring (L, 5, ""));
    gchar* desc = g_strdup(luaL_optstring (L, 6, ""));
    
    eth_pref_t* pref = g_malloc(sizeof(eth_pref_t));
    pref->name = abbr;
    pref->type = PREF_UINT;
    pref->value.u = def;
    pref->next = NULL;
    
    if (! proto->prefs_module)
        proto->prefs_module = prefs_register_protocol(proto->hfid, NULL);
    
    if (! proto->prefs) {
        proto->prefs = pref;
    } else {
        eth_pref_t* p;
        for (p = proto->prefs; p->next; p = p->next) ;
        p->next = pref;
    }
    
    prefs_register_uint_preference(proto->prefs_module, abbr,name,
                                   desc, base, &(pref->value.u));
    
    return 0;
}

static int Proto_add_bool_pref(lua_State* L) {
    Proto proto = checkProto(L,1);
    gchar* abbr = g_strdup(luaL_checkstring(L,2));
    gboolean def = (gboolean)luaL_optint(L,3,FALSE);
    gchar* name = g_strdup(luaL_optstring (L, 4, ""));
    gchar* desc = g_strdup(luaL_optstring (L, 5, ""));
    
    eth_pref_t* pref = g_malloc(sizeof(eth_pref_t));
    pref->name = abbr;
    pref->type = PREF_BOOL;
    pref->value.b = def;
    pref->next = NULL;
    
    if (! proto->prefs_module)
        proto->prefs_module = prefs_register_protocol(proto->hfid, NULL);
    
    if (! proto->prefs) {
        proto->prefs = pref;
    } else {
        eth_pref_t* p;
        for (p = proto->prefs; p->next; p = p->next) ;
        p->next = pref;
    }
    
    prefs_register_bool_preference(proto->prefs_module, abbr,name,
                                   desc, &(pref->value.b));
        
    return 0;
}

static int Proto_add_string_pref(lua_State* L) {
    Proto proto = checkProto(L,1);
    gchar* abbr = g_strdup(luaL_checkstring(L,2));
    gchar* def = g_strdup(luaL_optstring (L, 3, ""));
    gchar* name = g_strdup(luaL_optstring (L, 4, ""));
    gchar* desc = g_strdup(luaL_optstring (L, 5, ""));
    
    eth_pref_t* pref = g_malloc(sizeof(eth_pref_t));
    pref->name = abbr;
    pref->type = PREF_STRING;
    pref->value.s = def;
    pref->next = NULL;
    
    if (! proto->prefs_module)
        proto->prefs_module = prefs_register_protocol(proto->hfid, NULL);
    
    if (! proto->prefs) {
        proto->prefs = pref;
    } else {
        eth_pref_t* p;
        for (p = proto->prefs; p->next; p = p->next) ;
        p->next = pref;
    }
    
    prefs_register_string_preference(proto->prefs_module, abbr,name,
                                     desc, &(pref->value.s));
        
    
    return 0;
}

static int Proto_get_pref(lua_State* L) {
    Proto proto = checkProto(L,1);
    const gchar* abbr = luaL_checkstring(L,2);

    if (!proto) {
        luaL_argerror(L,1,"not a good proto");
        return 0;
    }
    
    if (!abbr) {
        luaL_argerror(L,2,"not a good abbrev");
        return 0;
    }
    
    if (proto->prefs) {
        eth_pref_t* p;
        for (p = proto->prefs; p; p = p->next) {
            if (g_str_equal(p->name,abbr)) {
                switch(p->type) {
                    case PREF_BOOL:
                        lua_pushboolean(L, p->value.b);
                        break;
                    case PREF_UINT:
                        lua_pushnumber(L, (lua_Number)(p->value.u));
                        break;
                    case PREF_STRING:
                        lua_pushstring(L, p->value.s);
                        break;
                }
                return 1;
            }
        }
        
        luaL_argerror(L,2,"no such preference for this protocol");
        return 0;
        
    } else {
        luaL_error(L,"no preferences set for this protocol");
        return 0;
    }
}


static int Proto_tostring(lua_State* L) { 
    Proto proto = checkProto(L,1);
    gchar* s;
    
    if (!proto) return 0;
    
    s = g_strdup_printf("Proto: %s",proto->name);
    lua_pushstring(L,s);
    g_free(s);
    
    return 1;
}

static int Proto_register_postdissector(lua_State* L) { 
    Proto proto = checkProto(L,1);
    if (!proto) return 0;
    
    if(!proto->is_postdissector) {
        register_postdissector(proto->handle);
    } else {
        luaL_argerror(L,1,"this protocol is already registered as postdissector");
    }
    
    return 0;
}


static int Proto_get_dissector(lua_State* L) { 
    Proto proto = checkProto(L,1);
    if (!proto) return 0;
    
    if (proto->handle) {
        pushDissector(L,proto->handle);
        return 1;
    } else {
        luaL_error(L,"The protocol hasn't been registered yet");
        return 0;
    }
}

static const luaL_reg Proto_methods[] = {
    {"new",   Proto_new},
    {"register_field_array",   Proto_register_field_array},
    {"add_uint_pref",   Proto_add_uint_pref},
    {"add_bool_pref",   Proto_add_bool_pref},
    {"add_string_pref",   Proto_add_string_pref},
    {"get_pref",   Proto_get_pref},
    {"register_as_postdissector",   Proto_register_postdissector},
    {"get_dissector",   Proto_get_dissector},
    {0,0}
};

static const luaL_reg Proto_meta[] = {
    {"__tostring", Proto_tostring},
    {0, 0}
};

int Proto_register(lua_State* L) {
    luaL_openlib(L, PROTO, Proto_methods, 0);
    luaL_newmetatable(L, PROTO);
    luaL_openlib(L, 0, Proto_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
}

/*
 * Dissector class
 */

static int Dissector_get (lua_State *L) {
    const gchar* name = luaL_checkstring(L,1);
    Dissector d;
    
    if (!name) {
        return 0;
    }
    
    if ((d = find_dissector(name))) {
        pushDissector(L, d);
        return 1;
    } else {
        luaL_argerror(L,1,"No such dissector");
        return 0;
    }
    
}

static int Dissector_call(lua_State* L) {
    Dissector d = checkDissector(L,1);
    Tvb tvb = checkTvb(L,2);
    Pinfo pinfo = checkPinfo(L,3);
    ProtoTree tree = checkProtoTree(L,4);
    
    if (! ( d && tvb && pinfo) ) return 0;
    
    TRY {
        call_dissector(d, tvb, pinfo, tree);
    } CATCH(ReportedBoundsError) {
        proto_tree_add_protocol_format(lua_tree, lua_malformed, lua_tvb, 0, 0, "[Malformed Frame: Packet Length]" );
        luaL_error(L,"Malformed Frame");
        return 0;
    } ENDTRY;
    
    return 0;
}


static int Dissector_tostring(lua_State* L) {
    Dissector d = checkDissector(L,1);
    if (!d) return 0;
    lua_pushstring(L,dissector_handle_get_short_name(d));
    return 1;
}

static const luaL_reg Dissector_methods[] = {
    {"get", Dissector_get },
    {"call", Dissector_call },
    {0,0}
};

static const luaL_reg Dissector_meta[] = {
    {"__tostring", Dissector_tostring},
    {0, 0}
};

int Dissector_register(lua_State* L) {
    luaL_openlib(L, DISSECTOR, Dissector_methods, 0);
    luaL_newmetatable(L, DISSECTOR);
    luaL_openlib(L, 0, Dissector_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
};


/*
 * DissectorTable class
 */
static int DissectorTable_new (lua_State *L) {
    gchar* name = (void*)luaL_checkstring(L,1);
    gchar* ui_name = (void*)luaL_optstring(L,2,name);
    const gchar* ftstr = luaL_optstring(L,3,"FT_UINT32");
    enum ftenum type;
    base_display_e base = luaL_optint(L,4,BASE_DEC);
    
    if(!(name && ui_name && ftstr)) return 0;
    
    name = g_strdup(name);
    ui_name = g_strdup(ui_name);
    type = get_ftenum(ftstr);
    
    switch(type) {
        case FT_STRING:
            base = BASE_NONE;
        case FT_UINT8:
        case FT_UINT16:
        case FT_UINT24:
        case FT_UINT32:
        {
            DissectorTable dt = g_malloc(sizeof(struct _eth_distbl_t));
            
            dt->table = register_dissector_table(name, ui_name, type, base);
            dt->name = name;
            pushDissectorTable(L, dt);
        }
            return 1;
        default:
            luaL_argerror(L,3,"Invalid ft_type");
            return 0;
    }
    
}

static int DissectorTable_get (lua_State *L) {
    const gchar* name = luaL_checkstring(L,1);
    dissector_table_t table;
    
    if(!name) return 0;
    
    table = find_dissector_table(name);
    
    if (table) {
        DissectorTable dt = g_malloc(sizeof(struct _eth_distbl_t));
        dt->table = table;
        dt->name = g_strdup(name);
        
        pushDissectorTable(L, dt);
        
        return 1;
    } else {
        luaL_error(L,"No such dissector_table");
        return 0;
    }
    
}


static int DissectorTable_add (lua_State *L) {
    DissectorTable dt = checkDissectorTable(L,1);
    Proto p = checkProto(L,3);
    ftenum_t type;
    
    if (!(dt && p)) return 0;
    
    type = get_dissector_table_selector_type(dt->name);
    
    if (type == FT_STRING) {
        gchar* pattern = g_strdup(luaL_checkstring(L,2));
        dissector_add_string(dt->name, pattern,p->handle);
    } else if ( type == FT_UINT32 || type == FT_UINT16 || type ==  FT_UINT8 || type ==  FT_UINT24 ) {
        int port = luaL_checkint(L, 2);
        dissector_add(dt->name, port, p->handle);
    }
    
    return 0;
}

static int DissectorTable_remove (lua_State *L) {
    DissectorTable dt = checkDissectorTable(L,1);
    Proto p = checkProto(L,3);
    ftenum_t type;
    
    if (!(dt && p)) return 0;
    
    type = get_dissector_table_selector_type(dt->name);
    
    if (type == FT_STRING) {
        gchar* pattern = g_strdup(luaL_checkstring(L,2));
        dissector_delete_string(dt->name, pattern,p->handle);
    } else if ( type == FT_UINT32 || type == FT_UINT16 || type ==  FT_UINT8 || type ==  FT_UINT24 ) {
        int port = luaL_checkint(L, 2);
        dissector_delete(dt->name, port, p->handle);
    }
    
    return 0;
}


static int DissectorTable_try (lua_State *L) {
    DissectorTable dt = checkDissectorTable(L,1);
    Tvb tvb = checkTvb(L,3);
    Pinfo pinfo = checkPinfo(L,4);
    ProtoTree tree = checkProtoTree(L,5);
    ftenum_t type;
    
    if (! (dt && tvb && pinfo && tree) ) return 0;
    
    type = get_dissector_table_selector_type(dt->name);
    
    if (type == FT_STRING) {
        const gchar* pattern = luaL_checkstring(L,2);
        
        if (!pattern) return 0;
        
        TRY {
            if (dissector_try_string(dt->table,pattern,tvb,pinfo,tree))
                return 0;
        } CATCH(ReportedBoundsError) {
            proto_tree_add_protocol_format(lua_tree, lua_malformed, lua_tvb, 0, 0, "[Malformed Frame: Packet Length]" );
            luaL_error(L,"Malformed Frame");
            return 0;
        } ENDTRY;
        
    } else if ( type == FT_UINT32 || type == FT_UINT16 || type ==  FT_UINT8 || type ==  FT_UINT24 ) {
        int port = luaL_checkint(L, 2);

        TRY {
            if (dissector_try_port(dt->table,port,tvb,pinfo,tree))
                return 0;
        } CATCH(ReportedBoundsError) {
            proto_tree_add_protocol_format(lua_tree, lua_malformed, lua_tvb, 0, 0, "[Malformed Frame: Packet Length]" );
            luaL_error(L,"Malformed Frame");
            return 0;
        } ENDTRY;
        
    } else {
        luaL_error(L,"No such type of dissector_table");
    }
    
    call_dissector(lua_data_handle,tvb,pinfo,tree);
    return 0;
}

static int DissectorTable_get_dissector (lua_State *L) {
    DissectorTable dt = checkDissectorTable(L,1);
    ftenum_t type;
    dissector_handle_t handle = lua_data_handle;
    
    if (!dt) return 0;
    
    type = get_dissector_table_selector_type(dt->name);
    
    if (type == FT_STRING) {
        const gchar* pattern = luaL_checkstring(L,2);
        handle = dissector_get_string_handle(dt->table,pattern);
    } else if ( type == FT_UINT32 || type == FT_UINT16 || type ==  FT_UINT8 || type ==  FT_UINT24 ) {
        int port = luaL_checkint(L, 2);
        handle = dissector_get_port_handle(dt->table,port);
    }
    
    pushDissector(L,handle);
    
    return 1;
    
}


static int DissectorTable_tostring(lua_State* L) {
    DissectorTable dt = checkDissectorTable(L,1);
    GString* s;
    ftenum_t type;
    
    if (!dt) return 0;
    
    type =  get_dissector_table_selector_type(dt->name);
    s = g_string_new("DissectorTable ");
    
    switch(type) {
        case FT_STRING:
        {
            g_string_sprintfa(s,"%s String:\n",dt->name);
            break;
        }
        case FT_UINT8:
        case FT_UINT16:
        case FT_UINT24:
        case FT_UINT32:
        {
            int base = get_dissector_table_base(dt->name);
            g_string_sprintfa(s,"%s Integer(%i):\n",dt->name,base);
            break;
        }
        default:
            luaL_error(L,"Strange table type");
    }            
    
    lua_pushstring(L,s->str);
    g_string_free(s,TRUE);
    return 1;
}

static const luaL_reg DissectorTable_methods[] = {
    {"new", DissectorTable_new },
    {"get", DissectorTable_get },
    {"add", DissectorTable_add },
    {"remove", DissectorTable_remove },
    {"try", DissectorTable_try },
    {"get_dissector", DissectorTable_get_dissector },
    {0,0}
};

static const luaL_reg DissectorTable_meta[] = {
    {"__tostring", DissectorTable_tostring},
    {0, 0}
};

int DissectorTable_register(lua_State* L) {
    luaL_openlib(L, DISSECTOR_TABLE, DissectorTable_methods, 0);
    luaL_newmetatable(L, DISSECTOR_TABLE);
    luaL_openlib(L, 0, DissectorTable_meta, 0);
    lua_pushliteral(L, "__index");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pushliteral(L, "__metatable");
    lua_pushvalue(L, -3);
    lua_rawset(L, -3);
    lua_pop(L, 1);
    
    return 1;
};



