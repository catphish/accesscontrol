#include <stdint.h>
#include "board.h"
#include "gpio.h"
#include "door.h"

int door_lock_at = 0;

void door_init() {
#ifdef PROD
  gpio_port_mode(GPIOE, 2,  1, 0, 0, 0); // Door
#else
  gpio_port_mode(GPIOB, 0,  1, 0, 0, 0); // Status LED
  gpio_port_mode(GPIOB, 7,  1, 0, 0, 0); // Status LED
#endif

  door_lock_now();
}

void door_main() {
  if(door_lock_at && door_lock_at < TIM2->CNT) {
    door_lock_at = 0;
    door_lock_now();
  }
}

void door_open_timed(int t) {
  door_open_now();
  door_lock_at = TIM2->CNT + t;
}

void door_open_now() {
#ifdef PROD
  GPIOE->BSRR = (1<<18);
#else
  GPIOB->BSRR = (1<<23);
#endif
}

void door_lock_now() {
#ifdef PROD
  GPIOE->BSRR = (1<<2);
#else
  GPIOB->BSRR = (1<<7);
#endif
}
