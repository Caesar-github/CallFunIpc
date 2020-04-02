// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>

#include <glib.h>

#include <pthread.h>
#include <gdbus.h>

#include "json-c/json.h"
#include "call_fun_ipc.h"
#include "shared_memory.h"
#include "dbus.h"

DBusConnection *dbus_conn = NULL;
static struct FunMap *fun_map = NULL;
static int fun_num = 0;
char *DbusName = NULL;
char *DbusPath = NULL;
char *DbusIf = NULL;
char *SharePath = NULL;

DBusMessage *dbus_message_new_method_return_string(DBusMessage *msg, char *str)
{
    DBusMessageIter array;
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply)
        return NULL;

    dbus_message_iter_init_append(reply, &array);
    dbus_message_iter_append_basic(&array, DBUS_TYPE_STRING, &str);

    return reply;
}

DBusMessage *method_callfun(DBusConnection *conn,
                           DBusMessage *msg, void *data)
{
    int ret = 0;
    char *json_str;
    json_object *j_cfg;

    dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &json_str,
                          DBUS_TYPE_INVALID);

    j_cfg = json_tokener_parse(json_str);
    //printf("%s, json_str = %s\n", __func__, json_str);
    char *FunName = (char *)json_object_get_string(json_object_object_get(j_cfg, "FunName"));

    for (int i = 0; i < fun_num; i++) {
        if (fun_map[i].fun_name && fun_map[i].fun) {
            if (!strcmp(fun_map[i].fun_name, FunName)) {
                char *SharePath = (char *)json_object_get_string(json_object_object_get(j_cfg, "SharePath"));
                int ShareId = (int)json_object_get_int(json_object_object_get(j_cfg, "ShareId"));
                int ShareSize = (int)json_object_get_int(json_object_object_get(j_cfg, "ShareSize"));
                int shmid = GetShm(SharePath, ShareId, ShareSize);
                void *para_share = (void *)Shmat(shmid);
                fun_map[i].fun(para_share);
                Shmdt((char *)para_share);
            }
        }
    }

    json_object_put(j_cfg);

    json_object *j_ret = json_object_new_object();
    json_object_object_add(j_ret, "Return", json_object_new_int(ret));
    char *str = (char *)json_object_to_json_string(j_ret);

    DBusMessage *reply = dbus_message_new_method_return_string(msg, str);
    json_object_put(j_ret);

    return reply;
}

const GDBusMethodTable methods[] = {
    {
        GDBUS_ASYNC_METHOD("callfun",
        GDBUS_ARGS({ "json", "s" }), GDBUS_ARGS({ "json", "s" }),
        method_callfun)
    },
    { },
};

const GDBusSignalTable signals[] = {
    {
        GDBUS_SIGNAL("callback",
        GDBUS_ARGS({ "json", "s" }))
    },
    { },
};

static int dbus_manager_init(DBusConnection *dbus_conn, char *dbus_if, char *dbus_path)
{
    g_dbus_register_interface(dbus_conn, dbus_path,
                              dbus_if,
                              methods,
                              signals, NULL, dbus_path, NULL);

    return 0;
}

void call_fun_ipc_server_init(struct FunMap *map, int num, char *dbus_name, char *dbus_if, char *dbus_path)
{
    DBusError dbus_err;

    dbus_error_init(&dbus_err);
    dbus_conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, dbus_name, &dbus_err);
    fun_map = map;
    fun_num = num;
    dbus_manager_init(dbus_conn, dbus_if, dbus_path);
}

void call_fun_ipc_server_deinit(void)
{

}

int call_fun_ipc_call(char *funname, void *data, int len, int restore)
{
    static int projid = 0;
    int ret = -1;
    char *str_ret = NULL;
    json_object *j_cfg = json_object_new_object();

    projid++;
    int shmid = CreateShm((char *)SharePath, projid, len);
    void *para_share = (void *)Shmat(shmid);
    memcpy(para_share, data, len);
    json_object_object_add(j_cfg, "FunName", json_object_new_string(funname));
    json_object_object_add(j_cfg, "SharePath", json_object_new_string(SharePath));
    json_object_object_add(j_cfg, "ShareId", json_object_new_int(projid));
    json_object_object_add(j_cfg, "ShareSize", json_object_new_int(len));

    struct UserData* userdata;
    userdata = dbus_connection();

    dbus_method_call(userdata->connection,
                     DbusName, DbusPath,
                     DbusIf, "callfun",
                     populate_get, userdata, append_path, (char *)json_object_to_json_string(j_cfg));
    dbus_async(userdata);
    str_ret = userdata->json_str;
    dbus_deconnection(userdata);

    json_object_put(j_cfg);

    if (str_ret) {
        json_object *j_ret = json_tokener_parse(str_ret);
        ret = (int)json_object_get_int(json_object_object_get(j_ret, "Return"));
        json_object_put(j_ret);
        g_free(str_ret);
    }
    if (restore)
        memcpy(data, para_share, len);
    Shmdt((char *)para_share);
    DestroyShm(shmid);

    return ret;
}

void call_fun_ipc_client_init(char *dbus_name, char *dbus_if, char *dbus_path, char *share_path)
{
    DBusError dbus_err;

    dbus_error_init(&dbus_err);
    dbus_conn = g_dbus_setup_bus(DBUS_BUS_SYSTEM, dbus_name, &dbus_err);
    DbusName = g_strdup(dbus_name);
    DbusPath = g_strdup(dbus_path);
    DbusIf = g_strdup(dbus_if);
    SharePath = g_strdup(share_path);
}

void call_fun_ipc_client_deinit(void)
{

}