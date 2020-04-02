// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include <inttypes.h>
#include <glib.h>
#include <gdbus.h>

#include "call_fun_ipc.h"
#include "common.h"

int examples(void* data);

struct FunMap map[] = {
    {"examples", &examples}
};

int examples(void* data)
{
    struct Examples_s *para = data;
    printf("%s r = %d, g = %d, b = %d\n", __func__, para->r, para->g, para->b);
    para->ret = 100;
}

int main( int argc , char ** argv)
{
    GMainLoop *main_loop;

    call_fun_ipc_server_init(map, sizeof(map) / sizeof(struct FunMap), DBUS_NAME, DBUS_IF, DBUS_PATH);

    main_loop = g_main_loop_new(NULL, FALSE);
    printf("call_fun_ipc_demo_server init\n");

    g_main_loop_run(main_loop);
    if (main_loop)
        g_main_loop_unref(main_loop);

    return 0;
}