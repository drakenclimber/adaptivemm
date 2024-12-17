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
 * time of day cause
 *
 * This file processes time of day causes
 *
 */

#define _XOPEN_SOURCE

#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

struct example_cause_opts {
	int value;
};

static void free_opts(struct example_cause_opts * const opts)
{
	if (!opts)
		return;

	free(opts);
}

int example_cause_get_value(const struct adaptived_cause * const cse, int * const value)
{
	struct example_cause_opts *opts;

	if (!cse)
		return -EINVAL;
	if (!value)
		return -EINVAL;
	if (strcmp(cse->name, "example_cause") != 0)
		return -EINVAL;

	opts = (struct example_cause_opts *)cse->data;

	*value = opts->value;
	return 0;
}

int example_cause_set_value(const struct adaptived_cause * const cse, int value)
{
	struct example_cause_opts *opts;

	if (!cse)
		return -EINVAL;
	if (!value)
		return -EINVAL;
	if (strcmp(cse->name, "example_cause") != 0)
		return -EINVAL;

	opts = (struct example_cause_opts *)cse->data;

	opts->value = value;
	return 0;
}

int example_cause_init(struct adaptived_cause * const cse, struct json_object *args_obj, int interval)
{
	struct example_cause_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct example_cause_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}

	memset(opts, 0, sizeof(struct example_cause_opts));

	ret = adaptived_parse_int(args_obj, "value", &opts->value);
	if (ret)
		goto error;

	cse->data = (void *)opts;

	return ret;

error:
	free_opts(opts);
	return ret;
}

int example_cause_main(struct adaptived_cause * const cse, int time_since_last_run)
{
	struct example_cause_opts *opts = (struct example_cause_opts *)cse->data;

	adaptived_err("example_cause: value = %d\n", opts->value);
	return 1;
}

void example_cause_exit(struct adaptived_cause * const cse)
{
	struct example_cause_opts *opts = (struct example_cause_opts *)cse->data;

	free_opts(opts);
}
