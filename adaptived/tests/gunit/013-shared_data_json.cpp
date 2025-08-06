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
 * adaptived googletest for converting shared_data to json objects
 */

#include <adaptived-utils.h>
#include <adaptived.h>

#include "gtest/gtest.h"
#include "shared_data.h"
#include "cause.h"

class SharedDataJsonTest : public ::testing::Test {
};

static void populate_cause(struct adaptived_cause * const cse, int idx)
{
	char *name;

	name = (char *)malloc(sizeof(char) * 16);
	ASSERT_NE(name, nullptr);

	sprintf(name, "test013-%d", idx);

	cse->idx = (enum cause_enum)idx;
	cse->name = name;
	cse->fns = NULL;
	cse->sdata = NULL;
	cse->data = NULL;
}

void build_cgsetval_ll(struct adaptived_cgroup_setting_and_value **cgsetval,
	const char * const cg_name, const char * const setting, long long value)
{
	struct adaptived_cgroup_setting_and_value *tmpc;
	struct adaptived_cgroup_value *tmpv;


	tmpc = (struct adaptived_cgroup_setting_and_value *)malloc(
		sizeof(struct adaptived_cgroup_setting_and_value));
	ASSERT_NE(tmpc, nullptr);

	tmpc->cgroup_name = (char *)malloc(sizeof(char) * (strlen(cg_name) + 1));
	ASSERT_NE(tmpc->cgroup_name, nullptr);
	sprintf(tmpc->cgroup_name, "%s", cg_name);

	tmpc->setting = (char *)malloc(sizeof(char) * (strlen(setting) + 1));
	ASSERT_NE(tmpc->setting, nullptr);
	sprintf(tmpc->setting, "%s", setting);

	tmpv = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(tmpv, nullptr);

	tmpv->type = ADAPTIVED_CGVAL_LONG_LONG;
	tmpv->value.ll_value = value;

	tmpc->value = tmpv;

	*cgsetval = tmpc;
}

void build_cgsetval_str(struct adaptived_cgroup_setting_and_value **cgsetval,
	const char * const cg_name, const char * const setting, const char * const value)
{
	struct adaptived_cgroup_setting_and_value *tmpc;
	struct adaptived_cgroup_value *tmpv;
	char *value_copy;


	tmpc = (struct adaptived_cgroup_setting_and_value *)malloc(
		sizeof(struct adaptived_cgroup_setting_and_value));
	ASSERT_NE(tmpc, nullptr);

	tmpc->cgroup_name = (char *)malloc(sizeof(char) * (strlen(cg_name) + 1));
	ASSERT_NE(tmpc->cgroup_name, nullptr);
	sprintf(tmpc->cgroup_name, "%s", cg_name);

	tmpc->setting = (char *)malloc(sizeof(char) * (strlen(setting) + 1));
	ASSERT_NE(tmpc->setting, nullptr);
	sprintf(tmpc->setting, "%s", setting);

	tmpv = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(tmpv, nullptr);

	value_copy = strdup(value);
	ASSERT_NE(value_copy, nullptr);

	tmpv->type = ADAPTIVED_CGVAL_STR;
	tmpv->value.str_value = value_copy;

	tmpc->value = tmpv;

	*cgsetval = tmpc;
}

void build_cgsetval_float(struct adaptived_cgroup_setting_and_value **cgsetval,
	const char * const cg_name, const char * const setting, float value)
{
	struct adaptived_cgroup_setting_and_value *tmpc;
	struct adaptived_cgroup_value *tmpv;


	tmpc = (struct adaptived_cgroup_setting_and_value *)malloc(
		sizeof(struct adaptived_cgroup_setting_and_value));
	ASSERT_NE(tmpc, nullptr);

	tmpc->cgroup_name = (char *)malloc(sizeof(char) * (strlen(cg_name) + 1));
	ASSERT_NE(tmpc->cgroup_name, nullptr);
	sprintf(tmpc->cgroup_name, "%s", cg_name);

	tmpc->setting = (char *)malloc(sizeof(char) * (strlen(setting) + 1));
	ASSERT_NE(tmpc->setting, nullptr);
	sprintf(tmpc->setting, "%s", setting);

	tmpv = (struct adaptived_cgroup_value *)malloc(sizeof(struct adaptived_cgroup_value));
	ASSERT_NE(tmpv, nullptr);

	tmpv->type = ADAPTIVED_CGVAL_FLOAT;
	tmpv->value.float_value = value;

	tmpc->value = tmpv;

	*cgsetval = tmpc;
}

TEST_F(SharedDataJsonTest, InvalidOptions)
{
	struct json_object *json_obj;
	struct adaptived_cause cse;
	int ret;

	populate_cause(&cse, 0);

	ret = adaptived_sdata_to_json(&cse, NULL, NULL);
	ASSERT_EQ(ret, -EINVAL);

	json_obj = NULL;
	ret = adaptived_sdata_to_json(NULL, NULL, &json_obj);
	ASSERT_EQ(ret, -EINVAL);

	json_obj = (struct json_object *)0x1234;
	ret = adaptived_sdata_to_json(NULL, NULL, &json_obj);
	ASSERT_EQ(ret, -EINVAL);

	json_obj = NULL;
	ret = adaptived_sdata_to_json(&cse, NULL, &json_obj);
	ASSERT_EQ(ret, -ENODATA);

	free(cse.name);
}

