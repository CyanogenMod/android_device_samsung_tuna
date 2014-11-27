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
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <strings.h>
#include <dlfcn.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <linux/bltsville.h>
#include <video/dsscomp.h>
#include <video/omap_hwc.h>

#include <cutils/log.h>
#include <cutils/properties.h>
#include <hardware/hwcomposer.h>

#include "hwc_dev.h"

static int rgz_handle_to_stride(IMG_native_handle_t *h);
#define BVDUMP(p,t,parms)
#define HANDLE_TO_BUFFER(h) NULL
/* Needs to be meaningful for TILER & GFX buffers and NV12 */
#define HANDLE_TO_STRIDE(h) rgz_handle_to_stride(h)
#define DSTSTRIDE(dstgeom) dstgeom->virtstride

/* Borrowed macros from hwc.c vvv - consider sharing later */
#define min(a, b) ( { typeof(a) __a = (a), __b = (b); __a < __b ? __a : __b; } )
#define max(a, b) ( { typeof(a) __a = (a), __b = (b); __a > __b ? __a : __b; } )
#define swap(a, b) do { typeof(a) __a = (a); (a) = (b); (b) = __a; } while (0)

#define WIDTH(rect) ((rect).right - (rect).left)
#define HEIGHT(rect) ((rect).bottom - (rect).top)

#define is_RGB(format) ((format) == HAL_PIXEL_FORMAT_BGRA_8888 || (format) == HAL_PIXEL_FORMAT_RGB_565 || (format) == HAL_PIXEL_FORMAT_BGRX_8888)
#define is_BGR(format) ((format) == HAL_PIXEL_FORMAT_RGBX_8888 || (format) == HAL_PIXEL_FORMAT_RGBA_8888)
#define is_NV12(format) ((format) == HAL_PIXEL_FORMAT_TI_NV12 || (format) == HAL_PIXEL_FORMAT_TI_NV12_PADDED)

#define HAL_PIXEL_FORMAT_BGRX_8888 0x1FF
#define HAL_PIXEL_FORMAT_TI_NV12 0x100
#define HAL_PIXEL_FORMAT_TI_NV12_PADDED 0x101
/* Borrowed macros from hwc.c ^^^ */
#define is_OPAQUE(format) ((format) == HAL_PIXEL_FORMAT_RGB_565 || (format) == HAL_PIXEL_FORMAT_RGBX_8888 || (format) == HAL_PIXEL_FORMAT_BGRX_8888)

/* OUTP the means for grabbing diagnostic data */
#define OUTP ALOGI
#define OUTE ALOGE

#define IS_BVCMD(params) (params->op == RGZ_OUT_BVCMD_REGION || params->op == RGZ_OUT_BVCMD_PAINT)

#define RECT_INTERSECTS(a, b) (((a).bottom > (b).top) && ((a).top < (b).bottom) && ((a).right > (b).left) && ((a).left < (b).right))

/* Buffer indexes used to distinguish background and layers with the clear fb hint */
#define RGZ_BACKGROUND_BUFFIDX -2
#define RGZ_CLEARHINT_BUFFIDX -1

struct rgz_blts {
    struct rgz_blt_entry bvcmds[RGZ_MAX_BLITS];
    int idx;
};


static int rgz_hwc_layer_blit(rgz_out_params_t *params, rgz_layer_t *rgz_layer);
static void rgz_blts_init(struct rgz_blts *blts);
static void rgz_blts_free(struct rgz_blts *blts);
static struct rgz_blt_entry* rgz_blts_get(struct rgz_blts *blts, rgz_out_params_t *params);
static int rgz_blts_bvdirect(rgz_t* rgz, struct rgz_blts *blts, rgz_out_params_t *params);
static void rgz_get_src_rect(hwc_layer_1_t* layer, blit_rect_t *subregion_rect, blit_rect_t *res_rect);
static int hal_to_ocd(int color);
static int rgz_get_orientation(unsigned int transform);
static int rgz_get_flip_flags(unsigned int transform, int use_src2_flags);
static int rgz_hwc_scaled(hwc_layer_1_t *layer);

int debug = 0;
struct rgz_blts blts;
/* Represents a screen sized background layer */
static hwc_layer_1_t bg_layer;

static void svgout_header(int htmlw, int htmlh, int coordw, int coordh)
{
    OUTP("<svg xmlns=\"http://www.w3.org/2000/svg\""
         "width=\"%d\" height=\"%d\""
         "viewBox=\"0 0 %d %d\">",
        htmlw, htmlh, coordw, coordh);
}

static void svgout_footer(void)
{
    OUTP("</svg>");
}

static void svgout_rect(blit_rect_t *r, char *color, char *text)
{
    OUTP("<rect x=\"%d\" y=\"%d\" width=\"%d\" height=\"%d\" fill=\"%s\" "
         "fill-opacity=\"%f\" stroke=\"black\" stroke-width=\"1\" />",
         r->left, r->top, r->right - r->left, r->bottom - r->top, color, 1.0f);

    if (!text)
        return;

    OUTP("<text x=\"%d\" y=\"%d\" style=\"font-size:30\" fill=\"black\">%s"
         "</text>",
         r->left, r->top + 40, text);
}

static int empty_rect(blit_rect_t *r)
{
    return !r->left && !r->top && !r->right && !r->bottom;
}

static int get_top_rect(blit_hregion_t *hregion, int subregion, blit_rect_t **routp)
{
    int l = hregion->nlayers - 1;
    do {
        *routp = &hregion->blitrects[l][subregion];
        if (!empty_rect(*routp))
            break;
    }
    while (--l >= 0);
    return l;
}

/*
 * The idea here is that we walk the layers from front to back and count the
 * number of layers in the hregion until the first layer which doesn't require
 * blending.
 */
static int get_layer_ops(blit_hregion_t *hregion, int subregion, int *bottom)
{
    int l = hregion->nlayers - 1;
    int ops = 0;
    *bottom = -1;
    do {
        if (!empty_rect(&hregion->blitrects[l][subregion])) {
            ops++;
            *bottom = l;
            hwc_layer_1_t *layer = &hregion->rgz_layers[l]->hwc_layer;
            IMG_native_handle_t *h = (IMG_native_handle_t *)layer->handle;
            if ((layer->blending != HWC_BLENDING_PREMULT) || is_OPAQUE(h->iFormat))
                break;
        }
    }
    while (--l >= 0);
    return ops;
}

static int get_layer_ops_next(blit_hregion_t *hregion, int subregion, int l)
{
    while (++l < hregion->nlayers) {
        if (!empty_rect(&hregion->blitrects[l][subregion]))
            return l;
    }
    return -1;
}

static int svgout_intersects_display(blit_rect_t *a, int dispw, int disph)
{
    return ((a->bottom > 0) && (a->top < disph) &&
            (a->right > 0) && (a->left < dispw));
}

static void svgout_hregion(blit_hregion_t *hregion, int dispw, int disph)
{
    char *colors[] = {"red", "orange", "yellow", "green", "blue", "indigo", "violet", NULL};
    int b;
    for (b = 0; b < hregion->nsubregions; b++) {
        blit_rect_t *rect;
        (void)get_top_rect(hregion, b, &rect);
        /* Only generate SVG for subregions intersecting the displayed area */
        if (!svgout_intersects_display(rect, dispw, disph))
            continue;
        svgout_rect(rect, colors[b % 7], NULL);
    }
}

static void rgz_out_svg(rgz_t *rgz, rgz_out_params_t *params)
{
    if (!rgz || !(rgz->state & RGZ_REGION_DATA)) {
        OUTE("rgz_out_svg invoked with bad state");
        return;
    }
    blit_hregion_t *hregions = rgz->hregions;
    svgout_header(params->data.svg.htmlw, params->data.svg.htmlh,
                  params->data.svg.dispw, params->data.svg.disph);
    int i;
    for (i = 0; i < rgz->nhregions; i++) {

        OUTP("<!-- hregion %d (subcount %d)-->", i, hregions[i].nsubregions);
        svgout_hregion(&hregions[i], params->data.svg.dispw,
                       params->data.svg.disph);
    }
    svgout_footer();
}

/* XXX duplicate of hwc.c version */
static void dump_layer(hwc_layer_1_t const* l, int iserr)
{
#define FMT(f) ((f) == HAL_PIXEL_FORMAT_TI_NV12 ? "NV12" : \
                (f) == HAL_PIXEL_FORMAT_BGRX_8888 ? "xRGB32" : \
                (f) == HAL_PIXEL_FORMAT_RGBX_8888 ? "xBGR32" : \
                (f) == HAL_PIXEL_FORMAT_BGRA_8888 ? "ARGB32" : \
                (f) == HAL_PIXEL_FORMAT_RGBA_8888 ? "ABGR32" : \
                (f) == HAL_PIXEL_FORMAT_RGB_565 ? "RGB565" : "??")

    OUTE("%stype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            iserr ? ">>  " : "    ",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
    if (l->handle) {
        IMG_native_handle_t *h = (IMG_native_handle_t *)l->handle;
        OUTE("%s%d*%d(%s)",
            iserr ? ">>  " : "    ",
            h->iWidth, h->iHeight, FMT(h->iFormat));
        OUTE("hndl %p", l->handle);
    }
}

static void dump_all(rgz_layer_t *rgz_layers, unsigned int layerno, unsigned int errlayer)
{
    unsigned int i;
    for (i = 0; i < layerno; i++) {
        hwc_layer_1_t *l = &rgz_layers[i].hwc_layer;
        OUTE("Layer %d", i);
        dump_layer(l, errlayer == i);
    }
}

static int rgz_out_bvdirect_paint(rgz_t *rgz, rgz_out_params_t *params)
{
    int rv = 0;
    int i;
    (void)rgz;

    rgz_blts_init(&blts);

    /* Begin from index 1 to remove the background layer from the output */
    rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;
    for (i = 1; i < cur_fb_state->rgz_layerno; i++) {
        rv = rgz_hwc_layer_blit(params, &cur_fb_state->rgz_layers[i]);
        if (rv) {
            OUTE("bvdirect_paint: error in layer %d: %d", i, rv);
            dump_all(cur_fb_state->rgz_layers, cur_fb_state->rgz_layerno, i);
            rgz_blts_free(&blts);
            return rv;
        }
    }
    rgz_blts_bvdirect(rgz, &blts, params);
    rgz_blts_free(&blts);
    return rv;
}

