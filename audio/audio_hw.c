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
#define LOG_NDEBUG 0

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>

#include <hardware/hardware.h>
#include <system/audio.h>
#include <hardware/audio.h>

#include <tinyalsa/asoundlib.h>
#include <speex/speex_resampler.h>

#include "ril_interface.h"

/* Mixer control names */
#define MIXER_DL1_MEDIA_PLAYBACK_VOLUME     "DL1 Media Playback Volume"
#define MIXER_DL1_VOICE_PLAYBACK_VOLUME     "DL1 Voice Playback Volume"
#define MIXER_DL2_MEDIA_PLAYBACK_VOLUME     "DL2 Media Playback Volume"
#define MIXER_DL2_VOICE_PLAYBACK_VOLUME     "DL2 Voice Playback Volume"
#define MIXER_SDT_DL_VOLUME                 "SDT DL Volume"

#define MIXER_HEADSET_PLAYBACK_VOLUME       "Headset Playback Volume"
#define MIXER_HANDSFREE_PLAYBACK_VOLUME     "Handsfree Playback Volume"
#define MIXER_EARPHONE_PLAYBACK_VOLUME      "Earphone Playback Volume"
#define MIXER_BT_UL_VOLUME                  "BT UL Volume"

#define MIXER_DL1_MIXER_MULTIMEDIA          "DL1 Mixer Multimedia"
#define MIXER_DL1_MIXER_VOICE               "DL1 Mixer Voice"
#define MIXER_DL2_MIXER_MULTIMEDIA          "DL2 Mixer Multimedia"
#define MIXER_DL2_MIXER_VOICE               "DL2 Mixer Voice"
#define MIXER_SIDETONE_MIXER_PLAYBACK       "Sidetone Mixer Playback"
#define MIXER_DL1_PDM_SWITCH                "DL1 PDM Switch"
#define MIXER_DL1_BT_VX_SWITCH              "DL1 BT_VX Switch"
#define MIXER_VOICE_CAPTURE_MIXER_CAPTURE   "Voice Capture Mixer Capture"

#define MIXER_HS_LEFT_PLAYBACK              "HS Left Playback"
#define MIXER_HS_RIGHT_PLAYBACK             "HS Right Playback"
#define MIXER_HF_LEFT_PLAYBACK              "HF Left Playback"
#define MIXER_HF_RIGHT_PLAYBACK             "HF Right Playback"
#define MIXER_EARPHONE_DRIVER_SWITCH        "Earphone Driver Switch"

#define MIXER_ANALOG_LEFT_CAPTURE_ROUTE     "Analog Left Capture Route"
#define MIXER_ANALOG_RIGHT_CAPTURE_ROUTE    "Analog Right Capture Route"
#define MIXER_CAPTURE_PREAMPLIFIER_VOLUME   "Capture Preamplifier Volume"
#define MIXER_CAPTURE_VOLUME                "Capture Volume"
#define MIXER_AMIC_UL_VOLUME                "AMIC UL Volume"
#define MIXER_AUDUL_VOICE_UL_VOLUME         "AUDUL Voice UL Volume"
#define MIXER_MUX_VX0                       "MUX_VX0"
#define MIXER_MUX_VX1                       "MUX_VX1"
#define MIXER_MUX_UL10                      "MUX_UL10"
#define MIXER_MUX_UL11                      "MUX_UL11"

/* Mixer control gain and route values */
#define MIXER_ABE_GAIN_0DB                  120
#define MIXER_ABE_GAIN_MINUS1DB             118
#define MIXER_CODEC_VOLUME_MAX              15
#define MIXER_PLAYBACK_HS_DAC               "HS DAC"
#define MIXER_PLAYBACK_HF_DAC               "HF DAC"
#define MIXER_MAIN_MIC                      "Main Mic"
#define MIXER_SUB_MIC                       "Sub Mic"
#define MIXER_HS_MIC                        "Headset Mic"
#define MIXER_AMIC0                         "AMic0"
#define MIXER_AMIC1                         "AMic1"
#define MIXER_BT_LEFT                       "BT Left"
#define MIXER_BT_RIGHT                      "BT Right"

/* ALSA ports for OMAP4 */
#define PORT_MM 0
#define PORT_MM2_UL 1
#define PORT_VX 2
#define PORT_TONES 3
#define PORT_VIBRA 4
#define PORT_MODEM 5
#define PORT_MM_LP 6

#define RESAMPLER_BUFFER_SIZE 8192

#define DEFAULT_OUT_SAMPLING_RATE 44100

