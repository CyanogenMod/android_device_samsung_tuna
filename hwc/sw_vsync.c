/*
 * Copyright (C) Texas Instruments - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/resource.h>
#include <pthread.h>
#include <time.h>

#include <cutils/properties.h>
#include <cutils/log.h>
#include <utils/Timers.h>

#include "hwc_dev.h"

static pthread_t vsync_thread;
static pthread_mutex_t vsync_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t vsync_cond;
static bool vsync_loop_active = false;

nsecs_t vsync_rate;

static struct timespec diff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec) < 0) {
        temp.tv_sec = end.tv_sec-start.tv_sec - 1;
        temp.tv_nsec = 1000000000 + end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec - start.tv_sec;
        temp.tv_nsec = end.tv_nsec - start.tv_nsec;
    }
    return temp;
}

static void *vsync_loop(void *data)
{
    struct timespec tp, tp_next, tp_sleep;
    nsecs_t now = 0, period = vsync_rate, next_vsync = 0, next_fake_vsync = 0, sleep = 0;
    omap_hwc_device_t *hwc_dev = (omap_hwc_device_t *)data;
    tp_sleep.tv_sec = tp_sleep.tv_nsec = 0;
    bool reset_timers = true;

    setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY);

    for (;;) {
        pthread_mutex_lock(&vsync_mutex);
        while (!vsync_loop_active) {
            pthread_cond_wait(&vsync_cond, &vsync_mutex);
        }
        /* the vsync_rate should be re-read after
        * user sets the vsync_rate by calling start_sw_vsync
        * explicitly. This is guaranteed by re-reading it
        * after the vsync_cond is signalled.
        */
        period = vsync_rate; /* re-read rate */

        pthread_mutex_unlock(&vsync_mutex);

        clock_gettime(CLOCK_MONOTONIC, &tp);
        now = (tp.tv_sec * 1000000000) + tp.tv_nsec;
        next_vsync = next_fake_vsync;
        sleep = next_vsync - now;
        if (sleep < 0) {
            /* we missed, find where the next vsync should be */
            sleep = (period - ((now - next_vsync) % period));
            next_vsync = now + sleep;
        }
        next_fake_vsync = next_vsync + period;
        tp_next.tv_sec = (next_vsync / 1000000000);
        tp_next.tv_nsec = (next_vsync % 1000000000);
        tp_sleep = diff(tp, tp_next);

        nanosleep(&tp_sleep, NULL);
        if (hwc_dev->procs && hwc_dev->procs->vsync) {
            hwc_dev->procs->vsync(hwc_dev->procs, 0, next_vsync);
        }
    }
    return NULL;
}

bool use_sw_vsync()
{
    char board[PROPERTY_VALUE_MAX];
    bool rv = false;
    property_get("ro.product.board", board, "");
    if ((strncmp("blaze", board, PROPERTY_VALUE_MAX) == 0) ||
        (strncmp("panda5", board, PROPERTY_VALUE_MAX) == 0)) {
        /* TODO: panda5 really should support h/w vsync */
        rv = true;
    } else {
        char value[PROPERTY_VALUE_MAX];
        property_get("persist.hwc.sw_vsync", value, "0");
        int use_sw_vsync = atoi(value);
        rv = use_sw_vsync > 0;
    }
    ALOGI("Expecting %s vsync for %s", rv ? "s/w" : "h/w", board);
    return rv;
}

void init_sw_vsync(omap_hwc_device_t *hwc_dev)
{
    pthread_cond_init(&vsync_cond, NULL);
    pthread_create(&vsync_thread, NULL, vsync_loop, (void *)hwc_dev);
}

void start_sw_vsync()
{
    char refresh_rate[PROPERTY_VALUE_MAX];
    property_get("persist.hwc.sw_vsync_rate", refresh_rate, "60");

    pthread_mutex_lock(&vsync_mutex);
    int rate = atoi(refresh_rate);
    if (rate <= 0)
        rate = 60;
    vsync_rate = 1000000000 / rate;
    if (vsync_loop_active) {
        pthread_mutex_unlock(&vsync_mutex);
        return;
    }
    vsync_loop_active = true;
    pthread_mutex_unlock(&vsync_mutex);
    pthread_cond_signal(&vsync_cond);
}

void stop_sw_vsync()
{
    pthread_mutex_lock(&vsync_mutex);
    if (!vsync_loop_active) {
        pthread_mutex_unlock(&vsync_mutex);
        return;
    }
    vsync_loop_active = false;
    pthread_mutex_unlock(&vsync_mutex);
    pthread_cond_signal(&vsync_cond);
}
