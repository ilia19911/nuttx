enable spi
System Type -> STM32 Peripherals -> SPI2 -> Enable

enable sx1262
System Type -> Device Drivers -> Wireless Device Support -> LPWAN -> SX1262 -> Enable

add stm32_spi.c to Make.defs

add to board.h
    
```c
#define GPIO_SX126X_NSS   (GPIO_OUTPUT | GPIO_CNF_OUTPP | GPIO_MODE_50MHz | GPIO_OUTPUT_SET | GPIO_PORTB | GPIO_PIN1)
#define GPIO_SX126X_RESET (GPIO_OUTPUT | GPIO_CNF_OUTPP | GPIO_MODE_50MHz | GPIO_OUTPUT_SET | GPIO_PORTB | GPIO_PIN2)
#define GPIO_SX126X_DIO1  (GPIO_INPUT  | GPIO_CNF_INFLOAT    | GPIO_EXTI        | GPIO_OUTPUT_SET | GPIO_PORTC | GPIO_PIN4)
```
