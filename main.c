#include "board.h"
#include <stdint.h>
#include "gpio.h"
#include "util.h"
#include "ethernet.h"
#include "cloud.h"
#include "door.h"
#include "rfid.h"

int main() {
  gpio_init();

  ethernet_init();
  cloud_init();
  door_init();
  rfid_init();

  while(1) {
    ethernet_main();
    door_main();
    rfid_main();
  }

}
