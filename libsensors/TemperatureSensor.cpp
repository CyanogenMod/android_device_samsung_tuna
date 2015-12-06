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

#include "TemperatureSensor.h"

#define TEMPERATURE_CELCIUS (1.0f/10.0f)

TemperatureSensor::TemperatureSensor()
    : SamsungSensorBase("barometer", ABS_MISC)
{
    mPendingEvent.sensor = ID_T;
    mPendingEvent.type = SENSOR_TYPE_AMBIENT_TEMPERATURE;
}

bool TemperatureSensor::handleEvent(input_event const *event) {
    mPendingEvent.temperature = event->value * TEMPERATURE_CELCIUS;
    return true;
}
