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
 * Test to exercise the rest_server effect
 *
 * Note that this test creates a fake cgroup hierarchy directly in the
 * tests/ftests directory and operates on it.
 *
 */

#include <sys/stat.h>
#include <sys/wait.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h>
#include <errno.h>

#include <adaptived.h>

#include "ftests.h"

#define EXPECTED_RET -ETIME

static const char * const cgroup_dirs[] = {
	"./test073cgroup",
	"./test073cgroup/child1",
	"./test073cgroup/child1/grandchild11/",
	"./test073cgroup/child2",
	"./test073cgroup/child3",
	"./test073cgroup/child3/grandchild31",
};
static const int cgroup_dirs_cnt = ARRAY_SIZE(cgroup_dirs);

static const char * const cgroup_files[] = {
	"./test073cgroup/memory.current",
	"./test073cgroup/cpu.uclamp.min",
	"./test073cgroup/cpuset.cpus.effective",
	"./test073cgroup/cpu.max",

	"./test073cgroup/child1/memory.current",
	"./test073cgroup/child1/cpu.uclamp.min",
	"./test073cgroup/child1/cpuset.cpus.effective",
	"./test073cgroup/child1/cpu.max",

	"./test073cgroup/child1/grandchild11/memory.current",
	"./test073cgroup/child1/grandchild11/cpu.uclamp.min",
	"./test073cgroup/child1/grandchild11/cpuset.cpus.effective",
	"./test073cgroup/child1/grandchild11/cpu.max",

	"./test073cgroup/child2/memory.current",
	"./test073cgroup/child2/cpu.uclamp.min",
	"./test073cgroup/child2/cpuset.cpus.effective",
	"./test073cgroup/child2/cpu.max",

	"./test073cgroup/child3/memory.current",
	"./test073cgroup/child3/cpu.uclamp.min",
	"./test073cgroup/child3/cpuset.cpus.effective",
	"./test073cgroup/child3/cpu.max",

	"./test073cgroup/child3/grandchild31/memory.current",
	"./test073cgroup/child3/grandchild31/cpu.uclamp.min",
	"./test073cgroup/child3/grandchild31/cpuset.cpus.effective",
	"./test073cgroup/child3/grandchild31/cpu.max",
};
static const int cgroup_files_cnt = ARRAY_SIZE(cgroup_files);

/*
 * Write the file contents as strings since that's how adaptived will read out
 * the data.  adaptived will then try to determine the correct data type.
 */
static const char * const file_contents[] = {
	"4096000",
	"1.234",
	"1-2,4-7,10,12",
	"max 100000",

	"8192000",
	"3.456",
	"1-20",
	"10000 100000",

	"1024000",
	"0.000",
	"1,3,5,7,9,11",
	"250000 1000000",

	"0",
	"9.876",
	"5-8",
	"max 1000000",

	"12345678",
	"4.680",
	"8,10",
	"500000 1000000",

	"55555555",
	"123.456",
	"1-5,7-10,14",
	"900000 1000000",
};
static_assert(ARRAY_SIZE(file_contents) == ARRAY_SIZE(cgroup_files),
	      "file_contents should be the same size as cgroup_files_cnt");

static const char * const expected1 = "{ \"memory.current\": { \"child1\": 8192000, \"child2\": 0, \"child3\": 12345678 }, \"cpu.uclamp.min\": { \"child1\": 3.4560000896453857, \"child2\": 9.8760004043579102, \"child3\": 4.679999828338623 }, \"cpuset.cpus.effective\": { \"child1\": \"1-20\", \"child2\": \"5-8\", \"child3\": \"8,10\" } }";
static const char * const expected1alt = "{ \"memory.current\": { \"child3\": 12345678, \"child1\": 8192000, \"child2\": 0 }, \"cpu.uclamp.min\": { \"child3\": 4.679999828338623, \"child1\": 3.4560000896453857, \"child2\": 9.8760004043579102 }, \"cpuset.cpus.effective\": { \"child3\": \"8,10\", \"child1\": \"1-20\", \"child2\": \"5-8\" } }";
static const char * const expected2 = "{ \"memory.current\": { \"child1\": 8192000, \"child2\": 0, \"child3\": 12345678 } }";
static const char * const expected2alt = "{ \"memory.current\": { \"child3\": 12345678, \"child1\": 8192000, \"child2\": 0 } }";
static const char * const expected3 = "{ \"cpu.uclamp.min\": { \"child1\": 3.4560000896453857, \"child2\": 9.8760004043579102, \"child3\": 4.679999828338623 } }";
static const char * const expected3alt = "{ \"cpu.uclamp.min\": { \"child3\": 4.679999828338623, \"child1\": 3.4560000896453857, \"child2\": 9.8760004043579102 } }";
static const char * const expected4 = "{ \"cpuset.cpus.effective\": { \"child1\": \"1-20\", \"child2\": \"5-8\", \"child3\": \"8,10\" } }";
static const char * const expected4alt = "{ \"cpuset.cpus.effective\": { \"child3\": \"8,10\", \"child1\": \"1-20\", \"child2\": \"5-8\" } }";
static const char * const expected5 = "{ }";

