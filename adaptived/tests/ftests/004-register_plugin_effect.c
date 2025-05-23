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
 * Test to register a effect at runtime and utilize it
 *
 */

#include <json-c/json.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -123

int main(int argc, char *argv[])
{
	char config_path[FILENAME_MAX];
	struct adaptived_ctx *ctx;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/004-register_plugin_effect.json", argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';
	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	if (argc == 3) {
		ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, atoi(argv[2]));
		if (ret)
			goto err;
	}

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_MAX_LOOPS, 10);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 1000);
	if (ret)
		goto err;

	ret = adaptived_register_cause(ctx, tod_name, &tod_fns);
	if (ret)
		goto err;
	ret = adaptived_register_effect(ctx, tod_validate_name, &tod_validate_fns);
	if (ret)
		goto err;

	ret = adaptived_loop(ctx, true);
	if (ret != EXPECTED_RET)
		goto err;

	adaptived_release(&ctx);
	return AUTOMAKE_PASSED;

err:
	adaptived_release(&ctx);
	return AUTOMAKE_HARD_ERROR;
}
