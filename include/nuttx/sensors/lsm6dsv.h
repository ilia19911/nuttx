#ifndef __INCLUDE_NUTTX_SENSORS_LSM6DSV_H
#define __INCLUDE_NUTTX_SENSORS_LSM6DSV_H

#include <nuttx/config.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sensors/ioctl.h>
#include <stdint.h>

/* IOCTL */

#define SNIOC_LSM6DSVSENSORREAD _SNIOC(0x0047)

/* I2C Addresses ************************************************************/

/* Accelerometer addresses */

#define LSM6DSVACCEL_ADDR0  (0xD4 >> 1) /* 0x6A low */
#define LSM6DSVACCEL_ADDR1  (0xD6 >> 1) /* 0x6B high */

/* Gyroscope addresses */

#define LSM6DSVGYRO_ADDR0   (0xD4 >> 1) /* 0x6A low */
#define LSM6DSVGYRO_ADDR1   (0xD6 >> 1) /* 0x6B high */

/* Registers (based on LSM6DSV datasheet) */

#define LSM6DSV_FUNC_CFG_ACCESS 0x01
#define LSM6DSV_PIN_CTRL        0x02
#define LSM6DSV_IF_CFG          0x03

#define LSM6DSV_WHO_AM_I        0x0F
#define LSM6DSV_WHO_AM_I_VALUE  0x70

#define LSM6DSV_CTRL1_XL        0x10
#define LSM6DSV_CTRL2_G         0x11
#define LSM6DSV_CTRL3_C         0x12
#define LSM6DSV_CTRL4_C         0x13
#define LSM6DSV_CTRL5_C         0x14
#define LSM6DSV_CTRL6_C         0x15
#define LSM6DSV_CTRL7_G         0x16
#define LSM6DSV_CTRL8_XL        0x17
#define LSM6DSV_CTRL9_XL        0x18
#define LSM6DSV_CTRL10_C        0x19

#define LSM6DSV_STATUS_REG      0x1E

/* CTRL3_C bits */
#define LSM6DSV_CTRL3_C_SW_RESET (1 << 0)
#define LSM6DSV_CTRL3_C_IF_INC   (1 << 2)
#define LSM6DSV_CTRL3_C_BDU      (1 << 6)

#define LSM6DSV_OUTX_L_G     0x22
#define LSM6DSV_OUTX_H_G     0x23
#define LSM6DSV_OUTY_L_G     0x24
#define LSM6DSV_OUTY_H_G     0x25
#define LSM6DSV_OUTZ_L_G     0x26
#define LSM6DSV_OUTZ_H_G     0x27

#define LSM6DSV_OUTX_L_XL    0x28
#define LSM6DSV_OUTX_H_XL    0x29
#define LSM6DSV_OUTY_L_XL    0x2A
#define LSM6DSV_OUTY_H_XL    0x2B
#define LSM6DSV_OUTZ_L_XL    0x2C
#define LSM6DSV_OUTZ_H_XL    0x2D

/* Data struct */

struct lsm6dsv_sensor_data_s
{
  int16_t ax;
  int16_t ay;
  int16_t az;

  int16_t gx;
  int16_t gy;
  int16_t gz;
};

/* Device */

struct lsm6dsv_dev_s
{
  FAR struct i2c_master_s *i2c;
  uint8_t addr;
};

/* API */

int lsm6dsv_sensor_register(FAR const char *devpath,
                            FAR struct i2c_master_s *i2c,
                            uint8_t addr);

#endif /* __INCLUDE_NUTTX_SENSORS_LSM6DSV_H */