TEST_F(SharedDataJsonTest, InvalidSharedDataTypes)
{
	struct adaptived_cgroup_setting_and_value *c1;
	struct json_object *json_obj = NULL;
	struct adaptived_cause cse;
	char *shared_str;
	int ret;

	populate_cause(&cse, 2);

	build_cgsetval_ll(&c1, "AnotherCgroup", "memory.low", 4096);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c1, NULL, 0);
	ASSERT_EQ(ret, 0);

	shared_str = (char *)malloc(sizeof(char) * 16);
	ASSERT_NE(shared_str, nullptr);
	sprintf(shared_str, "hello world");

	ret =  adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_STR, shared_str, NULL, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_sdata_to_json(&cse, NULL, &json_obj);
	ASSERT_EQ(ret, -ENOTSUP);

	free_shared_data(&cse, false);
	free(cse.name);
}


TEST_F(SharedDataJsonTest, SeveralCgroupsSingleIntSetting)
{
	const char * const expected = "{ \"memory.max\": { \"cgroup1\": 12345678, \"cgroup2.slice\": 1260588259, \"cgroup3.slice\\/database.scope\": 33333333 } }";
	struct adaptived_cgroup_setting_and_value *c1, *c2, *c3;
	struct json_object *json_obj = NULL;
	struct adaptived_cause cse;
	const char *json_str;
	int ret;

	populate_cause(&cse, 1);

	build_cgsetval_ll(&c1, "cgroup1", "memory.max", 12345678);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c1, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_ll(&c2, "cgroup2.slice", "memory.max", 5555555555);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c2, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_ll(&c3, "cgroup3.slice/database.scope", "memory.max", 33333333);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c3, NULL, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_sdata_to_json(&cse, NULL, &json_obj);
	ASSERT_EQ(ret, 0);

	json_str = json_object_to_json_string(json_obj);
	ASSERT_STREQ(json_str, expected);

	free_shared_data(&cse, false);
	free(cse.name);
	json_object_put(json_obj);
}

TEST_F(SharedDataJsonTest, OneCgroupSeveralSettings)
{
	const char * const expected = "{ \"memory.max\": { \"database.slice\": -2050298496 }, \"cpuset.cpus.effective\": { \"database.slice\": \"1-3,7,9,13\" }, \"cpu.uclamp.min\": { \"database.slice\": 9.876500129699707 }, \"cpu.max\": { \"database.slice\": \"100000 max\" } }";
	struct adaptived_cgroup_setting_and_value *c1, *c2, *c3, *c4;
	struct json_object *json_obj = NULL;
	struct adaptived_cause cse;
	const char *json_str;
	int ret;

	populate_cause(&cse, 1);

	build_cgsetval_ll(&c1, "database.slice", "memory.max", 2244668800);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c1, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_str(&c2, "database.slice", "cpuset.cpus.effective", "1-3,7,9,13");
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c2, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_float(&c3, "database.slice", "cpu.uclamp.min", 9.8765);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c3, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_str(&c4, "database.slice", "cpu.max", "100000 max");
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c4, NULL, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_sdata_to_json(&cse, NULL, &json_obj);
	ASSERT_EQ(ret, 0);

	json_str = json_object_to_json_string(json_obj);
	ASSERT_STREQ(json_str, expected);

	free_shared_data(&cse, false);
	free(cse.name);
	json_object_put(json_obj);
}

TEST_F(SharedDataJsonTest, SeveralCgroupsSeveralSettings)
{
	const char * const expected1 = "{ \"memory.max\": { \"database.slice\": -2050298496, \"cloud.slice\": 1686630204 }, \"cpuset.cpus.effective\": { \"database.slice\": \"1-3,7,9,13\" }, \"cpu.uclamp.min\": { \"database.slice\": 9.876500129699707 }, \"cpu.max\": { \"database.slice\": \"100000 max\", \"cloud.slice\": \"500000 1000000\" } }";
	const char * const expected2 = "{ \"memory.max\": { \"database.slice\": -2050298496, \"cloud.slice\": 1686630204 } }";
	struct adaptived_cgroup_setting_and_value *c1, *c2, *c3, *c4, *c5, *c6;
	struct json_object *json_obj1 = NULL, *json_obj2 = NULL;
	struct adaptived_cause cse;
	const char *json_str;
	int ret;

	populate_cause(&cse, 1);

	build_cgsetval_ll(&c1, "database.slice", "memory.max", 2244668800);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c1, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_str(&c2, "database.slice", "cpuset.cpus.effective", "1-3,7,9,13");
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c2, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_float(&c3, "database.slice", "cpu.uclamp.min", 9.8765);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c3, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_str(&c4, "database.slice", "cpu.max", "100000 max");
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c4, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_ll(&c5, "cloud.slice", "memory.max", 113355779900);
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c5, NULL, 0);
	ASSERT_EQ(ret, 0);

	build_cgsetval_str(&c6, "cloud.slice", "cpu.max", "500000 1000000");
	ret = adaptived_write_shared_data(&cse, ADAPTIVED_SDATA_CGROUP_SETTING_VALUE, c6, NULL, 0);
	ASSERT_EQ(ret, 0);

	ret = adaptived_sdata_to_json(&cse, NULL, &json_obj1);
	ASSERT_EQ(ret, 0);

	json_str = json_object_to_json_string(json_obj1);
	ASSERT_STREQ(json_str, expected1);

	ret = adaptived_sdata_to_json(&cse, "memory.max", &json_obj2);
	ASSERT_EQ(ret, 0);

	json_str = json_object_to_json_string(json_obj2);
	ASSERT_STREQ(json_str, expected2);

	free_shared_data(&cse, false);
	free(cse.name);
	json_object_put(json_obj1);
	json_object_put(json_obj2);
}
