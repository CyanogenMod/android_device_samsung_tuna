/*
 * Copyright (C) 2011 Invensense, Inc.
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

//#define LOG_NDEBUG 0
//see also the EXTRA_VERBOSE define, below

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <float.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <stdlib.h>
#include <sys/select.h>
#include <dlfcn.h>
#include <pthread.h>

#include <cutils/log.h>
#include <utils/KeyedVector.h>

#include "MPLSensor.h"

#include "math.h"
#include "ml.h"
#include "mlFIFO.h"
#include "mlsl.h"
#include "mlos.h"
#include "ml_stored_data.h"
#include "mldl_cfg.h"
#include "mldl.h"

#include "mpu.h"
#include "accel.h"
#include "compass.h"
#include "kernel/timerirq.h"
#include "kernel/mpuirq.h"
#include "kernel/slaveirq.h"

extern "C" {
#include "mlsupervisor.h"
}

#include "mlcontrol.h"

#define EXTRA_VERBOSE (0)
//#define FUNC_LOG ALOGV("%s", __PRETTY_FUNCTION__)
#define FUNC_LOG
#define VFUNC_LOG ALOGV_IF(EXTRA_VERBOSE, "%s", __PRETTY_FUNCTION__)
/* this mask must turn on only the sensors that are present and managed by the MPL */
#define ALL_MPL_SENSORS_NP (INV_THREE_AXIS_ACCEL | INV_THREE_AXIS_COMPASS | INV_THREE_AXIS_GYRO)

#define CALL_MEMBER_FN(pobject,ptrToMember)  ((pobject)->*(ptrToMember))

/******************************************/

static unsigned long long irq_timestamp = 0;
/* ***************************************************************************
 * MPL interface misc.
 */
//static pointer to the object that will handle callbacks
static MPLSensor* gMPLSensor = NULL;

/* we need to pass some callbacks to the MPL.  The mpl is a C library, so
 * wrappers for the C++ callback implementations are required.
 */
extern "C" {
//callback wrappers go here
void mot_cb_wrapper(uint16_t val)
{
    if (gMPLSensor) {
        gMPLSensor->cbOnMotion(val);
    }
    ALOGI("%s: Got motion callback! Value: %hu", __func__, val);
}

void procData_cb_wrapper()
{
    if (gMPLSensor) {
        gMPLSensor->cbProcData();
    }
}

} //end of extern C

void setCallbackObject(MPLSensor* gbpt)
{
    gMPLSensor = gbpt;
}


/*****************************************************************************
 * sensor class implementation
 */

#define GY_ENABLED ((1<<ID_GY) & enabled_sensors)
#define A_ENABLED  ((1<<ID_A)  & enabled_sensors)
#define O_ENABLED  ((1<<ID_O)  & enabled_sensors)
#define M_ENABLED  ((1<<ID_M)  & enabled_sensors)
#define LA_ENABLED ((1<<ID_LA) & enabled_sensors)
#define GR_ENABLED ((1<<ID_GR) & enabled_sensors)
#define RV_ENABLED ((1<<ID_RV) & enabled_sensors)

