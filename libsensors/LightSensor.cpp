/*
 * Copyright (C) 2011 Samsung
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <cutils/log.h>
#include <pthread.h>

#include "LightSensor.h"

LightSensor::LightSensor()
    : SamsungSensorBase(NULL, "lightsensor-level", ABS_MISC)
{
    mPendingEvent.sensor = ID_L;
    mPendingEvent.type = SENSOR_TYPE_LIGHT;
    mPreviousLight = -1;
}

bool LightSensor::handleEvent(input_event const *event) {
    mPendingEvent.light = indexToValue(event->value);
    if (mPendingEvent.light != mPreviousLight) {
        mPreviousLight = mPendingEvent.light;
        return true;
    }
    return false;
}

float LightSensor::indexToValue(size_t index) const {
    /* Driver gives a rolling average adc value.  We convert it lux levels. */
    static const struct adcToLux {
        size_t adc_value;
        float  lux_value;
    } adcToLux[] = {
        {  150,   10.0 },  /* from    0 -  150 adc, we map to    10.0 lux */
        {  800,  160.0 },  /* from  151 -  800 adc, we map to   160.0 lux */
        {  900,  225.0 },  /* from  801 -  900 adc, we map to   225.0 lux */
        { 1000,  320.0 },  /* from  901 - 1000 adc, we map to   320.0 lux */
        { 1200,  640.0 },  /* from 1001 - 1200 adc, we map to   640.0 lux */
        { 1400, 1280.0 },  /* from 1201 - 1400 adc, we map to  1280.0 lux */
        { 1600, 2600.0 },  /* from 1401 - 1600 adc, we map to  2600.0 lux */
        { 4095, 10240.0 }, /* from 1601 - 4095 adc, we map to 10240.0 lux */
    };
    size_t i;
    for (i = 0; i < ARRAY_SIZE(adcToLux); i++) {
        if (index < adcToLux[i].adc_value) {
            return adcToLux[i].lux_value;
        }
    }
    return adcToLux[ARRAY_SIZE(adcToLux)-1].lux_value;
}
