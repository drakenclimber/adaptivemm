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
 * print effect
 *
 * This file runs the print effect
 *
 */

#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

struct example_effect_opts {
	int increment;

	const struct adaptived_cause *cse;
};

int example_effect_init(struct adaptived_effect * const eff, struct json_object *args_obj,
	       const struct adaptived_cause * const cse)
{
	struct example_effect_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct example_effect_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct example_effect_opts));

	ret = adaptived_parse_int(args_obj, "increment", &opts->increment);
	if (ret)
		goto error;

	if (strcmp(cse->name, "example_cause") != 0) {
		adaptived_err("This effect (example_effect) is tightly coupled with the "
			      "example_cause cause.  Provided cause: %s is unsupported\n",
			      cse->name);
		ret = -EINVAL;
		goto error;
	}

	if (cse->next != NULL) {
		adaptived_err("This effect currently only supports a single cause - "
			      "example_cause\n");
		ret = -EINVAL;
		goto error;
	}

	opts->cse = cse;

	eff->data = (void *)opts;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int example_effect_main(struct adaptived_effect * const eff)
{
	struct example_effect_opts *opts = (struct example_effect_opts *)eff->data;
	int value, ret;

	ret = example_cause_get_value(opts->cse, &value);
	if (ret)
		return ret;

	value += opts->increment;

	ret = example_cause_set_value(opts->cse, value);
	if (ret)
		return ret;

	return 0;
}

void example_effect_exit(struct adaptived_effect * const eff)
{
	struct example_effect_opts *opts = (struct example_effect_opts *)eff->data;

	free(opts);
}
