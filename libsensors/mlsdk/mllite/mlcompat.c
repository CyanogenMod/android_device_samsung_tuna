/**
 * mlcompat! where symbols referenced by libinvensense_mpl.so go to die. :(
 * They may one day rise again, e.g. inv_get_motion_state looks fun, but
 * this helps keep track of where things are used and by whom are they used.
 */
#include "mlcompat.h"
#include "ml.h"
#include "mlMathFunc.h"
#include "mlmath.h"

inv_error_t inv_pressure_supervisor(void)
{
    return INV_SUCCESS;
}



/**
 *  @brief  inv_get_motion_state is used to determine if the device is in
 *          a 'motion' or 'no motion' state.
 *          inv_get_motion_state returns INV_MOTION of the device is moving,
 *          or INV_NO_MOTION if the device is not moving.
 *
 *  @pre    inv_dmp_open()
 *          @ifnot MPL_MF
 *              or inv_open_low_power_pedometer()
 *              or inv_eis_open_dmp()
 *          @endif
 *          and inv_dmp_start()
 *          must have been called.
 *
 *  @return INV_SUCCESS if successful or Non-zero error code otherwise.
 */
int inv_get_motion_state(void)
{
    return inv_obj.motion_state;
}



/**
 * Performs one iteration of the filter, generating a new y(0)
 *         1     / N                /  N             \\
 * y(0) = ---- * |SUM b(k) * x(k) - | SUM a(k) * y(k)|| for N = length
 *        a(0)   \k=0               \ k=1            //
 *
 * The filters A and B should be (sizeof(long) * state->length).
 * The state variables x and y should be (sizeof(long) * (state->length - 1))
 *
 * The state variables x and y should be initialized prior to the first call
 *
 * @param state Contains the length of the filter, filter coefficients and
 *              filter state variables x and y.
 * @param x New input into the filter.
 */
void inv_filter_long(struct filter_long *state, long x)
{
    const long *b = state->b;
    const long *a = state->a;
    long length = state->length;
    long long tmp;
    int ii;

    /* filter */
    tmp = (long long)x *(b[0]);
    for (ii = 0; ii < length - 1; ii++) {
        tmp += ((long long)state->x[ii] * (long long)(b[ii + 1]));
    }
    for (ii = 0; ii < length - 1; ii++) {
        tmp -= ((long long)state->y[ii] * (long long)(a[ii + 1]));
    }
    tmp /= (long long)a[0];

    /* Delay */
    for (ii = length - 2; ii > 0; ii--) {
        state->y[ii] = state->y[ii - 1];
        state->x[ii] = state->x[ii - 1];
    }
    /* New values */
    state->y[0] = (long)tmp;
    state->x[0] = x;
}

void inv_q_multf(const float *q1, const float *q2, float *qProd)
{
    qProd[0] = (q1[0] * q2[0] - q1[1] * q2[1] - q1[2] * q2[2] - q1[3] * q2[3]);
    qProd[1] = (q1[0] * q2[1] + q1[1] * q2[0] + q1[2] * q2[3] - q1[3] * q2[2]);
    qProd[2] = (q1[0] * q2[2] - q1[1] * q2[3] + q1[2] * q2[0] + q1[3] * q2[1]);
    qProd[3] = (q1[0] * q2[3] + q1[1] * q2[2] - q1[2] * q2[1] + q1[3] * q2[0]);
}

void inv_q_addf(float *q1, float *q2, float *qSum)
{
    qSum[0] = q1[0] + q2[0];
    qSum[1] = q1[1] + q2[1];
    qSum[2] = q1[2] + q2[2];
    qSum[3] = q1[3] + q2[3];
}

