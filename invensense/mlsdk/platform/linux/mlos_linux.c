/*
 $License:
   Copyright 2011 InvenSense, Inc.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
  $
 */
/*******************************************************************************
 *
 * $Id: mlos_linux.c 5629 2011-06-11 03:13:08Z mcaramello $
 *
 *******************************************************************************/

/**
 *  @defgroup MLOS
 *  @brief OS Interface.
 *
 *  @{
 *      @file mlos.c
 *      @brief OS Interface.
**/

/* ------------- */
/* - Includes. - */
/* ------------- */

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "stdint_invensense.h"

#include "mlos.h"
#include <errno.h>


/* -------------- */
/* - Functions. - */
/* -------------- */

/**
 *  @brief  Allocate space
 *  @param  numBytes  number of bytes
 *  @return pointer to allocated space
**/
void *inv_malloc(unsigned int numBytes)
{
    // Allocate space.
    void *allocPtr = malloc(numBytes);
    return allocPtr;
}


/**
 *  @brief  Free allocated space
 *  @param  ptr pointer to space to deallocate
 *  @return error code.
**/
inv_error_t inv_free(void *ptr)
{
    // Deallocate space.
    free(ptr);

    return INV_SUCCESS;
}


/**
 *  @brief  Mutex create function
 *  @param  mutex   pointer to mutex handle
 *  @return error code.
 */
inv_error_t inv_create_mutex(HANDLE *mutex)
{
    int res;
    pthread_mutex_t *pm = malloc(sizeof(pthread_mutex_t));
    if(pm == NULL)
        return INV_ERROR;

    res = pthread_mutex_init(pm, NULL);
    if(res == -1) {
        free(pm);
        return INV_ERROR_OS_CREATE_FAILED;
    }

    *mutex = (HANDLE)pm;

    return INV_SUCCESS;
}


/**
 *  @brief  Mutex lock function
 *  @param  mutex   Mutex handle
 *  @return error code.
 */
inv_error_t inv_lock_mutex(HANDLE mutex)
{
    int res;
    pthread_mutex_t *pm = (pthread_mutex_t*)mutex;

    res = pthread_mutex_lock(pm);
    if(res == -1)
        return INV_ERROR_OS_LOCK_FAILED;

    return INV_SUCCESS;
}


/**
 *  @brief  Mutex unlock function
 *  @param  mutex   mutex handle
 *  @return error code.
**/
inv_error_t inv_unlock_mutex(HANDLE mutex)
{
    int res;
    pthread_mutex_t *pm = (pthread_mutex_t*)mutex;

    res = pthread_mutex_unlock(pm);
    if(res == -1)
        return INV_ERROR_OS_LOCK_FAILED;

    return INV_SUCCESS;
}


/**
 *  @brief  open file
 *  @param  filename    name of the file to open.
 *  @return error code.
 */
FILE *inv_fopen(char *filename)
{
    FILE *fp = fopen(filename,"r");
    return fp;
}


/**
 *  @brief  close the file.
 *  @param  fp  handle to file to close.
 *  @return error code.
 */
void inv_fclose(FILE *fp)
{
    fclose(fp);
}

/**
 *  @brief  Close Handle
 *  @param  handle  handle to the resource.
 *  @return Zero if success, an error code otherwise.
 */
inv_error_t inv_destroy_mutex(HANDLE handle)
{
    int error;
    pthread_mutex_t *pm = (pthread_mutex_t*)handle;
    error = pthread_mutex_destroy(pm);
    if (error) {
        return errno;
    }
    free((void*) handle);

    return INV_SUCCESS;}


/**
 *  @brief  Sleep function.
 */
void inv_sleep(int mSecs)
{
    usleep(mSecs*1000);
}


/**
 *  @brief  get system's internal tick count.
 *          Used for time reference.
 *  @return current tick count.
 */
unsigned long inv_get_tick_count()
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) !=0)
        return 0;

    return (long)((tv.tv_sec * 1000000LL + tv.tv_usec) / 1000LL);
}

  /**********************/
 /** @} */ /* defgroup */
/**********************/