MPLSensor::MPLSensor() :
    SensorBase(NULL),
            mNewData(0),
            mDmpStarted(false),
            mMasterSensorMask(INV_ALL_SENSORS),
            mLocalSensorMask(ALL_MPL_SENSORS_NP),
            mCurFifoRate(-1), mHaveGoodMpuCal(false), mHaveGoodCompassCal(false),
            mUseTimerIrqAccel(false), mUsetimerIrqCompass(true),
            mUseTimerirq(false),
            mEnabled(0), mPendingMask(0),
            mForceSleep(false), mNineAxisEnabled(false)
{
    FUNC_LOG;
    int mpu_int_fd;
    char *port = NULL;

    ALOGV_IF(EXTRA_VERBOSE, "MPLSensor constructor: numSensors = %d", numSensors);

    pthread_mutex_init(&mMplMutex, NULL);

    for (size_t i = 0; i < ARRAY_SIZE(mPollFds); i++) {
        mPollFds[i].fd = -1;
        mPollFds[i].events = 0;
    }

    pthread_mutex_lock(&mMplMutex);

    mpu_int_fd = open("/dev/mpuirq", O_RDWR);
    if (mpu_int_fd != -1) {
        fcntl(mpu_int_fd, F_SETFL, O_NONBLOCK);
        mIrqFds.add(MPUIRQ_FD, mpu_int_fd);
        mPollFds[MPUIRQ_FD].fd = mpu_int_fd;
        mPollFds[MPUIRQ_FD].events = POLLIN;
    } else {
        ALOGE("could not open the mpu irq device node");
    }

    accel_fd = open("/dev/accelirq", O_RDWR);
    if (accel_fd != -1) {
        fcntl(accel_fd, F_SETFL, O_NONBLOCK);
        mIrqFds.add(ACCELIRQ_FD, accel_fd);
        mPollFds[ACCELIRQ_FD].fd = accel_fd;
        mPollFds[ACCELIRQ_FD].events = POLLIN;
    } else {
        ALOGE("could not open the accel irq device node");
    }

    timer_fd = open("/dev/timerirq", O_RDWR);
    if (timer_fd != -1) {
        fcntl(timer_fd, F_SETFL, O_NONBLOCK);
        mIrqFds.add(TIMERIRQ_FD, timer_fd);
        mPollFds[TIMERIRQ_FD].fd = timer_fd;
        mPollFds[TIMERIRQ_FD].events = POLLIN;
        if (accel_fd == -1) {
            // no accel irq, but timer is available
            mUseTimerIrqAccel = true;
            ALOGW("using timer irq for accel");
        }
    } else {
        ALOGE("could not open the timer irq device node");
    }

    data_fd = mpu_int_fd;

    memset(mPendingEvents, 0, sizeof(mPendingEvents));

    mPendingEvents[RotationVector].version = sizeof(sensors_event_t);
    mPendingEvents[RotationVector].sensor = ID_RV;
    mPendingEvents[RotationVector].type = SENSOR_TYPE_ROTATION_VECTOR;

    mPendingEvents[LinearAccel].version = sizeof(sensors_event_t);
    mPendingEvents[LinearAccel].sensor = ID_LA;
    mPendingEvents[LinearAccel].type = SENSOR_TYPE_LINEAR_ACCELERATION;

    mPendingEvents[Gravity].version = sizeof(sensors_event_t);
    mPendingEvents[Gravity].sensor = ID_GR;
    mPendingEvents[Gravity].type = SENSOR_TYPE_GRAVITY;

    mPendingEvents[Gyro].version = sizeof(sensors_event_t);
    mPendingEvents[Gyro].sensor = ID_GY;
    mPendingEvents[Gyro].type = SENSOR_TYPE_GYROSCOPE;

    mPendingEvents[Accelerometer].version = sizeof(sensors_event_t);
    mPendingEvents[Accelerometer].sensor = ID_A;
    mPendingEvents[Accelerometer].type = SENSOR_TYPE_ACCELEROMETER;

    mPendingEvents[MagneticField].version = sizeof(sensors_event_t);
    mPendingEvents[MagneticField].sensor = ID_M;
    mPendingEvents[MagneticField].type = SENSOR_TYPE_MAGNETIC_FIELD;
    mPendingEvents[MagneticField].magnetic.status = SENSOR_STATUS_ACCURACY_HIGH;

    mPendingEvents[Orientation].version = sizeof(sensors_event_t);
    mPendingEvents[Orientation].sensor = ID_O;
    mPendingEvents[Orientation].type = SENSOR_TYPE_ORIENTATION;
    mPendingEvents[Orientation].orientation.status = SENSOR_STATUS_ACCURACY_HIGH;

    mHandlers[RotationVector] = &MPLSensor::rvHandler;
    mHandlers[LinearAccel] = &MPLSensor::laHandler;
    mHandlers[Gravity] = &MPLSensor::gravHandler;
    mHandlers[Gyro] = &MPLSensor::gyroHandler;
    mHandlers[Accelerometer] = &MPLSensor::accelHandler;
    mHandlers[MagneticField] = &MPLSensor::compassHandler;
    mHandlers[Orientation] = &MPLSensor::orienHandler;

    for (int i = 0; i < numSensors; i++)
        mDelays[i] = 30000000LLU; // 30 ms by default

    if (inv_serial_start(port) != INV_SUCCESS) {
        ALOGE("Fatal Error : could not open MPL serial interface");
    }

    //initialize library parameters
    initMPL();

    //setup the FIFO contents
    setupFIFO();

    pthread_mutex_unlock(&mMplMutex);
}