static void rgz_set_async(struct rgz_blt_entry *e, int async)
{
    e->bp.flags = async ? e->bp.flags | BVFLAG_ASYNC : e->bp.flags & ~BVFLAG_ASYNC;
}

static void rgz_get_screen_info(rgz_out_params_t *params, struct bvsurfgeom **screen_geom)
{
    *screen_geom = params->data.bvc.dstgeom;
}

static int rgz_is_blending_disabled(rgz_out_params_t *params)
{
    return params->data.bvc.noblend;
}

static void rgz_get_displayframe_rect(hwc_layer_1_t *layer, blit_rect_t *res_rect)
{
    res_rect->left = layer->displayFrame.left;
    res_rect->top = layer->displayFrame.top;
    res_rect->bottom = layer->displayFrame.bottom;
    res_rect->right = layer->displayFrame.right;
}

/*
 * Returns a clock-wise rotated view of the inner rectangle relative to
 * the outer rectangle. The inner rectangle must be contained in the outer
 * rectangle and coordinates must be relative to the top,left corner of the outer
 * rectangle.
 */
static void rgz_get_rotated_view(blit_rect_t *outer_rect, blit_rect_t *inner_rect,
    blit_rect_t *res_rect, int orientation)
{
    int outer_width = WIDTH(*outer_rect);
    int outer_height = HEIGHT(*outer_rect);
    int inner_width = WIDTH(*inner_rect);
    int inner_height = HEIGHT(*inner_rect);
    int delta_top = inner_rect->top - outer_rect->top;
    int delta_left = inner_rect->left - outer_rect->left;

    /* Normalize the angle */
    orientation = (orientation % 360) + 360;

    /*
     * Calculate the top,left offset of the inner rectangle inside the outer
     * rectangle depending on the tranformation value.
     */
    switch(orientation % 360) {
        case 0:
            res_rect->left = delta_left;
            res_rect->top = delta_top;
            break;
        case 180:
            res_rect->left = outer_width - inner_width - delta_left;
            res_rect->top = outer_height - inner_height - delta_top;
            break;
        case 90:
            res_rect->left = outer_height - inner_height - delta_top;
            res_rect->top = delta_left;
            break;
        case 270:
            res_rect->left = delta_top;
            res_rect->top = outer_width - inner_width - delta_left;
            break;
        default:
            OUTE("Invalid transform value %d", orientation);
    }

    if (orientation % 180)
        swap(inner_width, inner_height);

    res_rect->right = res_rect->left + inner_width;
    res_rect->bottom = res_rect->top + inner_height;
}

static void rgz_get_src_rect(hwc_layer_1_t* layer, blit_rect_t *subregion_rect, blit_rect_t *res_rect)
{
    if (rgz_hwc_scaled(layer)) {
        /*
         * If the layer is scaled we use the whole cropping rectangle from the
         * source and just move the clipping rectangle for the region we want to
         * blit, this is done to prevent any artifacts when blitting subregions of
         * a scaled layer.
         */
        res_rect->top = layer->sourceCrop.top;
        res_rect->left = layer->sourceCrop.left;
        res_rect->bottom = layer->sourceCrop.bottom;
        res_rect->right = layer->sourceCrop.right;
        return;
    }

    blit_rect_t display_frame;
    rgz_get_displayframe_rect(layer, &display_frame);

    /*
     * Get the rotated subregion rectangle with respect to the display frame.
     * In order to get this correctly we need to take in account the HWC
     * orientation is clock-wise so to return to the 0 degree view we need to
     * rotate counter-clock wise the orientation. For example, if the
     * orientation is 90 we need to rotate -90 to return to a 0 degree view.
     */
    int src_orientation = 0 - rgz_get_orientation(layer->transform);
    rgz_get_rotated_view(&display_frame, subregion_rect, res_rect, src_orientation);

    /*
     * In order to translate the resulting rectangle relative to the cropping
     * rectangle the only thing left is account for the offset (result is already
     * rotated).
     */
    res_rect->left += layer->sourceCrop.left;
    res_rect->right += layer->sourceCrop.left;
    res_rect->top += layer->sourceCrop.top;
    res_rect->bottom += layer->sourceCrop.top;
}

/*
 * Convert a destination geometry and rectangle to a specified rotated view.
 * Since clipping rectangle is relative to the destination geometry it will be
 * rotated as well.
 */
static void rgz_rotate_dst(struct rgz_blt_entry* e, int dst_orientation)
{
    struct bvsurfgeom *dstgeom = &e->dstgeom;
    struct bvrect *dstrect = &e->bp.dstrect;
    struct bvrect *cliprect = &e->bp.cliprect;

    /*
     * Create a rectangle that represents the destination geometry (outter
     * rectangle), destination and clipping rectangles (inner rectangles).
     */
    blit_rect_t dstgeom_r;
    dstgeom_r.top = dstgeom_r.left = 0;
    dstgeom_r.bottom = dstgeom->height;
    dstgeom_r.right = dstgeom->width;

    blit_rect_t dstrect_r;
    dstrect_r.top = dstrect->top;
    dstrect_r.left = dstrect->left;
    dstrect_r.bottom = dstrect->top + dstrect->height;
    dstrect_r.right = dstrect->left + dstrect->width;

    blit_rect_t cliprect_r;
    cliprect_r.top = cliprect->top;
    cliprect_r.left = cliprect->left;
    cliprect_r.bottom = cliprect->top + cliprect->height;
    cliprect_r.right = cliprect->left + cliprect->width;

    /* Get the CW rotated view of the destination rectangle */
    blit_rect_t res_rect;
    rgz_get_rotated_view(&dstgeom_r, &dstrect_r, &res_rect, dst_orientation);
    dstrect->left = res_rect.left;
    dstrect->top = res_rect.top;
    dstrect->width = WIDTH(res_rect);
    dstrect->height = HEIGHT(res_rect);

    rgz_get_rotated_view(&dstgeom_r, &cliprect_r, &res_rect, dst_orientation);
    cliprect->left = res_rect.left;
    cliprect->top = res_rect.top;
    cliprect->width = WIDTH(res_rect);
    cliprect->height = HEIGHT(res_rect);

    if (dst_orientation % 180)
        swap(e->dstgeom.width, e->dstgeom.height);
}

static void rgz_set_dst_data(rgz_out_params_t *params, blit_rect_t *subregion_rect,
    struct rgz_blt_entry* e, int dst_orientation)
{
    struct bvsurfgeom *screen_geom;
    rgz_get_screen_info(params, &screen_geom);

    /* omaplfb is in charge of assigning the correct dstdesc in the kernel */
    e->dstgeom.structsize = sizeof(struct bvsurfgeom);
    e->dstgeom.format = screen_geom->format;
    e->dstgeom.width = screen_geom->width;
    e->dstgeom.height = screen_geom->height;
    e->dstgeom.orientation = dst_orientation;
    e->dstgeom.virtstride = DSTSTRIDE(screen_geom);

    e->bp.dstrect.left = subregion_rect->left;
    e->bp.dstrect.top = subregion_rect->top;
    e->bp.dstrect.width = WIDTH(*subregion_rect);
    e->bp.dstrect.height = HEIGHT(*subregion_rect);

    /* Give a rotated buffer representation of the destination if requested */
    if (e->dstgeom.orientation)
        rgz_rotate_dst(e, dst_orientation);
}

/* Convert a source geometry and rectangle to a specified rotated view */
static void rgz_rotate_src(struct rgz_blt_entry* e, int src_orientation, int is_src2)
{
    struct bvsurfgeom *srcgeom = is_src2 ? &e->src2geom : &e->src1geom;
    struct bvrect *srcrect = is_src2 ? &e->bp.src2rect : &e->bp.src1rect;

    /*
     * Create a rectangle that represents the source geometry (outter rectangle),
     * source rectangle (inner rectangle).
     */
    blit_rect_t srcgeom_r;
    srcgeom_r.top = srcgeom_r.left = 0;
    srcgeom_r.bottom = srcgeom->height;
    srcgeom_r.right = srcgeom->width;

    blit_rect_t srcrect_r;
    srcrect_r.top = srcrect->top;
    srcrect_r.left = srcrect->left;
    srcrect_r.bottom = srcrect->top + srcrect->height;
    srcrect_r.right = srcrect->left + srcrect->width;

    /* Get the CW rotated view of the source rectangle */
    blit_rect_t res_rect;
    rgz_get_rotated_view(&srcgeom_r, &srcrect_r, &res_rect, src_orientation);

    srcrect->left = res_rect.left;
    srcrect->top = res_rect.top;
    srcrect->width = WIDTH(res_rect);
    srcrect->height = HEIGHT(res_rect);

    if (src_orientation % 180)
        swap(srcgeom->width, srcgeom->height);
}

static void rgz_set_src_data(rgz_out_params_t *params, rgz_layer_t *rgz_layer,
    blit_rect_t *subregion_rect, struct rgz_blt_entry* e, int src_orientation,
    int is_src2)
{
    hwc_layer_1_t *hwc_layer = &rgz_layer->hwc_layer;
    struct bvbuffdesc *srcdesc = is_src2 ? &e->src2desc : &e->src1desc;
    struct bvsurfgeom *srcgeom = is_src2 ? &e->src2geom : &e->src1geom;
    struct bvrect *srcrect = is_src2 ? &e->bp.src2rect : &e->bp.src1rect;
    IMG_native_handle_t *handle = (IMG_native_handle_t *)hwc_layer->handle;

    srcdesc->structsize = sizeof(struct bvbuffdesc);
    srcdesc->length = handle->iHeight * HANDLE_TO_STRIDE(handle);
    srcdesc->auxptr = (void*)rgz_layer->buffidx;
    srcgeom->structsize = sizeof(struct bvsurfgeom);
    srcgeom->format = hal_to_ocd(handle->iFormat);
    srcgeom->width = handle->iWidth;
    srcgeom->height = handle->iHeight;
    srcgeom->virtstride = HANDLE_TO_STRIDE(handle);

    /* Find out what portion of the src we want to use for the blit */
    blit_rect_t res_rect;
    rgz_get_src_rect(hwc_layer, subregion_rect, &res_rect);
    srcrect->left = res_rect.left;
    srcrect->top = res_rect.top;
    srcrect->width = WIDTH(res_rect);
    srcrect->height = HEIGHT(res_rect);

    /* Give a rotated buffer representation of this source if requested */
    if (src_orientation) {
        srcgeom->orientation = src_orientation;
        rgz_rotate_src(e, src_orientation, is_src2);
    } else
        srcgeom->orientation = 0;
}

