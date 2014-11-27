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
#include <sys/mman.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <png.h>

#include <linux/fb.h>

#include "hwc_dev.h"
#include "dock_image.h"

static struct dock_image_state {
    void *buffer;               /* start of fb for hdmi */
    uint32_t buffer_size;       /* size of fb for hdmi */

    uint32_t max_width;
    uint32_t max_height;

    image_info_t image;
} dock_image;

static void free_png_image(image_info_t *img)
{
    memset(img, 0, sizeof(*img));
}

static int load_png_image(char *path, image_info_t *img)
{
    void *ptr = NULL;
    png_bytepp row_pointers = NULL;

    FILE *fd = fopen(path, "rb");
    if (!fd) {
        ALOGE("failed to open PNG file %s: (%d)", path, errno);
        return -EINVAL;
    }

    const int SIZE_PNG_HEADER = 8;
    uint8_t header[SIZE_PNG_HEADER];
    fread(header, 1, SIZE_PNG_HEADER, fd);
    if (png_sig_cmp(header, 0, SIZE_PNG_HEADER)) {
        ALOGE("%s is not a PNG file", path);
        goto fail;
    }

    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
         goto fail_alloc;
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
         goto fail_alloc;

    if (setjmp(png_jmpbuf(png_ptr)))
        goto fail_alloc;

    png_init_io(png_ptr, fd);
    png_set_sig_bytes(png_ptr, SIZE_PNG_HEADER);
    png_set_user_limits(png_ptr, dock_image.max_width, dock_image.max_height);
    png_read_info(png_ptr, info_ptr);

    uint8_t bit_depth = png_get_bit_depth(png_ptr, info_ptr);
    uint32_t width = png_get_image_width(png_ptr, info_ptr);
    uint32_t height = png_get_image_height(png_ptr, info_ptr);
    uint8_t color_type = png_get_color_type(png_ptr, info_ptr);

    switch (color_type) {
    case PNG_COLOR_TYPE_PALETTE:
        png_set_palette_to_rgb(png_ptr);
        png_set_filler(png_ptr, 128, PNG_FILLER_AFTER);
        break;
    case PNG_COLOR_TYPE_GRAY:
        if (bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png_ptr);
            if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
                png_set_tRNS_to_alpha(png_ptr);
        } else {
            png_set_filler(png_ptr, 128, PNG_FILLER_AFTER);
        }
        /* fall through */
    case PNG_COLOR_TYPE_GRAY_ALPHA:
        png_set_gray_to_rgb(png_ptr);
        break;
    case PNG_COLOR_TYPE_RGB:
        png_set_filler(png_ptr, 128, PNG_FILLER_AFTER);
        /* fall through */
    case PNG_COLOR_TYPE_RGB_ALPHA:
        png_set_bgr(png_ptr);
        break;
    default:
        ALOGE("unsupported PNG color: %x", color_type);
        goto fail_alloc;
    }

    if (bit_depth == 16)
        png_set_strip_16(png_ptr);

    const uint32_t bpp = 4;
    img->size = ALIGN(width * height * bpp, 4096);
    if ((uint32_t)img->size > dock_image.buffer_size) {
        ALOGE("image does not fit into framebuffer area (%d > %d)", img->size, dock_image.buffer_size);
        goto fail_alloc;
    }
    img->ptr = dock_image.buffer;

    row_pointers = calloc(height, sizeof(*row_pointers));
    if (!row_pointers) {
        ALOGE("failed to allocate row pointers");
        goto fail_alloc;
    }
    uint32_t i;
    for (i = 0; i < height; i++)
        row_pointers[i] = img->ptr + i * width * bpp;
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_read_update_info(png_ptr, info_ptr);
    img->rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, NULL);
    free(row_pointers);
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
    fclose(fd);
    img->width = width;
    img->height = height;
    return 0;

fail_alloc:
    free_png_image(img);
    free(row_pointers);
    if (!png_ptr || !info_ptr)
        ALOGE("failed to allocate PNG structures");
    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
fail:
    fclose(fd);
    return -EINVAL;
}

int init_dock_image(omap_hwc_device_t *hwc_dev, uint32_t max_width, uint32_t max_height)
{
    int err = 0;

    struct fb_fix_screeninfo fix;
    if (ioctl(hwc_dev->fb_fd, FBIOGET_FSCREENINFO, &fix)) {
        ALOGE("failed to get fb info (%d)", errno);
        err = -errno;
        goto done;
    }

    dock_image.buffer_size = fix.smem_len;
    dock_image.buffer = mmap(NULL, fix.smem_len, PROT_WRITE, MAP_SHARED, hwc_dev->fb_fd, 0);
    if (dock_image.buffer == MAP_FAILED) {
        ALOGE("failed to map fb memory");
        err = -errno;
        goto done;
    }

    dock_image.max_width = max_width;
    dock_image.max_height = max_height;

    done:
    return err;
}

void load_dock_image()
{
    if (!dock_image.image.rowbytes) {
        char value[PROPERTY_VALUE_MAX];
        property_get("persist.hwc.dock_image", value, "/vendor/res/images/dock/dock.png");
        load_png_image(value, &dock_image.image);
    }
}

image_info_t *get_dock_image()
{
    return &dock_image.image;
}