MPLSensor::~MPLSensor()
{
    FUNC_LOG;
    pthread_mutex_lock(&mMplMutex);
    if (inv_dmp_stop() != INV_SUCCESS) {
        ALOGW("Error: could not stop the DMP correctly.\n");
    }

    if (inv_dmp_close() != INV_SUCCESS) {
        ALOGW("Error: could not close the DMP");
    }

    if (inv_serial_stop() != INV_SUCCESS) {
        ALOGW("Error : could not close the serial port");
    }
    pthread_mutex_unlock(&mMplMutex);
    pthread_mutex_destroy(&mMplMutex);
}

/* clear any data from our various filehandles */
void MPLSensor::clearIrqData(bool* irq_set)
{
    unsigned int i;
    int nread;
    struct mpuirq_data irqdata;

    poll(mPollFds, ARRAY_SIZE(mPollFds), 0); //check which ones need to be cleared

    for (i = 0; i < ARRAY_SIZE(mPollFds); i++) {
        int cur_fd = mPollFds[i].fd;
        if (mPollFds[i].revents & POLLIN) {
            nread = read(cur_fd, &irqdata, sizeof(irqdata));
            if (nread > 0) {
                irq_set[i] = true;
                irq_timestamp = irqdata.irqtime;
            }
        }
        mPollFds[i].revents = 0;
    }
}

/* set the power states of the various sensors based on the bits set in the
 * enabled_sensors parameter.
 * this function modifies globalish state variables.  It must be called with the mMplMutex held. */
