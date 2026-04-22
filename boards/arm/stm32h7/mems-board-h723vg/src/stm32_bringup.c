/****************************************************************************
 * boards/arm/stm32h7/nucleo-h723vg/src/stm32_bringup.c
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

#include "nuttx/arch.h"
#include "nuttx/mtd/mtd.h"
// #include <nuttx/mtd/ftl.h>
// #include <nuttx/mtd/s512.h>
#include "fcntl.h"
#include "nuttx/binfmt/symtab.h"
#include "stdio.h"
#include "sys/poll.h"
#include "sys/stat.h"

#include <nuttx/config.h>

#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

#include <arch/board/board.h>

#include <nuttx/fs/fs.h>

#ifdef CONFIG_USBMONITOR
#include <nuttx/usb/usbmonitor.h>
#endif

#ifdef CONFIG_STM32H7_OTGFS
#include "stm32_usbhost.h"
#endif

#include "mems-board-h723vg.h"

#ifdef CONFIG_INPUT_BUTTONS
#include <nuttx/input/buttons.h>
#endif

#ifdef HAVE_RTC_DRIVER
#include "stm32_rtc.h"
#include <nuttx/timers/rtc.h>
#endif

#ifdef CONFIG_STM32_ROMFS
#include "stm32_romfs.h"
#endif

#ifdef CONFIG_CAPTURE
#include "stm32_capture.h"
#include <nuttx/timers/capture.h>
#endif

#ifdef CONFIG_STM32H7_IWDG
#include "stm32_wdg.h"
#endif

#ifdef CONFIG_RNDIS
#include <nuttx/usb/rndis.h>
#endif

#if defined(CONFIG_MTD_PROGMEM)
#include <nuttx/mtd/mtd.h>
#endif

#ifdef CONFIG_FSUTILS_MKFATFS
struct fat_format_s;
extern int mkfatfs(FAR const char *pathname, FAR struct fat_format_s *fmt);
#endif

#include "stm32_gpio.h"
#include <string.h>

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_capture_setup
 *
 * Description:
 *   Initialize and register capture drivers.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   Zero (OK) on success; a negated errno value on failure.
 *
 ****************************************************************************/

#ifdef CONFIG_CAPTURE
static int stm32_capture_setup(void) {
  int ret;
  struct cap_lowerhalf_s *lower[] = {
#if defined(CONFIG_STM32H7_TIM1_CAP)
      stm32_cap_initialize(1),
#endif
#if defined(CONFIG_STM32H7_TIM2_CAP)
      stm32_cap_initialize(2),
#endif
#if defined(CONFIG_STM32H7_TIM3_CAP)
      stm32_cap_initialize(3),
#endif
#if defined(CONFIG_STM32H7_TIM4_CAP)
      stm32_cap_initialize(4),
#endif
#if defined(CONFIG_STM32H7_TIM5_CAP)
      stm32_cap_initialize(5),
#endif
#if defined(CONFIG_STM32H7_TIM8_CAP)
      stm32_cap_initialize(8),
#endif
#if defined(CONFIG_STM32H7_TIM12_CAP)
      stm32_cap_initialize(12),
#endif
#if defined(CONFIG_STM32H7_TIM13_CAP)
      stm32_cap_initialize(13),
#endif
#if defined(CONFIG_STM32H7_TIM14_CAP)
      stm32_cap_initialize(14),
#endif
#if defined(CONFIG_STM32H7_TIM15_CAP)
      stm32_cap_initialize(15),
#endif
#if defined(CONFIG_STM32H7_TIM16_CAP)
      stm32_cap_initialize(16),
#endif
#if defined(CONFIG_STM32H7_TIM17_CAP)
      stm32_cap_initialize(17),
#endif
      /* TODO: LPTIMy_CAP */
  };

  size_t count = sizeof(lower) / sizeof(lower[0]);

  /* Nothing to do if no timers enabled */

  if (count == 0) {
    return OK;
  }

  /* This will register “/dev/cap0” ... “/dev/cap<count-1>” */

  ret = cap_register_multiple("/dev/cap", lower, count);
  if (ret == EINVAL) {
    syslog(LOG_ERR, "ERROR: cap_register_multiple path is invalid\n");
  } else if (ret == EEXIST) {
    syslog(LOG_ERR, "ERROR: cap_register_multiple an inode "
                    "already exists at this path\n");
  } else if (ret == ENOMEM) {
    syslog(LOG_ERR, "ERROR: cap_register_multiple not enough "
                    "memory to register capture drivers\n");
  } else if (ret < 0) {
    syslog(LOG_ERR, "ERROR: cap_register_multiple failed: %d\n", ret);
  }

  return ret;
}
#endif