struct pcm_config pcm_config_mm = {
    .channels = 2,
    .rate = 48000,
    .period_size = 1024,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_vx = {
    .channels = 1,
    .rate = 8000,
    .period_size = 160,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

#define MIN(x, y) ((x) > (y) ? (y) : (x))

struct route_setting
{
    char *ctl_name;
    int intval;
    char *strval;
};

/* These are values that never change */
struct route_setting defaults[] = {
    /* general */
    {
        .ctl_name = MIXER_DL1_MEDIA_PLAYBACK_VOLUME,
        .intval = MIXER_ABE_GAIN_MINUS1DB,
    },
    {
        .ctl_name = MIXER_DL2_MEDIA_PLAYBACK_VOLUME,
        .intval = MIXER_ABE_GAIN_MINUS1DB,
    },
    {
        .ctl_name = MIXER_DL1_VOICE_PLAYBACK_VOLUME,
        .intval = MIXER_ABE_GAIN_MINUS1DB,
    },
    {
        .ctl_name = MIXER_DL2_VOICE_PLAYBACK_VOLUME,
        .intval = MIXER_ABE_GAIN_MINUS1DB,
    },
    {
        .ctl_name = MIXER_SDT_DL_VOLUME,
        .intval = MIXER_ABE_GAIN_0DB,
    },
    {
        .ctl_name = MIXER_HEADSET_PLAYBACK_VOLUME,
        .intval = 13,
    },
    {
        .ctl_name = MIXER_EARPHONE_PLAYBACK_VOLUME,
        .intval = 15,
    },
    {
        .ctl_name = MIXER_HANDSFREE_PLAYBACK_VOLUME,
        .intval = 26, /* max for no distortion */
    },
    {
        .ctl_name = MIXER_AUDUL_VOICE_UL_VOLUME,
        .intval = MIXER_ABE_GAIN_0DB,
    },
    {
        .ctl_name = MIXER_CAPTURE_PREAMPLIFIER_VOLUME,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_CAPTURE_VOLUME,
        .intval = 4,
    },

    /* speaker */
    {
        .ctl_name = MIXER_HF_LEFT_PLAYBACK,
        .strval = MIXER_PLAYBACK_HF_DAC,
    },
    {
        .ctl_name = MIXER_HF_RIGHT_PLAYBACK,
        .strval = MIXER_PLAYBACK_HF_DAC,
    },

    /* headset */
    {
        .ctl_name = MIXER_SIDETONE_MIXER_PLAYBACK,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_DL1_PDM_SWITCH,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_HS_LEFT_PLAYBACK,
        .strval = MIXER_PLAYBACK_HS_DAC,
    },
    {
        .ctl_name = MIXER_HS_RIGHT_PLAYBACK,
        .strval = MIXER_PLAYBACK_HS_DAC,
    },

    /* bt */
    {
        .ctl_name = MIXER_BT_UL_VOLUME,
        .intval = MIXER_ABE_GAIN_MINUS1DB,
    },
    {
        .ctl_name = NULL,
    },
};

/* MM UL front-end paths */
struct route_setting mm_ul2_bt[] = {
    {
        .ctl_name = MIXER_MUX_UL10,
        .strval = MIXER_BT_LEFT,
    },
    {
        .ctl_name = MIXER_MUX_UL11,
        .strval = MIXER_BT_RIGHT,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting mm_ul2_amic[] = {
    {
        .ctl_name = MIXER_MUX_UL10,
        .strval = MIXER_AMIC0,
    },
    {
        .ctl_name = MIXER_MUX_UL11,
        .strval = MIXER_AMIC1,
    },
    {
        .ctl_name = NULL,
    },
};

/* VX UL front-end paths */
struct route_setting vx_ul_amic[] = {
    {
        .ctl_name = MIXER_MUX_VX0,
        .strval = MIXER_AMIC0,
    },
    {
        .ctl_name = MIXER_MUX_VX1,
        .strval = MIXER_AMIC1,
    },
    {
        .ctl_name = MIXER_VOICE_CAPTURE_MIXER_CAPTURE,
        .intval = 1,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting vx_ul_bt[] = {
    {
        .ctl_name = MIXER_MUX_VX0,
        .strval = MIXER_BT_LEFT,
    },
    {
        .ctl_name = MIXER_MUX_VX1,
        .strval = MIXER_BT_RIGHT,
    },
    {
        .ctl_name = MIXER_VOICE_CAPTURE_MIXER_CAPTURE,
        .intval = 1,
    },
    {
        .ctl_name = NULL,
    },
};

struct mixer_ctls
{
    struct mixer_ctl *mm_dl1;
    struct mixer_ctl *mm_dl2;
    struct mixer_ctl *vx_dl1;
    struct mixer_ctl *vx_dl2;
    struct mixer_ctl *earpiece_switch;
    struct mixer_ctl *dl1_headset;
    struct mixer_ctl *dl1_bt;
    struct mixer_ctl *left_capture;
    struct mixer_ctl *right_capture;
};

struct tuna_audio_device {
    struct audio_hw_device hw_device;

    pthread_mutex_t lock;
    struct mixer *mixer;
    struct mixer_ctls mixer_ctls;
    int mode;
    int devices;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
    int in_call;
    float voice_volume;
    struct tuna_stream_in *active_input;
    /* RIL */
    struct ril_handle ril;
};

struct tuna_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;
    struct pcm_config config;
    struct pcm *pcm;
    int device;
    SpeexResamplerState *speex;
    char *buffer;
    int standby;

    struct tuna_audio_device *dev;
};

struct tuna_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;
    struct pcm_config config;
    struct pcm *pcm;
    int device;
    SpeexResamplerState *speex;
    char *buffer;
    size_t frames_in;
    unsigned int requested_rate;
    int port;
    int standby;

    struct tuna_audio_device *dev;
};

static void select_output_device(struct tuna_audio_device *adev);
static void select_input_device(struct tuna_audio_device *adev);
static int adev_set_voice_volume(struct audio_hw_device *dev, float volume);

/* The enable flag when 0 makes the assumption that enums are disabled by
 * "Off" and integers/booleans by 0 */
static int set_route_by_array(struct mixer *mixer, struct route_setting *route,
                              int enable)
{
    struct mixer_ctl *ctl;
    unsigned int i, j;

    /* Go through the route array and set each value */
    i = 0;
    while (route[i].ctl_name) {
        ctl = mixer_get_ctl_by_name(mixer, route[i].ctl_name);
        if (!ctl)
            return -EINVAL;

        if (route[i].strval) {
            if (enable)
                mixer_ctl_set_enum_by_string(ctl, route[i].strval);
            else
                mixer_ctl_set_enum_by_string(ctl, "Off");
        } else {
            /* This ensures multiple (i.e. stereo) values are set jointly */
            for (j = 0; j < mixer_ctl_get_num_values(ctl); j++) {
                if (enable)
                    mixer_ctl_set_value(ctl, j, route[i].intval);
                else
                    mixer_ctl_set_value(ctl, j, 0);
            }
        }
        i++;
    }

    return 0;
}

static int start_call(struct tuna_audio_device *adev)
{
    /* Open modem PCM channels */
    if (adev->pcm_modem_dl == NULL) {
        adev->pcm_modem_dl = pcm_open(0, PORT_MODEM, PCM_OUT, &pcm_config_vx);
        if (!pcm_is_ready(adev->pcm_modem_dl)) {
            LOGE("cannot open PCM modem DL stream: %s", pcm_get_error(adev->pcm_modem_dl));
            goto err_open_dl;
        }
    }

    if (adev->pcm_modem_ul == NULL) {
        adev->pcm_modem_ul = pcm_open(0, PORT_MODEM, PCM_IN, &pcm_config_vx);
        if (!pcm_is_ready(adev->pcm_modem_ul)) {
            LOGE("cannot open PCM modem UL stream: %s", pcm_get_error(adev->pcm_modem_ul));
            goto err_open_ul;
        }
    }

    ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_START);
    ril_set_call_audio_path(&adev->ril, SOUND_AUDIO_PATH_HANDSET);

    pcm_start(adev->pcm_modem_dl);
    pcm_start(adev->pcm_modem_ul);

    return 0;

err_open_dl:
    pcm_close(adev->pcm_modem_dl);
    adev->pcm_modem_dl = NULL;
err_open_ul:
    pcm_close(adev->pcm_modem_ul);
    adev->pcm_modem_ul = NULL;

    return -ENOMEM;
}

static void end_call(struct tuna_audio_device *adev)
{
    pcm_stop(adev->pcm_modem_dl);
    pcm_stop(adev->pcm_modem_ul);
    pcm_close(adev->pcm_modem_dl);
    pcm_close(adev->pcm_modem_ul);
    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;
}

static void select_mode(struct tuna_audio_device *adev)
{
    if (adev->mode == AUDIO_MODE_IN_CALL) {
        if (!adev->in_call) {
            select_output_device(adev);
            start_call(adev);
            adev_set_voice_volume(&adev->hw_device, adev->voice_volume);
            adev->in_call = 1;
        }
    } else {
        if (adev->in_call) {
            adev->in_call = 0;
            end_call(adev);
            select_output_device(adev);
            select_input_device(adev);
        }
    }
}

static void select_output_device(struct tuna_audio_device *adev)
{
    int headset_on;
    int speaker_on;
    int earpiece_on;
    int bt_on;
    int dl1_on;

    /* tear down call stream before changing route,
    otherwise microphone does not function */
    if (adev->in_call)
        end_call(adev);

    headset_on = adev->devices &
                (AUDIO_DEVICE_OUT_WIRED_HEADSET | AUDIO_DEVICE_OUT_WIRED_HEADPHONE);
    speaker_on = adev->devices & AUDIO_DEVICE_OUT_SPEAKER;
    earpiece_on = adev->devices & AUDIO_DEVICE_OUT_EARPIECE;
    bt_on = adev->devices & AUDIO_DEVICE_OUT_ALL_SCO;
    dl1_on = headset_on | earpiece_on | bt_on;

    /* Select front end */
    mixer_ctl_set_value(adev->mixer_ctls.mm_dl2, 0, speaker_on);
    mixer_ctl_set_value(adev->mixer_ctls.vx_dl2, 0,
                        speaker_on && (adev->mode == AUDIO_MODE_IN_CALL));
    mixer_ctl_set_value(adev->mixer_ctls.mm_dl1, 0, dl1_on);
    mixer_ctl_set_value(adev->mixer_ctls.vx_dl1, 0,
                        dl1_on && (adev->mode == AUDIO_MODE_IN_CALL));
    /* Select back end */
    mixer_ctl_set_value(adev->mixer_ctls.dl1_headset, 0, headset_on | earpiece_on);
    mixer_ctl_set_value(adev->mixer_ctls.dl1_bt, 0, bt_on);
    mixer_ctl_set_value(adev->mixer_ctls.earpiece_switch, 0, earpiece_on);

    /* Special case: select input path if in a call, otherwise
       in_set_parameters is used to update the input route
       todo: use sub mic for handsfree case */
    if (adev->mode == AUDIO_MODE_IN_CALL) {
        if (bt_on)
            set_route_by_array(adev->mixer, vx_ul_bt, bt_on);
        else {
            set_route_by_array(adev->mixer, vx_ul_amic,
                              (speaker_on | headset_on | earpiece_on));
            if (headset_on)
                mixer_ctl_set_enum_by_string(adev->mixer_ctls.left_capture, MIXER_HS_MIC);
            else
                mixer_ctl_set_enum_by_string(adev->mixer_ctls.left_capture,
                                            (speaker_on | earpiece_on) ?
                                             MIXER_MAIN_MIC : "Off");
        }
    }
    if (adev->in_call)
        start_call(adev);
}

static void select_input_device(struct tuna_audio_device *adev)
{
    int headset_on;
    int main_mic_on;
    int sub_mic_on = 0; /* not routing to sub-mic for now */
    int bt_on;
    int anlg_mic_on;
    int port;

    headset_on = adev->devices & AUDIO_DEVICE_IN_WIRED_HEADSET;
    main_mic_on = adev->devices & AUDIO_DEVICE_IN_BUILTIN_MIC;
    bt_on = adev->devices & AUDIO_DEVICE_IN_ALL_SCO;
    anlg_mic_on = headset_on | main_mic_on | sub_mic_on;

    /* PORT_MM2_UL is only used when not in call and active input uses it. */
    port = PORT_VX;
    if ((adev->mode != AUDIO_MODE_IN_CALL) && (adev->active_input != 0))
        port = adev->active_input->port;

    /* tear down call stream before changing route,
     * otherwise microphone does not function
     */
    if (adev->in_call)
        end_call(adev);

   /* TODO: check how capture is possible during voice calls or if
    * both use cases are mutually exclusive.
    */
    if (bt_on) {
        set_route_by_array(adev->mixer, mm_ul2_bt, (port != PORT_VX));
        set_route_by_array(adev->mixer, vx_ul_bt, (port == PORT_VX));
    } else {
        /* Select front end */
        set_route_by_array(adev->mixer, mm_ul2_amic,
                           anlg_mic_on && (port != PORT_VX));
        set_route_by_array(adev->mixer, vx_ul_amic,
                           anlg_mic_on && (port == PORT_VX));

        /* Select back end */
        if (headset_on)
            mixer_ctl_set_enum_by_string(adev->mixer_ctls.left_capture,
                                         MIXER_HS_MIC);
        else
            mixer_ctl_set_enum_by_string(adev->mixer_ctls.left_capture,
                                         main_mic_on ? MIXER_MAIN_MIC : "Off");
        /* TODO: set up sub mic for BACK_MIC when gpio for sub_mic is enabled */
    }

    if (adev->in_call)
        start_call(adev);
}

static int start_output_stream(struct tuna_stream_out *out)
{
    struct tuna_audio_device *adev = out->dev;

    pthread_mutex_lock(&adev->lock);
    adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
    adev->devices |= out->device;
    select_output_device(adev);
    pthread_mutex_unlock(&adev->lock);

    out->pcm = pcm_open(0, PORT_MM, PCM_OUT, &out->config);
    if (!pcm_is_ready(out->pcm)) {
        LOGE("cannot open pcm_out driver: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        return -ENOMEM;
    }

    return 0;
}

static int check_input_parameters(uint32_t sample_rate, int format, int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT)
        return -EINVAL;

    if ((channel_count < 1) || (channel_count > 2))
        return -EINVAL;

    switch(sample_rate) {
    case 8000:
    case 11025:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate, int format, int channel_count)
{
    size_t size;
    size_t device_rate;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    switch (sample_rate) {
    case 8000:
        size = pcm_config_vx.period_size;
        device_rate = 8000;
        break;

    case 11025:
    case 16000:
        size = pcm_config_vx.period_size * 2;
        device_rate = 16000;
        break;

    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        size = pcm_config_mm.period_size;
        device_rate = 48000;
        break;

    default:
        return 0;
    }

    size = (((size * sample_rate) / device_rate + 15) / 16) * 16;

    return size * channel_count * sizeof(short);
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return DEFAULT_OUT_SAMPLING_RATE;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;

    /* take resampling into account and return the closest majoring
    multiple of 16 frames, as audioflinger expects audio buffers to
    be a multiple of 16 frames */
    size_t size = (out->config.period_size * DEFAULT_OUT_SAMPLING_RATE) /
                  out->config.rate;
    size = ((size + 15) / 16) * 16;
    return size * audio_stream_frame_size((struct audio_stream *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_OUT_STEREO;
}

static int out_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int out_set_format(struct audio_stream *stream, int format)
{
    return 0;
}

static int out_standby(struct audio_stream *stream)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;

    pthread_mutex_lock(&out->lock);
    if (!out->standby) {
        pcm_close(out->pcm);
        out->pcm = NULL;
        out->standby = 1;
    }
    pthread_mutex_unlock(&out->lock);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;
    struct tuna_audio_device *adev = out->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&out->lock);
        if ((out->device != val) && (val != 0)) {
            out->device = val;
            pthread_mutex_unlock(&out->lock);
            pthread_mutex_lock(&adev->lock);
            if (adev->mode == AUDIO_MODE_IN_CALL) {
                adev->devices &= ~AUDIO_DEVICE_OUT_ALL;
                adev->devices |= out->device;
                select_output_device(adev);
                pthread_mutex_unlock(&adev->lock);
            } else {
                pthread_mutex_unlock(&adev->lock);
                out_standby(stream);
            }
        } else
            pthread_mutex_unlock(&out->lock);
    }

    str_parms_destroy(parms);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;

    return (out->config.period_size * out->config.period_count * 1000) /
            out->config.rate;
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void* buffer,
                         size_t bytes)
{
    int ret;
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;
    struct tuna_audio_device *adev = out->dev;
    spx_uint32_t in_frames = bytes / 4; /* todo */
    spx_uint32_t out_frames = RESAMPLER_BUFFER_SIZE / 4;
    unsigned int total_bytes;
    unsigned int max_bytes;
    unsigned int remaining_bytes;
    unsigned int pos;

    pthread_mutex_lock(&out->lock);
    if (out->standby) {
        ret = start_output_stream(out);
        if (ret == 0)
            out->standby = 0;
    }
    speex_resampler_process_interleaved_int(out->speex, buffer, &in_frames,
                                            (spx_int16_t *)out->buffer,
                                            &out_frames);

    total_bytes = out_frames * 4;
    max_bytes = pcm_get_buffer_size(out->pcm);
    remaining_bytes = total_bytes;
    for (pos = 0; pos < total_bytes; pos += max_bytes) {
        int bytes_to_write = MIN(max_bytes, remaining_bytes);

        ret = pcm_write(out->pcm, (void *)(out->buffer + pos), bytes_to_write);

        if (ret != 0) {
            usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
                   out_get_sample_rate(&stream->common));
            pthread_mutex_unlock(&out->lock);
            return bytes;
        }

        remaining_bytes -= bytes_to_write;
    }

    pthread_mutex_unlock(&out->lock);
    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

/** audio_stream_in implementation **/
static int start_input_stream(struct tuna_stream_in *in)
{
    int ret = 0;
    struct tuna_audio_device *adev = in->dev;

    pthread_mutex_lock(&adev->lock);
    adev->devices &= ~AUDIO_DEVICE_IN_ALL;
    adev->devices |= in->device;
    adev->active_input = in;
    select_input_device(adev);
    pthread_mutex_unlock(&adev->lock);

    /* this assumes routing is done previously */
    in->pcm = pcm_open(0, in->port, PCM_IN, &in->config);
    if (!pcm_is_ready(in->pcm)) {
        LOGE("cannot open pcm_in driver: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }
    /* if no supported sample rate is available, use the resampler */
    if (in->speex) {
        speex_resampler_reset_mem(in->speex);
        in->frames_in = 0;
    }
    return 0;
}

static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 AUDIO_FORMAT_PCM_16_BIT,
                                 in->config.channels);
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;

    if (in->config.channels == 1) {
        return AUDIO_CHANNEL_IN_MONO;
    } else {
        return AUDIO_CHANNEL_IN_STEREO;
    }
}

static int in_get_format(const struct audio_stream *stream)
{
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, int format)
{
    return 0;
}

static int in_standby(struct audio_stream *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;
    struct tuna_audio_device *adev = in->dev;

    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;
        adev->active_input = 0;
        pthread_mutex_lock(&adev->lock);
        adev->devices &= ~AUDIO_DEVICE_IN_ALL;
        adev->active_input = 0;
        select_input_device(adev);
        pthread_mutex_unlock(&adev->lock);
        in->standby = 1;
    }
    pthread_mutex_unlock(&in->lock);
    return 0;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;
    struct tuna_audio_device *adev = in->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;

    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        pthread_mutex_lock(&in->lock);
        if ((in->device != val) && (val != 0)) {
            in->device = val;
            pthread_mutex_unlock(&in->lock);
            in_standby(stream);
        } else
            pthread_mutex_unlock(&in->lock);
    }

    str_parms_destroy(parms);
    return ret;
}

static char * in_get_parameters(const struct audio_stream *stream,
                                const char *keys)
{
    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void* buffer,
                       size_t bytes)
{
    int ret = 0;
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;
    struct tuna_audio_device *adev = in->dev;

    pthread_mutex_lock(&in->lock);
    if (in->standby) {
        ret = start_input_stream(in);
        if (ret == 0)
            in->standby = 0;
    }

    if (ret < 0)
        goto exit;

    if (in->speex) {
        size_t frame_size = audio_stream_frame_size(&in->stream.common);
        size_t frames_rq = bytes / frame_size;
        size_t frames_wr = 0;

        while (frames_wr < frames_rq) {
            size_t frames_in;
            size_t frames_out;

            if (in->frames_in == 0) {
                ret = pcm_read(in->pcm, in->buffer, in->config.period_size * frame_size);
                if (ret != 0)
                    break;
                in->frames_in = in->config.period_size;
            }

            frames_out = frames_rq - frames_wr;
            frames_in = in->frames_in;
            if (in->config.channels == 1) {
                speex_resampler_process_int(
                        in->speex,
                        0,
                        (short *)((char *)in->buffer +
                                (in->config.period_size - in->frames_in) * frame_size),
                        &frames_in,
                        (short *)((char *)buffer + frames_wr * frame_size),
                        &frames_out);
            } else {
                speex_resampler_process_interleaved_int(
                        in->speex,
                        (short *)((char *)in->buffer +
                                (in->config.period_size - in->frames_in) * frame_size),
                        &frames_in,
                        (short *)((char *)buffer + frames_wr * frame_size),
                        &frames_out);
            }
            frames_wr += frames_out;
            in->frames_in -= frames_in;
        }
    } else {
        ret = pcm_read(in->pcm, buffer, bytes);
    }

exit:
    if (ret < 0)
        usleep(bytes * 1000000 / audio_stream_frame_size(&stream->common) /
               in_get_sample_rate(&stream->common));

    pthread_mutex_unlock(&in->lock);
    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    return 0;
}

static int in_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}