/*
 * Set the clipping rectangle, if part of the subregion rectangle is outside
 * the boundaries of the destination, remove only the out-of-bounds area
 */
static void rgz_set_clip_rect(rgz_out_params_t *params, blit_rect_t *subregion_rect,
    struct rgz_blt_entry* e)
{
    struct bvsurfgeom *screen_geom;
    rgz_get_screen_info(params, &screen_geom);

    blit_rect_t clip_rect;
    clip_rect.left = max(0, subregion_rect->left);
    clip_rect.top = max(0, subregion_rect->top);
    clip_rect.bottom = min(screen_geom->height, subregion_rect->bottom);
    clip_rect.right = min(screen_geom->width, subregion_rect->right);

    e->bp.cliprect.left = clip_rect.left;
    e->bp.cliprect.top = clip_rect.top;
    e->bp.cliprect.width = WIDTH(clip_rect);
    e->bp.cliprect.height = HEIGHT(clip_rect);
}

/*
 * Configures blit entry to set src2 is the same as the destination
 */
static void rgz_set_src2_is_dst(rgz_out_params_t *params, struct rgz_blt_entry* e)
{
    /* omaplfb is in charge of assigning the correct src2desc in the kernel */
    e->src2geom = e->dstgeom;
    e->src2desc.structsize = sizeof(struct bvbuffdesc);
    e->src2desc.auxptr = (void*)HWC_BLT_DESC_FB_FN(0);
    e->bp.src2rect = e->bp.dstrect;
}

static int rgz_is_layer_nv12(hwc_layer_1_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
    return is_NV12(handle->iFormat);
}

/*
 * Configure the scaling mode according to the layer format
 */
static void rgz_cfg_scale_mode(struct rgz_blt_entry* e, hwc_layer_1_t *layer)
{
    /*
     * TODO: Revisit scaling mode assignment later, output between GPU and GC320
     * seem different
     */
    e->bp.scalemode = rgz_is_layer_nv12(layer) ? BVSCALE_9x9_TAP : BVSCALE_BILINEAR;
}

/*
 * Copies src1 into the framebuffer
 */
static struct rgz_blt_entry* rgz_hwc_subregion_copy(rgz_out_params_t *params,
    blit_rect_t *subregion_rect, rgz_layer_t *rgz_src1)
{
    struct rgz_blt_entry* e = rgz_blts_get(&blts, params);
    hwc_layer_1_t *hwc_src1 = &rgz_src1->hwc_layer;
    e->bp.structsize = sizeof(struct bvbltparams);
    e->bp.op.rop = 0xCCCC; /* SRCCOPY */
    e->bp.flags = BVFLAG_CLIP | BVFLAG_ROP;
    e->bp.flags |= rgz_get_flip_flags(hwc_src1->transform, 0);
    rgz_set_async(e, 1);

    blit_rect_t tmp_rect;
    if (rgz_hwc_scaled(hwc_src1)) {
        rgz_get_displayframe_rect(hwc_src1, &tmp_rect);
        rgz_cfg_scale_mode(e, hwc_src1);
    } else
        tmp_rect = *subregion_rect;

    int src1_orientation = rgz_get_orientation(hwc_src1->transform);
    int dst_orientation = 0;

    if (rgz_is_layer_nv12(hwc_src1)) {
        /*
         * Leave NV12 as 0 degree and rotate destination instead, this is done
         * because of a GC limitation. Rotate destination CW.
         */
        dst_orientation = 360 - src1_orientation;
        src1_orientation = 0;
    }

    rgz_set_src_data(params, rgz_src1, &tmp_rect, e, src1_orientation, 0);
    rgz_set_clip_rect(params, subregion_rect, e);
    rgz_set_dst_data(params, &tmp_rect, e, dst_orientation);

    if((e->src1geom.format == OCDFMT_BGR124) ||
       (e->src1geom.format == OCDFMT_RGB124) ||
       (e->src1geom.format == OCDFMT_RGB16))
        e->dstgeom.format = OCDFMT_BGR124;

    return e;
}

/*
 * Blends two layers and write the result in the framebuffer, src1 must be the
 * top most layer while src2 is the one behind. If src2 is NULL means src1 will
 * be blended with the current content of the framebuffer.
 */
static struct rgz_blt_entry* rgz_hwc_subregion_blend(rgz_out_params_t *params,
    blit_rect_t *subregion_rect, rgz_layer_t *rgz_src1, rgz_layer_t *rgz_src2)
{
    struct rgz_blt_entry* e = rgz_blts_get(&blts, params);
    hwc_layer_1_t *hwc_src1 = &rgz_src1->hwc_layer;
    e->bp.structsize = sizeof(struct bvbltparams);
    e->bp.op.blend = BVBLEND_SRC1OVER;
    e->bp.flags = BVFLAG_CLIP | BVFLAG_BLEND;
    e->bp.flags |= rgz_get_flip_flags(hwc_src1->transform, 0);
    rgz_set_async(e, 1);

    blit_rect_t tmp_rect;
    if (rgz_hwc_scaled(hwc_src1)) {
        rgz_get_displayframe_rect(hwc_src1, &tmp_rect);
        rgz_cfg_scale_mode(e, hwc_src1);
    } else
        tmp_rect = *subregion_rect;

    int src1_orientation = rgz_get_orientation(hwc_src1->transform);
    int dst_orientation = 0;

    if (rgz_is_layer_nv12(hwc_src1)) {
        /*
         * Leave NV12 as 0 degree and rotate destination instead, this is done
         * because of a GC limitation. Rotate destination CW.
         */
        dst_orientation = 360 - src1_orientation;
        src1_orientation = 0;
    }

    rgz_set_src_data(params, rgz_src1, &tmp_rect, e, src1_orientation, 0);
    rgz_set_clip_rect(params, subregion_rect, e);
    rgz_set_dst_data(params, &tmp_rect, e, dst_orientation);

    if (rgz_src2) {
        /*
         * NOTE: Due to an API limitation it's not possible to blend src1 and
         * src2 if both have scaling, hence only src1 is used for now
         */
        hwc_layer_1_t *hwc_src2 = &rgz_src2->hwc_layer;
        if (rgz_hwc_scaled(hwc_src2))
            OUTE("src2 layer %p has scaling, this is not supported", hwc_src2);
        /*
         * We shouldn't receive a NV12 buffer as src2 at this point, this is an
         * invalid parameter for the blend request
         */
        if (rgz_is_layer_nv12(hwc_src2))
            OUTE("invalid input layer, src2 layer %p is NV12", hwc_src2);
        e->bp.flags |= rgz_get_flip_flags(hwc_src2->transform, 1);
        int src2_orientation = rgz_get_orientation(hwc_src2->transform);
        rgz_set_src_data(params, rgz_src2, subregion_rect, e, src2_orientation, 1);
    } else
        rgz_set_src2_is_dst(params, e);

    return e;
}

/*
 * Clear the destination buffer, if rect is NULL means the whole screen, rect
 * cannot be outside the boundaries of the screen
 */
static void rgz_out_clrdst(rgz_out_params_t *params, blit_rect_t *rect)
{
    struct rgz_blt_entry* e = rgz_blts_get(&blts, params);
    e->bp.structsize = sizeof(struct bvbltparams);
    e->bp.op.rop = 0xCCCC; /* SRCCOPY */
    e->bp.flags = BVFLAG_CLIP | BVFLAG_ROP;
    rgz_set_async(e, 1);

    struct bvsurfgeom *screen_geom;
    rgz_get_screen_info(params, &screen_geom);

    e->src1desc.structsize = sizeof(struct bvbuffdesc);
    e->src1desc.length = 4; /* 1 pixel, 32bpp */
    /*
     * With the HWC we don't bother having a buffer for the fill we'll get the
     * OMAPLFB to fixup the src1desc and stride if the auxiliary pointer is -1
     */
    e->src1desc.auxptr = (void*)-1;
    e->src1geom.structsize = sizeof(struct bvsurfgeom);
    e->src1geom.format = OCDFMT_RGBA24;
    e->bp.src1rect.left = e->bp.src1rect.top = e->src1geom.orientation = 0;
    e->src1geom.height = e->src1geom.width = e->bp.src1rect.height = e->bp.src1rect.width = 1;

    blit_rect_t clear_rect;
    if (rect) {
        clear_rect.left = rect->left;
        clear_rect.top = rect->top;
        clear_rect.right = rect->right;
        clear_rect.bottom = rect->bottom;
    } else {
        clear_rect.left = clear_rect.top = 0;
        clear_rect.right = screen_geom->width;
        clear_rect.bottom = screen_geom->height;
    }

    rgz_set_clip_rect(params, &clear_rect, e);
    rgz_set_dst_data(params, &clear_rect, e, 0);
}

