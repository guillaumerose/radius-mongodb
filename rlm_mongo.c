/*
 * rlm_mongo.c
 *
 * Version:	$Id$
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Copyright 2000,2006  The FreeRADIUS server project
 * Copyright 2010 Guillaume Rose <guillaume.rose@gmail.com>
 * Copyright 2011 Roman Shterenzon <romanbsd@yahoo.com>
 */

#include <freeradius-devel/ident.h>
RCSID("$Id$")

#include <freeradius-devel/radiusd.h>
#include <freeradius-devel/modules.h>

#include "mongo.h"

#define MONGO_STRING_LENGTH 8196

typedef struct rlm_mongo_t {
	char	*ip;
	int		port;
	
	char	*base;
	char	*acct_base;
	char	*search_field;
	char	*username_field;
	char	*password_field;
	char	*mac_field;
	char	*enable_field;
} rlm_mongo_t;

static const CONF_PARSER module_config[] = {
  { "port", PW_TYPE_INTEGER, offsetof(rlm_mongo_t,port), NULL, "27017" },
  { "ip",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,ip), NULL, "127.0.0.1"},

  { "base",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,base), NULL,  ""},
  { "acct_base",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,acct_base), NULL,  ""},
  { "search_field",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,search_field), NULL,  ""},
  { "username_field",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,username_field), NULL,  ""},
  { "password_field",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,password_field), NULL,  ""},
  { "mac_field",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,mac_field), NULL,  ""},
  { "enable_field",  PW_TYPE_STRING_PTR, offsetof(rlm_mongo_t,enable_field), NULL,  ""},
  
  { NULL, -1, 0, NULL, NULL }		/* end the list */
};

mongo conn[1];

int mongo_start(rlm_mongo_t *data)
{
	if (mongo_connect(conn, data->ip, data->port)){
	  radlog(L_ERR, "rlm_mongodb: Failed to connect");
	  return 0;
	}

	radlog(L_DBG, "Connected to MongoDB");
	return 1;
}

void find_in_array(bson_iterator *it, char *key_ref, char *value_ref, char *key_needed, char *value_needed) 
{
	char value_ref_found[MONGO_STRING_LENGTH];
	char value_needed_found[MONGO_STRING_LENGTH];

	bson_iterator i;
	
	while(bson_iterator_next(it)) {
		switch(bson_iterator_type(it)){
			case BSON_STRING:
				if (strcmp(bson_iterator_key(it), key_ref) == 0)
					strcpy(value_ref_found, bson_iterator_string(it));
				if (strcmp(bson_iterator_key(it), key_needed) == 0)
					strcpy(value_needed_found, bson_iterator_string(it));
				break;
			case BSON_OBJECT:
			case BSON_ARRAY:
				bson_iterator_subiterator(it, &i);
				find_in_array(&i, key_ref, value_ref, key_needed, value_needed);
				break;
			default:
				break;
		}
	}
	
	if (strcmp(value_ref_found, value_ref) == 0)
		strcpy(value_needed, value_needed_found);
}

int find_radius_options(rlm_mongo_t *data, char *username, char *mac, char *password) 
{
	bson query;
	bson field;
	bson result;
	
	bson_init(&query);
	
	bson_append_string(&query, data->search_field, username);
	
	if (strcmp(data->mac_field, "") != 0) {
		bson_append_string(&query, data->mac_field, mac);
	}
	
	if (strcmp(data->enable_field, "") != 0) {
		bson_append_bool(&query, data->enable_field, 1);
	}
	bson_finish(&query);

	bson_empty(&field);
	bson_empty(&result);

	int res = mongo_find_one(conn, data->base, &query, &field, &result);
	bson_destroy(&query);

	if ( res == MONGO_ERROR && conn->err == MONGO_IO_ERROR ) {
		mongo_reconnect(conn);
		return 0;
	}
	
	bson_iterator it;
	bson_iterator_init(&it, &result);
	
	find_in_array(&it, data->username_field, username, data->password_field, password);
	return 1;
}