static int in_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    return 0;
}


static int adev_open_output_stream(struct audio_hw_device *dev,
                                   uint32_t devices, int *format,
                                   uint32_t *channels, uint32_t *sample_rate,
                                   struct audio_stream_out **stream_out)
{
    struct tuna_audio_device *ladev = (struct tuna_audio_device *)dev;
    struct tuna_stream_out *out;
    int ret;

    out = (struct tuna_stream_out *)calloc(1, sizeof(struct tuna_stream_out));
    if (!out)
        return -ENOMEM;

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;

    out->config = pcm_config_mm;

    out->speex = speex_resampler_init(2, DEFAULT_OUT_SAMPLING_RATE, 48000,
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT, &ret);
    speex_resampler_reset_mem(out->speex);
    out->buffer = malloc(RESAMPLER_BUFFER_SIZE); /* todo: allow for reallocing */

    out->device = devices;
    out->dev = ladev;
    out->standby = !!start_output_stream(out);

    *format = out_get_format(&out->stream.common);
    *channels = out_get_channels(&out->stream.common);
    *sample_rate = out_get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;
    return 0;

err_open:
    free(out);
    *stream_out = NULL;
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;

    out_standby(&stream->common);
    if (out->buffer)
        free(out->buffer);
    if (out->speex)
        speex_resampler_destroy(out->speex);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct tuna_audio_device *adev = (struct tuna_audio_device *)dev;

