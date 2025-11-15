/*
 * SPDX-FileCopyrightText: 2019-2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rtthread.h>
#include <stdio.h>
#include <board.h>
#include "drv_io.h"
#include "string.h"

extern int platform_init(void);
extern int platform_post_init(void);
extern int mnt_init(void);
extern int app_main(void);

void rt_init_thread_entry(void *parameter)
{
    rt_kprintf("Hello RT-Thread!\n");

    mnt_init();
    {
        extern int liveimport_init();
        liveimport_init();
    }
    platform_init();
    platform_post_init();
    app_main();
}

int rt_application_init()
{
    rt_thread_t tid;

    tid = rt_thread_create("init",
                           rt_init_thread_entry, RT_NULL,
                           2048, RT_THREAD_PRIORITY_MIDDLE, RT_THREAD_TICK_DEFAULT * 2);

    if (tid != RT_NULL)
        rt_thread_startup(tid);

    return 0;
}


