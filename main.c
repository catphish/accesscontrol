#include <stm32f439xx.h>
//#include <stm32f407xx.h>
#include <stdint.h>
#include "gpio.h"
#include "util.h"
#include "ethernet.h"

int main() {
  gpio_init();
  gpio_port_mode(GPIOB, 0,  1, 0, 0, 0);
  gpio_port_mode(GPIOB, 7,  1, 0, 0, 0);
  gpio_port_mode(GPIOB, 14, 1, 0, 0, 0);
//  GPIOB->ODR = (1<<14);

  ethernet_init();

  while(1) {
    ethernet_main();
  }
}
