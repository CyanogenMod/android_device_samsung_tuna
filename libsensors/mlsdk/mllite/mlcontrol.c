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

/*******************************************************************************
 *
 * $Id: mlcontrol.c 5641 2011-06-14 02:10:02Z mcaramello $
 *
 *******************************************************************************/

/**
 *  @defgroup   CONTROL
 *  @brief      Motion Library - Control Engine.
 *              The Control Library processes gyroscopes, accelerometers, and
 *              compasses to provide control signals that can be used in user
 *              interfaces.
 *              These signals can be used to manipulate objects such as documents,
 *              images, cursors, menus, etc.
 *
 *  @{
 *      @file   mlcontrol.c
 *      @brief  The Control Library.
 *
 */

/* ------------------ */
/* - Include Files. - */
/* ------------------ */

#include "mltypes.h"
#include "mlinclude.h"
#include "mltypes.h"
#include "ml.h"
#include "mlos.h"
#include "mlsl.h"
#include "mldl.h"
#include "mlcontrol.h"
#include "dmpKey.h"
#include "mlstates.h"
#include "mlFIFO.h"
#include "string.h"

/* - Global Vars. - */
struct control_params cntrl_params = {
    {
     MLCTRL_SENSITIVITY_0_DEFAULT,
     MLCTRL_SENSITIVITY_1_DEFAULT,
     MLCTRL_SENSITIVITY_2_DEFAULT,
     MLCTRL_SENSITIVITY_3_DEFAULT}, // sensitivity
    MLCTRL_FUNCTIONS_DEFAULT,   // functions
    {
     MLCTRL_PARAMETER_ARRAY_0_DEFAULT,
     MLCTRL_PARAMETER_ARRAY_1_DEFAULT,
     MLCTRL_PARAMETER_ARRAY_2_DEFAULT,
     MLCTRL_PARAMETER_ARRAY_3_DEFAULT}, // parameterArray
    {
     MLCTRL_PARAMETER_AXIS_0_DEFAULT,
     MLCTRL_PARAMETER_AXIS_1_DEFAULT,
     MLCTRL_PARAMETER_AXIS_2_DEFAULT,
     MLCTRL_PARAMETER_AXIS_3_DEFAULT},  // parameterAxis
    {
     MLCTRL_GRID_THRESHOLD_0_DEFAULT,
     MLCTRL_GRID_THRESHOLD_1_DEFAULT,
     MLCTRL_GRID_THRESHOLD_2_DEFAULT,
     MLCTRL_GRID_THRESHOLD_3_DEFAULT},  // gridThreshold
    {
     MLCTRL_GRID_MAXIMUM_0_DEFAULT,
     MLCTRL_GRID_MAXIMUM_1_DEFAULT,
     MLCTRL_GRID_MAXIMUM_2_DEFAULT,
     MLCTRL_GRID_MAXIMUM_3_DEFAULT},    // gridMaximum
    MLCTRL_GRID_CALLBACK_DEFAULT    // gridCallback
};

/* - Extern Vars. - */
struct control_obj cntrl_obj;
extern const unsigned char *dmpConfig1;

/* -------------- */
/* - Functions. - */
/* -------------- */

/**
 *  @brief  inv_get_control_data is used to get the current control data.
 *
 *  @pre    inv_dmp_open() Must be called with MLDmpDefaultOpen() or
 *          inv_open_low_power_pedometer().
 *
 *  @param  controlSignal   Indicates which control signal is being queried.
 *                          Must be one of:
 *                          - INV_CONTROL_1,
 *                          - INV_CONTROL_2,
 *                          - INV_CONTROL_3 or
 *                          - INV_CONTROL_4.
 *
 *  @param  gridNum     A pointer to pass gridNum info back to the user.
 *  @param  gridChange  A pointer to pass gridChange info back to the user.
 *
 *  @return Zero if the command is successful; an ML error code otherwise.
 */
inv_error_t inv_get_control_data(long *controlSignal, long *gridNum,
                                 long *gridChange)
{
    /* NOTE:
     * This symbol is referenced by libinvensense_mpl.so
     * That is the sole reason this is being kept around.
     * It's integrated with some other junk here though,
     * so for the moment I'm not comfortable moving it.
     */
    INVENSENSE_FUNC_START;
    int_fast8_t i = 0;

    if (inv_get_state() != INV_STATE_DMP_STARTED)
        return INV_ERROR_SM_IMPROPER_STATE;

    for (i = 0; i < 4; i++) {
        controlSignal[i] = cntrl_obj.controlInt[i];
        gridNum[i] = cntrl_obj.gridNum[i];
        gridChange[i] = cntrl_obj.gridChange[i];
    }
    return INV_SUCCESS;
}

/**
 * @}
 */
