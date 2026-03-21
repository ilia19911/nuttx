/****************************************************************************
 * boards/arm/stm32h7/nucleo-h723vg/src/stm32_lsm6dsl.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/arch.h>

#include <errno.h>
#include <debug.h>

#include "mems-board-h723vg.h"
#include "nuttx/sensors/lsm6dsv.h"
#include "stm32.h"
#include <nuttx/board.h>
#include <nuttx/i2c/i2c_master.h>

#include <nuttx/sensors/lsm6dsl.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#ifndef CONFIG_STM32H7_I2C1
#  error "LSM6DSL driver requires CONFIG_STM32H7_I2C1 to be enabled"
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_lsm6dsl_initialize
 *
 * Description:
 *   Initialize I2C-based LSM6DSL.
 *
 ****************************************************************************/

int stm32_lsm6dsv_initialize(char *devpath)
{
  stm32_configgpio(LSM6DSV_1_SCL);
  stm32_configgpio(LSM6DSV_1_SDA);

  stm32_configgpio(LSM6DSV_4_SCL);
  stm32_configgpio(LSM6DSV_4_SDA);
  struct i2c_master_s *i2c;
  int ret = OK;

  sninfo("Initializing LMS6DSL!\n");

  /* Configure the GPIO interrupt */

  stm32_configgpio(GPIO_LPS22HB_INT1);

#if defined(CONFIG_STM32H7_I2C1)
  i2c = stm32_i2cbus_initialize(1);
  if (i2c == NULL)
    {
      return -ENODEV;
    }

  sninfo("INFO: Initializing LMS6DSL accelero-gyro sensor over I2C%d\n",
         ret);

  /* WHO_AM_I check */
  uint8_t who = 0;
  struct i2c_config_s config;

  config.frequency = 400000;
  config.address   = LSM6DSVACCEL_ADDR0;
  config.addrlen   = 7;

  uint8_t reg = LSM6DSV_WHO_AM_I;

  i2c_writeread(i2c, &config, &reg, 1, &who, 1);

  sninfo("LSM6DSV WHO_AM_I raw = 0x%02X (addr=0x%02X)\n", who, LSM6DSVACCEL_ADDR0);



  ret = lsm6dsv_sensor_register(devpath, i2c, LSM6DSVACCEL_ADDR0);
  if (ret < 0)
    {
      snerr("ERROR: Failed to initialize LMS6DSL accelero-gyro driver %s\n",
            devpath);
      return -ENODEV;
    }

  sninfo("INFO: LMS6DSL sensor has been initialized successfully\n");
#endif

  return ret;
}
