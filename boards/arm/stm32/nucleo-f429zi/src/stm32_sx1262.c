//
// Created by Ilya Ostanin on 14.06.2025.
//
#include "nucleo-144.h"
#include "stm32_spi.h"
#include <nuttx/spi/spi.h>
#include <debug.h>
#include <arch/board/board.h>
#include <nuttx/arch.h>
#include <nuttx/wireless/lpwan/sx126x.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <fcntl.h>


#ifdef CONFIG_LPWAN_SX126X

/* Примитивный RESET через GPIO */
static void sx126x_reset_impl(void)
{
  stm32_gpiowrite(GPIO_SX126X_RESET, false);
  up_mdelay(300);
  stm32_gpiowrite(GPIO_SX126X_RESET, true);
  up_mdelay(100);
  // Ждём пока модуль выйдет из BUSY
  while (stm32_gpioread(GPIO_SX126X_BUSY)){
        up_udelay(10); // короткие интервалы
  }
}

/* Проверка частоты (например, всё разрешено) */
static int sx126x_check_freq(uint32_t freq)
{
  return 0; /* ok */
}

/* Получение настроек PA */
static int sx126x_get_pa_values(enum sx126x_device_e *model, uint8_t *hpmax, uint8_t *padutycycle)
{
  *model = SX1262; /* для SX1262 */
  *hpmax = 0x07;      /* max для SX1262 */
  *padutycycle = 0x04;
  return 0;
}

/* Ограничение TX мощности */
static int sx126x_limit_tx_power(uint8_t *power)
{
  if (*power > 22)
    *power = 22;
  return 0;
}

/* Привязка IRQ (через stm32_gpiosetevent) */
static int sx126x_irq0attach(xcpt_t isr, void *arg)
{
  return stm32_gpiosetevent(GPIO_SX126X_DIO1, true, false, true, isr, arg);
}

/* Глобальный объект lower */
static const struct sx126x_lower_s g_sx126x_lower =
{
          /* Index of radio to register.
   * ex: 0 is the primary radio, 1 is the secondary.
   * Must be within the maximum configured radios.
   */
  .dev_number         = 0,
  .reset              = sx126x_reset_impl,
    /* This controls which DIO reacts to interrupts
   * Depended on the pinout of the board / module.
   * Note that DIO 2 and DIO 3 can be already in use
   * by the module and setting them might interfere
   * with the operation or even damage them.
   */

    .masks              = {
    .dio1_mask = SX126X_IRQ_TXDONE_MASK | SX126X_IRQ_RXDONE_MASK | SX126X_IRQ_CRCERR_MASK,
    .dio2_mask = 0,
    .dio3_mask = 0
  },
  .dio3_voltage       = SX126X_TCXO_1_7V,
  .dio3_delay         = 10, /* us */
  .use_dio2_as_rf_sw  = true,
    /* Interrupt attachments. These should be
   * connected to one of the DIOx pins
   */
  .irq0attach         = sx126x_irq0attach,
  /* The regulator mode is board / module depended */
  .regulator_mode     = SX126X_LDO,
    /* Power amplifier control. DO NOT exceeds the
   * limits listed in SX1261-2 V2 datasheet.
   * 13.1.14 SetPaConfig
   * This can cause damage to the device.
   */
  .get_pa_values      = sx126x_get_pa_values,
    /* TX power control. Depending on the local RF regulations,
   * power might have to be limited.
   * Also depending on board or module,
   * power values have different charactersitics.
   * More info in sx1261-2 V2 datasheet 13.4.4 SetTxParams.
   * uint8_t *power is set and this function may limit it.
   */
  .limit_tx_power     = sx126x_limit_tx_power,
  .tx_ramp_time         = SX126X_SET_RAMP_10U,
    /* Frequency control
   * Typically boards have a limited range of frequencies.
   * Exceeding these can damage the radio.
   * Also depending on regulations, some frequencies are restricted.
   * This must return non zero in case a frequency is denied.
   */
  .check_frequency    = sx126x_check_freq,


};

void test_spi_send(FAR struct spi_dev_s *spi)
{
  /* Настрой SPI (если не настроен выше) */
  SPI_LOCK(spi, true); // блокируем шину

  /* Выбираем чип (NSS = LOW) */
  SPI_SELECT(spi, SPIDEV_LPWAN(0), true);

  /* Отправляем шаблон: 0xAA, 0x55 */
  SPI_SEND(spi, 0xAA);
  SPI_SEND(spi, 0x55);

  /* Отжимаем NSS */
  SPI_SELECT(spi, SPIDEV_LPWAN(0), false);
  SPI_LOCK(spi, false); // разблокируем шину
}

void SX126xReadRegisters(struct spi_dev_s *spi, uint16_t address, uint8_t *buffer )
{
    SPI_LOCK(spi, true);
    SPI_SELECT(spi, SPIDEV_LPWAN(0), true);
    SPI_SEND(spi, 0x1D);
    SPI_SEND(spi,  ( address & 0xFF00 ) >> 8);
    SPI_SEND(spi, address & 0x00FF); // Standby RC
    SPI_SEND(spi, 0);
    *buffer = SPI_SEND(spi, 0);
    SPI_SELECT(spi, SPIDEV_LPWAN(0), false);
    SPI_LOCK(spi, false);
    while (stm32_gpioread(GPIO_SX126X_BUSY)){
        up_udelay(10); // короткие интервалы
    }
}

void sx126x_spi_check(struct spi_dev_s *spi)
{
  printf("=== SX126x SPI Sanity Check ===\n");

  sx126x_reset_impl();

  uint8_t status;
  SX126xReadRegisters(spi, 0x80, &status);
  printf("SX126x GetStatus → 0x%02X\n", status);

}


void stm32_sx1262_initialize(void)
{
    stm32_configgpio(GPIO_SX126X_RESET);  // твой макрос на пин RESET
    stm32_configgpio(GPIO_SX126X_DIO1);
    stm32_configgpio(GPIO_SX126X_BUSY); // где GPIO_SX126X_BUSY — твой макрос
    FAR struct spi_dev_s *spi = NULL;
    spi = stm32_spibus_initialize(1);
    if (spi == NULL)
    {
        syslog(LOG_ERR, "ERROR: Failed to initialize SPI port 1\n");
    }
    else {
        syslog(LOG_INFO, "SPI port 1 initialized for SX1262\n");
        SPI_SETBITS(spi, 8);
        SPI_SETMODE(spi, SPIDEV_MODE0);
        SPI_SETFREQUENCY(spi, 29000000); // 1 MHz
//        SPI_SETBITORDER(spi, SPIDEV_BITORDER_MSBFIRST);
    }


    /* Регистрируем драйвер */
    sx126x_register(spi, &g_sx126x_lower, "/dev/lpw0");

//    sx126x_debug_check(spi);
//    sx126x_spi_check(spi);
}



#endif
