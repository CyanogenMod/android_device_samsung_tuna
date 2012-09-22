/*
 * Copyright (C) 2012 The Android Open Source Project
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
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "Tuna PowerHAL"
#include <utils/Log.h>

#include <hardware/hardware.h>
#include <hardware/power.h>

#define SCALINGMAXFREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
#define SCREENOFFMAXFREQ_PATH "/sys/devices/system/cpu/cpu0/cpufreq/screen_off_max_freq"
#define BOOSTPULSE_PATH "/sys/devices/system/cpu/cpufreq/interactive/boostpulse"

#define MAX_BUF_SZ  10

/* initialize to something safe */
static char screen_off_max_freq[MAX_BUF_SZ] = "700000";
static char scaling_max_freq[MAX_BUF_SZ] = "1200000";

/* for tracking previous screen state */
static int previous_state = 0;

struct tuna_power_module {
    struct power_module base;
    pthread_mutex_t lock;
    int boostpulse_fd;
    int boostpulse_warned;
};

static void sysfs_write(char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

int sysfs_read(const char *path, char *buf, size_t size)
{
  int fd, len;

  fd = open(path, O_RDONLY);
  if (fd < 0)
    return -1;

  do {
    len = read(fd, buf, size);
  } while (len < 0 && errno == EINTR);

  close(fd);

  return len;
}

static void tuna_power_init(struct power_module *module)
{
    /*
     * cpufreq interactive governor: timer 20ms, min sample 60ms,
     * hispeed 700MHz at load 50%.
     */

    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_rate",
                "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/min_sample_time",
                "60000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq",
                "700000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load",
                "50");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/above_hispeed_delay",
                "100000");
}

static int boostpulse_open(struct tuna_power_module *tuna)
{
    char buf[80];

    pthread_mutex_lock(&tuna->lock);

    if (tuna->boostpulse_fd < 0) {
        tuna->boostpulse_fd = open(BOOSTPULSE_PATH, O_WRONLY);

        if (tuna->boostpulse_fd < 0) {
            if (!tuna->boostpulse_warned) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error opening %s: %s\n", BOOSTPULSE_PATH, buf);
                tuna->boostpulse_warned = 1;
            }
        }
    }

    pthread_mutex_unlock(&tuna->lock);
    return tuna->boostpulse_fd;
}

static void tuna_power_set_interactive(struct power_module *module, int on)
{
    /*
     * Lower maximum frequency when screen changes from on to off.
     * Return it to previous value when screen changes from off to on.
     * CPU 0 and 1 share a cpufreq policy.
     */

    //screen state has changed since last call
    if (on != previous_state)
    {           
        char buf_screen_off_max[MAX_BUF_SZ], buf_scaling_max[MAX_BUF_SZ];
        int screen_off_max, scaling_max;

        previous_state = on;

        //read value of screen-off max from sysfs, and convert to int for comparison
        if (sysfs_read(SCREENOFFMAXFREQ_PATH, buf_screen_off_max, sizeof(buf_screen_off_max)) != -1)
            screen_off_max = atoi(buf_screen_off_max);

        //read value of max from sysfs, and convert to int for comparison
        if (sysfs_read(SCALINGMAXFREQ_PATH, buf_scaling_max, sizeof(buf_scaling_max)) != -1)
            scaling_max = atoi(buf_scaling_max);

        /* If scaling_max_freq > screen_off_max_freq, then scaling_max_freq is really the maximum frequency, so save it for the next time the screen comes on.
         * If screen_off_max_freq == 0, then we're just going to write our saved scalin_max_freq back to sysfs no matter what
         * If scaling_max_freq == screen_off_max_freq, then scaling_max_freq has the screen_off_max in it, so DON'T SAVE IT TO OUR MAX VARIABLE!
         */
        if (scaling_max > screen_off_max)
            memcpy(scaling_max_freq,
               (scaling_max > screen_off_max) ? buf_scaling_max : buf_screen_off_max,
               strlen((scaling_max > screen_off_max) ? buf_scaling_max : buf_screen_off_max));

        memcpy(screen_off_max_freq,
               (scaling_max <= screen_off_max && screen_off_max > 0) ? buf_scaling_max : buf_screen_off_max,
               strlen((scaling_max <= screen_off_max && screen_off_max > 0) ? buf_scaling_max : buf_screen_off_max));

        //write the appropriate value for scaling_max back to sysfs
        sysfs_write(SCALINGMAXFREQ_PATH, on?scaling_max_freq:screen_off_max_freq);
    }
}

static void tuna_power_hint(struct power_module *module, power_hint_t hint,
                            void *data)
{
    struct tuna_power_module *tuna = (struct tuna_power_module *) module;
    char buf[80];
    int len;
    int duration = 1;

    switch (hint) {
    case POWER_HINT_INTERACTION:
    case POWER_HINT_CPU_BOOST:
        if (data != NULL)
            duration = (int) data;

        if (boostpulse_open(tuna) >= 0) {
            snprintf(buf, sizeof(buf), "%d", duration);
            len = write(tuna->boostpulse_fd, buf, strlen(buf));

            if (len < 0) {
                strerror_r(errno, buf, sizeof(buf));
                ALOGE("Error writing to %s: %s\n", BOOSTPULSE_PATH, buf);
            }
        }
        break;

    case POWER_HINT_VSYNC:
        break;

    default:
        break;
    }
}

static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct tuna_power_module HAL_MODULE_INFO_SYM = {
    base: {
        common: {
            tag: HARDWARE_MODULE_TAG,
            module_api_version: POWER_MODULE_API_VERSION_0_2,
            hal_api_version: HARDWARE_HAL_API_VERSION,
            id: POWER_HARDWARE_MODULE_ID,
            name: "Tuna Power HAL",
            author: "The Android Open Source Project",
            methods: &power_module_methods,
        },

       init: tuna_power_init,
       setInteractive: tuna_power_set_interactive,
       powerHint: tuna_power_hint,
    },

    lock: PTHREAD_MUTEX_INITIALIZER,
    boostpulse_fd: -1,
    boostpulse_warned: 0,
};
