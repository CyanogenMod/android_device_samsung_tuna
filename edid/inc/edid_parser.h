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

#ifndef _EDID_PARSER_
#define _EDID_PARSER_

#define EDID_SIZE 256
#define MAX_VIC_CODES_PER_3D_FORMAT 16

struct edid_t;

enum datablock_id {
    DATABLOCK_AUDIO     = 1,
    DATABLOCK_VIDEO     = 2,
    DATABLOCK_VENDOR    = 3,
    DATABLOCK_SPEAKERS  = 4,
};

enum hdmi_3d_format {
    HDMI_FRAME_PACKING          = 0,
    HDMI_FIELD_ALTERNATIVE      = 1,
    HDMI_LINE_ALTERNATIVE       = 2,
    HDMI_SIDE_BY_SIDE_FULL      = 3,
    HDMI_L_DEPTH                = 4,
    HDMI_L_DEPTH_GFX_GFX_DEPTH  = 5,
    HDMI_TOPBOTTOM              = 6,
    HDMI_SIDE_BY_SIDE_HALF      = 8,
};

enum hdmi_3d_format_bits {
    HDMI_FRAME_PACKING_BIT = 1 << HDMI_FRAME_PACKING,
    HDMI_FIELD_ALTERNATIVE_BIT = 1 << HDMI_FIELD_ALTERNATIVE,
    HDMI_LINE_ALTERNATIVE_BIT  = 1 << HDMI_LINE_ALTERNATIVE,
    HDMI_SIDE_BY_SIDE_FULL_BIT = 1 << HDMI_SIDE_BY_SIDE_FULL,
    HDMI_L_DEPTH_BIT = 1 << HDMI_L_DEPTH ,
    HDMI_L_DEPTH_GFX_GFX_DEPTH_BIT = 1 << HDMI_L_DEPTH_GFX_GFX_DEPTH,
    HDMI_TOPBOTTOM_BIT = 1 << HDMI_TOPBOTTOM,
    HDMI_SIDE_BY_SIDE_HALF_BIT = 1 << HDMI_SIDE_BY_SIDE_HALF,
    HDMI_SIDE_BY_SIDE_HALF_QUINCUNX_BIT = 1 << 15,
};

//ALL = both horizontal and quincunx modes are supported
//HDMI_SS_QUINCUNX_ALL = all quincunx subsampling modes are supported
//OL = Odd left viewHorizontal sub
//OR = Odd right view
//ER = Even left view
//EL = Even right view
enum hdmi_3d_subsampling {
    HDMI_SS_HORZANDQUINCUNX = 0,
    HDMI_SS_HORIZONTAL      = 1,
    HDMI_SS_QUINCUNX_ALL    = 6,
    HDMI_SS_QUINCUNX_OLOR   = 7,
    HDMI_SS_QUINCUNX_OLER   = 8,
    HDMI_SS_QUINCUNX_ELOR   = 9,
    HDMI_SS_QUINCUNX_ELER   = 10,
    HDMI_SS_VERTICAL        = 0xF0000,
    HDMI_SS_NONE = 0xF0001,
};

enum hdmi_scan_type {
    HDMI_SCAN_PROGRESSIVE,
    HDMI_SCAN_INTERLACED,
};

struct svd_info_t {
    uint32_t xres;
    uint32_t yres;
    uint32_t hz;
    enum hdmi_scan_type scan_type;
    char name[9];
};

struct svd_t {
    uint8_t code;
    bool native;
    struct svd_info_t info;
};

struct hdmi_s3d_format_vic_info_t {
    uint8_t vic_pos;
    enum hdmi_3d_subsampling subsampling;
};

struct hdmi_s3d_format_info_t {
    enum hdmi_3d_format format;
    unsigned int num_valid_vic;
    struct hdmi_s3d_format_vic_info_t vic_info[MAX_VIC_CODES_PER_3D_FORMAT];
};

int edid_parser_init(struct edid_t **edid, const uint8_t *raw_edid_data);
void edid_parser_deinit(struct edid_t *edid);

bool edid_s3d_capable(struct edid_t *edid);
bool edid_supports_s3d_format(struct edid_t *edid, enum hdmi_3d_format format);
const struct hdmi_s3d_format_info_t * edid_get_s3d_format_info(struct edid_t *edid, enum hdmi_3d_format format);

void edid_get_svd_list(struct edid_t *edid, struct svd_t **list, unsigned int *num_elements);
const struct svd_t *edid_get_svd_descriptor(struct edid_t *edid, uint8_t vic_pos);

#endif //_EDID_PARSER_