static int mongo_instantiate(CONF_SECTION *conf, void **instance)
{
	rlm_mongo_t *data;

	data = rad_malloc(sizeof(*data));
	if (!data) {
		return -1;
	}
	memset(data, 0, sizeof(*data));

	if (cf_section_parse(conf, data, module_config) < 0) {
		free(data);
		return -1;
	}
	
	mongo_start(data);

	*instance = data;

	return 0;
}

static void format_mac(char *in, char *out) {
	int i;
	for (i = 0; i < 6; i++) {
		out[3 * i] = in[2 * i];
		out[3 * i + 1] = in[2 * i + 1];
		out[3 * i + 2] = ':';
	}
	out[17] = '\0';
}

static int mongo_authorize(void *instance, REQUEST *request)
{
  if (request->username == NULL)
  	return RLM_MODULE_NOOP;
  	
  rlm_mongo_t *data = (rlm_mongo_t *) instance;
	
	char password[MONGO_STRING_LENGTH] = "";
	char mac[MONGO_STRING_LENGTH] = "";
	
	if (strcmp(data->mac_field, "") != 0) {
		char mac_temp[MONGO_STRING_LENGTH] = "";
		radius_xlat(mac_temp, MONGO_STRING_LENGTH, "%{Calling-Station-Id}", request, NULL);
		format_mac(mac_temp, mac);
  } 
	
	if (!find_radius_options(data, request->username->vp_strvalue, mac, password)) {
		return RLM_MODULE_REJECT; 
	}
	
	RDEBUG("Authorisation request by username -> \"%s\"\n", request->username->vp_strvalue);
	RDEBUG("Password found in MongoDB -> \"%s\"\n\n", password);
	
	VALUE_PAIR *vp;

	/* quiet the compiler */
	instance = instance;
	request = request;

 	vp = pairmake("Cleartext-Password", password, T_OP_SET);
 	if (!vp) return RLM_MODULE_FAIL;
 	
	pairmove(&request->config_items, &vp);
	pairfree(&vp);

	return RLM_MODULE_OK;
}

/* Saves accounting information */
static int mongo_account(void *instance, REQUEST *request)
{
	rlm_mongo_t *data = (rlm_mongo_t *)instance;
	bson buf;
	const char *attr;
	char value[MAX_STRING_LEN+1];
	VALUE_PAIR *vp = request->packet->vps;

	bson_init(&buf);
	bson_append_new_oid(&buf, "_id");

	while (vp) {
		attr = vp->name;
		switch (vp->type) {
			case PW_TYPE_INTEGER:
				bson_append_int(&buf, attr, vp->vp_integer & 0xffffff);
				break;
			case PW_TYPE_BYTE:
			case PW_TYPE_SHORT:
				bson_append_int(&buf, attr, vp->vp_integer);
				break;
			case PW_TYPE_DATE:
				bson_append_time_t(&buf, attr, vp->vp_date);
				break;
			default:
				vp_prints_value(value, sizeof(value), vp, 0);
				bson_append_string(&buf, attr, value);
				break;
		}
		vp = vp->next;
	}
	bson_finish(&buf);

	int res = mongo_insert(conn, data->acct_base, &buf);
	if (res != MONGO_OK) {
		radlog(L_ERR, "mongo_insert failed");
		return RLM_MODULE_FAIL;
	}
	RDEBUG("accounting record was inserted");

	bson_destroy(&buf);
	return RLM_MODULE_OK;
}

static int mongo_detach(void *instance)
{
	free(instance);
	return 0;
}

module_t rlm_mongo = {
	RLM_MODULE_INIT,
	"mongo",
	RLM_TYPE_THREAD_SAFE,	/* type */
	mongo_instantiate,		/* instantiation */
	mongo_detach,			/* detach */
	{
		NULL,			    /* authentication */
		mongo_authorize,	/* authorization */
		NULL,			    /* preaccounting */
		mongo_account,		/* accounting */
		NULL,			    /* checksimul */
		NULL,			    /* pre-proxy */
		NULL,			    /* post-proxy */
		NULL			    /* post-auth */
	},
};
