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

#ifndef __DISPLAY__
#define __DISPLAY__

#include <stdint.h>

#define MAX_DISPLAYS 3
#define MAX_DISPLAY_CONFIGS 32

struct display_config {
    int xres;
    int yres;
    int fps;
    int xdpi;
    int ydpi;
};
typedef struct display_config display_config_t;

struct display {
    uint32_t num_configs;
    display_config_t *configs;
    uint32_t active_config_ix;
};
typedef struct display display_t;

typedef struct omap_hwc_device omap_hwc_device_t;

int init_primary_display(omap_hwc_device_t *hwc_dev);
int get_display_configs(omap_hwc_device_t *hwc_dev, int disp, uint32_t *configs, size_t *numConfigs);
int get_display_attributes(omap_hwc_device_t *hwc_dev, int disp, uint32_t config, const uint32_t *attributes, int32_t *values);
void free_displays(omap_hwc_device_t *hwc_dev);

#endif
