# Copyright (C) 2009 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Emit commands needed for Prime during OTA installation
(installing the bootloader and radio images)."""

import common

def FullOTA_InstallEnd(info):
  try:
    bootloader_img = info.input_zip.read("RADIO/bootloader.img")
  except KeyError:
    print "no bootloader.img in target_files; skipping install"
  else:
    WriteBootloader(info, bootloader_img)

  try:
    radio_img = info.input_zip.read("RADIO/radio.img")
  except KeyError:
    print "no radio.img in target_files; skipping install"
  else:
    WriteRadio(info, radio_img)

def IncrementalOTA_InstallEnd(info):
  try:
    target_bootloader_img = info.target_zip.read("RADIO/bootloader.img")
    try:
      source_bootloader_img = info.source_zip.read("RADIO/bootloader.img")
    except KeyError:
      source_bootloader_img = None

    if source_bootloader_img == target_bootloader_img:
      print "bootloader unchanged; skipping"
    else:
      WriteBootloader(info, target_bootloader_img)
  except KeyError:
    print "no bootloader.img in target target_files; skipping install"

  try:
    target_radio_img = info.target_zip.read("RADIO/radio.img")
    try:
      source_radio_img = info.source_zip.read("RADIO/radio.img")
    except KeyError:
      source_radio_img = None

    if source_radio_img == target_radio_img:
      print "radio unchanged; skipping"
    else:
      WriteRadio(info, target_radio_img)
  except KeyError:
    print "no radio.img in target target_files; skipping install"

def WriteBootloader(info, bootloader_img):
  common.ZipWriteStr(info.output_zip, "bootloader.img", bootloader_img)
  fstab = info.info_dict["fstab"]

  info.script.Print("Writing bootloader...")
  info.script.AppendExtra('''samsung.write_bootloader(
    package_extract_file("bootloader.img"), "%s", "%s");''' % \
    (fstab["/xloader"].device, fstab["/sbl"].device))

def WriteRadio(info, radio_img):
  common.ZipWriteStr(info.output_zip, "radio.img", radio_img)
  info.script.Print("Writing radio...")
  info.script.WriteRawImage("/radio", "radio.img")