static void write_cgroup_files(void)
{
	int i;

	for (i = 0; i < cgroup_files_cnt; i++)
		write_file(cgroup_files[i], file_contents[i]);
}

static void *adaptived_wrapper(void *arg)
{
	struct adaptived_ctx *ctx = arg;
	uintptr_t ret;

	ret = adaptived_loop(ctx, true);

	return (void *)ret;
}

int main(int argc, char *argv[])
{
	struct adaptived_ctx *ctx = NULL;
	char config_path[FILENAME_MAX];
	pthread_t adaptived_thread;
	char *res = NULL;
	void *tret;
	int ret;

	snprintf(config_path, FILENAME_MAX - 1, "%s/073-effect-rest_server.json",
		 argv[1]);
	config_path[FILENAME_MAX - 1] = '\0';

	ctx = adaptived_init(config_path);
	if (!ctx)
		return AUTOMAKE_HARD_ERROR;

	ret = create_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ret)
		goto err;

	write_cgroup_files();

	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_INTERVAL, 10000);
	if (ret)
		goto err;
	ret = adaptived_set_attr(ctx, ADAPTIVED_ATTR_LOG_LEVEL, LOG_ERR);
	if (ret)
		goto err;

	ret = pthread_create(&adaptived_thread, NULL, &adaptived_wrapper, ctx);
	if (ret)
		goto err;

	/* wait for the adaptived loop to get up and running */
	sleep(1);

	ret = curl("http://localhost:12345/cgroup", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected1) != 0 && strcmp(res, expected1alt)) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected1, res);
		goto err;
	}

	free(res);

	ret = curl("http://localhost:12345/cgroup/", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected1) != 0 && strcmp(res, expected1alt)) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected1, res);
		goto err;
	}

	free(res);

	ret = curl("http://localhost:12345/cgroup/memory.current", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected2) != 0 && strcmp(res, expected2alt)) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected2, res);
		goto err;
	}

	free(res);

	ret = curl("http://localhost:12345/cgroup/cpu.uclamp.min", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected3) != 0 && strcmp(res, expected3alt)) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected3, res);
		goto err;
	}

	free(res);

	ret = curl("http://localhost:12345/cgroup/cpuset.cpus.effective", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected4) != 0 && strcmp(res, expected4alt)) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected4, res);
		goto err;
	}

	free(res);
	res = NULL;

	ret = curl("http://localhost:12345/cgrouppppp", &res);
	if (ret != -EIO)
		goto err;

	ret = curl("http://localhost:12345/cgrou", &res);
	if (ret != -EIO)
		goto err;

	ret = curl("http://localhost:12345/cgroup/foo/bar", &res);
	if (ret != -EIO)
		goto err;

	ret = curl("http://localhost:12345/cgroup/foo", &res);
	if (ret)
		goto err;
	if (strcmp(res, expected5) != 0) {
		adaptived_err("Expected: %s\nReceived: %s\n", expected5, res);
		goto err;
	}
	
	pthread_kill(adaptived_thread, SIGTERM);
	pthread_join(adaptived_thread, &tret);

	if (tret != (void *)-EINTR)
		goto err;


	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);

	adaptived_release(&ctx);

	return AUTOMAKE_PASSED;

err:
	delete_file("071-cause-cgroup_data.out");
	delete_files(cgroup_files, cgroup_files_cnt);
	delete_dirs(cgroup_dirs, cgroup_dirs_cnt);
	if (ctx)
		adaptived_release(&ctx);
	if (res)
		free(res);

	return AUTOMAKE_HARD_ERROR;
}
