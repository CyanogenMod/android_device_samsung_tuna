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

#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>

#define LOG_TAG "EDID"
#include <utils/Log.h>

#include <inc/edid_parser.h>

static const char kHdmiEdidPathName[] = "/sys/devices/platform/omapdss/display1/edid";

static void print_s3d_format_info(struct edid_t *edid, const struct hdmi_s3d_format_info_t *info)
{
    unsigned int i;
    if(info == NULL) {
        return;
    }

    switch(info->format) {
        case HDMI_FRAME_PACKING:
            fprintf(stdout, "--Frame Packing");
            break;
        case HDMI_FIELD_ALTERNATIVE:
            fprintf(stdout, "--Filed Alternative");
            break;
        case HDMI_LINE_ALTERNATIVE:
            fprintf(stdout, "--Line Alternative");
            break;
        case HDMI_SIDE_BY_SIDE_FULL:
            fprintf(stdout, "--Side by Side FULL");
            break;
        case HDMI_L_DEPTH:
            fprintf(stdout, "--L + Depth");
            break;
        case HDMI_L_DEPTH_GFX_GFX_DEPTH:
            fprintf(stdout, "--L + Depth + Graphics + Graphics + Depth");
            break;
        case HDMI_TOPBOTTOM:
            fprintf(stdout, "--Top Bottom");
            break;
        case HDMI_SIDE_BY_SIDE_HALF:
            fprintf(stdout, "--Side by Side HALF");
            break;
        default:
            fprintf(stdout, "--Unkown format");
            break;
    }
    fprintf(stdout, "\n");

    for (i = 0; i < info->num_valid_vic; i++) {
        const struct svd_t * svd = edid_get_svd_descriptor(edid, info->vic_info[i].vic_pos);
        fprintf(stdout, "----Mode:%s sub-sampling: ", svd->info.name);
        switch(info->vic_info[i].subsampling) {
            case HDMI_SS_HORZANDQUINCUNX:
                fprintf(stdout, "Horizontal and Quincunx");
                break;
            case HDMI_SS_HORIZONTAL:
                fprintf(stdout, "Horizontal");
                break;
            case HDMI_SS_QUINCUNX_ALL:
                fprintf(stdout, "Quincunx");
                break;
            case HDMI_SS_QUINCUNX_OLOR:
                fprintf(stdout, "Quincunx Odd-Left/Odd-Right");
                break;
            case HDMI_SS_QUINCUNX_OLER:
                fprintf(stdout, "Quincunx Odd-Left/Even-Right");
                break;
            case HDMI_SS_QUINCUNX_ELOR:
                fprintf(stdout, "Quincunx Even-Left/Odd-Right");
                break;
            case HDMI_SS_QUINCUNX_ELER:
                fprintf(stdout, "Quincunx Even-Left/Even-Right");
                break;
            case HDMI_SS_VERTICAL:
                fprintf(stdout, "Vertical");
                break;
            case HDMI_SS_NONE:
                fprintf(stdout, "None");
                break;
            default:
                break;
        }
        fprintf(stdout, "\n");
    }

}
int main()
{
    unsigned int i;
    struct svd_t *svd_list;
    unsigned int num_svds;

    int fd = open(kHdmiEdidPathName, O_RDONLY);

    if (!fd) {
        return 1;
    }

    uint8_t edid_data[EDID_SIZE];
    size_t bytes_read = read(fd, edid_data, EDID_SIZE);
    close(fd);

    if (bytes_read < EDID_SIZE) {
        fprintf(stderr, "Could not read EDID data\n");
        return 1;
    }

    struct edid_t *edid = NULL;
    if(edid_parser_init(&edid, edid_data)) {
        fprintf(stderr, "Could not init parser\n");
        return 1;
    }

    edid_get_svd_list(edid, &svd_list, &num_svds);

    fprintf(stdout, "EDID Info\n");
    fprintf(stdout, "[Short Video Descriptors]\n");
    for (i = 0; i < num_svds; i++) {
        fprintf(stdout, "----%d: %s [code:%d, native:%d] [xres:%d, yres:%d Hz:%d]\n",
                i, svd_list[i].info.name, svd_list[i].code, svd_list[i].native,
                svd_list[i].info.xres, svd_list[i].info.yres, svd_list[i].info.hz);
    }

    fprintf(stdout, "\n[S3D Optional Formats]\n");
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_FRAME_PACKING));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_FIELD_ALTERNATIVE));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_LINE_ALTERNATIVE));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_SIDE_BY_SIDE_FULL));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_L_DEPTH));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_L_DEPTH_GFX_GFX_DEPTH));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_TOPBOTTOM));
    print_s3d_format_info(edid, edid_get_s3d_format_info(edid, HDMI_SIDE_BY_SIDE_HALF));

    edid_parser_deinit(edid);

    return 0;
}
