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

#define MIXER_DL1_MIXER_MULTIMEDIA          "DL1 Mixer Multimedia"
#define MIXER_DL1_MIXER_VOICE               "DL1 Mixer Voice"
#define MIXER_DL2_MIXER_MULTIMEDIA          "DL2 Mixer Multimedia"
#define MIXER_DL2_MIXER_VOICE               "DL2 Mixer Voice"
#define MIXER_SIDETONE_MIXER_PLAYBACK       "Sidetone Mixer Playback"
#define MIXER_DL1_PDM_SWITCH                "DL1 PDM Switch"
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

/* Mixer control gain and route values */
#define MIXER_ABE_GAIN_0DB                  120
#define MIXER_ABE_GAIN_MINUS1DB             118
#define MIXER_CODEC_VOLUME_MAX              15
#define MIXER_PLAYBACK_HS_DAC               "HS DAC"
#define MIXER_PLAYBACK_HF_DAC               "HF DAC"
#define MIXER_MAIN_MIC                      "Main Mic"
#define MIXER_SUB_MIC                       "Sub Mic"
#define MIXER_AMIC0                         "AMic0"
#define MIXER_AMIC1                         "AMic1"

/* ALSA ports for OMAP4 */
#define PORT_MM 0
#define PORT_MM2_UL 1
#define PORT_VX 2
#define PORT_TONES 3
#define PORT_VIBRA 4
#define PORT_MODEM 5
#define PORT_MM_LP 5

#define RESAMPLER_BUFFER_SIZE 8192

#define AUDIO_DEVICE_OUT_ALL_HEADSET (AUDIO_DEVICE_OUT_EARPIECE |\
                                      AUDIO_DEVICE_OUT_WIRED_HEADSET |\
                                      AUDIO_DEVICE_OUT_WIRED_HEADPHONE)

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

    {
        .ctl_name = NULL,
    },
};

struct route_setting earpiece_switch[] = {
    {
        .ctl_name = MIXER_EARPHONE_DRIVER_SWITCH,
        .intval = 1,
    },
    {
        .ctl_name = NULL,
    },
};