static int rgz_out_bvcmd_paint(rgz_t *rgz, rgz_out_params_t *params)
{
    int rv = 0;
    int i, j;
    params->data.bvc.out_blits = 0;
    params->data.bvc.out_nhndls = 0;
    rgz_blts_init(&blts);
    rgz_out_clrdst(params, NULL);

    /* Begin from index 1 to remove the background layer from the output */
    rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;
    for (i = 1, j = 0; i < cur_fb_state->rgz_layerno; i++) {
        rgz_layer_t *rgz_layer = &cur_fb_state->rgz_layers[i];
        hwc_layer_1_t *l = &rgz_layer->hwc_layer;

        //OUTP("blitting meminfo %d", rgz->rgz_layers[i].buffidx);

        /*
         * See if it is needed to put transparent pixels where this layer
         * is located in the screen
         */
        if (rgz_layer->buffidx == -1) {
            struct bvsurfgeom *scrgeom = params->data.bvc.dstgeom;
            blit_rect_t srcregion;
            srcregion.left = max(0, l->displayFrame.left);
            srcregion.top = max(0, l->displayFrame.top);
            srcregion.bottom = min(scrgeom->height, l->displayFrame.bottom);
            srcregion.right = min(scrgeom->width, l->displayFrame.right);
            rgz_out_clrdst(params, &srcregion);
            continue;
        }

        rv = rgz_hwc_layer_blit(params, rgz_layer);
        if (rv) {
            OUTE("bvcmd_paint: error in layer %d: %d", i, rv);
            dump_all(cur_fb_state->rgz_layers, cur_fb_state->rgz_layerno, i);
            rgz_blts_free(&blts);
            return rv;
        }
        params->data.bvc.out_hndls[j++] = l->handle;
        params->data.bvc.out_nhndls++;
    }

    /* Last blit is made sync to act like a fence for the previous async blits */
    struct rgz_blt_entry* e = &blts.bvcmds[blts.idx-1];
    rgz_set_async(e, 0);

    /* FIXME: we want to be able to call rgz_blts_free and populate the actual
     * composition data structure ourselves */
    params->data.bvc.cmdp = blts.bvcmds;
    params->data.bvc.cmdlen = blts.idx;

    if (params->data.bvc.out_blits >= RGZ_MAX_BLITS) {
        rv = -1;
    // rgz_blts_free(&blts); // FIXME
    }
    return rv;
}

static float getscalew(hwc_layer_1_t *layer)
{
    int w = WIDTH(layer->sourceCrop);
    int h = HEIGHT(layer->sourceCrop);

    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(w, h);

    return ((float)WIDTH(layer->displayFrame)) / (float)w;
}

static float getscaleh(hwc_layer_1_t *layer)
{
    int w = WIDTH(layer->sourceCrop);
    int h = HEIGHT(layer->sourceCrop);

    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(w, h);

    return ((float)HEIGHT(layer->displayFrame)) / (float)h;
}

/*
 * Simple bubble sort on an array, ascending order
 */
static void rgz_bsort(int *a, int len)
{
    int i, j;
    for (i = 0; i < len; i++) {
        for (j = 0; j < i; j++) {
            if (a[i] < a[j]) {
                int temp = a[i];
                a[i] = a[j];
                a[j] = temp;
            }
        }
    }
}

/*
 * Leave only unique numbers in a sorted array
 */
static int rgz_bunique(int *a, int len)
{
    int unique = 1;
    int base = 0;
    while (base + 1 < len) {
        if (a[base] == a[base + 1]) {
            int skip = 1;
            while (base + skip < len && a[base] == a[base + skip])
                skip++;
            if (base + skip == len)
                break;
            int i;
            for (i = 0; i < skip - 1; i++)
                a[base + 1 + i] = a[base + skip];
        }
        unique++;
        base++;
    }
    return unique;
}

static void rgz_gen_blitregions(rgz_t *rgz, blit_hregion_t *hregion, int screen_width)
{
/*
 * 1. Get the offsets (left/right positions) of each layer within the
 *    hregion. Assume that layers describe the bounds of the hregion.
 * 2. We should then be able to generate an array of rects
 * 3. Each layer will have a different z-order, for each z-order
 *    find the intersection. Some intersections will be empty.
 */

    int offsets[RGZ_SUBREGIONMAX];
    int noffsets=0;
    int l, r;

    /*
     * Add damaged region, then all layers. We are guaranteed to not go outside
     * of offsets array boundaries at this point.
     */
    offsets[noffsets++] = rgz->damaged_area.left;
    offsets[noffsets++] = rgz->damaged_area.right;

    for (l = 0; l < hregion->nlayers; l++) {
        hwc_layer_1_t *layer = &hregion->rgz_layers[l]->hwc_layer;
        /* Make sure the subregion is not outside the boundaries of the screen */
        offsets[noffsets++] = max(0, layer->displayFrame.left);
        offsets[noffsets++] = min(layer->displayFrame.right, screen_width);
    }
    rgz_bsort(offsets, noffsets);
    noffsets = rgz_bunique(offsets, noffsets);
    hregion->nsubregions = noffsets - 1;
    bzero(hregion->blitrects, sizeof(hregion->blitrects));
    for (r = 0; r + 1 < noffsets; r++) {
        blit_rect_t subregion;
        subregion.top = hregion->rect.top;
        subregion.bottom = hregion->rect.bottom;
        subregion.left = offsets[r];
        subregion.right = offsets[r+1];

        ALOGD_IF(debug, "                sub l %d r %d",
            subregion.left, subregion.right);
        for (l = 0; l < hregion->nlayers; l++) {
            hwc_layer_1_t *layer = &hregion->rgz_layers[l]->hwc_layer;
            if (RECT_INTERSECTS(subregion, layer->displayFrame)) {

                hregion->blitrects[l][r] = subregion;

                ALOGD_IF(debug, "hregion->blitrects[%d][%d] (%d %d %d %d)", l, r,
                        hregion->blitrects[l][r].left,
                        hregion->blitrects[l][r].top,
                        hregion->blitrects[l][r].right,
                        hregion->blitrects[l][r].bottom);
            }
        }
    }
}

static int rgz_hwc_scaled(hwc_layer_1_t *layer)
{
    int w = WIDTH(layer->sourceCrop);
    int h = HEIGHT(layer->sourceCrop);

    if (layer->transform & HWC_TRANSFORM_ROT_90)
        swap(w, h);

    return WIDTH(layer->displayFrame) != w || HEIGHT(layer->displayFrame) != h;
}

static int rgz_in_valid_hwc_layer(hwc_layer_1_t *layer)
{
    IMG_native_handle_t *handle = (IMG_native_handle_t *)layer->handle;
    if ((layer->flags & HWC_SKIP_LAYER) || !handle)
        return 0;

    if (is_NV12(handle->iFormat))
        return handle->iFormat == HAL_PIXEL_FORMAT_TI_NV12;

    /* FIXME: The following must be removed when GC supports vertical/horizontal
     * buffer flips, please note having a FLIP_H and FLIP_V means 180 rotation
     * which is supported indeed
     */
    if (layer->transform) {
        int is_flipped = !!(layer->transform & HWC_TRANSFORM_FLIP_H) ^ !!(layer->transform & HWC_TRANSFORM_FLIP_V);
        if (is_flipped) {
            ALOGE("Layer %p is flipped %d", layer, layer->transform);
            return 0;
        }
    }

    switch(handle->iFormat) {
    case HAL_PIXEL_FORMAT_BGRX_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
    case HAL_PIXEL_FORMAT_RGB_565:
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_BGRA_8888:
        break;
    default:
        return 0;
    }
    return 1;
}

/* Reset dirty region data and state */
static void rgz_delete_region_data(rgz_t *rgz){
    if (!rgz)
        return;
    if (rgz->hregions)
        free(rgz->hregions);
    rgz->hregions = NULL;
    rgz->nhregions = 0;
    rgz->state &= ~RGZ_REGION_DATA;
}

static rgz_fb_state_t* get_prev_fb_state(rgz_t *rgz)
{
    return &rgz->fb_states[rgz->fb_state_idx];
}

static rgz_fb_state_t* get_next_fb_state(rgz_t *rgz)
{
    rgz->fb_state_idx = (rgz->fb_state_idx + 1) % RGZ_NUM_FB;
    return &rgz->fb_states[rgz->fb_state_idx];
}

static void rgz_add_to_damaged_area(rgz_in_params_t *params, rgz_layer_t *rgz_layer,
    blit_rect_t *damaged_area)
{
    struct bvsurfgeom *screen_geom = params->data.hwc.dstgeom;
    hwc_layer_1_t *layer = &rgz_layer->hwc_layer;

    blit_rect_t screen_rect;
    screen_rect.left = screen_rect.top = 0;
    screen_rect.right = screen_geom->width;
    screen_rect.bottom = screen_geom->height;

    /* Ignore the layer rectangle if it doesn't intersect the screen */
    if (!RECT_INTERSECTS(screen_rect, layer->displayFrame))
        return;

    /* Clip the layer rectangle to the screen geometry */
    blit_rect_t layer_rect;
    rgz_get_displayframe_rect(layer, &layer_rect);
    layer_rect.left = max(0, layer_rect.left);
    layer_rect.top = max(0, layer_rect.top);
    layer_rect.right = min(screen_rect.right, layer_rect.right);
    layer_rect.bottom = min(screen_rect.bottom, layer_rect.bottom);

    /* Then add the rectangle to the damage area */
    if (empty_rect(damaged_area)) {
        /* Adding for the first time */
        damaged_area->left = layer_rect.left;
        damaged_area->top = layer_rect.top;
        damaged_area->right = layer_rect.right;
        damaged_area->bottom = layer_rect.bottom;
    } else {
        /* Grow current damaged area */
        damaged_area->left = min(damaged_area->left, layer_rect.left);
        damaged_area->top = min(damaged_area->top, layer_rect.top);
        damaged_area->right = max(damaged_area->right, layer_rect.right);
        damaged_area->bottom = max(damaged_area->bottom, layer_rect.bottom);
    }
}

/* Search a layer with the specified identity in the passed array */
static rgz_layer_t* rgz_find_layer(rgz_layer_t *rgz_layers, int rgz_layerno,
    uint32_t layer_identity)
{
    int i;
    for (i = 0; i < rgz_layerno; i++) {
        rgz_layer_t *rgz_layer = &rgz_layers[i];
        /* Ignore background layer, it has no identity */
        if (rgz_layer->buffidx == RGZ_BACKGROUND_BUFFIDX)
            continue;
        if (rgz_layer->identity == layer_identity)
            return rgz_layer;
    }
    return NULL;
}