/****************************************************************************
 * Name: stm32_i2c_register
 *
 * Description:
 *   Register one I2C drivers for the I2C tool.
 *
 ****************************************************************************/

#if defined(CONFIG_I2C) && defined(CONFIG_SYSTEM_I2CTOOL)
static void stm32_i2c_register(int bus) {
  struct i2c_master_s *i2c;
  int ret;

  i2c = stm32_i2cbus_initialize(bus);
  if (i2c == NULL) {
    syslog(LOG_ERR, "ERROR: Failed to get I2C%d interface\n", bus);
  } else {
    ret = i2c_register(i2c, bus);
    if (ret < 0) {
      syslog(LOG_ERR, "ERROR: Failed to register I2C%d driver: %d\n", bus, ret);
      stm32_i2cbus_uninitialize(i2c);
    }
  }
}
#endif

/****************************************************************************
 * Name: stm32_i2ctool
 *
 * Description:
 *   Register I2C drivers for the I2C tool.
 *
 ****************************************************************************/

#if defined(CONFIG_I2C) && defined(CONFIG_SYSTEM_I2CTOOL)
static void stm32_i2ctool(void) {
#ifdef CONFIG_STM32H7_I2C1
  stm32_i2c_register(1);
#endif
#ifdef CONFIG_STM32H7_I2C2
  stm32_i2c_register(2);
#endif
#ifdef CONFIG_STM32H7_I2C3
  stm32_i2c_register(3);
#endif
#ifdef CONFIG_STM32H7_I2C4
  stm32_i2c_register(4);
#endif
}
#endif

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Name: stm32_bringup
 *
 * Description:
 *   Perform architecture-specific initialization
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=y :
 *     Called from board_late_initialize().
 *
 *   CONFIG_BOARD_LATE_INITIALIZE=n && CONFIG_BOARDCTL=y &&
 *   CONFIG_NSH_ARCHINIT:
 *     Called from the NSH library
 *
 ****************************************************************************/
#include <nuttx/serial/serial.h>

struct fat_format_s {
  uint8_t ff_nfats;   /* Number of FATs */
  uint8_t ff_fattype; /* FAT size: 0 (autoselect), 12, 16, or 32 */
  uint8_t
      ff_clustshift; /* Log2 of sectors per cluster: 0-5, 0xff (autoselect) */
  uint8_t ff_volumelabel[11]; /* Volume label */
  uint16_t ff_backupboot;     /* Sector number of the backup boot sector (0=use
                                 default) */
  uint16_t ff_rootdirentries; /* Number of root directory entries */
  uint16_t ff_rsvdseccount;   /* Reserved sectors */
  uint32_t ff_hidsec;         /* Count of hidden sectors preceding fat */
  uint32_t ff_volumeid;       /* FAT volume id */
  uint32_t ff_nsectors; /* Number of sectors from device to use: 0: Use all */
};

