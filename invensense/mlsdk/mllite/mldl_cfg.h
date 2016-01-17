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

/**
 *  @addtogroup MLDL
 *
 *  @{
 *      @file   mldl_cfg.h
 *      @brief  The Motion Library Driver Layer Configuration header file.
 */

#ifndef __MLDL_CFG_H__
#define __MLDL_CFG_H__

#include "mltypes.h"
#include "mlsl.h"
#include <linux/mpu.h>
#if defined CONFIG_MPU_SENSORS_MPU6050A2
#    include "mpu6050a2.h"
#elif defined CONFIG_MPU_SENSORS_MPU6050B1
#    include "mpu6050b1.h"
#elif defined CONFIG_MPU_SENSORS_MPU3050
#  include "mpu3050.h"
#else
#error Invalid or undefined CONFIG_MPU_SENSORS_MPUxxxx
#endif

#include "log.h"

/*************************************************************************
 *  Sensors
 *************************************************************************/

#define INV_X_GYRO			(0x0001)
#define INV_Y_GYRO			(0x0002)
#define INV_Z_GYRO			(0x0004)
#define INV_DMP_PROCESSOR		(0x0008)

#define INV_X_ACCEL			(0x0010)
#define INV_Y_ACCEL			(0x0020)
#define INV_Z_ACCEL			(0x0040)

#define INV_X_COMPASS			(0x0080)
#define INV_Y_COMPASS			(0x0100)
#define INV_Z_COMPASS			(0x0200)

#define INV_X_PRESSURE			(0x0300)
#define INV_Y_PRESSURE			(0x0800)
#define INV_Z_PRESSURE			(0x1000)

#define INV_TEMPERATURE			(0x2000)
#define INV_TIME			(0x4000)

#define INV_THREE_AXIS_GYRO		(0x000F)
#define INV_THREE_AXIS_ACCEL		(0x0070)
#define INV_THREE_AXIS_COMPASS		(0x0380)
#define INV_THREE_AXIS_PRESSURE		(0x1C00)

#define INV_FIVE_AXIS			(0x007B)
#define INV_SIX_AXIS_GYRO_ACCEL		(0x007F)
#define INV_SIX_AXIS_ACCEL_COMPASS	(0x03F0)
#define INV_NINE_AXIS			(0x03FF)
#define INV_ALL_SENSORS			(0x7FFF)

#define MPL_PROD_KEY(ver, rev) (ver * 100 + rev)

/* -------------------------------------------------------------------------- */

/* Platform data for the MPU */
struct mldl_cfg {
	/* MPU related configuration */
	unsigned long requested_sensors;
	unsigned char ignore_system_suspend;
	unsigned char addr;
	unsigned char int_config;
	unsigned char ext_sync;
	unsigned char full_scale;
	unsigned char lpf;
	unsigned char clk_src;
	unsigned char divider;
	unsigned char dmp_enable;
	unsigned char fifo_enable;
	unsigned char dmp_cfg1;
	unsigned char dmp_cfg2;
	unsigned char offset_tc[GYRO_NUM_AXES];
	unsigned short offset[GYRO_NUM_AXES];
	unsigned char ram[MPU_MEM_NUM_RAM_BANKS][MPU_MEM_BANK_SIZE];

	/* MPU Related stored status and info */
	unsigned char product_revision;
	unsigned char silicon_revision;
	unsigned char product_id;
	unsigned short gyro_sens_trim;
#if defined CONFIG_MPU_SENSORS_MPU6050A2 || \
	defined CONFIG_MPU_SENSORS_MPU6050B1
	unsigned short accel_sens_trim;
#endif

	/* Driver/Kernel related state information */
	int gyro_is_bypassed;
	int i2c_slaves_enabled;
	int dmp_is_running;
	int gyro_is_suspended;
	int accel_is_suspended;
	int compass_is_suspended;
	int pressure_is_suspended;
	int gyro_needs_reset;

	/* Slave related information */
	struct ext_slave_descr *accel;
	struct ext_slave_descr *compass;
	struct ext_slave_descr *pressure;

	/* Platform Data */
	struct mpu_platform_data *pdata;
};

/* -------------------------------------------------------------------------- */

int inv_mpu_open(struct mldl_cfg *mldl_cfg,
		 void *mlsl_handle,
		 void *accel_handle,
		 void *compass_handle,
		 void *pressure_handle);
int inv_mpu_close(struct mldl_cfg *mldl_cfg,
		  void *mlsl_handle,
		  void *accel_handle,
		  void *compass_handle,
		  void *pressure_handle);
int inv_mpu_resume(struct mldl_cfg *mldl_cfg,
		   void *gyro_handle,
		   void *accel_handle,
		   void *compass_handle,
		   void *pressure_handle,
		   unsigned long sensors);
int inv_mpu_suspend(struct mldl_cfg *mldl_cfg,
		    void *gyro_handle,
		    void *accel_handle,
		    void *compass_handle,
		    void *pressure_handle,
		    unsigned long sensors);