void MPLSensor::setPowerStates(int enabled_sensors)
{
    FUNC_LOG;
    bool irq_set[5] = { false, false, false, false, false };

    if (LA_ENABLED || GR_ENABLED || RV_ENABLED || O_ENABLED) {
        mLocalSensorMask = ALL_MPL_SENSORS_NP;
    } else if (!A_ENABLED && !M_ENABLED && !GY_ENABLED) {
        mLocalSensorMask = 0;
    } else {
        if (GY_ENABLED) {
            mLocalSensorMask |= INV_THREE_AXIS_GYRO;
        } else {
            mLocalSensorMask &= ~INV_THREE_AXIS_GYRO;
        }

        if (A_ENABLED) {
            mLocalSensorMask |= (INV_THREE_AXIS_ACCEL);
        } else {
            mLocalSensorMask &= ~(INV_THREE_AXIS_ACCEL);
        }

        if (M_ENABLED) {
            mLocalSensorMask |= INV_THREE_AXIS_COMPASS;
        } else {
            mLocalSensorMask &= ~(INV_THREE_AXIS_COMPASS);
        }
    }

    //record the new sensor state
    inv_error_t rv;

    unsigned long sen_mask = mLocalSensorMask & mMasterSensorMask;

    bool changing_sensors = ((inv_get_dl_config()->requested_sensors
            != sen_mask) && (sen_mask != 0));
    bool restart = (!mDmpStarted) && (sen_mask != 0);

    if (changing_sensors || restart) {

        ALOGV_IF(EXTRA_VERBOSE, "cs:%d rs:%d ", changing_sensors, restart);

        if (mDmpStarted) {
            inv_dmp_stop();
            clearIrqData(irq_set);
            mDmpStarted = false;
        }

        if (sen_mask != inv_get_dl_config()->requested_sensors) {
            rv = inv_set_mpu_sensors(sen_mask);
            ALOGE_IF(rv != INV_SUCCESS,
                    "error: unable to set MPL sensor power states (sens=%ld retcode = %d)",
                    sen_mask, rv);
        }

        if (((mUsetimerIrqCompass && (sen_mask == INV_THREE_AXIS_COMPASS))
                || (mUseTimerIrqAccel && (sen_mask & INV_THREE_AXIS_ACCEL)))
                && ((sen_mask & INV_DMP_PROCESSOR) == 0)) {
            ALOGV_IF(EXTRA_VERBOSE, "Allowing TimerIRQ");
            mUseTimerirq = true;
        } else {
            if (mUseTimerirq) {
                ioctl(mIrqFds.valueFor(TIMERIRQ_FD), TIMERIRQ_STOP, 0);
                clearIrqData(irq_set);
            }
            ALOGV_IF(EXTRA_VERBOSE, "Not allowing TimerIRQ");
            mUseTimerirq = false;
        }

        if (!mDmpStarted) {
            if (mHaveGoodMpuCal || mHaveGoodCompassCal) {
                rv = inv_store_calibration();
                ALOGE_IF(rv != INV_SUCCESS,
                        "error: unable to store MPL calibration file");
                mHaveGoodMpuCal = false;
                mHaveGoodCompassCal = false;
            }
            rv = inv_dmp_start();
            ALOGE_IF(rv != INV_SUCCESS, "unable to start dmp");
            mDmpStarted = true;
        }
    }

    //check if we should stop the DMP
    if (mDmpStarted && (sen_mask == 0)) {
        rv = inv_dmp_stop();
        ALOGE_IF(rv != INV_SUCCESS, "error: unable to stop DMP (retcode = %d)",
                rv);
        if (mUseTimerirq) {
            ioctl(mIrqFds.valueFor(TIMERIRQ_FD), TIMERIRQ_STOP, 0);
        }
        clearIrqData(irq_set);

        mDmpStarted = false;
        mCurFifoRate = -1;
    }
}

/**
 * container function for all the calls we make once to set up the MPL.
 */
void MPLSensor::initMPL()
{
    FUNC_LOG;
    inv_error_t result;
    unsigned short bias_update_mask = 0xFFFF;

    if (inv_dmp_open() != INV_SUCCESS) {
        ALOGE("Fatal Error : could not open DMP correctly.\n");
    }

    result = inv_set_mpu_sensors(ALL_MPL_SENSORS_NP); //default to all sensors, also makes 9axis enable work
    ALOGE_IF(result != INV_SUCCESS,
            "Fatal Error : could not set enabled sensors.");

    if (inv_load_calibration() != INV_SUCCESS) {
        ALOGE("could not open MPL calibration file");
    }

    //check for the 9axis fusion library: if available load it and start 9x
    void* h_dmp_lib = dlopen("libinvensense_mpl.so", RTLD_NOW);
    if (h_dmp_lib) {
        const char* error;
        error = dlerror();
        inv_error_t (*fp_inv_enable_9x_fusion)() =
              (inv_error_t(*)()) dlsym(h_dmp_lib, "inv_enable_9x_fusion");
        if ((error = dlerror()) != NULL) {
            ALOGE("%s %s", error, "inv_enable_9x_fusion");
        } else if ((*fp_inv_enable_9x_fusion)() != INV_SUCCESS) {
            ALOGE( "Warning : 9 axis sensor fusion not available "
                  "- No compass detected.\n");
        } else {
            /*  9axis is loaded and enabled                            */
            /*  this variable is used for coming up with sensor list   */
            mNineAxisEnabled = true;
        }
    } else {
        const char* error = dlerror();
        ALOGE("libinvensense_mpl.so not found, 9x sensor fusion disabled (%s)",error);
    }

    if (inv_set_bias_update(bias_update_mask) != INV_SUCCESS) {
        ALOGE("Error : Bias update function could not be set.\n");
    }

    if (inv_set_motion_interrupt(1) != INV_SUCCESS) {
        ALOGE("Error : could not set motion interrupt");
    }

    if (inv_set_fifo_interrupt(1) != INV_SUCCESS) {
        ALOGE("Error : could not set fifo interrupt");
    }

    result = inv_set_fifo_rate(6);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_set_fifo_rate returned %d\n", result);
    }

    setupCallbacks();
}