#define MKFATFS_DEFAULT_NFATS 2         /* 2: Default number of FATs */
#define MKFATFS_DEFAULT_FATTYPE 0       /* 0: Autoselect FAT size */
#define MKFATFS_DEFAULT_CLUSTSHIFT 0xff /* 0xff: Autoselect cluster size */
#define MKFATFS_DEFAULT_VOLUMELABEL                                            \
  {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '}
#define MKFATFS_DEFAULT_BKUPBOOT                                               \
  0 /* 0: Determine sector number of the backup boot sector */
#define MKFATFS_DEFAULT_ROOTDIRENTS                                            \
  0 /* 0: Autoselect number of root directory entries */
#define MKFATFS_DEFAULT_RSVDSECCOUNT                                           \
  0 /* 0: Autoselect number reserved sectors (usually 32) */
#define MKFATFS_DEFAULT_HIDSEC 0   /* No hidden sectors */
#define MKFATFS_DEFAULT_VOLUMEID 0 /* No volume ID */
#define MKFATFS_DEFAULT_NSECTORS 0 /* 0: Use all sectors on device */

#define FAT_FORMAT_INITIALIZER                                                 \
  {MKFATFS_DEFAULT_NFATS,        MKFATFS_DEFAULT_FATTYPE,                      \
   MKFATFS_DEFAULT_CLUSTSHIFT,   MKFATFS_DEFAULT_VOLUMELABEL,                  \
   MKFATFS_DEFAULT_BKUPBOOT,     MKFATFS_DEFAULT_ROOTDIRENTS,                  \
   MKFATFS_DEFAULT_RSVDSECCOUNT, MKFATFS_DEFAULT_HIDSEC,                       \
   MKFATFS_DEFAULT_VOLUMEID,     MKFATFS_DEFAULT_NSECTORS}

#include <nuttx/compiler.h>
#include <nuttx/symtab.h>

const struct symtab_s g_symtab[] = {
    {"printf", (FAR const void *)printf},
    {"putchar", (FAR const void *)putchar},
    {"putc", (FAR const void *)putc},
    {"puts", (FAR const void *)puts},
    {"snprintf", (FAR const void *)snprintf},

    {"read", (FAR const void *)read},
    {"write", (FAR const void *)write},
    {"open", (FAR const void *)open},
    {"close", (FAR const void *)close},
    {"fcntl", (FAR const void *)fcntl},

    {"memset", (FAR const void *)memset},
    {"memcpy", (FAR const void *)memcpy},
    {"memmove", (FAR const void *)memmove},
    {"memcmp", (FAR const void *)memcmp},
    {"strlen", (FAR const void *)strlen},

    {"usleep", (FAR const void *)usleep},
    {"poll", (FAR const void *)poll},

    {"tcgetattr", (FAR const void *)tcgetattr},
    {"tcsetattr", (FAR const void *)tcsetattr},
    {"tcdrain", (FAR const void *)tcdrain},
    {"cfmakeraw", (FAR const void *)cfmakeraw},
    {"cfsetspeed", (FAR const void *)cfsetspeed},

    {"pthread_create", (FAR const void *)pthread_create},
    {"pthread_join", (FAR const void *)pthread_join},
    {"pthread_mutex_init", (FAR const void *)pthread_mutex_init},
    {"pthread_mutex_lock", (FAR const void *)pthread_mutex_lock},
    {"pthread_mutex_unlock", (FAR const void *)pthread_mutex_unlock},
    {"pthread_mutex_destroy", (FAR const void *)pthread_mutex_destroy},

    {"sem_init", (FAR const void *)sem_init},
    {"sem_wait", (FAR const void *)sem_wait},
    {"sem_post", (FAR const void *)sem_post},
    {"sem_destroy", (FAR const void *)sem_destroy},

    {"__errno", (FAR const void *)__errno},
};

int g_nsymbols = sizeof(g_symtab) / sizeof(g_symtab[0]);

#include <fcntl.h>

volatile void *keep_fcntl = (void *)fcntl;

int stm32_bringup(void) {
  printf("%s", "hello from nuttx");
  puts("Hello from nuttx");
  putchar('f');
  fcntl(-1, 0);
  poll(NULL, 0, 0);
  sem_post(NULL);
  sem_destroy(NULL);
  sem_wait(NULL);
  // exec_setsymtab(g_symtab, g_nsymbols);
  int ret = OK;
#ifdef HAVE_RTC_DRIVER
  struct rtc_lowerhalf_s *lower;
#endif

  UNUSED(ret);

#if defined(CONFIG_I2C) && defined(CONFIG_SYSTEM_I2CTOOL)
  stm32_i2ctool();
#endif

#ifdef CONFIG_FS_PROCFS
  /* Mount the procfs file system */

  ret = nx_mount(NULL, STM32_PROCFS_MOUNTPOINT, "procfs", 0, NULL);
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to mount the PROC filesystem: %d\n", ret);
  }
#endif /* CONFIG_FS_PROCFS */

#ifdef CONFIG_SENSORS_LSM6DSV
  ret = stm32_lsm6dsv_initialize("/dev/lsm6dsv");
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize LSM6DSV driver: %d\n", ret);
  }