/* The following four routes deliberately ensure that
the new mixer is enabled before the old are disabled */
struct route_setting speaker_mm[] = {
    {
        .ctl_name = MIXER_DL2_MIXER_MULTIMEDIA,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting headset_mm[] = {
    {
        .ctl_name = MIXER_DL1_MIXER_MULTIMEDIA,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting speaker_vx[] = {
    {
        .ctl_name = MIXER_DL2_MIXER_VOICE,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting headset_vx[] = {
    {
        .ctl_name = MIXER_DL1_MIXER_VOICE,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_DL1_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_MULTIMEDIA,
        .intval = 0,
    },
    {
        .ctl_name = MIXER_DL2_MIXER_VOICE,
        .intval = 0,
    },
    {
        .ctl_name = NULL,
    },
};

struct route_setting amic_vx[] = {
    {
        .ctl_name = MIXER_MUX_VX0,
        .strval = MIXER_AMIC0,
    },
    {
        .ctl_name = MIXER_VOICE_CAPTURE_MIXER_CAPTURE,
        .intval = 1,
    },
    {
        .ctl_name = MIXER_ANALOG_LEFT_CAPTURE_ROUTE,
        .strval = MIXER_MAIN_MIC,
    },
    {
        .ctl_name = NULL,
    },
};

struct tuna_audio_device {
    struct audio_hw_device device;

    pthread_mutex_t lock;
    struct mixer *mixer;
    int mode;
    int out_device;
    struct pcm *pcm_modem_dl;
    struct pcm *pcm_modem_ul;
    int in_call;

    /* RIL */
    void *ril_handle;
    void *ril_client;
};

struct tuna_stream_out {
    struct audio_stream_out stream;

    pthread_mutex_t lock;
    struct pcm_config config;
    struct pcm *pcm;
    SpeexResamplerState *speex;
    char *buffer;

    struct tuna_audio_device *dev;
};

struct tuna_stream_in {
    struct audio_stream_in stream;

    pthread_mutex_t lock;
    struct pcm_config config;
    struct pcm *pcm;
    SpeexResamplerState *speex;
    char *buffer;
    unsigned int requested_rate;
    int port;
    int standby;

    struct tuna_audio_device *dev;
};

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

    ril_set_call_clock_sync(adev->ril_client, SOUND_CLOCK_START);
    ril_set_call_audio_path(adev->ril_client, SOUND_AUDIO_PATH_HANDSET);

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
            set_route_by_array(adev->mixer, amic_vx, 1);
            /* force headset voice route otherwise microphone
            does not function */
            set_route_by_array(adev->mixer, headset_vx, 1);
            start_call(adev);
            adev->in_call = 1;
        }
    } else if (adev->mode == AUDIO_MODE_NORMAL) {
        if (adev->in_call) {
            adev->in_call = 0;
            end_call(adev);
            set_route_by_array(adev->mixer, amic_vx, 0);
        }
    }
}

/* Note: currently the headset/earpiece route gets priority
over speaker if both are selected as output devices. */
static void select_output_device(struct tuna_audio_device *adev)
{
    struct mixer_ctl *ctl;

    /* Select output device */
    if (adev->out_device & AUDIO_DEVICE_OUT_SPEAKER) {
        if (adev->in_call) {
            /* tear down call stream before changing route,
            otherwise microphone does not function */
            end_call(adev);
            set_route_by_array(adev->mixer, speaker_vx, 1);
            start_call(adev);
        } else
            set_route_by_array(adev->mixer, speaker_mm, 1);
    } else if (adev->out_device & AUDIO_DEVICE_OUT_ALL_HEADSET) {
        if (adev->in_call) {
            /* tear down call stream before changing route,
            otherwise microphone does not function */
            end_call(adev);
            set_route_by_array(adev->mixer, headset_vx, 1);
            if (adev->out_device & AUDIO_DEVICE_OUT_EARPIECE)
                set_route_by_array(adev->mixer, earpiece_switch, 1);
            else
                set_route_by_array(adev->mixer, earpiece_switch, 0);
            start_call(adev);
        } else {
            set_route_by_array(adev->mixer, headset_mm, 1);
            if (adev->out_device & AUDIO_DEVICE_OUT_EARPIECE)
                set_route_by_array(adev->mixer, earpiece_switch, 1);
            else
                set_route_by_array(adev->mixer, earpiece_switch, 0);
        }
    }
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    return 44100;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    return 0;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct tuna_stream_out *out = (struct tuna_stream_out *)stream;

    return pcm_get_buffer_size(out->pcm);
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
    int ret;

    parms = str_parms_create_str(kvpairs);
    pthread_mutex_lock(&adev->lock);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        if (adev->out_device != atoi(value)) {
            adev->out_device = atoi(value);
            select_output_device(adev);
        }
    }

    pthread_mutex_unlock(&adev->lock);
    str_parms_destroy(parms);
    return ret;
}

static char * out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    return strdup("");
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    int bytes_per_sample;

    if (pcm_config_mm.format == PCM_FORMAT_S32_LE)
        bytes_per_sample = 4;
    else
        bytes_per_sample = 2;

