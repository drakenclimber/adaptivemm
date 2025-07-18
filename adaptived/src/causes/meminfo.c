/*
 * Copyright (c) 2024, Oracle and/or its affiliates.
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
 * Cause to detect when a change in a /proc/meminfo field.
 *
 */

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "defines.h"

struct meminfo_opts {
	enum cause_op_enum op;
	char *meminfo_file;
	char *field;
	struct adaptived_cgroup_value threshold;
	bool share_data;
};

static void free_opts(struct meminfo_opts * const opts)
{
	if (!opts)
		return;

	if (opts->meminfo_file)
		free(opts->meminfo_file);
	if (opts->field)
		free(opts->field);

	free(opts);
}

int meminfo_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	const char *meminfo_file_str, *field_str;
	struct meminfo_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct meminfo_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof( struct meminfo_opts));

	ret = adaptived_parse_string(args_obj, "meminfo_file", &meminfo_file_str);
	if (ret == -ENOENT) {
		meminfo_file_str = PROC_MEMINFO;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse the meminfo_file\n");
		goto error;
	}

	opts->meminfo_file = malloc(sizeof(char) * strlen(meminfo_file_str) + 1);
	if (!opts->meminfo_file) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->meminfo_file, meminfo_file_str);
	opts->meminfo_file[strlen(meminfo_file_str)] = '\0';

	ret = adaptived_parse_string(args_obj, "field", &field_str);
	if (ret) {
		adaptived_err("Failed to parse the field\n");
		goto error;
	}

	opts->field = malloc(sizeof(char) * strlen(field_str) + 1);
	if (!opts->field) {
		ret = -ENOMEM;
		goto error;
	}

	strcpy(opts->field, field_str);
	opts->field[strlen(field_str)] = '\0';

	ret = parse_cause_operation(args_obj, NULL, &opts->op);
	if (ret)
		goto error;

	ret = adaptived_parse_cgroup_value(args_obj, "threshold", &opts->threshold);
	if (ret)
		goto error;

	opts->threshold.type = ADAPTIVED_CGVAL_LONG_LONG;

	ret = adaptived_parse_bool(args_obj, "share", &opts->share_data);
	if (ret == -ENOENT) {
		opts->share_data = false;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse share arg: %d\n", ret);
		goto error;
	}

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int share_data(struct adaptived_cause * const cse, const char * const field_name,
	       long long field_value)
{
	struct adaptived_name_and_value *name_value = NULL;
	struct adaptived_cgroup_value *value = NULL;
	char *name = NULL;
	int ret;

	name_value = (struct adaptived_name_and_value *)malloc(
			sizeof(struct adaptived_name_and_value));
	if (name_value == NULL)
		return -ENOMEM;

	name = strdup(field_name);
	if (name == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	value = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	if (value == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	value->type = ADAPTIVED_CGVAL_LONG_LONG;
	value->value.ll_value = field_value;

	name_value->name = name;
	name_value->value = value;

	ret = adaptived_write_shared_data(cse, ADAPTIVED_SDATA_NAME_VALUE, name_value, 0);

	return ret;

error:
	if (name_value)
		free(name_value);
	if (value)
		free(value);
	if (name)
		free(name);

	return ret;
}

int meminfo_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct meminfo_opts *opts = (struct meminfo_opts *)adaptived_cause_get_data(cse);
	long long ll_value = 0;
	int ret;

	ret = adaptived_get_meminfo_field(opts->meminfo_file, opts->field, &ll_value);
	if (ret)
		return ret;

	if (opts->share_data) {
		ret = share_data(cse, opts->field, ll_value);
		if (ret)
			return ret;
	}

	switch (opts->op) {
	case COP_GREATER_THAN:
		if (ll_value > opts->threshold.value.ll_value)
			return 1;
		break;
	case COP_LESS_THAN:
		if (ll_value < opts->threshold.value.ll_value)
			return 1;
		break;
	case COP_EQUAL:
		if (ll_value == opts->threshold.value.ll_value)
			return 1;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void meminfo_exit(struct adaptived_cause * const cse)
{
	struct meminfo_opts *opts = (struct meminfo_opts *)adaptived_cause_get_data(cse);

	free_opts(opts);
}