/* Determines if two layers with the same identity have changed its own window content */
static int rgz_has_layer_content_changed(rgz_layer_t *cur_rgz_layer, rgz_layer_t *prev_rgz_layer)
{
    hwc_layer_1_t *cur_hwc_layer = &cur_rgz_layer->hwc_layer;
    hwc_layer_1_t *prev_hwc_layer = &prev_rgz_layer->hwc_layer;

    /* The background has no identity and never changes */
    if (cur_rgz_layer->buffidx == RGZ_BACKGROUND_BUFFIDX &&
        prev_rgz_layer->buffidx == RGZ_BACKGROUND_BUFFIDX)
        return 0;

    if (cur_rgz_layer->identity != prev_rgz_layer->identity) {
        OUTE("%s: Invalid input, layer identities differ (current=%d, prev=%d)",
            __func__, cur_rgz_layer->identity, prev_rgz_layer->identity);
        return 1;
    }

    /* If the layer has the clear fb hint we don't care about the content */
    if (cur_rgz_layer->buffidx == RGZ_CLEARHINT_BUFFIDX &&
        prev_rgz_layer->buffidx == RGZ_CLEARHINT_BUFFIDX)
        return 0;

    /* Check if the layer content has changed */
    if (cur_hwc_layer->handle != prev_hwc_layer->handle ||
        cur_hwc_layer->transform != prev_hwc_layer->transform ||
        cur_hwc_layer->sourceCrop.top != prev_hwc_layer->sourceCrop.top ||
        cur_hwc_layer->sourceCrop.left != prev_hwc_layer->sourceCrop.left ||
        cur_hwc_layer->sourceCrop.bottom != prev_hwc_layer->sourceCrop.bottom ||
        cur_hwc_layer->sourceCrop.right != prev_hwc_layer->sourceCrop.right)
        return 1;

    return 0;
}

/* Determines if two layers with the same identity have changed their screen position */
static int rgz_has_layer_frame_moved(rgz_layer_t *cur_rgz_layer, rgz_layer_t *target_rgz_layer)
{
    hwc_layer_1_t *cur_hwc_layer = &cur_rgz_layer->hwc_layer;
    hwc_layer_1_t *target_hwc_layer = &target_rgz_layer->hwc_layer;

    if (cur_rgz_layer->identity != target_rgz_layer->identity) {
        OUTE("%s: Invalid input, layer identities differ (current=%d, target=%d)",
            __func__, cur_rgz_layer->identity, target_rgz_layer->identity);
        return 1;
    }

    if (cur_hwc_layer->displayFrame.top != target_hwc_layer->displayFrame.top ||
        cur_hwc_layer->displayFrame.left != target_hwc_layer->displayFrame.left ||
        cur_hwc_layer->displayFrame.bottom != target_hwc_layer->displayFrame.bottom ||
        cur_hwc_layer->displayFrame.right != target_hwc_layer->displayFrame.right)
        return 1;

    return 0;
}

static void rgz_handle_dirty_region(rgz_t *rgz, rgz_in_params_t *params,
    rgz_fb_state_t* prev_fb_state, rgz_fb_state_t* target_fb_state)
{
    /* Reset damaged area */
    bzero(&rgz->damaged_area, sizeof(rgz->damaged_area));

    int i;
    rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;

    for (i = 0; i < cur_fb_state->rgz_layerno; i++) {
        rgz_layer_t *cur_rgz_layer = &cur_fb_state->rgz_layers[i];
        rgz_layer_t *prev_rgz_layer = NULL;
        int layer_changed = 0;

        if (i == 0) {
            /*
             * Background is always zero, no need to search for it. If the previous state
             * is empty reset the dirty count for the background layer.
             */
            if (prev_fb_state->rgz_layerno)
                prev_rgz_layer = &prev_fb_state->rgz_layers[0];
        } else {
            /* Find out if this layer was present in the previous frame */
            prev_rgz_layer = rgz_find_layer(prev_fb_state->rgz_layers,
                prev_fb_state->rgz_layerno, cur_rgz_layer->identity);
        }

        /* Check if the layer is new or if the content changed from the previous frame */
        if (prev_rgz_layer && !rgz_has_layer_content_changed(cur_rgz_layer, prev_rgz_layer)) {
            /* Copy previous dirty count */
            cur_rgz_layer->dirty_count = prev_rgz_layer->dirty_count;
            cur_rgz_layer->dirty_count -= cur_rgz_layer->dirty_count ? 1 : 0;
        } else
            cur_rgz_layer->dirty_count = RGZ_NUM_FB;

        /* If the layer is new, redraw the layer area */
        if (!prev_rgz_layer) {
            rgz_add_to_damaged_area(params, cur_rgz_layer, &rgz->damaged_area);
            continue;
        }

        /* Nothing more to do with the background layer */
        if (i == 0)
            continue;

        /* Find out if the layer is present in the target frame */
        rgz_layer_t *target_rgz_layer = rgz_find_layer(target_fb_state->rgz_layers,
                target_fb_state->rgz_layerno, cur_rgz_layer->identity);

        if (target_rgz_layer) {
            /* Find out if the window size and position are different from the target frame */
            if (rgz_has_layer_frame_moved(cur_rgz_layer, target_rgz_layer)) {
                /*
                 * Redraw both layer areas. This will effectively clear the area where
                 * this layer was in the target frame and force to draw the new layer
                 * location.
                 */
                rgz_add_to_damaged_area(params, cur_rgz_layer, &rgz->damaged_area);
                rgz_add_to_damaged_area(params, target_rgz_layer, &rgz->damaged_area);
                cur_rgz_layer->dirty_count = RGZ_NUM_FB;
            }
        } else {
            /* If the layer is not in the target just draw it's new location */
            rgz_add_to_damaged_area(params, cur_rgz_layer, &rgz->damaged_area);
        }
    }

    /*
     * Add to damage area layers missing from the target frame to the current frame
     * ignoring the background
     */
    for (i = 1; i < target_fb_state->rgz_layerno; i++) {
        rgz_layer_t *target_rgz_layer = &target_fb_state->rgz_layers[i];

        rgz_layer_t *cur_rgz_layer = rgz_find_layer(cur_fb_state->rgz_layers,
            cur_fb_state->rgz_layerno, target_rgz_layer->identity);

        /* Layers present in the target have been handled already in the loop above */
        if (cur_rgz_layer)
            continue;

        /* The target layer is not present in the current frame, redraw its area */
        rgz_add_to_damaged_area(params, target_rgz_layer, &rgz->damaged_area);
    }
}

/* Adds the background layer in first the position of the passed fb state */
static void rgz_add_background_layer(rgz_fb_state_t *fb_state)
{
    rgz_layer_t *rgz_layer = &fb_state->rgz_layers[0];
    rgz_layer->hwc_layer = bg_layer;
    rgz_layer->buffidx = RGZ_BACKGROUND_BUFFIDX;
    /* Set dummy handle to maintain dirty region state */
    rgz_layer->hwc_layer.handle = (void*) 0x1;
}

static int rgz_in_hwccheck(rgz_in_params_t *p, rgz_t *rgz)
{
    hwc_layer_1_t *layers = p->data.hwc.layers;
    hwc_layer_extended_t *extlayers = p->data.hwc.extlayers;
    int layerno = p->data.hwc.layerno;

    rgz->state &= ~RGZ_STATE_INIT;

    if (!layers)
        return -1;

    /* For debugging */
    //dump_all(layers, layerno, 0);

    /*
     * Store buffer index to be sent in the HWC Post2 list. Any overlay
     * meminfos must come first
     */
    int l, memidx = 0;
    for (l = 0; l < layerno; l++) {
        if (layers[l].compositionType == HWC_OVERLAY)
            memidx++;
    }

    int possible_blit = 0, candidates = 0;

    /*
     * Insert the background layer at the beginning of the list, maintain a
     * state for dirty region handling
     */
    rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;
    rgz_add_background_layer(cur_fb_state);

    for (l = 0; l < layerno; l++) {
        if (layers[l].compositionType == HWC_FRAMEBUFFER) {
            candidates++;
            if (rgz_in_valid_hwc_layer(&layers[l]) &&
                    possible_blit < RGZ_INPUT_MAXLAYERS) {
                rgz_layer_t *rgz_layer = &cur_fb_state->rgz_layers[possible_blit+1];
                rgz_layer->hwc_layer = layers[l];
                rgz_layer->identity = extlayers[l].identity;
                rgz_layer->buffidx = memidx++;
                possible_blit++;
            }
            continue;
        }

        if (layers[l].hints & HWC_HINT_CLEAR_FB) {
            candidates++;
            if (possible_blit < RGZ_INPUT_MAXLAYERS) {
                /*
                 * Use only the layer rectangle as an input to regionize when the clear
                 * fb hint is present, mark this layer to identify it.
                 */
                rgz_layer_t *rgz_layer = &cur_fb_state->rgz_layers[possible_blit+1];
                rgz_layer->hwc_layer = layers[l];
                rgz_layer->identity = extlayers[l].identity;
                rgz_layer->buffidx = RGZ_CLEARHINT_BUFFIDX;
                /* Set dummy handle to maintain dirty region state */
                rgz_layer->hwc_layer.handle = (void*) 0x1;
                possible_blit++;
            }
        }
    }

    if (!possible_blit || possible_blit != candidates) {
        return -1;
    }

    rgz->state |= RGZ_STATE_INIT;
    cur_fb_state->rgz_layerno = possible_blit + 1; /* Account for background layer */

    /* Get the target and previous frame geometries */
    rgz_fb_state_t* prev_fb_state = get_prev_fb_state(rgz);
    rgz_fb_state_t* target_fb_state = get_next_fb_state(rgz);

    /* Modifiy dirty counters and create the damaged region */
    rgz_handle_dirty_region(rgz, p, prev_fb_state, target_fb_state);

    /* Copy the current geometry to use it in the next frame */
    memcpy(target_fb_state->rgz_layers, cur_fb_state->rgz_layers, sizeof(rgz_layer_t) * cur_fb_state->rgz_layerno);
    target_fb_state->rgz_layerno = cur_fb_state->rgz_layerno;

    return RGZ_ALL;
}

