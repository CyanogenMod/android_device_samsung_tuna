/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ANDROID_SAMSUNG_SENSORS_H
#define ANDROID_SAMSUNG_SENSORS_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

__BEGIN_DECLS

/*****************************************************************************/

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/*
 * The MPL-supported sensors need to have same IDs as in Invensense .so
 * (from ID_MPL_BASE up to ID_O)
 */
#define ID_MPL_BASE (0)
#define ID_RV (ID_MPL_BASE)
#define ID_LA (ID_RV + 1)
#define ID_GR (ID_LA + 1)
#define ID_GY (ID_GR + 1)
#define ID_A  (ID_GY + 1)
#define ID_M  (ID_A + 1)
#define ID_O  (ID_M + 1)

/*****************************************************************************/


__END_DECLS

#endif  /* ANDROID_SAMSUNG_SENSORS_H */