/** setup the fifo contents.
 */
void MPLSensor::setupFIFO()
{
    FUNC_LOG;
    inv_error_t result;

    result = inv_send_accel(INV_ALL, INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_accel returned %d\n", result);
    }

    result = inv_send_quaternion(INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_quaternion returned %d\n", result);
    }

    result = inv_send_linear_accel(INV_ALL, INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_linear_accel returned %d\n", result);
    }

    result = inv_send_linear_accel_in_world(INV_ALL, INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_linear_accel_in_world returned %d\n",
             result);
    }

    result = inv_send_gravity(INV_ALL, INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_gravity returned %d\n", result);
    }

    result = inv_send_gyro(INV_ALL, INV_32_BIT);
    if (result != INV_SUCCESS) {
        ALOGE("Fatal error: inv_send_gyro returned %d\n", result);
    }
}

/**
 *  set up the callbacks that we use in all cases (outside of gestures, etc)
 */
void MPLSensor::setupCallbacks()
{
    FUNC_LOG;
    if (inv_set_motion_callback(mot_cb_wrapper) != INV_SUCCESS) {
        ALOGE("Error : Motion callback could not be set.\n");
    }

    if (inv_set_fifo_processed_callback(procData_cb_wrapper) != INV_SUCCESS) {
        ALOGE("Error : Processed data callback could not be set.");
    }
}

/**
 * handle the motion/no motion output from the MPL.
 */
void MPLSensor::cbOnMotion(uint16_t val)
{
    FUNC_LOG;
    //after the first no motion, the gyro should be calibrated well
    if (val == 2) {
        if ((inv_get_dl_config()->requested_sensors) & INV_THREE_AXIS_GYRO) {
            // if gyros are on and we got a no motion, set a flag
            // indicating that the cal file can be written.
            mHaveGoodMpuCal = true;
        }
    }
}


void MPLSensor::cbProcData()
{
    mNewData = 1;
}

// these handlers transform mpl data into one of the Android sensor types.
// scaling and coordinate transforms should be done in the handlers

void MPLSensor::gyroHandler(sensors_event_t* s, uint32_t* pending_mask,
                             int index)
{
    VFUNC_LOG;
    inv_error_t res;
    res = inv_get_gyro_float(s->gyro.v);
    s->gyro.v[0] = s->gyro.v[0] * M_PI / 180.0;
    s->gyro.v[1] = s->gyro.v[1] * M_PI / 180.0;
    s->gyro.v[2] = s->gyro.v[2] * M_PI / 180.0;
    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
}

void MPLSensor::accelHandler(sensors_event_t* s, uint32_t* pending_mask,
                              int index)
{
    //VFUNC_LOG;
    inv_error_t res;
    res = inv_get_accel_float(s->acceleration.v);
    s->acceleration.v[0] = s->acceleration.v[0] * 9.81;
    s->acceleration.v[1] = s->acceleration.v[1] * 9.81;
    s->acceleration.v[2] = s->acceleration.v[2] * 9.81;
    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
}

int MPLSensor::estimateCompassAccuracy()
{
    inv_error_t res;
    int rv;

    res = inv_get_compass_accuracy(&rv);
    if (rv >= SENSOR_STATUS_ACCURACY_MEDIUM) {
         mHaveGoodCompassCal = true;
    }
    ALOGE_IF(res != INV_SUCCESS, "error returned from inv_get_compass_accuracy");

    return rv;
}

void MPLSensor::compassHandler(sensors_event_t* s, uint32_t* pending_mask,
                                int index)
{
    VFUNC_LOG;
    inv_error_t res;

    res = inv_get_magnetometer_float(s->magnetic.v);

    if (res != INV_SUCCESS) {
        ALOGW("compass_handler inv_get_magnetometer_float returned %d", res);
    }

    s->magnetic.status = estimateCompassAccuracy();

    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
}