#endif /* CONFIG_SENSORS_LSM6DSV */

  // #if defined(CONFIG_MTD)
  struct mtd_dev_s *mtd;

  mtd = progmem_initialize();
  if (mtd == NULL) {
    syslog(LOG_ERR, "ERROR: progmem_initialize\n");
  }

  struct mtd_geometry_s geo;
  mtd->ioctl(mtd, MTDIOC_GEOMETRY, (unsigned long)&geo);

  syslog(LOG_ERR, "MTD: blocksize=%lu erasesize=%lu neraseblocks=%lu\n",
         (unsigned long)geo.blocksize, (unsigned long)geo.erasesize,
         (unsigned long)geo.neraseblocks);

  uint32_t blocks_per_erase = geo.erasesize / geo.blocksize;

  /* Place filesystem at the END of flash to avoid overwrite by firmware */
  uint32_t start = blocks_per_erase * (geo.neraseblocks - 4);
  uint32_t count = blocks_per_erase * 4;

  struct mtd_dev_s *sub_mtd = mtd_partition(mtd, start, count);

  if (sub_mtd == NULL) {
    syslog(LOG_ERR, "ERROR: Failed to create MTD partition\n");
    return -1;
  }

  ret = register_mtddriver("/dev/mtd0", sub_mtd, 0755, NULL);
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: register_mtddriver failed: %d\n", ret);
  }

  /* Create 512-byte sector wrapper for FAT */
  struct mtd_dev_s *mtd_s512 = s512_initialize(sub_mtd);
  if (mtd_s512 == NULL) {
    syslog(LOG_ERR, "ERROR: s512_initialize failed\n");
    return -1;
  }

  /* Initialize FTL */
  int minor = 0;
  ret = ftl_initialize(minor, mtd_s512);
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: ftl_initialize failed: %d\n", ret);
    return ret;
  }

  syslog(LOG_INFO, "FTL initialized: /dev/mtdblock%d\n", minor);

  /* Create mount point */
  if (mkdir("/mnt", 0777) < 0 && errno != EEXIST) {
    syslog(LOG_ERR, "ERROR: mkdir /mnt failed: %d\n", errno);
  }

  /* Mount VFAT */
  ret = nx_mount("/dev/mtdblock0", "/mnt", "vfat", 0, NULL);
  if (ret < 0) {
#ifdef CONFIG_FSUTILS_MKFATFS
    syslog(LOG_WARNING, "VFAT mount failed (%d), formatting...\n", ret);
    struct fat_format_s fmt = FAT_FORMAT_INITIALIZER;
    // fmt.ff_fattype = 32;
    int fret = mkfatfs("/dev/mtdblock0", &fmt);
    if (fret < 0) {
      syslog(LOG_ERR, "ERROR: mkfatfs failed: %d\n", fret);
    } else {
      syslog(LOG_INFO, "mkfatfs OK, retry mount\n");
      ret = nx_mount("/dev/mtdblock0", "/mnt", "vfat", 0, NULL);
      if (ret < 0) {
        syslog(LOG_ERR, "ERROR: VFAT mount after format failed: %d\n", ret);
      }
    }
#else
    syslog(LOG_ERR, "ERROR: VFAT mount failed: %d\n", ret);
#endif
  }
// #endif
#ifdef CONFIG_FS_TMPFS
  /* Mount the tmpfs file system */

  ret = nx_mount(NULL, CONFIG_LIBC_TMPDIR, "tmpfs", 0, NULL);
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to mount tmpfs at %s: %d\n",
           CONFIG_LIBC_TMPDIR, ret);
  }
#endif

#ifdef CONFIG_STM32_ROMFS
  /* Mount the romfs partition */

  ret = stm32_romfs_initialize();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to mount romfs at %s: %d\n",
           CONFIG_STM32_ROMFS_MOUNTPOINT, ret);
  }
#endif

#ifdef HAVE_RTC_DRIVER
  /* Instantiate the STM32 lower-half RTC driver */

  lower = stm32_rtc_lowerhalf();
  if (!lower) {
    syslog(LOG_ERR, "ERROR: Failed to instantiate the RTC lower-half driver\n");
    return -ENOMEM;
  } else {
    /* Bind the lower half driver and register the combined RTC driver
     * as /dev/rtc0
     */

    ret = rtc_initialize(0, lower);
    if (ret < 0) {
      syslog(LOG_ERR, "ERROR: Failed to bind/register the RTC driver: %d\n",
             ret);
      return ret;
    }
  }
#endif

#ifdef CONFIG_INPUT_BUTTONS
  /* Register the BUTTON driver */

  ret = btn_lower_initialize("/dev/buttons");
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: btn_lower_initialize() failed: %d\n", ret);
  }
#endif /* CONFIG_INPUT_BUTTONS */

#ifdef HAVE_USBHOST
  /* Initialize USB host operation.  stm32_usbhost_initialize()
   * starts a thread will monitor for USB connection and
   * disconnection events.
   */

  ret = stm32_usbhost_initialize();
  if (ret != OK) {
    syslog(LOG_ERR, "ERROR: Failed to initialize USB host: %d\n", ret);
  }
#endif

#ifdef HAVE_USBMONITOR
  /* Start the USB Monitor */

  ret = usbmonitor_start();
  if (ret != OK) {
    syslog(LOG_ERR, "ERROR: Failed to start USB monitor: %d\n", ret);
  }
#endif

