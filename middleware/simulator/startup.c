/*
 * SPDX-FileCopyrightText: 2019-2025 SiFli Technologies(Nanjing) Co., Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <rthw.h>
#include <rtthread.h>
#include <string.h>
#include "board.h"

extern int  rt_application_init(void);

/**
 * This function will startup RT-Thread RTOS.
 */
void rtthread_startup(void)
{
    /* init board */
    rt_hw_board_init();

    /* show version */
    rt_show_version();

    /* init tick */
    rt_system_tick_init();

    /* init kernel object */
    rt_system_object_init();

    /* init timer system */
    rt_system_timer_init();

    /* init scheduler system */
    rt_system_scheduler_init();

    /* init application */
    rt_application_init();

    /* init timer thread */
    rt_system_timer_thread_init();

#ifdef BSP_USING_DATA_SVC
    {
        extern int datas_start(void);
        //datas_start();
        //simu_service_register();
    }
#endif

    /* init idle thread */
    rt_thread_idle_init();

    /* start scheduler */
    rt_system_scheduler_start();

    /* never reach here */
    return ;
}


int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        extern char g_uart_com[];
        strcpy(g_uart_com, argv[1]);
    }
    if (argc > 2)
    {
        extern char g_uart2_com[];
        strcpy(g_uart2_com, argv[2]);
    }

    /* disable interrupt first */
    rt_hw_interrupt_disable();

    /* startup RT-Thread RTOS */
    rtthread_startup();

    return 0;
}

/*@}*/