    adev->voice_volume = volume;

    if (adev->mode == AUDIO_MODE_IN_CALL)
        ril_set_call_volume(&adev->ril, SOUND_TYPE_VOICE, volume);

    return 0;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, int mode)
{
    struct tuna_audio_device *adev = (struct tuna_audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        adev->mode = mode;
        select_mode(adev);
    }
    pthread_mutex_unlock(&adev->lock);

    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    return -ENOSYS;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    return -ENOSYS;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         uint32_t sample_rate, int format,
                                         int channel_count)
{
    size_t size;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    return get_input_buffer_size(sample_rate, format, channel_count);
}

static int adev_open_input_stream(struct audio_hw_device *dev, uint32_t devices,
                                  int *format, uint32_t *channel_mask,
                                  uint32_t *sample_rate,
                                  audio_in_acoustics_t acoustics,
                                  struct audio_stream_in **stream_in)
{
    struct tuna_audio_device *ladev = (struct tuna_audio_device *)dev;
    struct tuna_stream_in *in;
    int ret;
    int channel_count = popcount(*channel_mask);

    if (check_input_parameters(*sample_rate, *format, channel_count) != 0)
        return -EINVAL;

    in = (struct tuna_stream_in *)calloc(1, sizeof(struct tuna_stream_in));
    if (!in)
        return -ENOMEM;

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->requested_rate = *sample_rate;

