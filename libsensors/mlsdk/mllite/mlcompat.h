#ifndef __INV_COMPAT_H__
#define __INV_COMPAT_H__

#include "mltypes.h"

#define INV_GYROS                     0x0001
#define INV_ACCELS                    0x0002
#define INV_ROTATION_MATRIX           0x0003
#define INV_QUATERNION                0x0004
#define INV_EULER_ANGLES              0x0005
#define INV_LINEAR_ACCELERATION       0x0006
#define INV_LINEAR_ACCELERATION_WORLD 0x0007
#define INV_GRAVITY                   0x0008
#define INV_ANGULAR_VELOCITY          0x0009
#define INV_GYRO_CALIBRATION_MATRIX   0x000B
#define INV_ACCEL_CALIBRATION_MATRIX  0x000C
#define INV_GYRO_BIAS                 0x000D
#define INV_ACCEL_BIAS                0x000E
#define INV_GYRO_TEMP_SLOPE           0x000F
#define INV_RAW_DATA                  0x0011
#define INV_EULER_ANGLES_X            0x0013
#define INV_EULER_ANGLES_Y            0x0014
#define INV_EULER_ANGLES_Z            0x0015
#define INV_MAGNETOMETER              0x001A
#define INV_MAG_RAW_DATA              0x001C
#define INV_MAG_CALIBRATION_MATRIX    0x001D
#define INV_MAG_BIAS                  0x001E
#define INV_HEADING                   0x001F
#define INV_MAG_BIAS_ERROR            0x0020
#define INV_PRESSURE                  0x0021
#define INV_LOCAL_FIELD               0x0022
#define INV_MAG_SCALE                 0x0023
#define INV_RELATIVE_QUATERNION       0x0024
#define INV_TEMPERATURE               0x2000


#ifdef __cplusplus
extern "C" {
#endif

    struct filter_long {
        int length;
        const long *b;
        const long *a;
        long *x;
        long *y;
    };

    inv_error_t inv_pressure_supervisor(void);

    int inv_get_motion_state(void);

    void inv_filter_long(struct filter_long *state, long x);
    void inv_q_multf(const float *q1, const float *q2, float *qProd);
    void inv_q_addf(float *q1, float *q2, float *qSum);
    void inv_q_normalizef(float *q);
    void inv_q_norm4(float *q);
    void inv_q_invertf(const float *q, float *qInverted);
    void inv_matrix_det_incd(double *a, double *b, int *n, int x, int y);
    double inv_matrix_detd(double *p, int *n);

    inv_error_t inv_get_array(int dataSet, long *data);
    inv_error_t inv_get_float_array(int dataSet, float *data);

#ifdef __cplusplus
}
#endif

#endif /* __INV_COMPAT_H__ */