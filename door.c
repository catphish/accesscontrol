#include <stdint.h>
#include "board.h"
#include "gpio.h"
#include "door.h"
#include "util.h"

struct time_t door_lock_at;

void door_init()
{
  gpio_port_mode(GPIOE, 2, 1, 0, 0, 0);  // Door
  gpio_port_mode(GPIOC, 6, 1, 0, 0, 0);  // Red
  gpio_port_mode(GPIOC, 7, 1, 0, 0, 0);  // Green
  gpio_port_mode(GPIOC, 8, 1, 0, 0, 0);  // Beep
  gpio_port_mode(GPIOE, 12, 1, 0, 0, 0); // Power LED on NIC
  gpio_port_mode(GPIOC, 3, 0, 0, 0, 0);  // Exit button
  GPIOC->PUPDR |= (1 << (3 * 2));        // Pull up exit button
  SYSCFG->EXTICR[1 - 1] |= SYSCFG_EXTICR1_EXTI3_PC;
  EXTI->IMR |= (1 << 3);
  EXTI->FTSR |= (1 << 3);
  NVIC->ISER[((uint32_t)(EXTI3_IRQn) >> 5)] |= (1 << ((uint32_t)(EXTI3_IRQn)&0x1F));

  // Turn off all the LEDs
  GPIOC->BSRR = (1 << 6);
  GPIOC->BSRR = (1 << 7);
  GPIOC->BSRR = (1 << 8);
  GPIOE->BSRR = (1 << 12);

  door_lock_now();
}

void door_main()
{
  if (door_lock_at.sec && time_passed(&door_lock_at))
  {
    door_lock_at.sec = 0;
    door_lock_now();
  }
}

void door_open_timed(int t)
{
  door_open_now();
  time_set(&door_lock_at, t, 0);
}

void door_open_now()
{
  GPIOE->BSRR = (1 << 18); // Unlock
  GPIOC->BSRR = (1 << 6);  // Red LED off
  GPIOC->BSRR = (1 << 23); // Green LED on
}

void door_lock_now()
{
  GPIOE->BSRR = (1 << 2);  // Lock
  GPIOC->BSRR = (1 << 22); // Red LED on
  GPIOC->BSRR = (1 << 7);  // Green LED off
}

void EXTI3_IRQHandler()
{
  door_open_timed(8);
  EXTI->PR = (1 << 3);
}
