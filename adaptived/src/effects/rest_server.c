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

#define DEFAULT_PORT 8888

/*
 * The opts structure is currently shared across the main adaptived thread and
 * the MHD handler.  At present, the MHD handler only consumes data in the
 * opts thread, so we currently only need a mutex around the mutable cse
 * shared data.
 */
struct rest_server_opts {
	char *endpoint;
	int port;
	bool shared_data;

	struct MHD_Daemon *daemon;
	const struct adaptived_cause *cse;
};

static void free_opts(struct rest_server_opts * const opts)
{
	if (!opts)
		return;

	if (opts->daemon)
		MHD_stop_daemon(opts->daemon);

	if (opts->endpoint)
		free(opts->endpoint);

	free(opts);
}

static int process_url(const char * const url, const char * const endpoint,
			char **field)
{
	char *url_copy = NULL, *slash;
	int i, ret = 0;

	if (!field)
		return -EINVAL;

	if (strlen(url) < strlen(endpoint)) {
		ret = -EINVAL;
		goto out;
	}

	url_copy = strdup(url);
	if (!url_copy) {
		ret = -ENOMEM;
		goto out;
	}

	/* remove all trailing "/" characters */
	i = strlen(url_copy) - 1;
	while(url_copy[i] == '/') {
		url_copy[i] = '\0';
		i--;
	}

	if (strlen(url_copy) == strlen(endpoint) &&
	    strcmp(url_copy, endpoint) == 0) {
		ret = 0;
		goto out;
	}

	slash = strstr(&url_copy[1], "/");
	if (slash) {
		slash = &slash[1];

		if (strstr(slash, "/") != NULL) {
			ret = -ENOTSUP;
			goto out;
		}

		*field = strdup(slash);
		if (!(*field)) {
			ret = -ENOMEM;
			goto out;
		}

		/*
		 * A valid field was specified.  No more processing is needed
		 */
		ret = 0;
		goto out;
	}

	/*
	 * The path is of the form /endpoint with no trailing data, e.g.
	 * endpoint/foo.  Verify the reqested endpoint matches our endpoint.
	 * This code is needed when the endpoint is e.g. foo, and the url
	 * provided is fooooo.
	 */
	if (strstr(url_copy, endpoint) != url_copy ||
	    strstr(endpoint, url_copy) != endpoint) {
		ret = -EINVAL;
		goto out;
	}

out:
	if (url_copy)
		free(url_copy);

	return ret;
}

static enum MHD_Result handle_get_call(
	void *cls, struct MHD_Connection *connection,
	const char *url, const char *method,
	const char *version, const char *upload_data,
	long unsigned int *upload_data_size, void **con_cls)
{
	struct json_object *json = NULL;
	struct rest_server_opts *opts;
	struct MHD_Response *response;
	const char *json_str, *page;
	char *field = NULL;
	int ret;

	opts = (struct rest_server_opts *)cls;

	ret = process_url(url, opts->endpoint, &field);
	if (ret)
		goto error;

	if (opts->shared_data) {
		ret = adaptived_sdata_to_json((struct adaptived_cause * const)opts->cse,
					      field, &json);
		if (ret)
			goto error;

		json_str = json_object_to_json_string(json);
		if (!json_str) {
			ret = -EINVAL;
			goto error;
		}

		page = json_str;
	} else {
		goto error;
	}

	response = MHD_create_response_from_buffer(strlen(page), (void *)page,
						   MHD_RESPMEM_PERSISTENT);

	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
	MHD_destroy_response(response);

	if (field)
		free(field);

	return ret;

error:
	response = MHD_create_response_from_buffer(0, NULL, MHD_RESPMEM_PERSISTENT);
	ret = MHD_queue_response(connection, MHD_HTTP_BAD_REQUEST, response);
	MHD_destroy_response(response);

	if (json)
		json_object_put(json);

	if (field)
		free(field);

	return ret;
}

int rest_server_init(struct adaptived_effect * const eff, struct json_object *args_obj,
		     const struct adaptived_cause * const cse)
{
	struct adaptived_cause *tmp_cse;
	struct rest_server_opts *opts;
	int cse_cnt = 0, ret = 0;
	const char *endpoint_str;

	opts = malloc(sizeof(struct rest_server_opts));
	if (!opts) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts, 0, sizeof(struct rest_server_opts));

	ret = adaptived_parse_string(args_obj, "endpoint", &endpoint_str);
	if (ret)
		goto error;

	/* + 1 for the leading / and + 1 for the null terminator */
	opts->endpoint = malloc(sizeof(char) * strlen(endpoint_str) + 2);
	if (!opts->endpoint) {
		ret = -ENOMEM;
		goto error;
	}
	memset(opts->endpoint, 0, sizeof(char) * strlen(endpoint_str) + 2);
	sprintf(opts->endpoint, "/%s", endpoint_str);

	ret = adaptived_parse_bool(args_obj, "shared_data", &opts->shared_data);
	if (ret == -ENOENT) {
		opts->shared_data = true;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse shared_data arg: %d\n", ret);
		goto error;
	}

	if (opts->shared_data == false) {
		adaptived_err("shared_data == false is currently unsupported\n");
		ret = -ENOTSUP;
		goto error;
	}

	opts->cse = cse;

	tmp_cse = (struct adaptived_cause * const)opts->cse;
	while (tmp_cse) {
		cse_cnt++;

		if (cse_cnt > 1) {
			adaptived_err("Currently only one cause supported\n");
			ret = -ENOTSUP;
			goto error;
		}

		tmp_cse = tmp_cse->next;
	}

	ret = adaptived_parse_int(args_obj, "port", &opts->port);
	if (ret == -ENOENT) {
		opts->port = DEFAULT_PORT;
		ret = 0;
	} else if (ret) {
		adaptived_err("Failed to parse port arg: %d\n", ret);
		goto error;
	}

	opts->daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD, opts->port, NULL,
					NULL, &handle_get_call, (void *)opts, MHD_OPTION_END);
	if (!opts->daemon) {
		/* todo -figure out the right error */
		ret = -EINVAL;
		goto error;
	}

	/* we have successfully setup the rest server effect */
	eff->data = (void *)opts;

	return ret;

error:
	free_opts(opts);

	return ret;
}

int rest_server_main(struct adaptived_effect * const eff)
{
	return 0;
}

void rest_server_exit(struct adaptived_effect * const eff)
{
	struct rest_server_opts *opts = (struct rest_server_opts *)eff->data;

	free_opts(opts);
}
