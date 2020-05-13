#include <stdint.h>
#include "board.h"
#include "gpio.h"
#include "rfid.h"
#include "door.h"
#include "util.h"
#include "string.h"
#include "ethernet.h"

volatile uint8_t card_in_bits[256];
volatile uint8_t card_in_pos;
volatile struct time_t card_in_timeout;

void rfid_init() {
  gpio_port_mode(GPIOC, 11, 0, 0, 0, 0); // RFID Magstripe Mode Clock
  gpio_port_mode(GPIOC, 12, 0, 0, 0, 0); // RFID Magstripe Mode Data
  SYSCFG->EXTICR[3-1] = SYSCFG_EXTICR3_EXTI11_PC;
  EXTI->IMR  = (1<<11);
  EXTI->FTSR = (1<<11);
  NVIC->ISER[((uint32_t)(EXTI15_10_IRQn) >> 5)] = (1 << ((uint32_t)(EXTI15_10_IRQn) & 0x1F));
  card_in_pos = 0;
  card_in_timeout.sec = 0;
}
static uint8_t start[] = {0,0,0,0,0,0,0,0,0,0,1,1,0,1,0};

void handle_incoming_card(uint8_t * number, uint8_t length) {
  if(length == 14)
    door_open_timed(1);
}

void rfid_main() {
  uint8_t bytes[32];
  if(card_in_timeout.sec && time_passed(&card_in_timeout)) {
    // Card reader stopped sending data. Look for a valid card number.
    for(int n=0; n<32; n++) {
      if(!memcmp_volatile(start, card_in_bits + n, 15)) {
        // Found a start sequence
        for(int m=0; m<32; m++) {
          bytes[m]  = (card_in_bits[n + 15 + m * 5 + 0] << 0);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 1] << 1);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 2] << 2);
          bytes[m] |= (card_in_bits[n + 15 + m * 5 + 3] << 3);
          if(bytes[m] == 0xf) {
            // Found an end byte
            handle_incoming_card(bytes, m);
            break;
          }
        }
        break;
      }
    }
    memset_volatile(card_in_bits, 0, sizeof(card_in_bits));
    card_in_timeout.sec = 0;
    card_in_pos = 0;
  }
}

void EXTI15_10_IRQHandler() {
  card_in_bits[card_in_pos++] = !(GPIOC->IDR & (1<<12));
  EXTI->PR = (1<<11);
  time_set(&card_in_timeout, 0, 100);
}