#ifdef CONFIG_ADC
  /* Initialize ADC and register the ADC driver. */

  ret = stm32_adc_setup();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: stm32_adc_setup failed: %d\n", ret);
  }
#endif /* CONFIG_ADC */

#ifdef CONFIG_DEV_GPIO
  /* Register the GPIO driver */

  ret = stm32_gpio_initialize();
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to initialize GPIO Driver: %d\n", ret);
    return ret;
  }
#endif

#ifdef CONFIG_SENSORS_LSM6DSL
  ret = stm32_lsm6dsl_initialize("/dev/lsm6dsl0");
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize LSM6DSL driver: %d\n", ret);
  }
#endif /* CONFIG_SENSORS_LSM6DSL */

#ifdef CONFIG_SENSORS_LSM9DS1
  ret = stm32_lsm9ds1_initialize();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize LSM9DS1 driver: %d\n", ret);
  }
#endif /* CONFIG_SENSORS_LSM9DS1 */

#ifdef CONFIG_SENSORS_LSM303AGR
  ret = stm32_lsm303agr_initialize("/dev/lsm303mag0");
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize LSM303AGR driver: %d\n", ret);
  }
#endif /* CONFIG_SENSORS_LSM303AGR */

#ifdef CONFIG_SENSORS_MPU60X0
  /* Initialize the MPU6000 device. */

  ret = stm32_mpu6000_initialize();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: stm32_mpu6000_initialize() failed: %d\n", ret);
  }
#endif

#ifdef CONFIG_PCA9635PW
  /* Initialize the PCA9635 chip */

  ret = stm32_pca9635_initialize();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: stm32_pca9635_initialize failed: %d\n", ret);
  }
#endif

#ifdef CONFIG_WL_NRF24L01
  ret = stm32_wlinitialize();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize wireless driver: %d\n", ret);
  }
#endif /* CONFIG_WL_NRF24L01 */

#if defined(CONFIG_CDCACM) && !defined(CONFIG_CDCACM_CONSOLE) &&               \
    !defined(CONFIG_CDCACM_COMPOSITE)
  /* Initialize CDCACM */

  syslog(LOG_INFO, "Initialize CDCACM device\n");

  ret = cdcacm_initialize(0, NULL);
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: cdcacm_initialize failed: %d\n", ret);
  }
#endif /* CONFIG_CDCACM & !CONFIG_CDCACM_CONSOLE */

#if defined(CONFIG_RNDIS) && !defined(CONFIG_RNDIS_COMPOSITE)
  uint8_t mac[6];
  mac[0] = 0xa0; /* TODO */
  mac[1] = (CONFIG_NETINIT_MACADDR_2 >> (8 * 0)) & 0xff;
  mac[2] = (CONFIG_NETINIT_MACADDR_1 >> (8 * 3)) & 0xff;
  mac[3] = (CONFIG_NETINIT_MACADDR_1 >> (8 * 2)) & 0xff;
  mac[4] = (CONFIG_NETINIT_MACADDR_1 >> (8 * 1)) & 0xff;
  mac[5] = (CONFIG_NETINIT_MACADDR_1 >> (8 * 0)) & 0xff;
  usbdev_rndis_initialize(mac);
#endif

#ifdef CONFIG_MMCSD_SPI
  /* Initialize the MMC/SD SPI driver (SPI3 is used) */

  ret = stm32_mmcsd_initialize(CONFIG_NSH_MMCSDMINOR);
  if (ret < 0) {
    syslog(LOG_ERR, "Failed to initialize SD slot %d: %d\n",
           CONFIG_NSH_MMCSDMINOR, ret);
  }
#endif

#ifdef CONFIG_PWM
  /* Initialize PWM and register the PWM device. */

  ret = stm32_pwm_setup();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: stm32_pwm_setup() failed: %d\n", ret);
  }
#endif

#ifdef CONFIG_CAPTURE
  /* Initialize the capture driver */

  ret = stm32_capture_setup();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: stm32_capture_setup() failed: %d\\n", ret);
  }
#endif

#ifdef CONFIG_MTD
#ifdef HAVE_PROGMEM_CHARDEV
  ret = stm32_progmem_init();
  if (ret < 0) {
    syslog(LOG_ERR, "ERROR: Failed to initialize MTD progmem: %d\n", ret);
  }
#endif /* HAVE_PROGMEM_CHARDEV */
#endif /* CONFIG_MTD */

#ifdef CONFIG_STM32H7_IWDG
  /* Initialize the watchdog timer */

  stm32_iwdginitialize("/dev/watchdog0", STM32_LSI_FREQUENCY);
#endif

  return OK;
}
