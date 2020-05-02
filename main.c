#include "board.h"
#include <stdint.h>
#include "gpio.h"
#include "util.h"
#include "ethernet.h"

int main() {
  gpio_init();
  // gpio_port_mode(GPIOB, 0,  1, 0, 0, 0);
  // gpio_port_mode(GPIOB, 7,  1, 0, 0, 0);
  gpio_port_mode(GPIOE, 2,  1, 0, 0, 0);
  gpio_port_mode(GPIOB, 14, 1, 0, 0, 0);
  GPIOB->ODR = (1<<14);

  ethernet_init();

  while(1) {
    ethernet_main();
  }

}

void open_door() {
  GPIOE->ODR = 0;
}

void close_door() {
  GPIOE->ODR = (1<<2);
}
