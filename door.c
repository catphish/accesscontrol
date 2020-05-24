#include <stdint.h>
#include "board.h"
#include "gpio.h"
#include "door.h"
#include "util.h"

struct time_t door_lock_at;

void door_init() {
#ifdef PROD
  gpio_port_mode(GPIOE, 2,  1, 0, 0, 0); // Door
  gpio_port_mode(GPIOC, 6,  1, 0, 0, 0); // Red
  gpio_port_mode(GPIOC, 7,  1, 0, 0, 0); // Green
  gpio_port_mode(GPIOC, 8,  1, 0, 0, 0); // Beep

  GPIOC->BSRR = (1<<6);
  GPIOC->BSRR = (1<<7);
  GPIOC->BSRR = (1<<8);
#else
  gpio_port_mode(GPIOB, 0,  1, 0, 0, 0); // Status LED
  gpio_port_mode(GPIOB, 7,  1, 0, 0, 0); // Status LED
#endif

  door_lock_now();
}

void door_main() {
  if(door_lock_at.sec && time_passed(&door_lock_at)) {
    door_lock_at.sec = 0;
    door_lock_now();
  }
}

void door_open_timed(int t) {
  door_open_now();
  time_set(&door_lock_at, t, 0);
}

void door_open_now() {
#ifdef PROD
  GPIOE->BSRR = (1<<18); // Unlock
  GPIOC->BSRR = (1<<6);  // Red LED off
  GPIOC->BSRR = (1<<23); // Green LED on
#else
  GPIOB->BSRR = (1<<23);
#endif
}

void door_lock_now() {
#ifdef PROD
  GPIOE->BSRR = (1<<2);  // Lock
  GPIOC->BSRR = (1<<22); // Red LED on
  GPIOC->BSRR = (1<<7);  // Green LED off
#else
  GPIOB->BSRR = (1<<7);
#endif
}
