/*
 * Copyright (c) 2025, Oracle and/or its affiliates.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
/**
 * adaptived file for sharing data between causes and effects
 */

#include <adaptived.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#include "adaptived-internal.h"
#include "shared_data.h"
#include "cause.h"

int write_sdata_cgroup_setting_value(struct adaptived_cause * const cse,
				     const char * const cgroup_name,
				     const char * const setting,
				     const struct adaptived_cgroup_value * const value,
				     uint32_t flags)
{
	struct adaptived_cgroup_setting_and_value *sdata = NULL;
	int ret;

	if (!cse || !cgroup_name || !setting || !value)
		return -EINVAL;

	sdata = malloc(sizeof(struct adaptived_cgroup_setting_and_value));
	if (!sdata)
		return -ENOMEM;
	memset(sdata, 0, sizeof(struct adaptived_cgroup_setting_and_value));

	sdata->cgroup_name = strdup(cgroup_name);
	if (!sdata->cgroup_name) {
		ret = -ENOMEM;
		goto error;
	}

	sdata->setting = strdup(setting);
	if (!sdata->setting) {
		ret = -ENOMEM;
		goto error;
	}

	sdata->value = malloc(sizeof(struct adaptived_cgroup_value));
	if (!sdata->value) {
		ret = -ENOMEM;
		goto error;
	}

	memcpy(sdata->value, value, sizeof(struct adaptived_cgroup_value));

	ret = adaptived_write_shared_data(cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE,
					  sdata, NULL, flags);

	return ret;

error:
	if (sdata->cgroup_name)
		free(sdata->cgroup_name);
	if (sdata->setting)
		free(sdata->setting);
	if (sdata->value)
		free(sdata->value);

	return ret;
}

static int insert_into_field_obj(struct json_object * const parent_obj, const char * const field,
				 const char * const key, struct json_object * const insert_obj)
{
	struct json_object *field_obj;
	json_bool exists;
	int ret;

	exists = json_object_object_get_ex(parent_obj, field, &field_obj);
	if (!exists || !field_obj)
		/*
		 * The field object doesn't exist.  We'll create a local one
		 * then add it at the end
		 */
		field_obj = json_object_new_object();

	ret = json_object_object_add(field_obj, key, insert_obj);
	if (ret) {
		adaptived_err("Failed to add key %s to %s object\n", field, key);
		return ret;
	}

	if (!exists) {
		ret = json_object_object_add(parent_obj, field, field_obj);
		if (ret) {
			adaptived_err("Failed to add %s object\n", field);
			return ret;
		}
	}

	return 0;
}

