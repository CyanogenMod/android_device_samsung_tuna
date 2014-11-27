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

#ifndef __HWC_DEV__
#define __HWC_DEV__

#include <stdint.h>
#include <stdbool.h>

#include <hardware/hwcomposer.h>
#ifdef OMAP_ENHANCEMENT_S3D
#include <ui/S3DFormat.h>
#endif

#include <linux/bltsville.h>
#include <video/dsscomp.h>
#include <video/omap_hwc.h>

#include "hal_public.h"
#include "rgz_2d.h"

struct ext_transform {
    uint8_t rotation : 3;          /* 90-degree clockwise rotations */
    uint8_t hflip    : 1;          /* flip l-r (after rotation) */
    uint8_t enabled  : 1;          /* cloning enabled */
    uint8_t docking  : 1;          /* docking vs. mirroring - used for state */
};
typedef struct ext_transform ext_transform_t;

/* cloning support and state */
struct omap_hwc_ext {
    /* support */
    ext_transform_t mirror;             /* mirroring settings */
    ext_transform_t dock;               /* docking settings */
    float lcd_xpy;                      /* pixel ratio for UI */
    bool avoid_mode_change;             /* use HDMI mode used for mirroring if possible */
    bool force_dock;                    /* must dock */

    /* state */
    bool hdmi_state;                    /* whether HDMI is connected */
    bool on_tv;                         /* using a tv */
    ext_transform_t current;            /* current settings */
    ext_transform_t last;               /* last-used settings */

    /* configuration */
    uint32_t last_xres_used;            /* resolution and pixel ratio used for mode selection */
    uint32_t last_yres_used;
    uint32_t last_mode;                 /* 2-s complement of last HDMI mode set, 0 if none */
    uint32_t mirror_mode;               /* 2-s complement of mode used when mirroring */
    float last_xpy;
    uint16_t width;                     /* external screen dimensions */
    uint16_t height;
    uint32_t xres;                      /* external screen resolution */
    uint32_t yres;
    float m[2][3];                      /* external transformation matrix */
    hwc_rect_t mirror_region;           /* region of screen to mirror */
#ifdef OMAP_ENHANCEMENT_S3D
    bool s3d_enabled;
    bool s3d_capable;
    enum S3DLayoutType s3d_type;
    enum S3DLayoutOrder s3d_order;
#endif
};
typedef struct omap_hwc_ext omap_hwc_ext_t;

enum bltpolicy {
    BLTPOLICY_DISABLED = 0,
    BLTPOLICY_DEFAULT = 1,    /* Default blit policy */
    BLTPOLICY_ALL,            /* Test mode to attempt to blit all */
};

enum bltmode {
    BLTMODE_PAINT = 0,    /* Attempt to blit layer by layer */
    BLTMODE_REGION = 1,   /* Attempt to blit layers via regions */
};

struct omap_hwc_module {
    hwc_module_t base;

    IMG_framebuffer_device_public_t *fb_dev;
};
typedef struct omap_hwc_module omap_hwc_module_t;

struct counts {
    uint32_t possible_overlay_layers;
    uint32_t composited_layers;
    uint32_t scaled_layers;
    uint32_t RGB;
    uint32_t BGR;
    uint32_t NV12;
    uint32_t dockable;
    uint32_t protected;
#ifdef OMAP_ENHANCEMENT_S3D
    uint32_t s3d;
#endif

    uint32_t max_hw_overlays;
    uint32_t max_scaling_overlays;
    uint32_t mem;
};
typedef struct counts counts_t;

struct omap_hwc_device {
    /* static data */
    hwc_composer_device_1_t base;
    hwc_procs_t *procs;
    pthread_t hdmi_thread;
    pthread_mutex_t lock;

    IMG_framebuffer_device_public_t *fb_dev;
    struct dsscomp_display_info fb_dis;
    int fb_fd;                   /* file descriptor for /dev/fb0 */
    int dsscomp_fd;              /* file descriptor for /dev/dsscomp */
    int hdmi_fb_fd;              /* file descriptor for /dev/fb1 */
    int pipe_fds[2];             /* pipe to event thread */

    int img_mem_size;           /* size of fb for hdmi */
    void *img_mem_ptr;          /* start of fb for hdmi */

    int flags_rgb_order;
    int flags_nv12_only;
    float upscaled_nv12_limit;

    bool on_tv;                  /* using a tv */
    int force_sgx;
    omap_hwc_ext_t ext;         /* external mirroring data */
    int idle;

    float primary_m[2][3];       /* internal transformation matrix */
    int primary_transform;
    int primary_rotation;
    hwc_rect_t primary_region;

    buffer_handle_t *buffers;
    bool use_sgx;
    bool swap_rb;
    uint32_t post2_layers;       /* buffers used with DSS pipes*/
    uint32_t post2_blit_buffers; /* buffers used with blit */
    int ext_ovls;                /* # of overlays on external display for current composition */
    int ext_ovls_wanted;         /* # of overlays that should be on external display for current composition */
    int last_ext_ovls;           /* # of overlays on external/internal display for last composition */
    int last_int_ovls;
#ifdef OMAP_ENHANCEMENT_S3D
    enum S3DLayoutType s3d_input_type;
    enum S3DLayoutOrder s3d_input_order;
#endif
    enum bltmode blt_mode;
    enum bltpolicy blt_policy;

    uint32_t blit_flags;
    int blit_num;
    struct omap_hwc_data comp_data; /* This is a kernel data structure */
    struct rgz_blt_entry blit_ops[RGZ_MAX_BLITS];

    counts_t counts;

    int ion_fd;
    struct ion_handle *ion_handles[2];
    bool use_sw_vsync;

};
typedef struct omap_hwc_device omap_hwc_device_t;

#endif