    if (in->requested_rate <= 8000) {
        in->port = PORT_VX;
        memcpy(&in->config, &pcm_config_vx, sizeof(pcm_config_vx));
        in->config.rate = 8000;
    } else if (in->requested_rate <= 16000) {
        in->port = PORT_VX; /* use voice uplink */
        memcpy(&in->config, &pcm_config_vx, sizeof(pcm_config_vx));
        in->config.rate = 16000;
        in->config.period_size *= 2;
    } else {
        in->port = PORT_MM2_UL; /* use multimedia uplink 2 */
        memcpy(&in->config, &pcm_config_mm, sizeof(pcm_config_mm));
    }
    in->config.channels = channel_count;

    if (in->requested_rate != in->config.rate) {
        in->speex = speex_resampler_init(in->config.channels, in->config.rate,
                                         in->requested_rate,
                                         SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                         &ret);
        if (ret != RESAMPLER_ERR_SUCCESS) {
            ret = -EINVAL;
            goto err;
        }
        in->buffer = malloc(in->config.period_size *
                            audio_stream_frame_size(&in->stream.common));
        if (!in->buffer) {
            ret = -ENOMEM;
            goto err;
        }
    }

    in->dev = ladev;
    in->standby = 1;
    in->device = devices;

    *stream_in = &in->stream;
    return 0;

err:
    if (in->speex)
        speex_resampler_destroy(in->speex);

    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;

