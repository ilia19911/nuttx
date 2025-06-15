//
// Created by Ilya Ostanin on 14.06.2025.
//
#include "nucleo-f103rb.h"
#include "stm32_spi.h"
#include <nuttx/spi/spi.h>
#include <debug.h>
#include <arch/board/board.h>
#include <nuttx/arch.h>

#ifdef CONFIG_LPWAN_SX126X


#include <nuttx/wireless/lpwan/sx126x.h>

/* Примитивный RESET через GPIO */
static void sx126x_reset_impl(void)
{
  stm32_gpiowrite(GPIO_SX126X_RESET, false);
  up_mdelay(10);
  stm32_gpiowrite(GPIO_SX126X_RESET, true);
  up_mdelay(20);
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
  *hpmax = 7;      /* max для SX1262 */
  *padutycycle = 0x04;
}

/* Ограничение TX мощности */
static int sx126x_limit_tx_power(uint8_t *power)
{
  if (*power > 22)
    *power = 22;
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
  .dio3_delay         = 5000, /* us */
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
  .tx_ramp_time         = SX126X_SET_RAMP_80U,
    /* Frequency control
   * Typically boards have a limited range of frequencies.
   * Exceeding these can damage the radio.
   * Also depending on regulations, some frequencies are restricted.
   * This must return non zero in case a frequency is denied.
   */
  .check_frequency    = sx126x_check_freq,


};


void stm32_sx1262_initialize(void)
{
    FAR struct spi_dev_s *spi = NULL;
    spi = stm32_spibus_initialize(2);
    if (spi == NULL)
    {
        syslog(LOG_ERR, "ERROR: Failed to initialize SPI port 1\n");
    }
    else {
        syslog(LOG_INFO, "SPI port 1 initialized for SX1262\n");
    }

    /* Регистрируем драйвер */
    sx126x_register(spi, &g_sx126x_lower, "/dev/lpw0");
}

#endif