void MPLSensor::rvHandler(sensors_event_t* s, uint32_t* pending_mask,
                           int index)
{
    VFUNC_LOG;
    float quat[4];
    float norm = 0;
    inv_error_t res;

    res = inv_get_quaternion_float(quat);

    if (res != INV_SUCCESS) {
        *pending_mask &= ~(1 << index);
        return;
    } else {
        *pending_mask |= (1 << index);
    }

    norm = quat[1] * quat[1] + quat[2] * quat[2] + quat[3] * quat[3]
            + FLT_EPSILON;

    if (norm > 1.0f) {
        //renormalize
        // TODO: Do we need to renormalize quat[0] as well?
        norm = sqrtf(norm);
        float inv_norm = 1.0f / norm;
        quat[1] = quat[1] * inv_norm;
        quat[2] = quat[2] * inv_norm;
        quat[3] = quat[3] * inv_norm;
    }

    if (quat[0] < 0.0) {
        quat[1] = -quat[1];
        quat[2] = -quat[2];
        quat[3] = -quat[3];
        quat[0] = -quat[0];
    }

    s->data[0] = quat[1]; // "x * sin(θ/2)"
    s->data[1] = quat[2]; // "y * sin(θ/2)"
    s->data[2] = quat[3]; // "z * sin(θ/2)"
    s->data[3] = quat[0]; // "    cos(θ/2)"
    s->data[4] = -1;      // "estimated heading Accuracy (in radians) (-1 if unavailable)"
}

void MPLSensor::laHandler(sensors_event_t* s, uint32_t* pending_mask,
                           int index)
{
    VFUNC_LOG;
    inv_error_t res;
    res = inv_get_linear_accel_float(s->gyro.v);
    s->gyro.v[0] *= 9.81;
    s->gyro.v[1] *= 9.81;
    s->gyro.v[2] *= 9.81;
    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
}

void MPLSensor::gravHandler(sensors_event_t* s, uint32_t* pending_mask,
                             int index)
{
    VFUNC_LOG;
    inv_error_t res;
    res = inv_get_gravity_float(s->gyro.v);
    s->gyro.v[0] *= 9.81;
    s->gyro.v[1] *= 9.81;
    s->gyro.v[2] *= 9.81;
    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
}

void MPLSensor::calcOrientationSensor(float *R, float *values)
{
    float tmp;

    //Azimuth
    if ((R[7] > 0.7071067f) || ((R[8] < 0) && (fabs(R[7]) > fabs(R[6])))) {
        values[0] = (float) atan2f(-R[3], R[0]);
    } else {
        values[0] = (float) atan2f(R[1], R[4]);
    }
    values[0] *= 57.295779513082320876798154814105f;
    if (values[0] < 0) {
        values[0] += 360.0f;
    }
    //Pitch
    tmp = R[7];
    if (tmp > 1.0f)
        tmp = 1.0f;
    if (tmp < -1.0f)
        tmp = -1.0f;
    values[1] = -asinf(tmp) * 57.295779513082320876798154814105f;
    if (R[8] < 0) {
        values[1] = 180.0f - values[1];
    }
    if (values[1] > 180.0f) {
        values[1] -= 360.0f;
    }
    //Roll
    if ((R[7] > 0.7071067f)) {
        values[2] = (float) atan2f(R[6], R[7]);
    } else {
        values[2] = (float) atan2f(R[6], R[8]);
    }

    values[2] *= 57.295779513082320876798154814105f;
    if (values[2] > 90.0f) {
        values[2] = 180.0f - values[2];
    }
    if (values[2] < -90.0f) {
        values[2] = -180.0f - values[2];
    }
}

