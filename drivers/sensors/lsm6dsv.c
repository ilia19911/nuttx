#include <nuttx/config.h>

#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <debug.h>

#include <nuttx/kmalloc.h>
#include <nuttx/fs/fs.h>
#include <nuttx/i2c/i2c_master.h>
#include <nuttx/sched.h>

#include <nuttx/sensors/lsm6dsv.h>

/* I2C helpers */

static int lsm6dsv_readreg8(FAR struct lsm6dsv_dev_s *priv,
                            uint8_t reg, uint8_t *value)
{
  struct i2c_config_s config;
  uint8_t buffer[1];

  config.frequency = 400000;
  config.address   = priv->addr;
  config.addrlen   = 7;

  buffer[0] = reg;

  int ret = i2c_writeread(priv->i2c, &config,
                          buffer, 1,
                          value, 1);
  return ret;
}

static int lsm6dsv_writereg8(FAR struct lsm6dsv_dev_s *priv,
                             uint8_t reg, uint8_t value)
{
  struct i2c_config_s config;
  uint8_t buffer[2];

  config.frequency = 400000;
  config.address   = priv->addr;
  config.addrlen   = 7;

  buffer[0] = reg;
  buffer[1] = value;

  return i2c_write(priv->i2c, &config, buffer, 2);
}

/* Init */
static int lsm6dsv_init(FAR struct lsm6dsv_dev_s *priv)
{
  uint8_t val;
  uint8_t test;

  /* reset */
  lsm6dsv_writereg8(priv, LSM6DSV_CTRL3_C, LSM6DSV_CTRL3_C_SW_RESET);

  /* wait reset done */
  do
  {
    lsm6dsv_readreg8(priv, LSM6DSV_CTRL3_C, &val);
  }
  while (val & LSM6DSV_CTRL3_C_SW_RESET);

  /* BDU + IF_INC */
  lsm6dsv_writereg8(priv, LSM6DSV_CTRL3_C,
                    LSM6DSV_CTRL3_C_BDU | LSM6DSV_CTRL3_C_IF_INC);

  /* enable accel and gyro, 120 Hz, high-performance */
  lsm6dsv_writereg8(priv, LSM6DSV_CTRL1_XL, 0x06);
  lsm6dsv_writereg8(priv, LSM6DSV_CTRL2_G,  0x06);

  lsm6dsv_readreg8(priv, 0x28, &test);
  sninfo("0x28 = %02X\n", test);

  lsm6dsv_readreg8(priv, 0x22, &test);
  sninfo("0x22 = %02X\n", test);

  return OK;
}

/* Read sensor */

static int lsm6dsv_read(FAR struct lsm6dsv_dev_s *priv,
                        FAR struct lsm6dsv_sensor_data_s *data)
{
  struct i2c_config_s config;
  uint8_t reg = LSM6DSV_OUTX_L_G;
  uint8_t buf[12];
  int ret;

  config.frequency = 1000000;
  config.address   = priv->addr;
  config.addrlen   = 7;

  ret = i2c_writeread(priv->i2c, &config,
                      &reg, 1,
                      buf, sizeof(buf));
  if (ret < 0)
  {
    return ret;
  }

  /* Gyro: 0x22..0x27 */
  data->gx = (int16_t)((buf[1] << 8) | buf[0]);
  data->gy = (int16_t)((buf[3] << 8) | buf[2]);
  data->gz = (int16_t)((buf[5] << 8) | buf[4]);

  /* Accel: 0x28..0x2D */
  data->ax = (int16_t)((buf[7] << 8) | buf[6]);
  data->ay = (int16_t)((buf[9] << 8) | buf[8]);
  data->az = (int16_t)((buf[11] << 8) | buf[10]);

  return sizeof(struct lsm6dsv_sensor_data_s);
}
/* File ops */

static ssize_t lsm6dsv_read_file(FAR struct file *filep,
                                 FAR char *buffer,
                                 size_t buflen)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct lsm6dsv_dev_s *priv = inode->i_private;

  if (buflen < sizeof(struct lsm6dsv_sensor_data_s))
    return -EINVAL;

  return lsm6dsv_read(priv,
         (FAR struct lsm6dsv_sensor_data_s *)buffer);
}

static int lsm6dsv_ioctl(FAR struct file *filep,
                         int cmd,
                         unsigned long arg)
{
  FAR struct inode *inode = filep->f_inode;
  FAR struct lsm6dsv_dev_s *priv = inode->i_private;

  switch (cmd)
    {
      case SNIOC_LSM6DSVSENSORREAD:
        return lsm6dsv_read(priv,
               (FAR struct lsm6dsv_sensor_data_s *)arg);

      default:
        return -ENOTTY;
    }
}

static const struct file_operations g_lsm6dsv_fops =
{
  NULL,               /* open */
  NULL,               /* close */
  lsm6dsv_read_file,  /* read */
  NULL,               /* write */
  NULL,               /* seek */
  lsm6dsv_ioctl,      /* ioctl */
};

/* Register */

int lsm6dsv_sensor_register(FAR const char *devpath,
                            FAR struct i2c_master_s *i2c,
                            uint8_t addr)
{
  FAR struct lsm6dsv_dev_s *priv;

  priv = kmm_zalloc(sizeof(*priv));
  if (!priv)
    return -ENOMEM;

  priv->i2c  = i2c;
  priv->addr = addr;

  int ret = lsm6dsv_init(priv);
  if (ret < 0)
    return ret;

  return register_driver(devpath, &g_lsm6dsv_fops, 0666, priv);
}