static int rgz_in_hwc(rgz_in_params_t *p, rgz_t *rgz)
{
    int i, j;
    int yentries[RGZ_SUBREGIONMAX];
    int dispw;  /* widest layer */
    int screen_width = p->data.hwc.dstgeom->width;
    int screen_height = p->data.hwc.dstgeom->height;
    rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;

    if (!(rgz->state & RGZ_STATE_INIT)) {
        OUTE("rgz_process started with bad state");
        return -1;
    }

    /*
     * Figure out if there is enough space to store the top-bottom coordinates
     * of each layer including the damaged area
     */
    if (((cur_fb_state->rgz_layerno + 1) * 2) > RGZ_SUBREGIONMAX) {
        OUTE("%s: Not enough space to store top-bottom coordinates of each layer (max %d, needed %d*2)",
            __func__, RGZ_SUBREGIONMAX, cur_fb_state->rgz_layerno + 1);
        return -1;
    }

    /* Delete the previous region data */
    rgz_delete_region_data(rgz);

    /*
     * Find the horizontal regions, add damaged area first which is already
     * inside display boundaries
     */
    int ylen = 0;
    yentries[ylen++] = rgz->damaged_area.top;
    yentries[ylen++] = rgz->damaged_area.bottom;
    dispw = rgz->damaged_area.right;

    /* Add the top and bottom coordinates of each layer */
    for (i = 0; i < cur_fb_state->rgz_layerno; i++) {
        hwc_layer_1_t *layer = &cur_fb_state->rgz_layers[i].hwc_layer;
        /* Maintain regions inside display boundaries */
        yentries[ylen++] = max(0, layer->displayFrame.top);
        yentries[ylen++] = min(layer->displayFrame.bottom, screen_height);
        dispw = dispw > layer->displayFrame.right ? dispw : layer->displayFrame.right;
    }
    rgz_bsort(yentries, ylen);
    ylen = rgz_bunique(yentries, ylen);

    /* at this point we have an array of horizontal regions */
    rgz->nhregions = ylen - 1;

    blit_hregion_t *hregions = calloc(rgz->nhregions, sizeof(blit_hregion_t));
    if (!hregions) {
        OUTE("Unable to allocate memory for hregions");
        return -1;
    }
    rgz->hregions = hregions;

    ALOGD_IF(debug, "Allocated %d regions (sz = %d), layerno = %d", rgz->nhregions,
        rgz->nhregions * sizeof(blit_hregion_t), cur_fb_state->rgz_layerno);

    for (i = 0; i < rgz->nhregions; i++) {
        hregions[i].rect.top = yentries[i];
        hregions[i].rect.bottom = yentries[i+1];
        /* Avoid hregions outside the display boundaries */
        hregions[i].rect.left = 0;
        hregions[i].rect.right = dispw > screen_width ? screen_width : dispw;
        hregions[i].nlayers = 0;
        for (j = 0; j < cur_fb_state->rgz_layerno; j++) {
            hwc_layer_1_t *layer = &cur_fb_state->rgz_layers[j].hwc_layer;
            if (RECT_INTERSECTS(hregions[i].rect, layer->displayFrame)) {
                int l = hregions[i].nlayers++;
                hregions[i].rgz_layers[l] = &cur_fb_state->rgz_layers[j];
            }
        }
    }

    /* Calculate blit regions */
    for (i = 0; i < rgz->nhregions; i++) {
        rgz_gen_blitregions(rgz, &hregions[i], screen_width);
        ALOGD_IF(debug, "hregion %3d: nsubregions %d", i, hregions[i].nsubregions);
        ALOGD_IF(debug, "           : %d to %d: ",
            hregions[i].rect.top, hregions[i].rect.bottom);
        for (j = 0; j < hregions[i].nlayers; j++)
            ALOGD_IF(debug, "              %p ", &hregions[i].rgz_layers[j]->hwc_layer);
    }
    rgz->state |= RGZ_REGION_DATA;
    return 0;
}

/*
 * generate a human readable description of the layer
 *
 * idx, flags, fmt, type, sleft, stop, sright, sbot, dleft, dtop, \
 * dright, dbot, rot, flip, blending, scalew, scaleh, visrects
 *
 */
static void rgz_print_layer(hwc_layer_1_t *l, int idx, int csv)
{
    char big_log[1024];
    int e = sizeof(big_log);
    char *end = big_log + e;
    e -= snprintf(end - e, e, "<!-- LAYER-DAT: %d", idx);


    e -= snprintf(end - e, e, "%s %p", csv ? "," : " hndl:",
            l->handle ? l->handle : NULL);

    e -= snprintf(end - e, e, "%s %s", csv ? "," : " flags:",
        l->flags & HWC_SKIP_LAYER ? "skip" : "none");

    IMG_native_handle_t *handle = (IMG_native_handle_t *)l->handle;
    if (handle) {
        e -= snprintf(end - e, e, "%s", csv ? ", " : " fmt: ");
        switch(handle->iFormat) {
        case HAL_PIXEL_FORMAT_BGRA_8888:
            e -= snprintf(end - e, e, "bgra"); break;
        case HAL_PIXEL_FORMAT_RGB_565:
            e -= snprintf(end - e, e, "rgb565"); break;
        case HAL_PIXEL_FORMAT_BGRX_8888:
            e -= snprintf(end - e, e, "bgrx"); break;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            e -= snprintf(end - e, e, "rgbx"); break;
        case HAL_PIXEL_FORMAT_RGBA_8888:
            e -= snprintf(end - e, e, "rgba"); break;
        case HAL_PIXEL_FORMAT_TI_NV12:
        case HAL_PIXEL_FORMAT_TI_NV12_PADDED:
            e -= snprintf(end - e, e, "nv12"); break;
        default:
            e -= snprintf(end - e, e, "unknown");
        }
        e -= snprintf(end - e, e, "%s", csv ? ", " : " type: ");
        if (handle->usage & GRALLOC_USAGE_HW_RENDER)
            e -= snprintf(end - e, e, "hw");
        else if (handle->usage & GRALLOC_USAGE_SW_READ_MASK ||
                 handle->usage & GRALLOC_USAGE_SW_WRITE_MASK)
            e -= snprintf(end - e, e, "sw");
        else
            e -= snprintf(end - e, e, "unknown");
    } else {
        e -= snprintf(end - e, e, csv ? ", unknown" : " fmt: unknown");
        e -= snprintf(end - e, e, csv ? ", na" : " type: na");
    }
    e -= snprintf(end - e, e, csv ? ", %d, %d, %d, %d" : " src: %d %d %d %d",
        l->sourceCrop.left, l->sourceCrop.top, l->sourceCrop.right,
        l->sourceCrop.bottom);
    e -= snprintf(end - e, e, csv ? ", %d, %d, %d, %d" : " disp: %d %d %d %d",
        l->displayFrame.left, l->displayFrame.top,
        l->displayFrame.right, l->displayFrame.bottom);

    e -= snprintf(end - e, e, "%s %s", csv ? "," : " rot:",
        l->transform & HWC_TRANSFORM_ROT_90 ? "90" :
            l->transform & HWC_TRANSFORM_ROT_180 ? "180" :
            l->transform & HWC_TRANSFORM_ROT_270 ? "270" : "none");

    char flip[5] = "";
    strcat(flip, l->transform & HWC_TRANSFORM_FLIP_H ? "H" : "");
    strcat(flip, l->transform & HWC_TRANSFORM_FLIP_V ? "V" : "");
    if (!(l->transform & (HWC_TRANSFORM_FLIP_V|HWC_TRANSFORM_FLIP_H)))
        strcpy(flip, "none");
    e -= snprintf(end - e, e, "%s %s", csv ? "," : " flip:", flip);

    e -= snprintf(end - e, e, "%s %s", csv ? "," : " blending:",
        l->blending == HWC_BLENDING_NONE ? "none" :
        l->blending == HWC_BLENDING_PREMULT ? "premult" :
        l->blending == HWC_BLENDING_COVERAGE ? "coverage" : "invalid");

    e -= snprintf(end - e, e, "%s %1.3f", csv ? "," : " scalew:", getscalew(l));
    e -= snprintf(end - e, e, "%s %1.3f", csv ? "," : " scaleh:", getscaleh(l));

    e -= snprintf(end - e, e, "%s %d", csv ? "," : " visrect:",
        l->visibleRegionScreen.numRects);

    if (!csv) {
        e -= snprintf(end - e, e, " -->");
        OUTP("%s", big_log);

        size_t i = 0;
        for (; i < l->visibleRegionScreen.numRects; i++) {
            hwc_rect_t const *r = &l->visibleRegionScreen.rects[i];
            OUTP("<!-- LAYER-VIS: %d: rect: %d %d %d %d -->",
                    i, r->left, r->top, r->right, r->bottom);
        }
    } else {
        size_t i = 0;
        for (; i < l->visibleRegionScreen.numRects; i++) {
            hwc_rect_t const *r = &l->visibleRegionScreen.rects[i];
            e -= snprintf(end - e, e, ", %d, %d, %d, %d",
                    r->left, r->top, r->right, r->bottom);
        }
        e -= snprintf(end - e, e, " -->");
        OUTP("%s", big_log);
    }
}

static void rgz_print_layers(hwc_display_contents_1_t* list, int csv)
{
    size_t i;
    for (i = 0; i < list->numHwLayers; i++) {
        hwc_layer_1_t *l = &list->hwLayers[i];
        rgz_print_layer(l, i, csv);
    }
}

static int hal_to_ocd(int color)
{
    switch(color) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
        return OCDFMT_BGRA24;
    case HAL_PIXEL_FORMAT_BGRX_8888:
        return OCDFMT_BGR124;
    case HAL_PIXEL_FORMAT_RGB_565:
        return OCDFMT_RGB16;
    case HAL_PIXEL_FORMAT_RGBA_8888:
        return OCDFMT_RGBA24;
    case HAL_PIXEL_FORMAT_RGBX_8888:
        return OCDFMT_RGB124;
    case HAL_PIXEL_FORMAT_TI_NV12:
        return OCDFMT_NV12;
    case HAL_PIXEL_FORMAT_YV12:
        return OCDFMT_YV12;
    default:
        return OCDFMT_UNKNOWN;
    }
}