void MPLSensor::orienHandler(sensors_event_t* s, uint32_t* pending_mask,
                              int index) //note that this is the handler for the android 'orientation' sensor, not the mpl orientation output
{
    VFUNC_LOG;
    inv_error_t res;
    float rot_mat[9];

    res = inv_get_rot_mat_float(rot_mat);

    calcOrientationSensor(rot_mat, s->orientation.v);

    s->orientation.status = estimateCompassAccuracy();

    if (res == INV_SUCCESS)
        *pending_mask |= (1 << index);
    else
        ALOGW("orienHandler: data not valid (%d)", (int) res);
}

int MPLSensor::enable(int32_t handle, int en)
{
    FUNC_LOG;

    int what = handleToDriver(handle);

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    int newState = en ? 1 : 0;
    int err = 0;

    pthread_mutex_lock(&mMplMutex);
    if ((uint32_t(newState) << what) != (mEnabled & (1 << what))) {
        short flags = newState;
        mEnabled &= ~(1 << what);
        mEnabled |= (uint32_t(flags) << what);
        ALOGV_IF(EXTRA_VERBOSE, "mEnabled = %x", mEnabled);
        setPowerStates(mEnabled);
        pthread_mutex_unlock(&mMplMutex);
        if (!newState)
            update_delay();
        return err;
    }
    pthread_mutex_unlock(&mMplMutex);
    return err;
}

int MPLSensor::setDelay(int32_t handle, int64_t ns)
{
    FUNC_LOG;
    ALOGV_IF(EXTRA_VERBOSE,
            " setDelay handle: %d rate %d", handle, (int) (ns / 1000000LL));

    int what = handleToDriver(handle);

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    if (ns < 0)
        return -EINVAL;

    pthread_mutex_lock(&mMplMutex);
    mDelays[what] = ns;
    pthread_mutex_unlock(&mMplMutex);
    return update_delay();
}

int MPLSensor::update_delay()
{
    FUNC_LOG;
    int rv = 0;
    bool irq_set[5];

    pthread_mutex_lock(&mMplMutex);

    if (mEnabled) {
        uint64_t wanted = -1LLU;
        for (int i = 0; i < numSensors; i++) {
            if (mEnabled & (1 << i)) {
                uint64_t ns = mDelays[i];
                wanted = wanted < ns ? wanted : ns;
            }
        }

        //Limit all rates to 100Hz max. 100Hz = 10ms = 10000000ns
        if (wanted < 10000000LLU) {
            wanted = 10000000LLU;
        }

        int rate = ((wanted) / 5000000LLU) - ((wanted % 5000000LLU == 0) ? 1
                                                                         : 0); //mpu fifo rate is in increments of 5ms
        if (rate == 0) //KLP disallow fifo rate 0
            rate = 1;

        if (rate != mCurFifoRate) {
            inv_error_t res; // = inv_dmp_stop();
            res = inv_set_fifo_rate(rate);
            ALOGE_IF(res != INV_SUCCESS, "error setting FIFO rate");

            mCurFifoRate = rate;
            rv = (res == INV_SUCCESS);
        }

        if ((inv_get_dl_config()->requested_sensors & INV_DMP_PROCESSOR) == 0) {
            if (mUseTimerirq) {
                ioctl(mIrqFds.valueFor(TIMERIRQ_FD), TIMERIRQ_STOP, 0);
                clearIrqData(irq_set);
                if (inv_get_dl_config()->requested_sensors
                        == INV_THREE_AXIS_COMPASS) {
                    ioctl(mIrqFds.valueFor(TIMERIRQ_FD), TIMERIRQ_START,
                          (unsigned long) (wanted / 1000000LLU));
                    ALOGV_IF(EXTRA_VERBOSE, "updated timerirq period to %d",
                            (int) (wanted / 1000000LLU));
                } else {
                    ioctl(mIrqFds.valueFor(TIMERIRQ_FD), TIMERIRQ_START,
                          (unsigned long) inv_get_sample_step_size_ms());
                    ALOGV_IF(EXTRA_VERBOSE, "updated timerirq period to %d",
                            (int) inv_get_sample_step_size_ms());
                }
            }
        }

    }
    pthread_mutex_unlock(&mMplMutex);
    return rv;
}