    in_standby(&stream->common);

    if (in->speex) {
        free(in->buffer);
        speex_resampler_destroy(in->speex);
    }

    free(stream);
    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct tuna_audio_device *adev = (struct tuna_audio_device *)device;

    /* RIL */
    ril_close(&adev->ril);

    mixer_close(adev->mixer);
    free(device);
    return 0;
}

static uint32_t adev_get_supported_devices(const struct audio_hw_device *dev)
{
    return (/* OUT */
            AUDIO_DEVICE_OUT_EARPIECE |
            AUDIO_DEVICE_OUT_SPEAKER |
            AUDIO_DEVICE_OUT_WIRED_HEADSET |
            AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
            AUDIO_DEVICE_OUT_AUX_DIGITAL |
            AUDIO_DEVICE_OUT_ANLG_DOCK_HEADSET |
            AUDIO_DEVICE_OUT_DGTL_DOCK_HEADSET |
            AUDIO_DEVICE_OUT_ALL_SCO |
            AUDIO_DEVICE_OUT_DEFAULT |
            /* IN */
            AUDIO_DEVICE_IN_COMMUNICATION |
            AUDIO_DEVICE_IN_AMBIENT |
            AUDIO_DEVICE_IN_BUILTIN_MIC |
            AUDIO_DEVICE_IN_WIRED_HEADSET |
            AUDIO_DEVICE_IN_AUX_DIGITAL |
            AUDIO_DEVICE_IN_BACK_MIC |
            AUDIO_DEVICE_IN_ALL_SCO |
            AUDIO_DEVICE_IN_DEFAULT);
}