    return (pcm_config_mm.period_size * pcm_config_mm.period_count * 1000) /
           (44100 * pcm_config_mm.channels * bytes_per_sample);
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

/** audio_stream_in implementation **/
static int start_input_stream(struct tuna_stream_in *in)
{
    int ret = 0;
    struct tuna_audio_device *adev = in->dev;

    set_route_by_array(adev->mixer, amic_vx, 1);
    /* force headset voice route otherwise microphone
    does not function */
    set_route_by_array(adev->mixer, headset_vx, 1);

    /* this assumes routing is done previously */
    in->pcm = pcm_open(0, in->port, PCM_IN, &in->config);
    if (!pcm_is_ready(in->pcm)) {
        LOGE("cannot open pcm_in driver: %s", pcm_get_error(in->pcm));
        pcm_close(in->pcm);
        return -ENOMEM;
    }

    /* if no supported sample rate is available, use the resampler */
    if (in->requested_rate != in->config.rate) {
        in->speex = speex_resampler_init(in->config.channels, in->config.rate,
                                         in->requested_rate,
                                         SPEEX_RESAMPLER_QUALITY_DEFAULT,
                                         &ret);
        speex_resampler_reset_mem(in->speex);
        /* todo: allow for reallocing */
        in->buffer = malloc(RESAMPLER_BUFFER_SIZE);
        if(!in->buffer) {
            pcm_close(in->pcm);
            return -ENOMEM;
        }
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
    size_t size;

    /* return the number of bytes per period */
    pthread_mutex_lock(&in->lock);
    if (in->pcm)
        size = (size_t)pcm_get_buffer_size(in->pcm) *
                       audio_stream_frame_size((struct audio_stream*)stream) /
                       in->config.period_count;
    else
        size = 0;
    pthread_mutex_unlock(&in->lock);

    return size;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    return AUDIO_CHANNEL_IN_MONO;
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

    pthread_mutex_lock(&in->lock);
    if (!in->standby) {
        pcm_close(in->pcm);
        in->pcm = NULL;
        if (in->buffer)
            free(in->buffer);
        if (in->speex)
            speex_resampler_destroy(in->speex);
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
    return 0;
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

    if (ret == 0)
        ret = pcm_read(in->pcm, buffer, bytes);

    /* TODO: enable resample */

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
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;

    out->config = pcm_config_mm;

    out->pcm = pcm_open(0, PORT_MM, PCM_OUT, &out->config);
    if (!pcm_is_ready(out->pcm)) {
        LOGE("cannot open pcm_out driver: %s", pcm_get_error(out->pcm));
        pcm_close(out->pcm);
        ret = -ENOMEM;
        goto err_open;
    }

    out->speex = speex_resampler_init(2, 44100, 48000,
                                      SPEEX_RESAMPLER_QUALITY_DEFAULT, &ret);
    speex_resampler_reset_mem(out->speex);
    out->buffer = malloc(RESAMPLER_BUFFER_SIZE); /* todo: allow for reallocing */

    out->dev = ladev;

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

    free(out->buffer);
    speex_resampler_destroy(out->speex);
    pcm_close(out->pcm);
    free(stream);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    return -ENOSYS;
}

static char * adev_get_parameters(const struct audio_hw_device *dev,
                                  const char *keys)
{
    return NULL;
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    struct tuna_audio_device *adev = (struct tuna_audio_device *)dev;

    /* convert the float volume to something suitable for the RIL */
    if (adev->in_call) {
        int int_volume = (int)(volume * 5);
        ril_set_call_volume(adev->ril_client, SOUND_TYPE_VOICE, int_volume);
    }

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
    return 320;
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
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;

    in->requested_rate = *sample_rate;
    in->config.channels = popcount(*channel_mask);
    if ((in->config.channels) > 2 || (in->requested_rate == 0)) {
        ret = -EINVAL;
        goto err;
    }

    if (in->requested_rate <= 8000) {
        in->port = PORT_VX;
        memcpy(&in->config, &pcm_config_vx, sizeof(pcm_config_vx));
        in->config.rate = 8000;
    } else if (in->requested_rate <= 16000) {
        in->port = PORT_VX; /* use voice uplink */
        memcpy(&in->config, &pcm_config_vx, sizeof(pcm_config_vx));
        in->config.rate = 16000;
    } else {
        in->port = PORT_MM; /* use multimedia uplink */
        memcpy(&in->config, &pcm_config_mm, sizeof(pcm_config_mm));
        in->config.rate = 48000;
    }

    in->dev = ladev;
    in->standby = !!start_input_stream(in);

    *stream_in = &in->stream;
    return 0;

err:
    free(in);
    *stream_in = NULL;
    return ret;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                   struct audio_stream_in *stream)
{
    struct tuna_stream_in *in = (struct tuna_stream_in *)stream;

    in_standby(&stream->common);
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
    ril_close(adev->ril_handle, adev->ril_client);

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

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = 0;
    adev->device.common.module = (struct hw_module_t *) module;
    adev->device.common.close = adev_close;

    adev->device.get_supported_devices = adev_get_supported_devices;
    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    adev->mixer = mixer_open(0);
    if (!adev->mixer) {
        free(adev);
        return -ENOMEM;
    }

    /* Set the default route before the PCM stream is opened */
    set_route_by_array(adev->mixer, defaults, 1);
    adev->mode = AUDIO_MODE_NORMAL;
    adev->out_device = AUDIO_DEVICE_OUT_SPEAKER;
    select_output_device(adev);

    adev->pcm_modem_dl = NULL;
    adev->pcm_modem_ul = NULL;

    /* RIL */
    ril_open(&adev->ril_handle, &adev->ril_client);

    *device = &adev->device.common;

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