/* Slave Read functions */
int inv_mpu_slave_read(struct mldl_cfg *mldl_cfg,
		       void *gyro_handle,
		       void *slave_handle,
		       struct ext_slave_descr *slave,
		       struct ext_slave_platform_data *pdata,
		       unsigned char *data);
static inline int inv_mpu_read_accel(struct mldl_cfg *mldl_cfg,
				     void *gyro_handle,
				     void *accel_handle, unsigned char *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_read(mldl_cfg, gyro_handle, accel_handle,
				  mldl_cfg->accel, &mldl_cfg->pdata->accel,
				  data);
}

static inline int inv_mpu_read_compass(struct mldl_cfg *mldl_cfg,
				       void *gyro_handle,
				       void *compass_handle,
				       unsigned char *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_read(mldl_cfg, gyro_handle, compass_handle,
				  mldl_cfg->compass, &mldl_cfg->pdata->compass,
				  data);
}

static inline int inv_mpu_read_pressure(struct mldl_cfg *mldl_cfg,
					void *gyro_handle,
					void *pressure_handle,
					unsigned char *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_read(mldl_cfg, gyro_handle, pressure_handle,
				  mldl_cfg->pressure,
				  &mldl_cfg->pdata->pressure, data);
}

/* Slave Config functions */
int inv_mpu_slave_config(struct mldl_cfg *mldl_cfg,
			 void *gyro_handle,
			 void *slave_handle,
			 struct ext_slave_config *data,
			 struct ext_slave_descr *slave,
			 struct ext_slave_platform_data *pdata);
static inline int inv_mpu_config_accel(struct mldl_cfg *mldl_cfg,
				       void *gyro_handle,
				       void *accel_handle,
				       struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_config(mldl_cfg, gyro_handle, accel_handle, data,
				    mldl_cfg->accel, &mldl_cfg->pdata->accel);
}

static inline int inv_mpu_config_compass(struct mldl_cfg *mldl_cfg,
					 void *gyro_handle,
					 void *compass_handle,
					 struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_config(mldl_cfg, gyro_handle, compass_handle, data,
				    mldl_cfg->compass,
				    &mldl_cfg->pdata->compass);
}

static inline int inv_mpu_config_pressure(struct mldl_cfg *mldl_cfg,
					  void *gyro_handle,
					  void *pressure_handle,
					  struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_slave_config(mldl_cfg, gyro_handle, pressure_handle,
				    data, mldl_cfg->pressure,
				    &mldl_cfg->pdata->pressure);
}

/* Slave get config functions */
int inv_mpu_get_slave_config(struct mldl_cfg *mldl_cfg,
			     void *gyro_handle,
			     void *slave_handle,
			     struct ext_slave_config *data,
			     struct ext_slave_descr *slave,
			     struct ext_slave_platform_data *pdata);

static inline int inv_mpu_get_accel_config(struct mldl_cfg *mldl_cfg,
					   void *gyro_handle,
					   void *accel_handle,
					   struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_get_slave_config(mldl_cfg, gyro_handle, accel_handle,
					data, mldl_cfg->accel,
					&mldl_cfg->pdata->accel);
}

static inline int inv_mpu_get_compass_config(struct mldl_cfg *mldl_cfg,
					     void *gyro_handle,
					     void *compass_handle,
					     struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_get_slave_config(mldl_cfg, gyro_handle, compass_handle,
					data, mldl_cfg->compass,
					&mldl_cfg->pdata->compass);
}

static inline int inv_mpu_get_pressure_config(struct mldl_cfg *mldl_cfg,
					      void *gyro_handle,
					      void *pressure_handle,
					      struct ext_slave_config *data)
{
	if (!mldl_cfg || !(mldl_cfg->pdata)) {
		LOG_RESULT_LOCATION(INV_ERROR_INVALID_PARAMETER);
		return INV_ERROR_INVALID_PARAMETER;
	}

	return inv_mpu_get_slave_config(mldl_cfg, gyro_handle,
					pressure_handle, data,
					mldl_cfg->pressure,
					&mldl_cfg->pdata->pressure);
}

/* -------------------------------------------------------------------------- */

static inline long inv_mpu_get_sampling_rate_hz(struct mldl_cfg *mldl_cfg)
{
	if (((mldl_cfg->lpf) == 0) || ((mldl_cfg->lpf) == 7))
		return 8000L / (mldl_cfg->divider + 1);
	else
		return 1000L / (mldl_cfg->divider + 1);
}

static inline long inv_mpu_get_sampling_period_us(struct mldl_cfg *mldl_cfg)
{
	if (((mldl_cfg->lpf) == 0) || ((mldl_cfg->lpf) == 7))
		return (long) (1000000L * (mldl_cfg->divider + 1)) / 8000L;
	else
		return (long) (1000000L * (mldl_cfg->divider + 1)) / 1000L;
}

#endif				/* __MLDL_CFG_H__ */

/**
 *@}
 */