static int adev_open(const hw_module_t* module, const char* name,
                     hw_device_t** device)
{
    struct tuna_audio_device *adev;
    int ret;

    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0)
        return -EINVAL;

    adev = calloc(1, sizeof(struct tuna_audio_device));
    if (!adev)
        return -ENOMEM;

    adev->hw_device.common.tag = HARDWARE_DEVICE_TAG;
    adev->hw_device.common.version = 0;
    adev->hw_device.common.module = (struct hw_module_t *) module;
    adev->hw_device.common.close = adev_close;

    adev->hw_device.get_supported_devices = adev_get_supported_devices;
    adev->hw_device.init_check = adev_init_check;
    adev->hw_device.set_voice_volume = adev_set_voice_volume;
    adev->hw_device.set_master_volume = adev_set_master_volume;
    adev->hw_device.set_mode = adev_set_mode;
    adev->hw_device.set_mic_mute = adev_set_mic_mute;
    adev->hw_device.get_mic_mute = adev_get_mic_mute;
    adev->hw_device.set_parameters = adev_set_parameters;
    adev->hw_device.get_parameters = adev_get_parameters;
    adev->hw_device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->hw_device.open_output_stream = adev_open_output_stream;
    adev->hw_device.close_output_stream = adev_close_output_stream;
    adev->hw_device.open_input_stream = adev_open_input_stream;
    adev->hw_device.close_input_stream = adev_close_input_stream;
    adev->hw_device.dump = adev_dump;

    adev->mixer = mixer_open(0);
    if (!adev->mixer) {
        free(adev);
        return -ENOMEM;
    }

    adev->mixer_ctls.mm_dl1 = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL1_MIXER_MULTIMEDIA);
    adev->mixer_ctls.vx_dl1 = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL1_MIXER_VOICE);
    adev->mixer_ctls.mm_dl2 = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL2_MIXER_MULTIMEDIA);
    adev->mixer_ctls.vx_dl2 = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL2_MIXER_VOICE);
    adev->mixer_ctls.dl1_headset = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL1_PDM_SWITCH);
    adev->mixer_ctls.dl1_bt = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_DL1_BT_VX_SWITCH);
    adev->mixer_ctls.earpiece_switch = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_EARPHONE_DRIVER_SWITCH);
    adev->mixer_ctls.left_capture = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_ANALOG_LEFT_CAPTURE_ROUTE);
    adev->mixer_ctls.right_capture = mixer_get_ctl_by_name(adev->mixer,
                                           MIXER_ANALOG_RIGHT_CAPTURE_ROUTE);

    if (!adev->mixer_ctls.mm_dl1 || !adev->mixer_ctls.vx_dl1 ||
        !adev->mixer_ctls.mm_dl2 || !adev->mixer_ctls.vx_dl2 ||
        !adev->mixer_ctls.dl1_headset || !adev->mixer_ctls.dl1_bt ||
        !adev->mixer_ctls.earpiece_switch || !adev->mixer_ctls.left_capture ||
        !adev->mixer_ctls.right_capture) {
        mixer_close(adev->mixer);
        free(adev);
        return -ENOMEM;
    }

    /* Set the default route before the PCM stream is opened */
    pthread_mutex_lock(&adev->lock);
    set_route_by_array(adev->mixer, defaults, 1);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->devices = AUDIO_DEVICE_OUT_SPEAKER | AUDIO_DEVICE_IN_BUILTIN_MIC;
    select_output_device(adev);

    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;
    adev->voice_volume = 1.0f;

    /* RIL */
    ril_open(&adev->ril);
    pthread_mutex_unlock(&adev->lock);

    *device = &adev->hw_device.common;

    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "Tuna audio HW HAL",
        .author = "The Android Open Source Project",
        .methods = &hal_module_methods,
    },
};