static BVFN_MAP bv_map;
static BVFN_BLT bv_blt;
static BVFN_UNMAP bv_unmap;

static int rgz_handle_to_stride(IMG_native_handle_t *h)
{
    int bpp = is_NV12(h->iFormat) ? 0 : (h->iFormat == HAL_PIXEL_FORMAT_RGB_565 ? 2 : 4);
    int stride = ALIGN(h->iWidth, HW_ALIGN) * bpp;
    return stride;
}

static int rgz_get_orientation(unsigned int transform)
{
    int orientation = 0;
    if ((transform & HWC_TRANSFORM_FLIP_H) && (transform & HWC_TRANSFORM_FLIP_V))
        orientation += 180;
    if (transform & HWC_TRANSFORM_ROT_90)
        orientation += 90;

    return orientation;
}

static int rgz_get_flip_flags(unsigned int transform, int use_src2_flags)
{
    /*
     * If vertical and horizontal flip flags are set it means a 180 rotation
     * (with no flip) is intended for the layer, so we return 0 in that case.
     */
    int flip_flags = 0;
    if (transform & HWC_TRANSFORM_FLIP_H)
        flip_flags |= (use_src2_flags ? BVFLAG_HORZ_FLIP_SRC2 : BVFLAG_HORZ_FLIP_SRC1);
    if (transform & HWC_TRANSFORM_FLIP_V)
        flip_flags = flip_flags ? 0 : flip_flags | (use_src2_flags ? BVFLAG_VERT_FLIP_SRC2 : BVFLAG_VERT_FLIP_SRC1);
    return flip_flags;
}

static int rgz_hwc_layer_blit(rgz_out_params_t *params, rgz_layer_t *rgz_layer)
{
    hwc_layer_1_t* layer = &rgz_layer->hwc_layer;
    blit_rect_t srcregion;
    rgz_get_displayframe_rect(layer, &srcregion);

    int noblend = rgz_is_blending_disabled(params);
    if (!noblend && layer->blending == HWC_BLENDING_PREMULT)
        rgz_hwc_subregion_blend(params, &srcregion, rgz_layer, NULL);
    else
        rgz_hwc_subregion_copy(params, &srcregion, rgz_layer);

    return 0;
}

static int rgz_can_blend_together(hwc_layer_1_t* src1_layer, hwc_layer_1_t* src2_layer)
{
    /* If any layer is scaled we cannot blend both layers in one blit */
    if (rgz_hwc_scaled(src1_layer) || rgz_hwc_scaled(src2_layer))
        return 0;

    /* NV12 buffers don't have alpha information on it */
    IMG_native_handle_t *src1_hndl = (IMG_native_handle_t *)src1_layer->handle;
    IMG_native_handle_t *src2_hndl = (IMG_native_handle_t *)src2_layer->handle;
    if (is_NV12(src1_hndl->iFormat) || is_NV12(src2_hndl->iFormat))
        return 0;

    return 1;
}

static void rgz_batch_entry(struct rgz_blt_entry* e, unsigned int flag, unsigned int set)
{
    e->bp.flags &= ~BVFLAG_BATCH_MASK;
    e->bp.flags |= flag;
    e->bp.batchflags |= set;
}

static int rgz_hwc_subregion_blit(blit_hregion_t *hregion, int sidx, rgz_out_params_t *params,
    blit_rect_t *damaged_area)
{
    int lix;
    int ldepth = get_layer_ops(hregion, sidx, &lix);
    if (ldepth == 0) {
        /* Impossible, there are no layers in this region even if the
         * background is covering the whole screen
         */
        OUTE("hregion %p subregion %d doesn't have any ops", hregion, sidx);
        return -1;
    }

    /* Determine if this region is dirty */
    int dirty = 0;
    blit_rect_t *subregion_rect = &hregion->blitrects[lix][sidx];
    if (RECT_INTERSECTS(*damaged_area, *subregion_rect)) {
        /* The subregion intersects the damaged area, draw unconditionally */
        dirty = 1;
    } else {
        int dirtylix = lix;
        while (dirtylix != -1) {
            rgz_layer_t *rgz_layer = hregion->rgz_layers[dirtylix];
            if (rgz_layer->dirty_count){
                /* One of the layers is dirty, we need to generate blits for this subregion */
                dirty = 1;
                break;
            }
            dirtylix = get_layer_ops_next(hregion, sidx, dirtylix);
        }
    }
    if (!dirty)
        return 0;

    /* Check if the bottom layer is the background */
    if (hregion->rgz_layers[lix]->buffidx == RGZ_BACKGROUND_BUFFIDX) {
        if (ldepth == 1) {
            /* Background layer is the only operation, clear subregion */
            rgz_out_clrdst(params, &hregion->blitrects[lix][sidx]);
            return 0;
        } else {
            /* No need to generate blits with background layer if there is
             * another layer on top of it, discard it
             */
            ldepth--;
            lix = get_layer_ops_next(hregion, sidx, lix);
        }
    }

    /*
     * See if the depth most layer needs to be ignored. If this layer is the
     * only operation, we need to clear this subregion.
     */
    if (hregion->rgz_layers[lix]->buffidx == RGZ_CLEARHINT_BUFFIDX) {
        ldepth--;
        if (!ldepth) {
            rgz_out_clrdst(params, &hregion->blitrects[lix][sidx]);
            return 0;
        }
        lix = get_layer_ops_next(hregion, sidx, lix);
    }

    int noblend = rgz_is_blending_disabled(params);

    if (!noblend && ldepth > 1) { /* BLEND */
        blit_rect_t *rect = &hregion->blitrects[lix][sidx];
        struct rgz_blt_entry* e;

        int s2lix = lix;
        lix = get_layer_ops_next(hregion, sidx, lix);

        /*
         * We save a read and a write from the FB if we blend the bottom
         * two layers, we can do this only if both layers are not scaled
         */
        int prev_layer_scaled = 0;
        int prev_layer_nv12 = 0;
        int first_batchflags = 0;
        rgz_layer_t *rgz_src1 = hregion->rgz_layers[lix];
        rgz_layer_t *rgz_src2 = hregion->rgz_layers[s2lix];
        if (rgz_can_blend_together(&rgz_src1->hwc_layer, &rgz_src2->hwc_layer))
            e = rgz_hwc_subregion_blend(params, rect, rgz_src1, rgz_src2);
        else {
            /* Return index to the first operation and make a copy of the first layer */
            lix = s2lix;
            rgz_src1 = hregion->rgz_layers[lix];
            e = rgz_hwc_subregion_copy(params, rect, rgz_src1);
            /*
             * First blit is a copy, the rest will be blends, hence the operation
             * changed on the second blit.
             */
            first_batchflags |= BVBATCH_OP;
            prev_layer_nv12 = rgz_is_layer_nv12(&rgz_src1->hwc_layer);
            prev_layer_scaled = rgz_hwc_scaled(&rgz_src1->hwc_layer);
        }

        /*
         * Regardless if the first blit is a copy or blend, src2 may have changed
         * on the second blit
         */
        first_batchflags |= BVBATCH_SRC2 | BVBATCH_SRC2RECT_ORIGIN | BVBATCH_SRC2RECT_SIZE;

        rgz_batch_entry(e, BVFLAG_BATCH_BEGIN, 0);

        /* Rest of layers blended with FB */
        while((lix = get_layer_ops_next(hregion, sidx, lix)) != -1) {
            int batchflags = first_batchflags;
            first_batchflags = 0;
            rgz_src1 = hregion->rgz_layers[lix];

            /* Blend src1 into dst */
            e = rgz_hwc_subregion_blend(params, rect, rgz_src1, NULL);

            /*
             * NOTE: After the first blit is configured, consequent blits are
             * blend operations done with src1 and the destination, that is,
             * src2 is the same as dst, any batchflag changed for the destination
             * applies to src2 as well.
             */

            /* src1 parameters always change on every blit */
            batchflags |= BVBATCH_SRC1 | BVBATCH_SRC1RECT_ORIGIN| BVBATCH_SRC1RECT_SIZE;

            /*
             * If the current/previous layer has scaling, destination rectangles
             * likely changed as well as the scaling mode. Clipping rectangle
             * remains the same as well as destination geometry.
             */
            int cur_layer_scaled = rgz_hwc_scaled(&rgz_src1->hwc_layer);
            if (cur_layer_scaled || prev_layer_scaled) {
                batchflags |= BVBATCH_DSTRECT_ORIGIN | BVBATCH_DSTRECT_SIZE |
                    BVBATCH_SRC2RECT_ORIGIN | BVBATCH_SRC2RECT_SIZE |
                    BVBATCH_SCALE;
            }
            prev_layer_scaled = cur_layer_scaled;

            /*
             * If the current/previous layer is NV12, the destination geometry
             * could have been rotated, hence the destination and clipping
             * rectangles might have been trasformed to match the rotated
             * destination geometry.
             */
            int cur_layer_nv12 = rgz_is_layer_nv12(&rgz_src1->hwc_layer);
            if (cur_layer_nv12 || prev_layer_nv12) {
                batchflags |= BVBATCH_DST | BVBATCH_DSTRECT_ORIGIN | BVBATCH_DSTRECT_SIZE |
                    BVBATCH_SRC2 | BVBATCH_SRC2RECT_ORIGIN | BVBATCH_SRC2RECT_SIZE |
                    BVBATCH_CLIPRECT;
            }
            prev_layer_nv12 = cur_layer_nv12;

            rgz_batch_entry(e, BVFLAG_BATCH_CONTINUE, batchflags);
        }

        if (e->bp.flags & BVFLAG_BATCH_BEGIN)
            rgz_batch_entry(e, 0, 0);
        else
            rgz_batch_entry(e, BVFLAG_BATCH_END, 0);

    } else { /* COPY */
        blit_rect_t *rect = &hregion->blitrects[lix][sidx];
        if (noblend)    /* get_layer_ops() doesn't understand this so get the top */
            lix = get_top_rect(hregion, sidx, &rect);
        rgz_hwc_subregion_copy(params, rect, hregion->rgz_layers[lix]);
    }
    return 0;
}

