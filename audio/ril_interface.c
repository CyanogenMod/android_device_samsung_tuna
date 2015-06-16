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

#define LOG_TAG "audio_hw_primary"
/*#define LOG_NDEBUG 0*/

#include <dlfcn.h>
#include <stdlib.h>

#include <utils/Log.h>
#include <cutils/properties.h>

#include "ril_interface.h"

#define VOLUME_STEPS_DEFAULT  "5"
#define VOLUME_STEPS_PROPERTY "ro.config.vc_call_vol_steps"

/* Audio WB AMR callback */
void (*_audio_set_wb_amr_callback)(void *, int);
void *callback_data = NULL;

void ril_register_set_wb_amr_callback(void *function, void *data)
{
    _audio_set_wb_amr_callback = function;
    callback_data = data;
}

/* This is the callback function that the RIL uses to
set the wideband AMR state */
static int ril_set_wb_amr_callback(void *ril_client,
                                   const void *data,
                                   size_t datalen)
{
    int enable = ((int *)data)[0];

    if (!callback_data || !_audio_set_wb_amr_callback)
        return -1;

    _audio_set_wb_amr_callback(callback_data, enable);

    return 0;
}

static int ril_connect_if_required(struct ril_handle *ril)
{
    if (isConnected_RILD(ril->client))
        return 0;

    if (Connect_RILD(ril->client) != RIL_CLIENT_ERR_SUCCESS) {
        ALOGE("Connect_RILD() failed");
        return -1;
    }

    /* get wb amr status to set pcm samplerate depending on
       wb amr status when ril is connected. */
    GetWB_AMR(ril->client, (RilOnComplete)ril_set_wb_amr_callback);

    return 0;
}

int ril_open(struct ril_handle *ril)
{
    char property[PROPERTY_VALUE_MAX];

    if (!ril)
        return -1;

    ril->client = OpenClient_RILD();
    if (!ril->client) {
        ALOGE("OpenClient_RILD() failed");
        return -1;
    }

    /* register the wideband AMR callback */
    RegisterUnsolicitedHandler(ril->client, RIL_UNSOL_WB_AMR_STATE,
                               (RilOnUnsolicited)ril_set_wb_amr_callback);

    property_get(VOLUME_STEPS_PROPERTY, property, VOLUME_STEPS_DEFAULT);
    ril->volume_steps_max = atoi(property);
    /* this catches the case where VOLUME_STEPS_PROPERTY does not contain
    an integer */
    if (ril->volume_steps_max == 0)
        ril->volume_steps_max = atoi(VOLUME_STEPS_DEFAULT);

    return 0;
}

int ril_close(struct ril_handle *ril)
{
    if (!ril || !ril->client)
        return -1;

    if ((Disconnect_RILD(ril->client) != RIL_CLIENT_ERR_SUCCESS) ||
        (CloseClient_RILD(ril->client) != RIL_CLIENT_ERR_SUCCESS)) {
        ALOGE("Disconnect_RILD() or CloseClient_RILD() failed");
        return -1;
    }

    return 0;
}

int ril_set_call_volume(struct ril_handle *ril, enum _SoundType sound_type,
                        float volume)
{
    if (ril_connect_if_required(ril))
        return 0;

    return SetCallVolume(ril->client, sound_type,
                         (int)(volume * ril->volume_steps_max));
}

int ril_set_call_audio_path(struct ril_handle *ril, enum _AudioPath path)
{
    if (ril_connect_if_required(ril))
        return 0;

    return SetCallAudioPath(ril->client, path);
}

int ril_set_mic_mute(struct ril_handle *ril, enum _MuteCondition state)
{
    if (ril_connect_if_required(ril))
        return 0;

    return SetMute(ril->client, state);
}
