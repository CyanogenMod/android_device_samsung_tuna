/*
 * Copyright (C) 2011 The Android Open Source Project
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "bootloader.h"

#define PARTITION_TABLE_SIZE 4096 // 4KB

#define BOOT_PART_LEN 0x20000 // 128KB

#define SMALL_BUFFER_SIZE 0x20

static const char* FAMILY_LOCATION = "/sys/board_properties/soc/family";
static const char* TYPE_LOCATION = "/sys/board_properties/soc/type";

unsigned int read_whole_file(const char* fname, char* buffer,
                             int buffer_size) {
  memset(buffer, 0, buffer_size);

  FILE* f = fopen(fname, "rb");
  if (f == NULL) {
    fprintf(stderr, "Cannot open %s!\n", fname);
    return 0;
  }

  int read_byte_count = fread(buffer, 1, buffer_size, f);
  fclose(f);
  if (read_byte_count >= buffer_size) {
    fprintf(stderr, "The data in %s is too large for the buffer", fname);
    return 0;
  }

  return 1;
}

// Four different device/chip type xloaders are supported by a bootloader.img:
// 4460 EMU, 4460 HS, 4430 EMU, 4430 HS.
// The layout of the bootloader.img is:
//
// Partition table (4KB)
// 4460 EMU xloader (128KB)
// 4460 HS xloader (128KB)
// 4430 EMU xloader (128KB)
// 4430 HS xloader(128KB)
// sbl (the rest)
unsigned int get_xloader_offset() {
  unsigned int offset = 0;
  char* file_data = malloc(SMALL_BUFFER_SIZE);
  if (file_data == NULL) {
    return -1;
  }

  if (!read_whole_file(FAMILY_LOCATION, file_data, SMALL_BUFFER_SIZE)) {
    fprintf(stderr, "Cannot read the family\n");
    free(file_data);
    return -1;
  }

  if (strncmp(file_data, "OMAP4430", 8) == 0) {
    offset += (BOOT_PART_LEN * 2);
  } else if (strncmp(file_data, "OMAP4460", 8) != 0) {
    fprintf(stderr, "Unknown family: %s\n", file_data);
    free(file_data);
    return -1;
  }

  if (!read_whole_file(TYPE_LOCATION, file_data, SMALL_BUFFER_SIZE)) {
    fprintf(stderr, "Cannot read the type\n");
    free(file_data);
    return -1;
  }

  if (strncmp(file_data, "HS", 2) == 0) {
    offset += BOOT_PART_LEN;
  } else if (strncmp(file_data, "EMU", 3) != 0) {
    fprintf(stderr, "Unknown type: %s\n", file_data);
    free(file_data);
    return -1;
  }

  return offset;
}

int update_bootloader(const char* image_data,
                      size_t image_size,
                      const char* xloader_loc,
                      const char* sbl_loc) {
  unsigned int xloader_offset=0;
  unsigned int sbl_offset=0;

  int type_family_offset = get_xloader_offset();
  if (type_family_offset < 0) {
    return -1;
  }

  // The offsets into the relevant parts of the bootloader image
  xloader_offset = PARTITION_TABLE_SIZE + type_family_offset;
  sbl_offset = PARTITION_TABLE_SIZE + (BOOT_PART_LEN * 4);

  if (image_size < sbl_offset) {
    fprintf(stderr, "image size %d is too small\n", image_size);
    return -1;
  }

  int written = 0;
  int close_status = 0;

  FILE* xloader = fopen(xloader_loc, "r+b");
  if (xloader == NULL) {
    fprintf(stderr, "Could not open %s\n", xloader_loc);
    return -1;
  }

  // index into the correct xloader offset
  written = fwrite(image_data+xloader_offset, 1, BOOT_PART_LEN, xloader);
  close_status = fclose(xloader);
  if (written != BOOT_PART_LEN || close_status != 0) {
    fprintf(stderr, "Failed writing to /xloader\n");
    return -1;
  }

  unsigned int sbl_size = image_size - sbl_offset;
  FILE* sbl = fopen(sbl_loc, "r+b");
  if (sbl == NULL) {
    fprintf(stderr, "Could not open %s\n", sbl_loc);
    return -1;
  }

  written = fwrite(image_data+sbl_offset, 1, sbl_size, sbl);
  close_status = fclose(sbl);
  if (written != sbl_size || close_status != 0) {
    fprintf(stderr, "Failed writing to /sbl\n");
    return -1;
  }

  return 0;
}