void inv_q_normalizef(float *q)
{
    float normSF = 0;
    float xHalf = 0;
    normSF = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (normSF < 2) {
        xHalf = 0.5f * normSF;
        normSF = normSF * (1.5f - xHalf * normSF * normSF);
        normSF = normSF * (1.5f - xHalf * normSF * normSF);
        normSF = normSF * (1.5f - xHalf * normSF * normSF);
        normSF = normSF * (1.5f - xHalf * normSF * normSF);
        q[0] *= normSF;
        q[1] *= normSF;
        q[2] *= normSF;
        q[3] *= normSF;
    } else {
        q[0] = 1.0;
        q[1] = 0.0;
        q[2] = 0.0;
        q[3] = 0.0;
    }
    normSF = (q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
}

/** Performs a length 4 vector normalization with a square root.
* @param[in,out] vector to normalize. Returns [1,0,0,0] is magnitude is zero.
*/
void inv_q_norm4(float *q)
{
    float mag;
    mag = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (mag) {
        q[0] /= mag;
        q[1] /= mag;
        q[2] /= mag;
        q[3] /= mag;
    } else {
        q[0] = 1.f;
        q[1] = 0.f;
        q[2] = 0.f;
        q[3] = 0.f;
    }
}

void inv_q_invertf(const float *q, float *qInverted)
{
    qInverted[0] = q[0];
    qInverted[1] = -q[1];
    qInverted[2] = -q[2];
    qInverted[3] = -q[3];
}

void inv_matrix_det_incd(double *a, double *b, int *n, int x, int y)
{
    int k, l, i, j;
    for (i = 0, k = 0; i < *n; i++, k++) {
        for (j = 0, l = 0; j < *n; j++, l++) {
            if (i == x)
                i++;
            if (j == y)
                j++;
            *(b + 10 * k + l) = *(a + 10 * i + j);
        }
    }
    *n = *n - 1;
}

double inv_matrix_detd(double *p, int *n)
{
    double d[10][10], sum = 0;
    int i, j, m;
    m = *n;
    if (*n == 2)
        return (*p ** (p + 11) - *(p + 1) ** (p + 10));
    for (i = 0, j = 0; j < m; j++) {
        *n = m;
        inv_matrix_det_incd(p, &d[0][0], n, i, j);
        sum =
            sum + *(p + 10 * i + j) * SIGNM(i + j) * inv_matrix_detd(&d[0][0],
                                                                     n);
    }

    return (sum);
}



inv_error_t inv_get_array(int dataSet, long *data)
{
    switch (dataSet) {
        case INV_GYROS:
            return inv_get_gyro(data);
        case INV_ACCELS:
            return inv_get_accel(data);
        case INV_TEMPERATURE:
            return inv_get_temperature(data);
        case INV_ROTATION_MATRIX:
            return inv_get_rot_mat(data);
        case INV_QUATERNION:
            return inv_get_quaternion(data);
        case INV_LINEAR_ACCELERATION:
            return inv_get_linear_accel(data);
        case INV_LINEAR_ACCELERATION_WORLD:
            return inv_get_linear_accel_in_world(data);
        case INV_GRAVITY:
            return inv_get_gravity(data);
        case INV_ANGULAR_VELOCITY:
            return inv_get_angular_velocity(data);
        case INV_EULER_ANGLES:
            return inv_get_euler_angles(data);
        case INV_EULER_ANGLES_X:
            return inv_get_euler_angles_x(data);
        case INV_EULER_ANGLES_Y:
            return inv_get_euler_angles_y(data);
        case INV_EULER_ANGLES_Z:
            return inv_get_euler_angles_z(data);
        case INV_GYRO_TEMP_SLOPE:
            return inv_get_gyro_temp_slope(data);
        case INV_GYRO_BIAS:
            return inv_get_gyro_bias(data);
        case INV_ACCEL_BIAS:
            return inv_get_accel_bias(data);
        case INV_MAG_BIAS:
            return inv_get_mag_bias(data);
        case INV_RAW_DATA:
            return inv_get_gyro_and_accel_sensor(data);
        case INV_MAG_RAW_DATA:
            return inv_get_mag_raw_data(data);
        case INV_MAGNETOMETER:
            return inv_get_magnetometer(data);
        case INV_PRESSURE:
            return inv_get_pressure(data);
        case INV_HEADING:
            return inv_get_heading(data);
        case INV_GYRO_CALIBRATION_MATRIX:
            return inv_get_gyro_cal_matrix(data);
        case INV_ACCEL_CALIBRATION_MATRIX:
            return inv_get_accel_cal_matrix(data);
        case INV_MAG_CALIBRATION_MATRIX:
            return inv_get_mag_cal_matrix(data);
        case INV_MAG_BIAS_ERROR:
            return inv_get_mag_bias_error(data);
        case INV_MAG_SCALE:
            return inv_get_mag_scale(data);
        case INV_LOCAL_FIELD:
            return inv_get_local_field(data);
        case INV_RELATIVE_QUATERNION:
            return inv_get_relative_quaternion(data);
    }
    return INV_ERROR_INVALID_PARAMETER;
}

inv_error_t inv_get_float_array(int dataSet, float *data)
{
    switch (dataSet) {
        case INV_GYROS:
            return inv_get_gyro_float(data);
        case INV_ACCELS:
            return inv_get_accel_float(data);
        case INV_TEMPERATURE:
            return inv_get_temperature_float(data);
        case INV_ROTATION_MATRIX:
            return inv_get_rot_mat_float(data);
        case INV_QUATERNION:
            return inv_get_quaternion_float(data);
        case INV_LINEAR_ACCELERATION:
            return inv_get_linear_accel_float(data);
        case INV_LINEAR_ACCELERATION_WORLD:
            return inv_get_linear_accel_in_world_float(data);
        case INV_GRAVITY:
            return inv_get_gravity_float(data);
        case INV_ANGULAR_VELOCITY:
            return inv_get_angular_velocity_float(data);
        case INV_EULER_ANGLES:
            return inv_get_euler_angles_float(data);
        case INV_EULER_ANGLES_X:
            return inv_get_euler_angles_x_float(data);
        case INV_EULER_ANGLES_Y:
            return inv_get_euler_angles_y_float(data);
        case INV_EULER_ANGLES_Z:
            return inv_get_euler_angles_z_float(data);
        case INV_GYRO_TEMP_SLOPE:
            return inv_get_gyro_temp_slope_float(data);
        case INV_GYRO_BIAS:
            return inv_get_gyro_bias_float(data);
        case INV_ACCEL_BIAS:
            return inv_get_accel_bias_float(data);
        case INV_MAG_BIAS:
            return inv_get_mag_bias_float(data);
        case INV_RAW_DATA:
            return inv_get_gyro_and_accel_sensor_float(data);
        case INV_MAG_RAW_DATA:
            return inv_get_mag_raw_data_float(data);
        case INV_MAGNETOMETER:
            return inv_get_magnetometer_float(data);
        case INV_PRESSURE:
            return inv_get_pressure_float(data);
        case INV_HEADING:
            return inv_get_heading_float(data);
        case INV_GYRO_CALIBRATION_MATRIX:
            return inv_get_gyro_cal_matrix_float(data);
        case INV_ACCEL_CALIBRATION_MATRIX:
            return inv_get_accel_cal_matrix_float(data);
        case INV_MAG_CALIBRATION_MATRIX:
            return inv_get_mag_cal_matrix_float(data);
        case INV_MAG_BIAS_ERROR:
            return inv_get_mag_bias_error_float(data);
        case INV_MAG_SCALE:
            return inv_get_mag_scale_float(data);
        case INV_LOCAL_FIELD:
            return inv_get_local_field_float(data);
        case INV_RELATIVE_QUATERNION:
            return inv_get_relative_quaternion_float(data);
    }
    return INV_ERROR_INVALID_PARAMETER;
}
