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
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <inc/edid_parser.h>
#include "edid_parser_priv.h"

const struct svd_info_t svd_table[] =
{
    {0, 0, 0, HDMI_SCAN_PROGRESSIVE, "reserved"},
    {640, 480, 60, HDMI_SCAN_PROGRESSIVE, "DMT0659"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480p"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480pH"},
    {1280, 720, 60, HDMI_SCAN_PROGRESSIVE, "720p"},
    {1920, 1080, 60, HDMI_SCAN_INTERLACED, "1080i"},
    {720, 480, 60, HDMI_SCAN_INTERLACED, "480i"},
    {720, 480, 60, HDMI_SCAN_INTERLACED, "480iH"},
    {720, 240, 60, HDMI_SCAN_PROGRESSIVE, "240p"},
    {1280, 720, 60, HDMI_SCAN_PROGRESSIVE, "240pH"},
    {720, 480, 60, HDMI_SCAN_INTERLACED, "480i4x"},
    {720, 480, 60, HDMI_SCAN_INTERLACED, "480i4xH"},
    {720, 240, 60, HDMI_SCAN_PROGRESSIVE, "240p4x"},
    {720, 240, 60, HDMI_SCAN_PROGRESSIVE, "240p4xH"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480p2x"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480p2xH"},
    {1920, 1080, 60, HDMI_SCAN_PROGRESSIVE, "1080p"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576p"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576pH"},
    {1280, 720, 50, HDMI_SCAN_PROGRESSIVE, "720p50"},
    {1920, 1080, 50, HDMI_SCAN_INTERLACED, "1080i25"},
    {720, 576, 50, HDMI_SCAN_INTERLACED, "576i"},
    {720, 576, 50, HDMI_SCAN_INTERLACED, "576iH"},
    {720, 288, 50, HDMI_SCAN_PROGRESSIVE, "288p"},
    {720, 288, 50, HDMI_SCAN_PROGRESSIVE, "288pH"},
    {720, 576, 50, HDMI_SCAN_INTERLACED, "576i4x"},
    {720, 576, 50, HDMI_SCAN_INTERLACED, "576i4xH"},
    {720, 288, 50, HDMI_SCAN_PROGRESSIVE, "288p4x"},
    {720, 288, 50, HDMI_SCAN_PROGRESSIVE, "288p4xH"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576p2x"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576p2xH"},
    {1920, 1080, 50, HDMI_SCAN_PROGRESSIVE, "1080p50"},
    {1920, 1080, 24, HDMI_SCAN_PROGRESSIVE, "1080p24"},
    {1920, 1080, 25, HDMI_SCAN_PROGRESSIVE, "1080p25"},
    {1920, 1080, 30, HDMI_SCAN_PROGRESSIVE, "1080p30"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480p4x"},
    {720, 480, 60, HDMI_SCAN_PROGRESSIVE, "480p4xH"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576p4x"},
    {720, 576, 50, HDMI_SCAN_PROGRESSIVE, "576p4xH"},
    {1920, 1080, 50, HDMI_SCAN_INTERLACED, "1080i25"},
    {1920, 1080, 100, HDMI_SCAN_INTERLACED, "1080i50"},
    {1280, 720, 100, HDMI_SCAN_PROGRESSIVE, "720p100"},
    {720, 576, 100, HDMI_SCAN_PROGRESSIVE, "576p100"},
    {720, 576, 100, HDMI_SCAN_PROGRESSIVE, "576p100H"},
    {720, 576, 100, HDMI_SCAN_INTERLACED, "576i50"},
    {720, 576, 100, HDMI_SCAN_INTERLACED, "576i50H"},
    {1920, 1080, 120, HDMI_SCAN_INTERLACED, "1080i60"},
    {1280, 720, 120, HDMI_SCAN_PROGRESSIVE, "720p120"},
    {720, 480, 120, HDMI_SCAN_PROGRESSIVE, "480p120"},
    {720, 480, 120, HDMI_SCAN_PROGRESSIVE, "480p120H"},
    {720, 480, 120, HDMI_SCAN_INTERLACED, "480i60"},
    {720, 480, 120, HDMI_SCAN_INTERLACED, "480i60H"},
    {720, 576, 200, HDMI_SCAN_PROGRESSIVE, "576p200"},
    {720, 576, 200, HDMI_SCAN_PROGRESSIVE, "576p200H"},
    {720, 576, 200, HDMI_SCAN_INTERLACED, "576i100"},
    {720, 576, 200, HDMI_SCAN_INTERLACED, "576i100H"},
    {720, 480, 240, HDMI_SCAN_PROGRESSIVE, "480p240"},
    {720, 480, 240, HDMI_SCAN_PROGRESSIVE, "480p240H"},
    {720, 480, 240, HDMI_SCAN_INTERLACED, "480i120"},
    {720, 480, 240, HDMI_SCAN_INTERLACED, "480i120H"},
    {1280, 720, 24, HDMI_SCAN_PROGRESSIVE, "720p24"},
    {1280, 720, 25, HDMI_SCAN_PROGRESSIVE, "720p25"},
    {1280, 720, 30, HDMI_SCAN_PROGRESSIVE, "720p30"},
    {1920, 1080, 120, HDMI_SCAN_PROGRESSIVE, "1080p120"}
};

const int NUM_SVD_ENTRIES = sizeof(svd_table)/sizeof(svd_table[0]);

static unsigned int count_set_bits(uint16_t value)
{
    unsigned int count;
    for (count = 0; value; count++)
        value &= value - 1;
    return count;
}

static void set_s3d_format_bits(const uint8_t *edid_data, int off, int hdmi_3d_len, uint16_t *s3d_struct_bits)
{
    //These are 3D formats signaled through 2D VIC order and 3D_structure-3D_Detail
    while (hdmi_3d_len > 0) {
        unsigned int val = edid_data[off++] & 0x0F;
        *s3d_struct_bits |= 1 << val;
        //3D Detail_x is included for 3D_Structure_x range 0x8-0xF
        if ( val >= HDMI_SIDE_BY_SIDE_HALF) {
            hdmi_3d_len--;
            off++;
        }
        hdmi_3d_len--;
    }
}

static void update_s3d_format(struct edid_t *edid, enum hdmi_3d_format format,
                                uint8_t vic_pos, enum hdmi_3d_subsampling subsamp)
{
    unsigned int format_ix, vic_pos_ix;
    if (vic_pos > edid->num_svds) {
        return;
    }

    for (format_ix = 0; format_ix < edid->num_s3d_formats; format_ix++) {
        if (edid->s3d_format_list[format_ix].format == format) {
            break;
        }
    }

    if (format_ix >= edid->num_s3d_formats) {
        return;
    }

    //In case this has already been signaled, we'll update the subsampling mode
    for (vic_pos_ix = 0; vic_pos_ix < edid->s3d_format_list[format_ix].num_valid_vic; vic_pos_ix++) {
        if (edid->s3d_format_list[format_ix].vic_info[vic_pos_ix].vic_pos == vic_pos) {
            break;
        }
    }

    if (vic_pos_ix >= edid->s3d_format_list[format_ix].num_valid_vic) {
        vic_pos_ix = edid->s3d_format_list[format_ix].num_valid_vic;
        edid->s3d_format_list[format_ix].num_valid_vic += 1;
    }

    edid->s3d_format_list[format_ix].vic_info[vic_pos_ix].vic_pos = vic_pos;
    edid->s3d_format_list[format_ix].vic_info[vic_pos_ix].subsampling = subsamp;
}

/* This function was originally written by Mythri pk */
static int edid_get_datablock_offset(const uint8_t *edid_data, enum datablock_id type, unsigned int *off)
{
    uint8_t val;
    uint8_t ext_length;
    uint8_t offset;

    //CEA extension signaled? If not, then no datablocks are contained
    if (edid_data[0x7e] == 0x00) {
        return 1;
    }

    //18-byte descriptors only? Otherwise, there are datablocks present
    ext_length = edid_data[0x82];
    if (ext_length == 0x4) {
        return 1;
    }

    //Start of first extended data block
    offset = 0x84;
    while (offset < (0x80 + ext_length)) {
        val = edid_data[offset];
        //Upper 3 bits indicate block type
        if ((val >> 5) == type) {
            *off = offset;
            return 0;
        } else {
            //lower 5 bits indicate block length
            offset += (val & 0x1F) + 1;
        }
    }
    return 1;
}

static void edid_parse_s3d_support(struct edid_t *edid, const uint8_t *edid_data)
{
    unsigned int off;
    unsigned int i, count;
    uint8_t val;
    uint8_t s3d_multi_present;
    uint8_t hdmi_3d_len;
    uint16_t s3d_struct_all = 0;
    uint16_t s3d_struct_bits = 0;
    uint16_t hdmi_vic_pos_bits = 0;

    //memset(edid->s3d_formats, 0, sizeof(edid->s3d_formats));

    //S3D HDMI information is signaled in the Vendor Specific datablock
    if(edid_get_datablock_offset(edid_data, DATABLOCK_VENDOR, &off))
        return;

    //Skip header and other non-S3D related fields
    off += 8;
    val = edid_data[off++];

    //HDMI_Video_present?
    if (!(val & 0x20)) {
        return;
    }

    //Latency_Fields_Present? Skip
    if (val & 0x80) {
        off += 2;
    }

    //I_Latency_Fields_Present? Skip
    if (val & 0x40) {
        off += 2;
    }

    val = edid_data[off++];
    //3D_Present?
    if (!(val & 0x80)) {
        return;
    }

    edid->s3d_capable = true;

    s3d_multi_present = (val & 0x60) >> 5;

    //Skip HDMI_XX_LEN
    val = edid_data[off++];
    off += (val & 0xE0) >> 5;
    hdmi_3d_len = (val & 0x1F);

    //3D capabilities signaled through bitmasks
    //s3d_struct_all has all the 3D formats supported (per bit)
    //hdmi_vic_mask has which of the corresponding VIC codes the 3D formats apply to
    //if s3d_multi_present = 1, then the 3D formats apply to all the first 16 VIC codes
    if (s3d_multi_present == 1 || s3d_multi_present == 2) {
        s3d_struct_all = (edid_data[off] << 8) | edid_data[off+1];
        hdmi_vic_pos_bits = 0xFFFF;
        hdmi_3d_len -= 2;
        off += 2;
    }

    if (s3d_multi_present == 2) {
        hdmi_vic_pos_bits = (edid_data[off] << 8) | edid_data[off+1];
        hdmi_3d_len -= 2;
        off += 2;
    }

    //Bit 15 signals same format as Bit 8 - HDMI_SIDE_BY_SIDE_HALF, they only differ in subsampling options
    s3d_struct_bits = s3d_struct_all & 0x7FFF;
    set_s3d_format_bits(edid_data, off, hdmi_3d_len, &s3d_struct_bits);

    edid->num_s3d_formats = count_set_bits(s3d_struct_bits);
    edid->s3d_format_list = (struct hdmi_s3d_format_info_t *) malloc(edid->num_s3d_formats * sizeof(struct hdmi_s3d_format_info_t));

    count = 0;
    for (i = 0; i <= HDMI_SIDE_BY_SIDE_HALF; i++) {
        if (s3d_struct_bits & (1 << i)) {
            edid->s3d_format_list[count++].format = (enum hdmi_3d_format)i;
        }
    }

    for (i = 0; i < edid->num_s3d_formats; i++) {
        unsigned int j;
        enum hdmi_3d_subsampling subsampling;
        if (edid->s3d_format_list[i].format == HDMI_SIDE_BY_SIDE_HALF) {
            uint16_t bitmask = HDMI_SIDE_BY_SIDE_HALF_QUINCUNX_BIT | HDMI_SIDE_BY_SIDE_HALF_BIT;
            if ( (s3d_struct_all & bitmask) == bitmask) {
                subsampling = HDMI_SS_HORZANDQUINCUNX;
            } else if ((s3d_struct_all & bitmask) == HDMI_SIDE_BY_SIDE_HALF_QUINCUNX_BIT) {
                subsampling = HDMI_SS_QUINCUNX_ALL;
            } else if ((s3d_struct_all & bitmask) == HDMI_SIDE_BY_SIDE_HALF_BIT) {
                subsampling = HDMI_SS_HORIZONTAL;
            }
        } else if (edid->s3d_format_list[i].format == HDMI_TOPBOTTOM) {
            subsampling = HDMI_SS_VERTICAL;
        } else {
            subsampling = HDMI_SS_NONE;
        }
        count = 0;
        for (j = 0; j < 16; j++) {
             if ((s3d_struct_all & (1 << edid->s3d_format_list[i].format)) &&
                (hdmi_vic_pos_bits & (1 << j))) {
                edid->s3d_format_list[i].vic_info[count].subsampling = subsampling;
                edid->s3d_format_list[i].vic_info[count++].vic_pos = j;
             }
        }
        edid->s3d_format_list[i].num_valid_vic = count;
    }

    //In this case, the 3D formats signaled only apply to the VIC codes signaled per bit
    //i.e. bit0 = VIC code 0 from the Short video descriptors list
    while (hdmi_3d_len > 0) {
        //Upper 4 bits indicate vic position, lower 4 bits are the 3D structure value
        enum hdmi_3d_subsampling subsampling = HDMI_SS_NONE;
        uint8_t vic_pos = (edid_data[off] & 0xF0) >> 4;
        enum hdmi_3d_format format = (enum hdmi_3d_format) (edid_data[off++] & 0x0F);
        if (format >= HDMI_SIDE_BY_SIDE_HALF) {
            subsampling = (enum hdmi_3d_subsampling)((edid_data[off++] & 0xF0) >> 4);
            hdmi_3d_len--;
        }
        if (format == HDMI_TOPBOTTOM) {
            subsampling = HDMI_SS_VERTICAL;
        }
        update_s3d_format(edid, format, vic_pos, subsampling);
        hdmi_3d_len--;
    }
}

static void edid_fill_svd_info(uint8_t code, struct svd_info_t *info)
{
    if(code > NUM_SVD_ENTRIES)
        code = 0;
    memcpy(info, &svd_table[code], sizeof(struct svd_info_t));
}

static void edid_parse_svds(struct edid_t *edid, const uint8_t *raw_edid_data)
{
    unsigned int offset;
    unsigned int i;
    if (edid_get_datablock_offset(raw_edid_data, DATABLOCK_VIDEO, &offset)) {
        edid->num_svds = 0;
        edid->svd_list = NULL;
        return ;
    }

    edid->num_svds = raw_edid_data[offset] & 0x1F;
    edid->svd_list = (struct svd_t *) malloc(edid->num_svds * sizeof(struct svd_t));
    for (i = 0; i < edid->num_svds; i++) {
        struct svd_t *svd = &edid->svd_list[i];
        svd->code = raw_edid_data[offset + i] & 0x7F;
        svd->native = (raw_edid_data[offset + i] & 0x80) == 0x80;
        edid_fill_svd_info(svd->code, &svd->info);
    }
}

/*=======================================================*/
int edid_parser_init(struct edid_t **edid_handle, const uint8_t *raw_edid_data)
{
    if(edid_handle == NULL) {
        return -1;
    }

    struct edid_t *edid = (struct edid_t *) malloc(sizeof(struct edid_t));
    if (edid == NULL) {
        return -1;
    }

    memset(edid, 0, sizeof(struct edid_t));
    edid_parse_svds(edid, raw_edid_data);
    edid_parse_s3d_support(edid, raw_edid_data);

    *edid_handle = edid;
    return 0;
}

void edid_parser_deinit(struct edid_t *edid)
{
    free(edid->s3d_format_list);
    free(edid->svd_list);
    free(edid);
}

bool edid_s3d_capable(struct edid_t *edid)
{
    return edid->s3d_capable;
}

bool edid_supports_s3d_format(struct edid_t *edid, enum hdmi_3d_format format)
{
    unsigned int i;
    for (i = 0; i < edid->num_s3d_formats; i++) {
        if (edid->s3d_format_list[i].format == format) {
            return true;
        }
    }
    return false;
}

const struct hdmi_s3d_format_info_t * edid_get_s3d_format_info(struct edid_t *edid, enum hdmi_3d_format format)
{
    unsigned int i;
    for (i = 0; i < edid->num_s3d_formats; i++) {
        if (edid->s3d_format_list[i].format == format) {
            return &edid->s3d_format_list[i];
        }
    }
    return NULL;
}

void edid_get_svd_list(struct edid_t *edid, struct svd_t **list, unsigned int *num_elements)
{
    if(list == NULL || num_elements == NULL)
        return;

    *list = edid->svd_list;
    *num_elements = edid->num_svds;
}

const struct svd_t *edid_get_svd_descriptor(struct edid_t *edid, uint8_t vic_pos)
{
    if(vic_pos > edid->num_svds)
        return NULL;
    return &edid->svd_list[vic_pos];
}
