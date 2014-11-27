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

#ifndef _TI_HANDLE_WRAPPER_H_
#define _TI_HANDLE_WRAPPER_H_

/*
 * Provide wrapper api for getting fields from gralloc
 * handles
 */
#define OMAP_LEGACY_HANDLE /* For now enable legacy handle by default */
#ifdef OMAP_LEGACY_HANDLE

#include "hal_public.h"
typedef IMG_native_handle_t ti_hndl_t;

#define HND_W(h) h->iWidth
#define HND_H(h) h->iHeight
#define HND_FMT(h) h->iFormat

/*
 * Return total number of ion fd entries in the handle.
 *
 * Legacy handle does not support ion fds, so will return 0, use this test
 * to determine whether to get pvr fds instead.
 */
#define GET_ION_FD_COUNT(h) 0

/*
 * Return ion fd at index 'n'.
 *
 * Legacy handle does not support ion fds.
 */
#define GET_ION_FD(h, n) -1

/*
 * Return pvr fd
 */
#define GET_PVR_FD(h) h->fd[0]

#else

#include "gralloc/gralloc_ti_handle.h"
#include "gralloc/hal_gpu_public.h"

typedef gralloc_ti_handle ti_hndl_t;

#define HND_W(h) h->width
#define HND_H(h) h->height
#define HND_FMT(h) h->format

/*
 * Return total number of ion fd entries in the handle.
 */
#define GET_ION_FD_COUNT(h) ti_gralloc_handle_num_of_planes(h)

/*
 * Return ion fd at index 'n'.
 *
 * When iterating through the fds, an fd with value < 0 indicates the start
 * of the unused fd entries.
 */
#define GET_ION_FD(h, n) h->export_fds[n]

/*
 * Return pvr fd
 */
#define GET_PVR_FD(h) h->gpu_sync_fd

#endif

#endif /* _TI_HANDLE_WRAPPER_H_ */
