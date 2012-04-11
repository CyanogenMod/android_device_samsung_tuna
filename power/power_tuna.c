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

/*
 * Tuna uses the legacy interface for requesting early suspend and late resume.
 */

#define LEGACY_SYS_POWER_STATE "/sys/power/state"

static int sPowerStatefd;
static const char *pwr_states[] = { "mem", "on" };

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

static void tuna_power_init(struct power_module *module)
{
    char buf[80];

    sPowerStatefd = open(LEGACY_SYS_POWER_STATE, O_RDWR);

    if (sPowerStatefd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error opening %s: %s\n", LEGACY_SYS_POWER_STATE, buf);
    }

    /*
     * cpufreq interactive governor: timer 20ms, min sample 100ms,
     * hispeed 700MHz at load 40%
     */

    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/timer_rate",
                "20000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/min_sample_time",
                "100000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/hispeed_freq",
                "700000");
    sysfs_write("/sys/devices/system/cpu/cpufreq/interactive/go_hispeed_load",
                "40");
}

static void tuna_power_set_interactive(struct power_module *module, int on)
{
    char buf[80];
    int len;

    /*
     * Lower maximum frequency when screen is off.  CPU 0 and 1 share a
     * cpufreq policy.
     */

    sysfs_write("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq",
                on ? "1200000" : "700000");

    len = write(sPowerStatefd, pwr_states[!!on], strlen(pwr_states[!!on]));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        ALOGE("Error writing to %s: %s\n", LEGACY_SYS_POWER_STATE, buf);
    }
}


static struct hw_module_methods_t power_module_methods = {
    .open = NULL,
};

struct power_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = POWER_HARDWARE_MODULE_ID,
        .name = "Tuna Power HAL",
        .author = "The Android Open Source Project",
        .methods = &power_module_methods,
    },

    .init = tuna_power_init,
    .setInteractive = tuna_power_set_interactive,
};
