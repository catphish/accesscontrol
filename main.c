#include <stm32f439xx.h>
#include <stdint.h>
#include "gpio.h"
#include "util.h"
#include "ethernet.h"

int main() {
  gpio_init();
  ethernet_init();

  gpio_port_mode(GPIOB, 0,  1, 0, 0, 0);
  gpio_port_mode(GPIOB, 7,  1, 0, 0, 0);
  gpio_port_mode(GPIOB, 14, 1, 0, 0, 0);

  while(1) {
    ethernet_main();
  }
}
