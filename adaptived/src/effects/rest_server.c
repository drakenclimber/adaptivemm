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
 * A rest server effect
 *
 * This file creates a simple rest server
 *
 */

#include <microhttpd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "adaptived-internal.h"
#include "defines.h"

struct rest_server_opts {
	bool shared_data;
	int port;

	struct MHD_Daemon *daemon;
	const struct adaptived_cause *cse;
};

static enum MHD_Result handle_get_call(
	void *cls, struct MHD_Connection *connection,
	const char *url, const char *method,
	const char *version, const char *upload_data,
	long unsigned int *upload_data_size, void **con_cls)
{
	const char *page  = "<html><body>Hello, browser!</body></html>";
	struct MHD_Response *response;
	int ret;

	response = MHD_create_response_from_buffer(strlen(page),
						   (void*) page, MHD_RESPMEM_PERSISTENT);

	ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
	MHD_destroy_response (response);

	return ret;
}

int rest_server_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		     const struct adaptived_cause * const cse)
{
	struct rest_server_opts *opts;
	int ret = 0;

	opts = malloc(sizeof(struct rest_server_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct rest_server_opts));

	ret = adaptived_parse_bool(args_obj, "shared_data", &opts->shared_data);
	if (ret == -ENOENT) {
		opts->shared_data = true;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse shared_data arg: %d\n", ret);
		goto error;
	}

	opts->cse = cse;

	opts->port = 8888;

	opts->daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, opts->port, NULL,
					NULL, &handle_get_call, NULL, MHD_OPTION_END);
	if (!opts->daemon) {
		/* todo -figure out the right error */
		ret = -EINVAL;
		goto error;
	}

	/* we have successfully setup the rest server effect */
	eff->data = (void *)opts;

	return ret;

error:
	if (opts)
		free(opts);

	return ret;
}

int rest_server_main(struct adaptived_effect * const eff)
{
	//struct rest_server_opts *opts = (struct rest_server_opts *)eff->data;

	return 0;
}

void rest_server_exit(struct adaptived_effect * const eff)
{
	struct rest_server_opts *opts = (struct rest_server_opts *)eff->data;

	MHD_stop_daemon(opts->daemon);

	free(opts);
}
