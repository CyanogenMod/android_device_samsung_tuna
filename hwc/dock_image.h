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

#ifndef __DOCK_IMAGE__
#define __DOCK_IMAGE__

#include <stdint.h>

/* ARGB image */
struct image_info {
    int width;
    int height;
    int rowbytes;
    int size;
    uint8_t *ptr;
};
typedef struct image_info image_info_t;

typedef struct omap_hwc_device omap_hwc_device_t;

int init_dock_image(omap_hwc_device_t *hwc_dev, uint32_t max_width, uint32_t max_height);
void load_dock_image();
image_info_t *get_dock_image();

#endif