struct bvbuffdesc gscrndesc = {
    .structsize = sizeof(struct bvbuffdesc), .length = 0,
    .auxptr = MAP_FAILED
};
struct bvsurfgeom gscrngeom = {
    .structsize = sizeof(struct bvsurfgeom), .format = OCDFMT_UNKNOWN
};

static void rgz_blts_init(struct rgz_blts *blts)
{
    bzero(blts, sizeof(*blts));
}

static void rgz_blts_free(struct rgz_blts *blts)
{
    /* TODO ??? maybe we should dynamically allocate this */
    rgz_blts_init(blts);
}

static struct rgz_blt_entry* rgz_blts_get(struct rgz_blts *blts, rgz_out_params_t *params)
{
    struct rgz_blt_entry *ne;
    if (blts->idx < RGZ_MAX_BLITS) {
        ne = &blts->bvcmds[blts->idx++];
        if (IS_BVCMD(params))
            params->data.bvc.out_blits++;
    } else {
        OUTE("!!! BIG PROBLEM !!! run out of blit entries");
        ne = &blts->bvcmds[blts->idx - 1]; /* Return last slot */
    }
    return ne;
}

static int rgz_blts_bvdirect(rgz_t *rgz, struct rgz_blts *blts, rgz_out_params_t *params)
{
    struct bvbatch *batch = NULL;
    int rv = -1;
    int idx = 0;

    while (idx < blts->idx) {
        struct rgz_blt_entry *e = &blts->bvcmds[idx];
        if (e->bp.flags & BVFLAG_BATCH_MASK)
            e->bp.batch = batch;
        rv = bv_blt(&e->bp);
        if (rv) {
            OUTE("BV_BLT failed: %d", rv);
            BVDUMP("bv_blt:", "  ", &e->bp);
            return -1;
        }
        if (e->bp.flags & BVFLAG_BATCH_BEGIN)
            batch = e->bp.batch;
        idx++;
    }
    return rv;
}

static int rgz_out_region(rgz_t *rgz, rgz_out_params_t *params)
{
    if (!(rgz->state & RGZ_REGION_DATA)) {
        OUTE("rgz_out_region invoked with bad state");
        return -1;
    }

    rgz_blts_init(&blts);
    ALOGD_IF(debug, "rgz_out_region:");

    if (IS_BVCMD(params))
        params->data.bvc.out_blits = 0;

    int i;
    for (i = 0; i < rgz->nhregions; i++) {
        blit_hregion_t *hregion = &rgz->hregions[i];
        int s;
        ALOGD_IF(debug, "h[%d] nsubregions = %d", i, hregion->nsubregions);
        if (hregion->nlayers == 0) {
            /* Impossible, there are no layers in this region even if the
             * background is covering the whole screen
             */
            OUTE("hregion %p doesn't have any ops", hregion);
            return -1;
        }
        for (s = 0; s < hregion->nsubregions; s++) {
            ALOGD_IF(debug, "h[%d] -> [%d]", i, s);
            if (rgz_hwc_subregion_blit(hregion, s, params, &rgz->damaged_area))
                return -1;
        }
    }

    int rv = 0;

    if (IS_BVCMD(params)) {
        int j;
        params->data.bvc.out_nhndls = 0;
        rgz_fb_state_t *cur_fb_state = &rgz->cur_fb_state;
        /* Begin from index 1 to remove the background layer from the output */
        for (j = 1, i = 0; j < cur_fb_state->rgz_layerno; j++) {
            rgz_layer_t *rgz_layer = &cur_fb_state->rgz_layers[j];
            /* We don't need the handles for layers marked as -1 */
            if (rgz_layer->buffidx == -1)
                continue;
            params->data.bvc.out_hndls[i++] = rgz_layer->hwc_layer.handle;
            params->data.bvc.out_nhndls++;
        }

        if (blts.idx > 0) {
            /* Last blit is made sync to act like a fence for the previous async blits */
            struct rgz_blt_entry* e = &blts.bvcmds[blts.idx-1];
            rgz_set_async(e, 0);
        }

        /* FIXME: we want to be able to call rgz_blts_free and populate the actual
         * composition data structure ourselves */
        params->data.bvc.cmdp = blts.bvcmds;
        params->data.bvc.cmdlen = blts.idx;
        if (params->data.bvc.out_blits >= RGZ_MAX_BLITS)
            rv = -1;
        //rgz_blts_free(&blts);
    } else {
        rv = rgz_blts_bvdirect(rgz, &blts, params);
        rgz_blts_free(&blts);
    }

    return rv;
}

void rgz_profile_hwc(hwc_display_contents_1_t* list, int dispw, int disph)
{
    if (!list)  /* A NULL composition list can occur */
        return;

    static char regiondump2[PROPERTY_VALUE_MAX] = "";
    char regiondump[PROPERTY_VALUE_MAX];
    property_get("debug.2dhwc.region", regiondump, "0");
    int dumpregions = strncmp(regiondump, regiondump2, PROPERTY_VALUE_MAX);
    if (dumpregions)
        strncpy(regiondump2, regiondump, PROPERTY_VALUE_MAX);
    else {
        dumpregions = !strncmp(regiondump, "all", PROPERTY_VALUE_MAX) &&
                      (list->flags & HWC_GEOMETRY_CHANGED);
        static int iteration = 0;
        if (dumpregions)
            sprintf(regiondump, "iteration %d", iteration++);
    }

    char dumplayerdata[PROPERTY_VALUE_MAX];
    /* 0 - off, 1 - human readable, 2 - CSV */
    property_get("debug.2dhwc.dumplayers", dumplayerdata, "0");
    int dumplayers = atoi(dumplayerdata);
    if (dumplayers && (list->flags & HWC_GEOMETRY_CHANGED)) {
        OUTP("<!-- BEGUN-LAYER-DUMP: %d -->", list->numHwLayers);
        rgz_print_layers(list, dumplayers == 1 ? 0 : 1);
        OUTP("<!-- ENDED-LAYER-DUMP -->");
    }

    if(!dumpregions)
        return;

    rgz_t rgz;
    rgz_in_params_t ip = { .data = { .hwc = {
                           .layers = list->hwLayers,
                           .layerno = list->numHwLayers } } };
    ip.op = RGZ_IN_HWCCHK;
    if (rgz_in(&ip, &rgz) == RGZ_ALL) {
        ip.op = RGZ_IN_HWC;
        if (rgz_in(&ip, &rgz) == RGZ_ALL) {
            OUTP("<!-- BEGUN-SVG-DUMP: %s -->", regiondump);
            OUTP("<b>%s</b>", regiondump);
            rgz_out_params_t op = {
                .op = RGZ_OUT_SVG,
                .data = {
                    .svg = {
                        .dispw = dispw, .disph = disph,
                        .htmlw = 450, .htmlh = 800
                    }
                },
            };
            rgz_out(&rgz, &op);
            OUTP("<!-- ENDED-SVG-DUMP -->");
        }
    }
    rgz_release(&rgz);
}

int rgz_get_screengeometry(int fd, struct bvsurfgeom *geom, int fmt)
{
    /* Populate Bltsville destination buffer information with framebuffer data */
    struct fb_fix_screeninfo fb_fixinfo;
    struct fb_var_screeninfo fb_varinfo;

    ALOGI("Attempting to get framebuffer device info.");
    if(ioctl(fd, FBIOGET_FSCREENINFO, &fb_fixinfo)) {
        OUTE("Error getting fb_fixinfo");
        return -EINVAL;
    }

    if(ioctl(fd, FBIOGET_VSCREENINFO, &fb_varinfo)) {
        ALOGE("Error gettting fb_varinfo");
        return -EINVAL;
    }

    bzero(&bg_layer, sizeof(bg_layer));
    bg_layer.displayFrame.left = bg_layer.displayFrame.top = 0;
    bg_layer.displayFrame.right = fb_varinfo.xres;
    bg_layer.displayFrame.bottom = fb_varinfo.yres;

    bzero(geom, sizeof(*geom));
    geom->structsize = sizeof(*geom);
    geom->width = fb_varinfo.xres;
    geom->height = fb_varinfo.yres;
    geom->virtstride = fb_fixinfo.line_length;
    geom->format = hal_to_ocd(fmt);
    geom->orientation = 0;
    return 0;
}

int rgz_in(rgz_in_params_t *p, rgz_t *rgz)
{
    int rv = -1;
    switch (p->op) {
    case RGZ_IN_HWC:
        rv = rgz_in_hwccheck(p, rgz);
        if (rv == RGZ_ALL)
            rv = rgz_in_hwc(p, rgz) ? 0 : RGZ_ALL;
        break;
    case RGZ_IN_HWCCHK:
        bzero(rgz, sizeof(rgz_t));
        rv = rgz_in_hwccheck(p, rgz);
        break;
    default:
        return -1;
    }
    return rv;
}

void rgz_release(rgz_t *rgz)
{
    if (!rgz)
        return;
    if (rgz->hregions)
        free(rgz->hregions);
    bzero(rgz, sizeof(*rgz));
}

int rgz_out(rgz_t *rgz, rgz_out_params_t *params)
{
    switch (params->op) {
    case RGZ_OUT_SVG:
        rgz_out_svg(rgz, params);
        return 0;
    case RGZ_OUT_BVDIRECT_PAINT:
        return rgz_out_bvdirect_paint(rgz, params);
    case RGZ_OUT_BVCMD_PAINT:
        return rgz_out_bvcmd_paint(rgz, params);
    case RGZ_OUT_BVDIRECT_REGION:
    case RGZ_OUT_BVCMD_REGION:
        return rgz_out_region(rgz, params);
    default:
        return -1;
    }
}

