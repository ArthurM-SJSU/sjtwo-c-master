// @file gpio_isr.c
#include "gpio_isr.h"
#include "lpc40xx.h"

static function_pointer_t gpio0_callbacks[32];

void gpio0__attach_interrupt(uint32_t pin, gpio_interrupt_e interrupt_type, function_pointer_t callback) {

  gpio0_callbacks[pin] = callback;
  if (interrupt_type == GPIO_INTR__FALLING_EDGE) {
    LPC_GPIOINT->IO0IntEnF |= (1 << pin);
  } else {
    LPC_GPIOINT->IO0IntEnR |= (1 << pin);
  }
}

void gpio0__interrupt_dispatcher(void) {
  int pin_that_generated_interrupt;

  for (int pin = 0; pin < 32; pin++) {
    if ((LPC_GPIOINT->IO0IntStatF & (1 << pin)) | (LPC_GPIOINT->IO0IntStatR & (1 << pin))) {
      pin_that_generated_interrupt = pin;
    }
  }
  function_pointer_t attached_user_handler = gpio0_callbacks[pin_that_generated_interrupt];

  attached_user_handler();

  LPC_GPIOINT->IO0IntClr |= (1 << pin_that_generated_interrupt);
}