static int name_value_to_json(struct json_object * const parent_obj,
			      const char * const requested_name,
			      const struct adaptived_name_and_value * const data)
{
	struct json_object *value_obj = NULL;
	int ret;

	if (requested_name && strcmp(requested_name, data->name) != 0)
		/*
		 * A specific name was requested, but this shared data object
		 * contains a different name.  Skip it
		 */
		return 0;

	switch(data->value->type) {
	case ADAPTIVED_CGVAL_STR:
		value_obj = json_object_new_string(data->value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		value_obj = json_object_new_int(data->value->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		value_obj = json_object_new_double(data->value->value.float_value);
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	if (!value_obj) {
		ret = -EINVAL;
		goto err;
	}

	ret = json_object_object_add(parent_obj, data->name, value_obj);
	if (ret) {
		adaptived_err("Failed to add object %s\n", data->name);
		return ret;
	}

	return ret;

err:
	if (value_obj)
		json_object_put(value_obj);

	return ret;
}

static int cgroup_setting_value_to_json(struct json_object * const parent_obj,
					const char * const setting,
					const struct adaptived_cgroup_setting_and_value * const data)
{
	struct json_object *value_obj = NULL;
	const char *field;
	int ret;

	if (setting && strcmp(setting, data->setting) != 0)
		/*
		 * A specific cgroup setting was requested, but this shared data
		 * object contains a different setting.  Skip it
		 */
		return 0;

	if (setting)
		field = setting;
	else
		field = data->setting;

	switch(data->value->type) {
	case ADAPTIVED_CGVAL_STR:
		value_obj = json_object_new_string(data->value->value.str_value);
		break;
	case ADAPTIVED_CGVAL_LONG_LONG:
		value_obj = json_object_new_int(data->value->value.ll_value);
		break;
	case ADAPTIVED_CGVAL_FLOAT:
		value_obj = json_object_new_double(data->value->value.float_value);
		break;
	default:
		ret = -EINVAL;
		goto err;
	}
	if (!value_obj) {
		ret = -EINVAL;
		goto err;
	}

	ret = insert_into_field_obj(parent_obj, field, data->cgroup_name, value_obj);
	if (ret)
		goto err;

	return ret;

err:
	if (value_obj)
		json_object_put(value_obj);

	return ret;
}

API int adaptived_sdata_to_json(struct adaptived_cause * const cse,
				const char * const field, struct json_object **json_obj)
{
	struct json_object *parent_obj = NULL;
	enum adaptived_sdata_type stype;
	struct shared_data *cur;
	int ret;

	if (!cse || !json_obj)
		return -EINVAL;

	if ((*json_obj) != NULL)
		return -EINVAL;

	if (!cse->sdata)
		return -ENODATA;

	parent_obj = json_object_new_object();
	if (!parent_obj)
		return -ENOMEM;

	cur = cse->sdata;
	stype = cur->type;

	while (cur) {
		if (cur->type != stype) {
			/*
			 * We currently can only convert the same shared data
			 * type in a single json object.
			 */
			ret = -ENOTSUP;
			goto err;
		}

		switch(cur->type) {
		case ADAPTIVED_SDATA_NAME_VALUE:
			ret = name_value_to_json(parent_obj, field, cur->data);
			if (ret)
				goto err;
			break;
		case ADAPTIVED_SDATA_CGROUP_SETTING_VALUE:
			ret = cgroup_setting_value_to_json(parent_obj, field, cur->data);
			if (ret)
				goto err;
			break;
		default:
			ret = -ENOTSUP;
			goto err;
		}

		cur = cur->next;
	}

	*json_obj = parent_obj;

	return ret;

err:
	if (parent_obj)
		json_object_put(parent_obj);

	return ret;
}

/*
 * Method for a cause to share data with effect(s) in the same rule.
 *
 * Note that the shared data is deleted at the end of each run of the
 * adaptived main loop
 */
API int adaptived_write_shared_data(struct adaptived_cause * const cse,
				    enum adaptived_sdata_type type, void *data,
				    adaptived_sdata_free free_fn,
				    uint32_t flags)
{
	struct shared_data *sdata, *prev;

	if (!cse || !data)
		return -EINVAL;

	if (type < 0)
		return -EINVAL;
	if (type >= ADAPTIVED_SDATA_CNT)
		return -EINVAL;
	if (type == ADAPTIVED_SDATA_CUSTOM && free_fn == NULL)
		return -EINVAL;
	if (type != ADAPTIVED_SDATA_CUSTOM && free_fn != NULL)
		return -EINVAL;

	sdata = malloc(sizeof(struct shared_data));
	if (!sdata)
		return -ENOMEM;

	sdata->type = type;
	sdata->data = data;
	sdata->free_fn = free_fn;
	sdata->flags = flags;
	sdata->next = NULL;

	if (cse->sdata == NULL) {
		cse->sdata = sdata;
	} else {
		prev = cse->sdata;

		while (prev != NULL) {
			if (prev->next == NULL)
				break;

			prev = prev->next;
		}

		prev->next = sdata;
	}

	return 0;
}

API int adaptived_update_shared_data(struct adaptived_cause * const cse, int index,
				     enum adaptived_sdata_type type, void *data,
				     uint32_t flags)
{
	struct shared_data *sdata;

	if (cse == NULL || data == NULL)
		return -EINVAL;

	if (index < 0)
		return -EINVAL;

	sdata = cse->sdata;

	if (sdata == NULL)
		return -ERANGE;

	while (index > 0) {
		if (sdata->next == NULL)
			return -ERANGE;

		sdata = sdata->next;
		index--;
	}

	if (sdata->type != type)
		/* Don't allow the changing of the data type */
		return -EINVAL;

	/*
	 * It's up to the user to ensure that the old data field is properly freed and not
	 * leaked
	 */
	sdata->data = data;
	sdata->flags = flags;

	return 0;
}

API int adaptived_get_shared_data_cnt(const struct adaptived_cause * const cse)
{
	struct shared_data *sdata = NULL;
	int cnt = 0;

	if (cse == NULL)
		return 0;

	sdata = cse->sdata;

	while (sdata) {
		cnt++;
		sdata = sdata->next;
	}

	return cnt;
}

API int adaptived_get_shared_data(const struct adaptived_cause * const cse, int index,
				  enum adaptived_sdata_type * const type, void **data,
				  uint32_t * const flags)
{
	struct shared_data *sdata;

	if (cse == NULL || type == NULL || data == NULL || flags == NULL)
		return -EINVAL;

	if (index < 0)
		return -EINVAL;

	sdata = cse->sdata;

	if (sdata == NULL)
		return -ERANGE;

	while (index > 0) {
		if (sdata->next == NULL)
			return -ERANGE;

		sdata = sdata->next;
		index--;
	}

	*type = sdata->type;
	*data = sdata->data;
	*flags = sdata->flags;

	return 0;
}

/* TODO - add locking around the sdata structure */
API void free_shared_data(struct adaptived_cause * const cse, bool force_delete)
{
	struct shared_data *cur, *next, *prev_valid = NULL, *first_valid = NULL;
	bool do_free, persist;

	if (cse == NULL)
		return;

	if (cse->sdata == NULL)
		return;

	cur = cse->sdata;

	while (cur != NULL) {
		next = cur->next;

		persist = (bool)(cur->flags & ADAPTIVED_SDATAF_PERSIST);

		do_free = force_delete || !persist;

		if (!do_free) {
			if (!first_valid)
				first_valid = cur;

			if (prev_valid)
				prev_valid->next = cur;

			prev_valid = cur;
			cur = next;
			continue;
		}

		switch(cur->type) {
		case ADAPTIVED_SDATA_CUSTOM:
			(*cur->free_fn)(cur->data);
			break;
		case ADAPTIVED_SDATA_CGROUP:
			adaptived_free_cgroup_value(cur->data);
			free(cur->data);
			break;
		case ADAPTIVED_SDATA_NAME_VALUE:
			struct adaptived_name_and_value *name_value;

			name_value = (struct adaptived_name_and_value *)cur->data;

			free(name_value->name);
			adaptived_free_cgroup_value(name_value->value);
			free(cur->data);
			break;
		case ADAPTIVED_SDATA_CGROUP_SETTING_VALUE:
			struct adaptived_cgroup_setting_and_value *cgsv;

			cgsv = (struct adaptived_cgroup_setting_and_value *)cur->data;

			free(cgsv->cgroup_name);
			free(cgsv->setting);
			adaptived_free_cgroup_value(cgsv->value);
			free(cur->data);
			break;
		default:
			free(cur->data);
			break;
		}

		free(cur);
		cur = next;
	}

	cse->sdata = first_valid;
}
