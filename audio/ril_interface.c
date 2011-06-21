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

#include <dlfcn.h>

#include <utils/Log.h>

#include "ril_interface.h"

int ril_open(void **ril_handle, void **ril_client)
{
    void *handle;
    void *client;

    handle = dlopen(RIL_CLIENT_LIBPATH, RTLD_NOW);

    if (!handle) {
        LOGE("Cannot open '%s'", RIL_CLIENT_LIBPATH);
        return -1;
    }

    ril_open_client = dlsym(handle, "OpenClient_RILD");
    ril_close_client = dlsym(handle, "CloseClient_RILD");
    ril_connect = dlsym(handle, "Connect_RILD");
    ril_is_connected = dlsym(handle, "isConnected_RILD");
    ril_disconnect = dlsym(handle, "Disconnect_RILD");
    ril_set_call_volume = dlsym(handle, "SetCallVolume");
    ril_set_call_audio_path = dlsym(handle, "SetCallAudioPath");
    ril_set_call_clock_sync = dlsym(handle, "SetCallClockSync");

    if (!ril_open_client || !ril_close_client || !ril_connect ||
        !ril_is_connected || !ril_disconnect || !ril_set_call_volume ||
        !ril_set_call_audio_path || !ril_set_call_clock_sync) {
        LOGE("Cannot get symbols from '%s'", RIL_CLIENT_LIBPATH);
        dlclose(handle);
        return -1;
    }

    client = ril_open_client();
    if (!client) {
        LOGE("ril_open_client() failed");
        dlclose(handle);
        return -1;
    }

    if (ril_connect(client) != RIL_CLIENT_ERR_SUCCESS ||
        !ril_is_connected(client)) {
        LOGE("ril_connect() failed");
        ril_close_client(client);
        dlclose(handle);
        return -1;
    }

    *ril_handle = handle;
    *ril_client = client;
    return 0;
}

int ril_close(void *ril_handle, void *ril_client)
{
    if (!ril_handle)
        return -1;

    if ((ril_disconnect(ril_client) != RIL_CLIENT_ERR_SUCCESS) ||
        (ril_close_client(ril_client) != RIL_CLIENT_ERR_SUCCESS)) {
        LOGE("ril_disconnect() or ril_close_client() failed");
        return -1;
    }

    dlclose(ril_handle);
    return 0;
}

