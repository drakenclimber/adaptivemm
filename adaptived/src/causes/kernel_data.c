/*
 * Copyright (c) 2024-2025, Oracle and/or its affiliates.
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
 * Cause that will gather cgroup data into a shared data structure
 *
 */

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <adaptived-utils.h>
#include <adaptived.h>

#include "adaptived-internal.h"
#include "shared_data.h"
#include "defines.h"

static const char * const zombie_cgroup_str = "zombie cgroups";

static const char * const kernel_files[] = {
	"/proc/sys/vm/watermark_boost_factor",
	"/proc/sys/vm/watermark_scale_factor",
	"/sys/kernel/sched_ext/nr_rejected",
	"/proc/pressure/memory",
	"/proc/pressure/cpu",
	zombie_cgroup_str,
};

struct kernel_data_opts {
	int placeholder; /* unused */
};

int kernel_data_init(struct adaptived_cause * const cse, struct json_object *args_obj,
		     int interval)
{
	struct kernel_data_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct kernel_data_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct kernel_data_opts));

	ret = adaptived_cause_set_data(cse, (void *)opts);
	if (ret)
		goto error;

	return ret;

error:
	free(opts);
	return ret;
}

static int get_zombie_cgroup_cnt(struct adaptived_cgroup_value * const value)
{
	value->type = ADAPTIVED_CGVAL_LONG_LONG;
	value->value.ll_value = 5;
	return 0;
}

static int get_value(const char * const file, struct adaptived_cgroup_value **value)
{
	struct adaptived_cgroup_value *tmp_val = NULL;
	int ret;

	tmp_val = malloc(sizeof(struct adaptived_cgroup_value));
	if (!tmp_val)
		return -ENOMEM;

	if (strcmp(file, zombie_cgroup_str) == 0) {
		ret = get_zombie_cgroup_cnt(tmp_val);
		if (ret)
			goto error;
	} else {
		tmp_val->type = ADAPTIVED_CGVAL_DETECT;
		ret = adaptived_cgroup_get_value(file, tmp_val);
		if (ret)
			goto error;
	}

	*value = tmp_val;

	return ret;

error:
	if (tmp_val) {
		adaptived_free_cgroup_value(tmp_val);
		free(tmp_val);
	}

	return ret;
}

int kernel_data_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct kernel_data_opts *opts = (struct kernel_data_opts *)adaptived_cause_get_data(cse);
	struct adaptived_name_and_value *name_val = NULL;
	int ret, i;

	(void)opts;

	free_shared_data(cse, true);

	for (i = 0; i < ARRAY_SIZE(kernel_files); i++) {
		name_val = malloc(sizeof(struct adaptived_name_and_value));
		if (!name_val)
			goto error;
		name_val->name = NULL;
		name_val->value = NULL;

		name_val->name = strdup(kernel_files[i]);
		if (!name_val) {
			ret = -ENOMEM;
			goto error;
		}

		ret = get_value(kernel_files[i], &name_val->value);
		if (ret)
			goto error;

		ret = adaptived_write_shared_data(cse, ADAPTIVED_SDATA_NAME_VALUE, name_val,
						  NULL, ADAPTIVED_SDATAF_PERSIST);
		if (ret)
			goto error;

		name_val = NULL;
	}

	return ret;

error:
	if (name_val) {
		if (name_val->name)
			free(name_val->name);
		adaptived_free_cgroup_value(name_val->value);
		free(name_val);
	}

	return ret;
}

void kernel_data_exit(struct adaptived_cause * const cse)
{
	struct kernel_data_opts *opts = (struct kernel_data_opts *)adaptived_cause_get_data(cse);

	free_shared_data(cse, true);
	free(opts);
}
