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
#include <stdint.h>
#include <stdbool.h>
#include <sys/ioctl.h>

#include <cutils/log.h>

#include <video/dsscomp.h>

#include "hwc_dev.h"
#include "display.h"

#define PRIMARY_DISPLAY_CONFIGS 1
#define PRIMARY_DISPLAY_FPS 60
#define PRIMARY_DISPLAY_DEFAULT_DPI 150
#define MAX_DISPLAY_ID (MAX_DISPLAYS - 1)
#define INCH_TO_MM 25.4f

static void free_display(display_t *display)
{
    if (display) {
        if (display->configs)
            free(display->configs);

        free(display);
    }
}

static int allocate_display(uint32_t max_configs, display_t **new_display)
{
    int err = 0;

    display_t *display = (display_t *)malloc(sizeof(*display));
    if (display == NULL) {
        err = -ENOMEM;
        goto err_out;
    }

    memset(display, 0, sizeof(*display));

    display->num_configs = max_configs;
    size_t config_data_size = sizeof(*display->configs) * display->num_configs;
    display->configs = (display_config_t *)malloc(config_data_size);
    if (display->configs == NULL) {
        err = -ENOMEM;
        goto err_out;
    }

    memset(display->configs, 0, config_data_size);

err_out:

    if (err) {
        ALOGE("Failed to allocate display (configs = %d)", max_configs);
        free_display(display);
    } else {
        *new_display = display;
    }

    return err;
}

int init_primary_display(omap_hwc_device_t *hwc_dev)
{
    int ret = ioctl(hwc_dev->dsscomp_fd, DSSCIOC_QUERY_DISPLAY, &hwc_dev->fb_dis);
    if (ret) {
        ALOGE("failed to get display info (%d): %m", errno);
        return -errno;
    }

    int err = allocate_display(PRIMARY_DISPLAY_CONFIGS, &hwc_dev->displays[HWC_DISPLAY_PRIMARY]);
    if (err)
        return err;

    display_config_t *config = &hwc_dev->displays[HWC_DISPLAY_PRIMARY]->configs[0];

    config->xres = hwc_dev->fb_dis.timings.x_res;
    config->yres = hwc_dev->fb_dis.timings.y_res;
    config->fps = PRIMARY_DISPLAY_FPS;

    if (hwc_dev->fb_dis.width_in_mm && hwc_dev->fb_dis.height_in_mm) {
        config->xdpi = (int)(config->xres * INCH_TO_MM) / hwc_dev->fb_dis.width_in_mm;
        config->ydpi = (int)(config->yres * INCH_TO_MM) / hwc_dev->fb_dis.height_in_mm;
    } else {
        config->xdpi = PRIMARY_DISPLAY_DEFAULT_DPI;
        config->ydpi = PRIMARY_DISPLAY_DEFAULT_DPI;
    }

    return 0;
}

int get_display_configs(omap_hwc_device_t *hwc_dev, int disp, uint32_t *configs, size_t *numConfigs)
{
    if (!numConfigs)
        return -EINVAL;

    if (*numConfigs == 0)
        return 0;

    if (!configs || disp < 0 || disp > MAX_DISPLAY_ID || !hwc_dev->displays[disp])
        return -EINVAL;

    display_t *display = hwc_dev->displays[disp];
    size_t num = display->num_configs;
    uint32_t c;

    if (num > *numConfigs)
        num = *numConfigs;

    for (c = 0; c < num; c++)
        configs[c] = c;

    *numConfigs = num;

    return 0;
}

int get_display_attributes(omap_hwc_device_t *hwc_dev, int disp, uint32_t cfg, const uint32_t *attributes, int32_t *values)
{
    if (!attributes || !values)
        return 0;

    if (disp < 0 || disp > MAX_DISPLAY_ID || !hwc_dev->displays[disp])
        return -EINVAL;

    display_t *display = hwc_dev->displays[disp];

    if (cfg >= display->num_configs)
        return -EINVAL;

    const uint32_t* attribute = attributes;
    int32_t* value = values;
    display_config_t *config = &display->configs[cfg];

    while (*attribute != HWC_DISPLAY_NO_ATTRIBUTE) {
        switch (*attribute) {
        case HWC_DISPLAY_VSYNC_PERIOD:
            *value = 1000000000 / config->fps;
            break;
        case HWC_DISPLAY_WIDTH:
            *value = config->xres;
            break;
        case HWC_DISPLAY_HEIGHT:
            *value = config->yres;
            break;
        case HWC_DISPLAY_DPI_X:
            *value = 1000 * config->xdpi;
            break;
        case HWC_DISPLAY_DPI_Y:
            *value = 1000 * config->ydpi;
            break;
        }

        attribute++;
        value++;
    }

    return 0;
}

void free_displays(omap_hwc_device_t *hwc_dev)
{
    int i;
    for (i = 0; i < MAX_DISPLAYS; i++)
        free_display(hwc_dev->displays[i]);
}
