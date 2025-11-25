/****************************************************************************
 * boards/arm/stm32/omnibusf4/src/stm32_mpu6000.c
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

#include <stdint.h>
#include <debug.h>

#include <nuttx/irq.h>
#include <nuttx/arch.h>
#include <nuttx/i2c/i2c_master.h>

#include <nuttx/sensors/mpu60x0.h>

#include "stm32_gpio.h"
#include "stm32_i2c.h"
#include "nucleo-h723zg.h"

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_mpu6000_initialize
 *
 * Description:
 *
 *   Initialize and register's Omnibus F4's MPU6000 IMU. The wiring
 *   isn't configurable, but we use macros anyway because some of the
 *   values are referred to in more than one place. And also, because
 *   that's generally what NuttX does.
 *
 *   In particular, the slave-select pin is defined by us, but
 *   controlled elsewhere as part of the SPI machinery. This is an odd
 *   thing in our case because nothing else is using the SPI port, but
 *   that's not the general presentation so I'm staying consistent
 *   with the pattern.
 *
 ****************************************************************************/

int stm32_mpu6000_initialize(void)
{

  stm32_configgpio(MPU6000_1_SCL);
  stm32_configgpio(MPU6000_1_SDA);

  stm32_configgpio(MPU6000_2_SCL);
  stm32_configgpio(MPU6000_2_SDA);

  stm32_configgpio(MPU6000_3_SCL);
  stm32_configgpio(MPU6000_3_SDA);

  stm32_configgpio(MPU6000_4_SCL);
  stm32_configgpio(MPU6000_4_SDA);
  /* Note: the "minor" concept doesn't really apply since we're
   * uniquely-identified by a CS pin, we're the only device on the SPI
   * bus, and because users will refer to us through our device-node
   * pathname; I'm leaving this here for now anyway, in case we decide
   * sometime soon to do things differently.
   *
   * Likewise, we'll probably add things like EXTI, etc. to
   * mpu_config_s as the driver learns to support them.
   */

  struct i2c_master_s *i2c1 = stm32_i2cbus_initialize(1);
  struct i2c_master_s *i2c2 = stm32_i2cbus_initialize(2);
  struct i2c_master_s *i2c3 = stm32_i2cbus_initialize(3);
  struct i2c_master_s *i2c4 = stm32_i2cbus_initialize(4);
  if (i2c1 == NULL || i2c2 == NULL || i2c3 == NULL || i2c4 == NULL)
  {
    return -ENODEV;
  }
  struct mpu_config_s config1 ={.i2c = i2c1, .addr = MPU6000_ADDRESS};
  struct mpu_config_s config2 ={.i2c = i2c2, .addr = MPU6000_ADDRESS};
  struct mpu_config_s config3 ={.i2c = i2c3, .addr = MPU6000_ADDRESS};
  struct mpu_config_s config4 ={.i2c = i2c4, .addr = MPU6000_ADDRESS};

  /* TODO: configure EXTI pin */

  /* Register the chip with the device driver. */

  int ret = mpu60x0_register(DEVNODE_MPU6000_0, &config1);
  ret |= mpu60x0_register(DEVNODE_MPU6000_1, &config2);
  ret |= mpu60x0_register(DEVNODE_MPU6000_2, &config3);
  ret |= mpu60x0_register(DEVNODE_MPU6000_3, &config4);
  return ret;
}