int MPLSensor::readEvents(sensors_event_t* data, int count)
{
    //VFUNC_LOG;
    bool irq_set[5] = { false, false, false, false, false };
    inv_error_t rv;
    if (count < 1)
        return -EINVAL;
    int numEventReceived = 0;

    clearIrqData(irq_set);

    pthread_mutex_lock(&mMplMutex);
    if (mDmpStarted) {
        rv = inv_update_data();
        ALOGE_IF(rv != INV_SUCCESS, "inv_update_data error (code %d)", (int) rv);
    } else {
        //probably just one extra read after shutting down
        ALOGV_IF(EXTRA_VERBOSE,
                "MPLSensor::readEvents called, but there's nothing to do.");
    }

    pthread_mutex_unlock(&mMplMutex);

    if (!mNewData) {
        ALOGV_IF(EXTRA_VERBOSE, "no new data");
        return 0;
    }
    mNewData = 0;

    /* google timestamp */
    pthread_mutex_lock(&mMplMutex);
    for (int i = 0; i < numSensors; i++) {
        if (mEnabled & (1 << i)) {
            CALL_MEMBER_FN(this,mHandlers[i])(mPendingEvents + i,
                                              &mPendingMask, i);
            mPendingEvents[i].timestamp = irq_timestamp;
        }
    }

    for (int j = 0; count && mPendingMask && j < numSensors; j++) {
        if (mPendingMask & (1 << j)) {
            mPendingMask &= ~(1 << j);
            if (mEnabled & (1 << j)) {
                *data++ = mPendingEvents[j];
                count--;
                numEventReceived++;
            }
        }
    }

    pthread_mutex_unlock(&mMplMutex);
    return numEventReceived;
}

int MPLSensor::getFd() const
{
    return data_fd;
}

int MPLSensor::getAccelFd() const
{
    return accel_fd;
}

int MPLSensor::getTimerFd() const
{
    return timer_fd;
}

int MPLSensor::getPowerFd() const
{
    return (uintptr_t) inv_get_serial_handle();
}

void MPLSensor::handlePowerEvent()
{
    VFUNC_LOG;
    mpuirq_data irqd;

    int fd = (uintptr_t) inv_get_serial_handle();
    read(fd, &irqd, sizeof(irqd));

    if (irqd.data == MPU_PM_EVENT_SUSPEND_PREPARE) {
        //going to sleep
        sleepEvent();
    } else if (irqd.data == MPU_PM_EVENT_POST_SUSPEND) {
        //waking up
        wakeEvent();
    }

    ioctl(fd, MPU_PM_EVENT_HANDLED, 0);
}

void MPLSensor::sleepEvent()
{
    VFUNC_LOG;
    pthread_mutex_lock(&mMplMutex);
    if (mEnabled != 0) {
        mForceSleep = true;
        mOldEnabledMask = mEnabled;
        setPowerStates(0);
    }
    pthread_mutex_unlock(&mMplMutex);
}

void MPLSensor::wakeEvent()
{
    VFUNC_LOG;
    pthread_mutex_lock(&mMplMutex);
    if (mForceSleep) {
        setPowerStates((mOldEnabledMask | mEnabled));
        mForceSleep = false;
    }
    pthread_mutex_unlock(&mMplMutex);
}

/** fill in the sensor list based on which sensors are configured.
 *  return the number of configured sensors.
 *  parameter list must point to a memory region of at least 7*sizeof(sensor_t)
 *  parameter len gives the length of the buffer pointed to by list
 */
int MPLSensor::populateSensorList(struct sensor_t *list, size_t len)
{
    int numsensors = 7;

    if (len < numsensors * sizeof(sensor_t)) {
        ALOGE("sensor list too small, not populating.");
        return 0;
    }

    if (!mNineAxisEnabled) {
        /* no 9-axis sensors, zero fill that part of the list */
        numsensors = 3;
        memset(list + numsensors, 0, (7 - numsensors) * sizeof(struct sensor_t));
    }

    return numsensors;
}
