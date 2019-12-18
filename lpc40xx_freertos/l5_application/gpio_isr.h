#ifndef gpio_isr_h
#define gpio_isr_h
#include <stdio.h>

typedef enum {
  GPIO_INTR__FALLING_EDGE,
  GPIO_INTR__RISING_EDGE,
} gpio_interrupt_e;

typedef void (*function_pointer_t)(void);

void gpio0__attach_interrupt(uint32_t pin, gpio_interrupt_e interrupt_type, function_pointer_t callback);

void gpio0__interrupt_dispatcher(void);

#endif /* gpio_isr_h */